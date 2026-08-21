#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <cassert>
#include <fcntl.h>
#include <poll.h>
#include <cstdint>
#include <string>

int am62x_gpio_num(std::string port, int pin);

struct Am62xGpioDescriptor
{
	std::string port;
	int pin = -1;
	uint32_t gpioBitShift = 0;
	uint32_t gpioBitMask = 0;
	uint32_t gpioBankIndex = 0;
	uint32_t gpioBaseAddr = 0;
	uint32_t regDirOffset = 0;
	uint32_t regSetOffset = 0;
	uint32_t regClrOffset = 0;
	uint32_t regInOffset = 0;
	uint32_t padcfgBaseAddr = 0;
	uint32_t padOffset = 0;
	uint32_t padConfigValue = 0;
};

Am62xGpioDescriptor am62x_gpio_descriptor(std::string port, int pin);

class Gpio
{
public:
	Gpio() {} // default constructor
	Gpio(int32_t pin); // constructor:  direction="in", no poll() support
  Gpio(int32_t pin, const char *edge); // constructor:  direction="in" + poll() support
	Gpio(int32_t pin, int32_t initialValue); // constructor:  direction="out"
  Gpio(const Gpio& other) = delete; // copy constructor
	Gpio(Gpio&& other) // move constructor
	{
		pin = other.pin;
		direction = other.direction;
		edge = other.edge;
		fd = other.fd;
		fdset = other.fdset;
		exported = other.exported;
		opened = other.opened;
		other.exported = false; // prevent destructor from unexporting
		other.opened = false; // prevent destructor from closing
	}
	~Gpio(); // destructor
  Gpio& operator= (const Gpio& other) = delete; // copy assignment operator
  Gpio& operator= (Gpio&& other) // move assignment operator
	{
		pin = other.pin;
		direction = other.direction;
		edge = other.edge;
		fd = other.fd;
		fdset = other.fdset;
		exported = other.exported;
		opened = other.opened;
		other.exported = false; // prevent destructor from unexporting
		other.opened = false; // prevent destructor from closing
		return *this;
	}
	int32_t Export();
	int32_t Unexport();
	int32_t SetDirection(const char *direction); // in/out
	int32_t SetValue(int32_t value);
	int32_t GetValue(int32_t *value);
	int32_t SetEdge(const char *edge); // rising/falling/both
	int32_t Poll(int32_t timeoutMs);
	int32_t Open();
	int32_t Close();
	bool isOpen() { return opened; }
private:
  int32_t pin = -1;
	const char *direction = nullptr;
	const char *edge = nullptr;
	int32_t fd = -1;
	struct pollfd fdset;
	bool exported = false;
	bool opened = false;
};





