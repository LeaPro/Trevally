#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <thread>
#include "gpio.h"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cstdint>

struct Am62xGpioEntry
{
	const char *port;
	int pin;
	uint32_t padOffset;
};

static const Am62xGpioEntry kAm62xGpioEntries[] = {
	{"GPIO0", 0, 0x000},
	{"GPIO0", 1, 0x004},
	{"GPIO0", 2, 0x008},
	{"GPIO0", 3, 0x00C},
	{"GPIO0", 4, 0x010},
	{"GPIO0", 5, 0x014},
	{"GPIO0", 6, 0x018},
	{"GPIO0", 7, 0x01C},
	{"GPIO0", 8, 0x020},
	{"GPIO0", 9, 0x024},
	{"GPIO0", 10, 0x028},
	{"GPIO0", 11, 0x02C},
	{"GPIO0", 12, 0x030},
	{"GPIO0", 13, 0x034},
	{"GPIO0", 14, 0x038},
	{"GPIO0", 15, 0x03C},
	{"GPIO0", 16, 0x040},
	{"GPIO0", 17, 0x044},
	{"GPIO0", 18, 0x048},
	{"GPIO0", 19, 0x04C},
	{"GPIO0", 20, 0x050},
	{"GPIO0", 21, 0x054},
	{"GPIO0", 22, 0x058},
	{"GPIO0", 23, 0x05C},
	{"GPIO0", 24, 0x060},
	{"GPIO0", 25, 0x064},
	{"GPIO0", 26, 0x068},
	{"GPIO0", 27, 0x06C},
	{"GPIO0", 28, 0x070},
	{"GPIO0", 29, 0x074},
	{"GPIO0", 30, 0x078},
	{"GPIO0", 31, 0x07C},
	{"GPIO0", 32, 0x084},
	{"GPIO0", 33, 0x088},
	{"GPIO0", 34, 0x08C},
	{"GPIO0", 35, 0x090},
	{"GPIO0", 36, 0x094},
	{"GPIO0", 37, 0x098},
	{"GPIO0", 38, 0x09C},
	{"GPIO0", 39, 0x0A0},
	{"GPIO0", 40, 0x0A4},
	{"GPIO0", 41, 0x0A8},
	{"GPIO0", 42, 0x0AC},
	{"GPIO0", 43, 0x0B0},
	{"GPIO0", 44, 0x0B4},
	{"GPIO0", 45, 0x0B8},
	{"GPIO0", 46, 0x0BC},
	{"GPIO0", 47, 0x0C0},
	{"GPIO0", 48, 0x0C4},
	{"GPIO0", 49, 0x0C8},
	{"GPIO0", 50, 0x0CC},
	{"GPIO0", 51, 0x0D0},
	{"GPIO0", 52, 0x0D4},
	{"GPIO0", 53, 0x0D8},
	{"GPIO0", 54, 0x0DC},
	{"GPIO0", 55, 0x0E0},
	{"GPIO0", 56, 0x0E4},
	{"GPIO0", 57, 0x0E8},
	{"GPIO0", 58, 0x0EC},
	{"GPIO0", 59, 0x0F0},
	{"GPIO0", 60, 0x0F4},
	{"GPIO0", 61, 0x0F8},
	{"GPIO0", 62, 0x0FC},
	{"GPIO0", 63, 0x100},
	{"GPIO0", 64, 0x104},
	{"GPIO0", 65, 0x108},
	{"GPIO0", 66, 0x10C},
	{"GPIO0", 67, 0x110},
	{"GPIO0", 68, 0x114},
	{"GPIO0", 69, 0x118},
	{"GPIO0", 70, 0x120},
	{"GPIO0", 71, 0x124},
	{"GPIO0", 72, 0x128},
	{"GPIO0", 73, 0x12C},
	{"GPIO0", 74, 0x130},
	{"GPIO0", 75, 0x134},
	{"GPIO0", 76, 0x138},
	{"GPIO0", 77, 0x13C},
	{"GPIO0", 78, 0x140},
	{"GPIO0", 79, 0x144},
	{"GPIO0", 80, 0x148},
	{"GPIO0", 81, 0x14C},
	{"GPIO0", 82, 0x150},
	{"GPIO0", 83, 0x154},
	{"GPIO0", 84, 0x158},
	{"GPIO0", 85, 0x15C},
	{"GPIO0", 86, 0x160},
	{"GPIO0", 87, 0x164},
	{"GPIO0", 88, 0x168},
	{"GPIO0", 89, 0x16C},
	{"GPIO0", 90, 0x170},
	{"GPIO0", 91, 0x174},
	{"GPIO1", 0, 0x178},
	{"GPIO1", 1, 0x17C},
	{"GPIO1", 2, 0x180},
	{"GPIO1", 3, 0x184},
	{"GPIO1", 4, 0x188},
	{"GPIO1", 5, 0x18C},
	{"GPIO1", 6, 0x190},
	{"GPIO1", 7, 0x194},
	{"GPIO1", 8, 0x198},
	{"GPIO1", 9, 0x19C},
	{"GPIO1", 10, 0x1A0},
	{"GPIO1", 11, 0x1A4},
	{"GPIO1", 12, 0x1A8},
	{"GPIO1", 13, 0x1AC},
	{"GPIO1", 14, 0x1B0},
	{"GPIO1", 15, 0x1B4},
	{"GPIO1", 16, 0x1B8},
	{"GPIO1", 17, 0x1BC},
	{"GPIO1", 18, 0x1C0},
	{"GPIO1", 19, 0x1C4},
	{"GPIO1", 20, 0x1C8},
	{"GPIO1", 21, 0x1CC},
	{"GPIO1", 22, 0x1D0},
	{"GPIO1", 23, 0x1D4},
	{"GPIO1", 24, 0x1D8},
	{"GPIO1", 25, 0x1DC},
	{"GPIO1", 26, 0x1E0},
	{"GPIO1", 27, 0x1E4},
	{"GPIO1", 28, 0x1E8},
	{"GPIO1", 29, 0x1EC},
	{"GPIO1", 30, 0x1F0},
	{"GPIO1", 31, 0x1F4},
	{"GPIO1", 32, 0x1F8},
	{"GPIO1", 33, 0x1FC},
	{"GPIO1", 34, 0x200},
	{"GPIO1", 35, 0x204},
	{"GPIO1", 36, 0x208},
	{"GPIO1", 37, 0x20C},
	{"GPIO1", 38, 0x210},
	{"GPIO1", 39, 0x214},
	{"GPIO1", 40, 0x218},
	{"GPIO1", 41, 0x220},
	{"GPIO1", 42, 0x224},
	{"GPIO1", 43, 0x228},
	{"GPIO1", 44, 0x22C},
	{"GPIO1", 45, 0x230},
	{"GPIO1", 46, 0x234},
	{"GPIO1", 47, 0x23C},
	{"GPIO1", 48, 0x240},
	{"GPIO1", 49, 0x244},
	{"GPIO1", 50, 0x254},
	{"GPIO1", 51, 0x258},
	{"MCU_GPIO0", 0, 0x000},
	{"MCU_GPIO0", 1, 0x004},
	{"MCU_GPIO0", 2, 0x008},
	{"MCU_GPIO0", 3, 0x00C},
	{"MCU_GPIO0", 4, 0x010},
	{"MCU_GPIO0", 5, 0x014},
	{"MCU_GPIO0", 6, 0x018},
	{"MCU_GPIO0", 7, 0x01C},
	{"MCU_GPIO0", 8, 0x020},
	{"MCU_GPIO0", 9, 0x024},
	{"MCU_GPIO0", 10, 0x028},
	{"MCU_GPIO0", 11, 0x02C},
	{"MCU_GPIO0", 12, 0x030},
	{"MCU_GPIO0", 13, 0x034},
	{"MCU_GPIO0", 14, 0x038},
	{"MCU_GPIO0", 15, 0x03C},
	{"MCU_GPIO0", 16, 0x040},
	{"MCU_GPIO0", 17, 0x044},
	{"MCU_GPIO0", 18, 0x048},
	{"MCU_GPIO0", 19, 0x04C},
	{"MCU_GPIO0", 20, 0x050},
	{"MCU_GPIO0", 21, 0x054},
	{"MCU_GPIO0", 22, 0x058},
	{"MCU_GPIO0", 23, 0x05C},
};

