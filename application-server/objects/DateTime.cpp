#include "objects.h"
#include "DateTime.h"
#include "rtc.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <stdexcept>

using std::string;

static const char *const MODE_AUTO = "auto";
static const char *const MODE_MANUAL = "manual";
static const char *const DEFAULT_TIMEZONE = "America/Indiana/Indianapolis"; // South Bend, IN
static const char *const ZONEINFO_DIR = "/usr/share/zoneinfo/";

// true if id is a safe IANA identifier that exists in the tz database
static bool validTimezone(const string &id)
{
  if (id.empty() || id.size() > 64 || id.front() == '/' || id.find("..") != string::npos)
    return false;
  for (char c : id)
    if (!isalnum((unsigned char)c) && c != '_' && c != '+' && c != '-' && c != '/')
      return false;
  std::error_code ec;
  return std::filesystem::is_regular_file(string(ZONEINFO_DIR) + id, ec);
}

DateTime::DateTime(string path, Object *parent) : LeafObject(path, parent), appliedTimezone(DEFAULT_TIMEZONE)
{
  string name;
  name = "mode"; elements[name] = modePtr = std::make_shared<StringControl>(path + "/" + name, this, MODE_AUTO/*default*/, 16);
  name = "timeFormat"; elements[name] = timeFormatPtr = std::make_shared<StringControl>(path + "/" + name, this, "12"/*default*/, 4);
  name = "timezone"; elements[name] = timezonePtr = std::make_shared<StringControl>(path + "/" + name, this, DEFAULT_TIMEZONE/*default*/, 64);
  name = "displayTime"; elements[name] = displayTimePtr = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 16);
  name = "displayDate"; elements[name] = displayDatePtr = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 16);
  methods.insert("setTime"); // advertised via "info"
}

void DateTime::initialize()
{
  LeafObject::initialize();
}

json DateTime::processJson(const string &method, json &j, int client)
{
  if (method == "setTime")
  {
    if (!j.is_object() || !j.contains("time") || !j["time"].is_string())
      throw std::runtime_error("setTime expects {\"time\": \"YYYY-MM-DD HH:MM:SS\"}");
    if (modePtr->get() != MODE_MANUAL)
      throw std::runtime_error(string("setTime requires mode \"manual\" (current mode is \"") + modePtr->get() + "\")");
    applySetTime(j["time"].get<string>());
    updateDisplayTime();
    return true;
  }
  return LeafObject::processJson(method, j, client);
}

// Applies the IANA zone to this process's local time (localtime(), mktime(), used by UI clocks and setTime).
// An invalid zone is rejected and the last valid one is kept.
void DateTime::applyTimezone()
{
  const string requested = timezonePtr->get();
  if (validTimezone(requested))
    appliedTimezone = requested;
  else
  {
    printf("DateTime: invalid timezone \"%s\", keeping \"%s\"\n", requested.c_str(), appliedTimezone.c_str()); fflush(stdout);
    timezonePtr->set(appliedTimezone);
  }
  setenv("TZ", appliedTimezone.c_str(), 1);
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
    snprintf(buf, sizeof(buf), "%d:%02d", h12 == 0 ? 12 : h12, tm.tm_min);
  }
  if (displayTimePtr->get() != buf)
    displayTimePtr->set(buf);

  static const char *const MONTHS[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                         "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  snprintf(buf, sizeof(buf), "%s %d", MONTHS[tm.tm_mon % 12], tm.tm_mday);
  if (displayDatePtr->get() != buf)
    displayDatePtr->set(buf);
}

// text is local wall time in the configured zone ("YYYY-MM-DD HH:MM:SS"); the RTC and system clock are set to UTC.
void DateTime::applySetTime(const string &text)
{
  int year, month, day, hour, minute, second = 0;
  int n = sscanf(text.c_str(), "%d-%d-%d%*[ T]%d:%d:%d", &year, &month, &day, &hour, &minute, &second);
  if (n < 5 || year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31 ||
      hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59)
    throw std::runtime_error("invalid time \"" + text + "\" (expected YYYY-MM-DD HH:MM:SS)");

  applyTimezone(); // make sure mktime() uses the current zone
  struct tm tm = {};
  tm.tm_year = year - 1900;
  tm.tm_mon = month - 1;
  tm.tm_mday = day;
  tm.tm_hour = hour;
  tm.tm_min = minute;
  tm.tm_sec = second;
  tm.tm_isdst = -1; // let the tz database decide whether DST applies on that date
  time_t utc = mktime(&tm);
  // mktime() normalizes invalid input (Feb 30, or a local time skipped by spring-forward); reject those
  if (utc == (time_t)-1 || tm.tm_year != year - 1900 || tm.tm_mon != month - 1 || tm.tm_mday != day ||
      tm.tm_hour != hour || tm.tm_min != minute)
    throw std::runtime_error("time \"" + text + "\" does not exist in " + appliedTimezone);
  printf("DateTime: setting time to %s (%s), utc epoch = %lld\n", text.c_str(), appliedTimezone.c_str(), (long long)utc); fflush(stdout);
#ifdef __ARM_ARCH
  // host/dev build: never touch the development machine's own clock
  bool ok = Rtc::setSystemClock(utc);
  ok = Rtc::write(utc) && ok;
  if (!ok)
    throw std::runtime_error("failed to write system clock/RTC");
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
  bool settingsChanged = isModified({modePtr, timezonePtr});
  bool formatChanged = isModified({timeFormatPtr});

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

  updateDisplayTime();
}
