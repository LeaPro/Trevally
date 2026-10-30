#pragma once
#include <ctime>

// Userspace access to the battery-backed RTC (TI BQ32000 on &main_i2c0, address 0x68;
// the "rtc@68" node in am62x-lea-trevally.dts, aliased as rtc0). The kernel driver exposes
// it as /dev/rtc0; this class talks to it with the RTC_RD_TIME / RTC_SET_TIME ioctls.
//
// The RTC always holds UTC (the usual Linux convention); all time_t values passed to / returned
// from this class are UTC epoch seconds. Local-time conversion is the caller's responsibility.
class Rtc final
{
public:
  static constexpr const char *DEVICE = "/dev/rtc0";

  // Read the RTC. Returns false (and leaves utc untouched) on failure.
  static bool read(time_t &utc);
  // Set the RTC to the given UTC time. Returns false on failure.
  static bool write(time_t utc);
  // Set the RTC from the current system clock (like `hwclock --systohc`).
  static bool writeFromSystemClock();
  // Set the system clock (CLOCK_REALTIME) to the given UTC time. Requires root.
  static bool setSystemClock(time_t utc);
};