static uint32_t am62x_controller_base_addr(const std::string &port)
{
	if (port == "GPIO0")
	{
		return 0x00600000;
	}
	if (port == "GPIO1")
	{
		return 0x00601000;
	}
	if (port == "MCU_GPIO0")
	{
		return 0x04201000;
	}
	throw std::invalid_argument("Unknown GPIO port: " + port);
}

static int am62x_sysfs_gpio_base(const std::string &port)
{
	// Board-specific fixed Linux GPIO bases.
	if (port == "GPIO0")
	{
		return 539;
	}
	if (port == "GPIO1")
	{
		return 631;
	}
	if (port == "MCU_GPIO0")
	{
		return 515;
	}
	throw std::invalid_argument("Unknown GPIO port: " + port);
}

static const Am62xGpioEntry *find_am62x_gpio_entry(const std::string &port, int pin)
{
	for (const Am62xGpioEntry &entry : kAm62xGpioEntries)
	{
		if (port == entry.port && pin == entry.pin)
		{
			return &entry;
		}
	}
	return nullptr;
}

const uint32_t AM62X_GPIO_REG_DIR = 0x10;
const uint32_t AM62X_GPIO_REG_SET = 0x18;
const uint32_t AM62X_GPIO_REG_CLR = 0x1C;
const uint32_t AM62X_GPIO_REG_IN = 0x20;

