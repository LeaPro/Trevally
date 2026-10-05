#pragma once
#include <cstdint>

// Raw I2C driver for the IS31FL3235A 28-channel LED driver (on &main_i2c3 / /dev/i2c-3, addr 0x3c).
// No kernel devicetree binding is used for this chip -- it's bit-banged directly from userspace
// Caller is responsible for driving the LED_CTRL_nRESET gpio (gpio0 pin 3) high before use.
class led_controller final
{
public:
  static constexpr uint8_t NUM_CHANNELS = 28; // OUT1..OUT28
  static constexpr uint8_t I2C_ADDRESS = 0x3c;

  static bool initialize();
  static void shutdown(bool shutdown);
  static bool setChannelBrightness(uint8_t channel, uint8_t brightness);
  static bool setAllBrightness(uint8_t brightness);
};
