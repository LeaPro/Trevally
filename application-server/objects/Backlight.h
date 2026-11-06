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
// inactivity and restored on the next touch.
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

private:
  bool locateDevice();
  bool applyPercent(float percent);

  std::string deviceDir; // e.g. /sys/class/backlight/backlight (empty until located)
  long maxBrightness = 0;
  bool screenOff = false; // true while the inactivity timeout has the backlight off
};