const uint32_t AM62X_PADCFG_BASE = 0x000F4000;
const uint32_t AM62X_PADCFG_GPIO_MODE_PULLUP_RXACTIVE = 0x00060007;

static std::string normalize_port(std::string port)
{
	std::transform(port.begin(), port.end(), port.begin(), [](unsigned char c) {
		return static_cast<char>(std::toupper(c));
	});
	return port;
}

Am62xGpioDescriptor am62x_gpio_descriptor(std::string port, int pin)
{
	if (pin < 0)
	{
		throw std::invalid_argument("GPIO pin must be non-negative: " + std::to_string(pin));
	}

	port = normalize_port(port);
	const Am62xGpioEntry *entry = find_am62x_gpio_entry(port, pin);
	if (entry == nullptr)
	{
		throw std::invalid_argument("No static GPIO metadata available for " + port + "_" + std::to_string(pin));
	}

	Am62xGpioDescriptor desc;
	desc.port = port;
	desc.pin = pin;
	desc.gpioBitShift = static_cast<uint32_t>(pin % 32);
	desc.gpioBitMask = static_cast<uint32_t>(1u << desc.gpioBitShift);
	desc.gpioBankIndex = static_cast<uint32_t>(pin / 32);
	desc.gpioBaseAddr = am62x_controller_base_addr(port);
	desc.regDirOffset = AM62X_GPIO_REG_DIR;
	desc.regSetOffset = AM62X_GPIO_REG_SET;
	desc.regClrOffset = AM62X_GPIO_REG_CLR;
	desc.regInOffset = AM62X_GPIO_REG_IN;
	desc.padcfgBaseAddr = AM62X_PADCFG_BASE;
	desc.padOffset = entry->padOffset;
	desc.padConfigValue = AM62X_PADCFG_GPIO_MODE_PULLUP_RXACTIVE;
	return desc;
}

/**
 * Maps a given AM62x GPIO port and pin offset to the global Linux sysfs GPIO number.
 * * @param port The port name (e.g., "GPIO0", "GPIO1", "MCU_GPIO0")
 * @param pin The pin offset inside the bank (typically 0-31)
 * @return The global Linux GPIO integer used for /sys/class/gpio/export
 */
int am62x_gpio_num(std::string port, int pin)
{
	if (pin < 0)
	{
		throw std::invalid_argument("GPIO pin must be non-negative: " + std::to_string(pin));
	}

	port = normalize_port(port);
	const Am62xGpioEntry *entry = find_am62x_gpio_entry(port, pin);
	if (entry == nullptr)
	{
		throw std::invalid_argument("No static GPIO metadata available for " + port + "_" + std::to_string(pin));
	}
	return am62x_sysfs_gpio_base(port) + pin;
}

#define SYSFS_GPIO_DIR "/sys/class/gpio"
#define BUF_SIZE 64

Gpio::Gpio(int32_t pin) // constructor:  direction="in", no poll() support
{
	this->pin = pin;
	Export();
	SetDirection("in");
	Open();
}
Gpio::Gpio(int32_t pin, const char *edge) // constructor:  direction="in" + poll() support
{
	this->pin = pin;
	Export();
	SetDirection("in");
	SetEdge(edge);
	Open();
}
Gpio::Gpio(int32_t pin, int32_t initialValue) // constructor:  direction="high" or "low"
{
	this->pin = pin;
	Export();
	SetDirection(initialValue == 1 ? "high" : "low");
	Open();
}
Gpio::~Gpio() // destructor
{
	//if (this->exported) Unexport(); // really don't want to do this, because it will revert the pin to its default state
	if (this->opened) Close();
}

