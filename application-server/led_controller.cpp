#include "led_controller.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <cmath>

#ifdef __ARM_ARCH
#include <linux/i2c-dev.h>
#endif

static std::recursive_mutex mutex_;
static bool enabled = true;
static int fd = -1;

bool led_controller::initialize()
{
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  enabled = true;
  bool result = false;
#ifdef __ARM_ARCH
  // main_i2c3 -- confirmed via `i2cdetect -l` to enumerate as /dev/i2c-3 (devicetree
  // aliases i2c0/2/3/5 map 1:1 to the kernel's adapter numbers on this board)
  if (fd < 0)
    fd = open("/dev/i2c-3", O_RDWR);
  if (fd >= 0)
  {
    if (ioctl(fd, I2C_SLAVE, led_controller::I2C_ADDRESS) != -1)
    {
      uint8_t buf[2];
      buf[0] = 0x00; buf[1] = 0x01; // take out of shutdown
      if (write(fd, buf, sizeof(buf)) == (ssize_t)sizeof(buf))
      {
        // enable all outputs @ 1/2 current
        buf[1] = 0x03;
        for (uint8_t reg = 0x2a; reg <= 0x45; reg++)
        {
          buf[0] = reg;
          write(fd, buf, sizeof(buf));
        }
        result = true;
        printf("led_controller::initialize success\n");
      }
      else
      {
        perror("led_controller::initialize write");
      }
    }
    else
    {
      perror("led_controller::initialize ioctl");
    }
  }
  else
  {
    perror("led_controller::initialize open");
  }
#endif
  if (!result)
    shutdown(true);
  return result;
}

bool led_controller::reset()
{
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  bool result = false;
#ifdef __ARM_ARCH
  if (fd < 0)
    fd = open("/dev/i2c-3", O_RDWR);
  if (fd >= 0 && ioctl(fd, I2C_SLAVE, led_controller::I2C_ADDRESS) != -1)
  {
    uint8_t buf[2] = {0x4f, 0x00}; // reset register
    result = write(fd, buf, sizeof(buf)) == (ssize_t)sizeof(buf);
  }
  if (!result)
    perror("led_controller::reset");
#endif
  enabled = false;
  return result;
}

void led_controller::shutdown(bool shutdownFlag)
{
  std::lock_guard<std::recursive_mutex> lock(mutex_);
#ifdef __ARM_ARCH
  if (enabled && fd >= 0)
  {
    if (ioctl(fd, I2C_SLAVE, led_controller::I2C_ADDRESS) < 0)
    {
      enabled = false;
    }
    else
    {
      uint8_t buf[2];
      buf[0] = 0x00; buf[1] = shutdownFlag ? 0x00 : 0x01;
      if (write(fd, buf, sizeof(buf)) < 0)
        enabled = false;
    }
  }
#endif
  if (shutdownFlag)
    enabled = false;
}

bool led_controller::setChannelBrightness(uint8_t channel, uint8_t brightness)
{
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (channel >= NUM_CHANNELS)
    return false;
#ifdef __ARM_ARCH
  if (!enabled || fd < 0)
    return false;
  if (ioctl(fd, I2C_SLAVE, led_controller::I2C_ADDRESS) < 0)
  {
    enabled = false;
    return false;
  }
  uint8_t buf[2];
  buf[0] = 0x05 + channel; buf[1] = brightness;
  if (write(fd, buf, sizeof(buf)) < 0)
  {
    enabled = false;
    return false;
  }
  buf[0] = 0x25; buf[1] = 0x00; // latch/update register
  if (write(fd, buf, sizeof(buf)) < 0)
  {
    enabled = false;
    return false;
  }
  return true;
#else
  return false;
#endif
}

bool led_controller::setAllBrightness(uint8_t brightness)
{
  bool ok = true;
  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++)
    ok = setChannelBrightness(ch, brightness) && ok;
  return ok;
}

bool led_controller::setRgb(uint8_t r, uint8_t g, uint8_t b)
{
  std::lock_guard<std::recursive_mutex> lock(mutex_);
#ifdef __ARM_ARCH
  if (!enabled || fd < 0)
    return false;
  if (ioctl(fd, I2C_SLAVE, led_controller::I2C_ADDRESS) < 0)
  {
    enabled = false;
    return false;
  }
  uint8_t buf[2];
  for (uint8_t base : RGB_GROUP_BASE)
  {
    const uint8_t values[][2] = {{BLUE_OFFSET, b}, {RED_OFFSET, r}, {GREEN_OFFSET, g}};
    for (auto &v : values)
    {
      buf[0] = 0x05 + base + v[0]; buf[1] = v[1];
      if (write(fd, buf, sizeof(buf)) < 0)
      {
        enabled = false;
        return false;
      }
    }
  }
  buf[0] = 0x25; buf[1] = 0x00; // latch/update register
  if (write(fd, buf, sizeof(buf)) < 0)
  {
    enabled = false;
    return false;
  }
  return true;
#else
  (void)r; (void)g; (void)b;
  return false;
#endif
}

static std::thread pulseThread;
static std::mutex pulseMutex;
static std::condition_variable pulseCv;
static bool pulseStop = false;

void led_controller::startPulse(uint8_t r, uint8_t g, uint8_t b, uint32_t periodMs)
{
  stopPulse();
  {
    std::lock_guard<std::mutex> lk(pulseMutex);
    pulseStop = false;
  }
  pulseThread = std::thread([r, g, b, periodMs]()
  {
    const auto start = std::chrono::steady_clock::now();
    std::unique_lock<std::mutex> lk(pulseMutex);
    while (!pulseStop)
    {
      double phase = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / periodMs;
      double level = 0.5 - 0.5 * std::cos(2.0 * M_PI * phase); // 0..1, starts dark
      level *= level; // rough gamma so the fade looks even to the eye
      setRgb((uint8_t)std::lround(r * level), (uint8_t)std::lround(g * level), (uint8_t)std::lround(b * level));
      pulseCv.wait_for(lk, std::chrono::milliseconds(20), [] { return pulseStop; });
    }
  });
}

void led_controller::stopPulse()
{
  if (!pulseThread.joinable())
    return;
  {
    std::lock_guard<std::mutex> lk(pulseMutex);
    pulseStop = true;
  }
  pulseCv.notify_all();
  pulseThread.join();
  setRgb(0, 0, 0);
}
