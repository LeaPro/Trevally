#pragma once
#include <object.h>
#include <string>

// Date/time settings, backed by the on-board RTC (see rtc.h).
//
//  mode       "auto"   : the system clock follows network time (NTP); the RTC is kept in sync by the kernel.
//             "manual" : NTP is disabled and the clock is set with "setTime".
//  dst        true adds one hour to the zone offset (daylight saving time).
//  timeFormat "12" or "24": display format for UI clocks.
//  timezone   offset from GMT in whole hours (-12..14).
//  setTime    only honoured in "manual" mode. Local time (timezone + dst applied) formatted as
//             "YYYY-MM-DD HH:MM:SS" ('T' separator and omitted seconds are also accepted).
//             Writes the system clock and the RTC (as UTC), then reads back as "" (not persisted).
//  displayTime (sensor) current local time formatted for the UI per timeFormat: "3:07 PM" (12) or "15:07" (24).
class DateTime final : public LeafObject
{
public:
  DateTime(std::string path, Object *parent);
  void initialize() override;
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;

  std::shared_ptr<StringControl> modePtr;
  std::shared_ptr<BoolControl> dstPtr;
  std::shared_ptr<StringControl> timeFormatPtr;
  std::shared_ptr<FloatControl> timezonePtr;
  std::shared_ptr<StringControl> setTimePtr;
  std::shared_ptr<StringSensor> displayTimePtr;

  // Effective offset from UTC in seconds (timezone plus one hour when dst is enabled).
  long utcOffsetSeconds();

private:
  void applyTimezone();
  void applyMode();
  void updateDisplayTime();
  bool applySetTime(const std::string &text);

  bool applied = false; // true once the persisted settings have been applied after startup
};