int32_t Gpio::Export()
{
	int32_t fd, len;
	char buf[BUF_SIZE];
	char valueBuf[BUF_SIZE];
	char directionBuf[BUF_SIZE];
	char edgeBuf[BUF_SIZE];

	fd = open(SYSFS_GPIO_DIR "/export", O_WRONLY);
	if (fd < 0) {
		perror("Gpio::Export");
		return fd;
	}

	len = snprintf(buf, sizeof(buf), "%d", this->pin);
	int tmp = write(fd, buf, len);
	close(fd);
	if (tmp == -1)
	{
		// perror("Gpio::Export"); // likely pin just already exported
		return tmp;
	}

  // wait while sysFs is created; see https://stackoverflow.com/questions/39524234/bug-with-writing-to-file-in-linux-sys-class-gpio
  sprintf(valueBuf, "%s/gpio%d/value", SYSFS_GPIO_DIR, this->pin);
  sprintf(directionBuf, "%s/gpio%d/direction", SYSFS_GPIO_DIR, this->pin);
  sprintf(edgeBuf, "%s/gpio%d/edge", SYSFS_GPIO_DIR, this->pin);
	const int timeout = 10000;
  for (int i = 0; i < 10000; i++)
	{
		if (i == timeout-1)
		{
			printf("Gpio::Export timeout waiting for gpio%d sysfs entries to be created\n", this->pin);
			return -1;
		}
		else if (access(valueBuf, F_OK | R_OK | W_OK) == 0 &&
        access(directionBuf, F_OK | R_OK | W_OK) == 0 &&
        access(edgeBuf, F_OK | R_OK | W_OK) == 0)
		{
			break;
		}
		else
		{
    	std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}

  exported = true;

	return 0;
}

int32_t Gpio::Unexport()
{
	int32_t fd, len;
	char buf[BUF_SIZE];

	fd = open(SYSFS_GPIO_DIR "/unexport", O_WRONLY);
	if (fd < 0) {
		perror("Gpio::Unexport");
		return fd;
	}

	len = snprintf(buf, sizeof(buf), "%d", this->pin);
	if (write(fd, buf, len) == -1) abort();
	close(fd);
	exported = false;
	return 0;
}

int32_t Gpio::SetDirection(const char *direction)
{
	int32_t fd;
	char buf[BUF_SIZE];

	snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR  "/gpio%d/direction", this->pin);

	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		perror("Gpio::SetDirection");
		return fd;
	}

	if (write(fd, direction, strlen(direction)+1) == -1) abort();

	close(fd);
	this->direction = direction;
	return 0;
}

int32_t Gpio::SetValue(int32_t value)
{
	int32_t fd;
	char buf[BUF_SIZE];

	snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", this->pin);

	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		perror("Gpio::SetValue");
		return fd;
	}

	if (value)
	{
		if (write(fd, "1", 2) == -1) abort();
	}
	else
	{
		if (write(fd, "0", 2) == -1) abort();
	}

	close(fd);
	return 0;
}

int32_t Gpio::GetValue(int32_t *value)
{
	int32_t fd;
	char buf[BUF_SIZE];
	char ch;

	snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", this->pin);

	fd = open(buf, O_RDONLY);
	if (fd < 0) {
		perror("Gpio::GetValue");
		return fd;
	}

	if (read(fd, &ch, 1) == -1) abort();

	if (ch != '0') {
		*value = 1;
	} else {
		*value = 0;
	}

	close(fd);
	return 0;
}

int32_t Gpio::SetEdge(const char *edge)
{
	int32_t fd, len;
	char buf[BUF_SIZE];

	snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/edge", this->pin);

	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		perror("Gpio::SetEdge");
		return fd;
	}

	len = strlen(edge) + 1;
	if (write(fd, edge, len) == -1) abort(); 
	close(fd);
	this->edge = edge;
	return 0;
}

int32_t Gpio::Poll(int32_t timeoutMs)
{
	if (!strcmp(this->direction, "in") && this->edge != nullptr)
	{
		memset((void*)&this->fdset, 0, sizeof(this->fdset));
		this->fdset.fd = this->fd;
		this->fdset.events = POLLPRI;
RetryInterruptedSystemCall:
		int32_t	rc = poll(&fdset, 1, timeoutMs);      
		if (rc < 0) {
			if (errno == EINTR) // can happen when profiling
			  goto RetryInterruptedSystemCall;
			printf("\npoll() failed!\n");
			return -1;
		}
		else if (rc == 0) { // timeout
		}
		else if (this->fdset.revents & POLLPRI) { // interrupt: clear it
			char buf[1];
			lseek(this->fdset.fd, 0, SEEK_SET);
			if (read(this->fdset.fd, buf, 1) == -1) abort();
			rc = 1;
		}
		else { //?
			rc = 2;
		}
		return rc;
	}
	else
	{
		printf("\ngpio not configured for poll()!\n");
		return -1;
	}
}

int32_t Gpio::Open()
{
	if (this->fd < 0)
	{
		char buf[BUF_SIZE];
		snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", this->pin);
	
		this->fd = open(buf, O_RDONLY | O_NONBLOCK );
		if (this->fd < 0) {
			perror("Gpio::Open");
		}
		if (!strcmp(this->direction, "in") && this->edge != nullptr)
		{
		  // clear initial interrupt (otherwise first call to Poll()/poll() returns immediately)
			if (Poll(1) == 1)
			{
				if (read(this->fdset.fd, buf, 1) == -1) abort(); 
			}
		}
	}
	opened = true;
	return this->fd;
}

int32_t Gpio::Close()
{
	int32_t rc = 0;
	if (this->fd >= 0)
	{
		rc = close(this->fd);
		this->fd = -1;
	}
	opened = false;
	return rc;
}
