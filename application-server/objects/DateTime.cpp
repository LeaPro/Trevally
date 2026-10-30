#include "objects.h"
#include "DateTime.h"
#include "rtc.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>

using std::string;

static const char *const MODE_AUTO = "auto";
static const char *const MODE_MANUAL = "manual";

DateTime::DateTime(string path, Object *parent) : LeafObject(path, parent)
{
  string name;
  name = "mode"; elements[name] = modePtr = std::make_shared<StringControl>(path + "/" + name, this, MODE_AUTO/*default*/, 16);
  name = "dst"; elements[name] = dstPtr = std::make_shared<BoolControl>(path + "/" + name, this, false/*default*/);
  name = "timeFormat"; elements[name] = timeFormatPtr = std::make_shared<StringControl>(path + "/" + name, this, "12"/*default*/, 4);
  name = "timezone"; elements[name] = timezonePtr = std::make_shared<FloatControl>(path + "/" + name, this, -12.0f/*min*/, 14.0f/*max*/, 0.0f/*default*/, 1.0f/*multipleOf*/, "hours"/*units*/);
  // one-shot command: not persisted, so a stale value is never re-applied on restart
  name = "setTime"; elements[name] = setTimePtr = std::make_shared<StringControl>(path + "/" + name, this, ""/*default*/, 32, false/*persistFlag*/);
  name = "displayTime"; elements[name] = displayTimePtr = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 16);
}

void DateTime::initialize()
{
  LeafObject::initialize();
}

long DateTime::utcOffsetSeconds()
{
  long hours = std::lround(timezonePtr->get()) + (dstPtr->get() ? 1 : 0);
  return hours * 3600L;
}

// Applies the offset to this process's local time (localtime() etc., used by UI clocks).
// POSIX TZ offsets are positive west of GMT, i.e. the opposite sign of the user-facing offset.
void DateTime::applyTimezone()
{
  long hours = utcOffsetSeconds() / 3600L;
  char tz[24];
  snprintf(tz, sizeof(tz), "UTC%+ld", -hours);
  setenv("TZ", tz, 1);
  tzset();
}

void DateTime::applyMode()
{
#ifdef __ARM_ARCH
  // network time is provided by systemd-timesyncd; manual mode must disable it, otherwise it
  // would overwrite the time that was set by hand
  system(modePtr->get() == MODE_AUTO ? "timedatectl set-ntp true >/dev/null 2>&1"
                                     : "timedatectl set-ntp false >/dev/null 2>&1");
#endif
}

void DateTime::updateDisplayTime()
{
  time_t now = time(nullptr);
  struct tm tm;
  localtime_r(&now, &tm); // honors the TZ set by applyTimezone()
  char buf[16];
  if (timeFormatPtr->get() == "24")
    snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
  else
  {
    int h12 = tm.tm_hour % 12;
    snprintf(buf, sizeof(buf), "%d:%02d %s", h12 == 0 ? 12 : h12, tm.tm_min, tm.tm_hour < 12 ? "AM" : "PM");
  }
  if (displayTimePtr->get() != buf)
    displayTimePtr->set(buf);
}

// text is local time ("YYYY-MM-DD HH:MM:SS"); the RTC and system clock are set to UTC.
bool DateTime::applySetTime(const string &text)
{
  int year, month, day, hour, minute, second = 0;
  int n = sscanf(text.c_str(), "%d-%d-%d%*[ T]%d:%d:%d", &year, &month, &day, &hour, &minute, &second);
  if (n < 5 || year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31 ||
      hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 60)
  {
    printf("DateTime: invalid setTime \"%s\" (expected YYYY-MM-DD HH:MM:SS)\n", text.c_str()); fflush(stdout);
    return false;
  }
  struct tm tm = {};
  tm.tm_year = year - 1900;
  tm.tm_mon = month - 1;
  tm.tm_mday = day;
  tm.tm_hour = hour;
  tm.tm_min = minute;
  tm.tm_sec = second;
  time_t utc = timegm(&tm) - utcOffsetSeconds();
  printf("DateTime: setting time to %s (local), utc epoch = %lld\n", text.c_str(), (long long)utc); fflush(stdout);
#ifdef __ARM_ARCH
  // host/dev build: never touch the development machine's own clock
  bool ok = Rtc::setSystemClock(utc);
  ok = Rtc::write(utc) && ok;
  return ok;
#else
  return true;
#endif
}

void DateTime::update(bool sensorsOnly, bool refreshVolatileElements)
{
  if (sensorsOnly)
  {
    updateDisplayTime();
    return;
  }

  // note: isModified() with a list clears every listed element's flag
  bool settingsChanged = isModified({modePtr, dstPtr, timezonePtr});
  bool formatChanged = isModified({timeFormatPtr});
  bool setTimeRequested = isModified({setTimePtr});

  if (formatChanged && timeFormatPtr->get() != "12" && timeFormatPtr->get() != "24")
    timeFormatPtr->set("12");

  if (settingsChanged || !applied)
  {
    if (modePtr->get() != MODE_AUTO && modePtr->get() != MODE_MANUAL)
      modePtr->set(MODE_AUTO);
    applyTimezone();
    applyMode();
    applied = true;
  }

  string text = setTimePtr->get();
  if (setTimeRequested && !text.empty())
  {
    if (modePtr->get() == MODE_MANUAL)
      applySetTime(text);
    else
    {
      printf("DateTime: setTime ignored, mode is \"%s\" (must be \"manual\")\n", modePtr->get().c_str()); fflush(stdout);
    }
    setTimePtr->set("");
  }
  updateDisplayTime();
}
