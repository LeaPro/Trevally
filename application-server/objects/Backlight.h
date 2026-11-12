#pragma once
#include <object.h>
#include <string>

// Display backlight control.
//
// The panel backlight is the "pwm-backlight" node in am62x-lea-trevally.dts
// (EHRPWM0_A, 20V boost PWM). The kernel exposes it as
// /sys/class/backlight/<name>/{brightness,max_brightness}; brightness is
// expressed as an index into the DT "brightness-levels" table (0..max_brightness).
//
// "brightness" is a 0-100 percentage mapped onto that range. When "timeout"
// is enabled the backlight is switched off after "timeoutSeconds" of UI
// inactivity and restored on the next touch. sleepNow() switches it off right
// away (the home screen's sleep icon), also until the next touch.
class Backlight final : public LeafObject
{
public:
  Backlight(std::string path, Object *parent);
  void initialize() override;
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;

  std::shared_ptr<FloatControl> brightnessPtr;
  std::shared_ptr<BoolControl> timeoutPtr;
  std::shared_ptr<FloatControl> timeoutSecondsPtr;

  void sleepNow();
  bool isScreenOff() const { return screenOff; }

private:
  bool locateDevice();
  bool applyPercent(float percent);

  std::string deviceDir; // e.g. /sys/class/backlight/backlight (empty until located)
  long maxBrightness = 0;
  bool screenOff = false; // true while the inactivity timeout or sleepNow() has the backlight off
  bool asleep = false;    // off by sleepNow(): stays off until the next input, whatever the timeout
  uint32_t lastInactiveMs = 0; // lv_display_get_inactive_time() on the previous tick: it drops on input
};
