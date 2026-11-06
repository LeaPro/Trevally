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
  // software reset: all registers to defaults (chip in shutdown, all outputs off). The chip is on
  // always-on power, so it keeps whatever a previous process left in it across reboots.
  static bool reset();
  static void shutdown(bool shutdown);
  static bool setChannelBrightness(uint8_t channel, uint8_t brightness);
  static bool setAllBrightness(uint8_t brightness);
  // sets every RGB LED to the same color (single latch)
  static bool setRgb(uint8_t r, uint8_t g, uint8_t b);
  // breathes all RGB LEDs at the given color on a background thread until stopPulse()
  static void startPulse(uint8_t r, uint8_t g, uint8_t b, uint32_t periodMs);
  static void stopPulse();

  // Each RGB group drives two LEDs in parallel; per schematic OUTn=B, OUTn+1=R, OUTn+2=G.
  // 0-based channels for OUT1, OUT9, OUT17, OUT25
  static constexpr uint8_t RGB_GROUP_BASE[] = {0, 8, 16, 24};
  static constexpr uint8_t BLUE_OFFSET = 0;
  static constexpr uint8_t RED_OFFSET = 1;
  static constexpr uint8_t GREEN_OFFSET = 2;
};
