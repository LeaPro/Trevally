#include "led_controller.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <mutex>

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
