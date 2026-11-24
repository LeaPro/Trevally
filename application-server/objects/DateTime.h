#pragma once
#include <object.h>
#include <string>

// Date/time settings, backed by the on-board RTC (see rtc.h).
//
//  mode       "auto"   : the system clock follows network time (NTP); the RTC is kept in sync by the kernel.
//             "manual" : NTP is disabled and the clock is set with the "setTime" method.
//  timeFormat "12" or "24": display format for UI clocks.
//  timezone   IANA identifier (e.g. "America/Indiana/Indianapolis"); DST rules come from the tz database,
//             so they are correct for any date. Must exist under /usr/share/zoneinfo; invalid values are
//             rejected and the previous zone is kept.
//  displayTime (sensor) current local time formatted for the UI per timeFormat: "3:07" (12, no AM/PM) or "15:07" (24).
//  displayDate (sensor) current local date formatted for the UI: "Mar 3".
//
// Method "setTime": params is {"time": "YYYY-MM-DD HH:MM:SS"}, local wall time in the configured timezone
//             ('T' separator and omitted seconds are also accepted).
//             Only allowed in "manual" mode. Writes the system clock and the RTC (as UTC).
//             Errors (auto mode, bad format, nonexistent local time, write failure) are returned as exceptions.
//             Local times inside the DST fall-back hour are ambiguous; the C library picks one.
class DateTime final : public LeafObject
{
public:
  DateTime(std::string path, Object *parent);
  void initialize() override;
  json processJson(const std::string &method, json &j, int client = -1) override;
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;

  std::shared_ptr<StringControl> modePtr;
  std::shared_ptr<StringControl> timeFormatPtr;
  std::shared_ptr<StringControl> timezonePtr;
  std::shared_ptr<StringSensor> displayTimePtr;
  std::shared_ptr<StringSensor> displayDatePtr;

private:
  void applyTimezone();
  void applyMode();
  void updateDisplayTime();
  void applySetTime(const std::string &text); // throws std::runtime_error on failure

  std::string appliedTimezone; // last valid IANA zone, restored when an invalid one is written
  bool applied = false; // true once the persisted settings have been applied after startup
};
