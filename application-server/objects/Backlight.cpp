#include "objects.h"
#include "Backlight.h"
#include <lvgl.h>
#include <boost/filesystem.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>

using std::string;
namespace fs = boost::filesystem;

Backlight::Backlight(string path, Object *parent) : LeafObject(path, parent)
{
  string name;
  name = "brightness"; elements[name] = brightnessPtr = std::make_shared<FloatControl>(path + "/" + name, this, 0.0f/*min*/, 100.0f/*max*/, 80.0f/*default*/, 1.0f/*multipleOf*/, "%"/*units*/);
  name = "timeout"; elements[name] = timeoutPtr = std::make_shared<BoolControl>(path + "/" + name, this, false/*default*/);
  name = "timeoutSeconds"; elements[name] = timeoutSecondsPtr = std::make_shared<FloatControl>(path + "/" + name, this, 5.0f/*min*/, 3600.0f/*max*/, 60.0f/*default*/, 1.0f/*multipleOf*/, "seconds"/*units*/);
}

void Backlight::initialize()
{
  LeafObject::initialize();
}

// Find the sysfs backlight device. The DT node is named "backlight", so prefer
// /sys/class/backlight/backlight and otherwise fall back to the first device found.
bool Backlight::locateDevice()
{
  if (!deviceDir.empty())
    return true;
  try
  {
    const fs::path base("/sys/class/backlight");
    if (!fs::exists(base))
      return false;
    fs::path found;
    for (fs::directory_iterator it(base), end; it != end; ++it)
    {
      if (found.empty() || it->path().filename() == "backlight")
        found = it->path();
    }
    if (found.empty())
      return false;
    std::ifstream maxFile((found / "max_brightness").string());
    long max = 0;
    if (!(maxFile >> max) || max <= 0)
      return false;
    deviceDir = found.string();
    maxBrightness = max;
    printf("Backlight: using %s (max_brightness = %ld)\n", deviceDir.c_str(), maxBrightness); fflush(stdout);
    return true;
  }
  catch (...)
  {
    return false;
  }
}

bool Backlight::applyPercent(float percent)
{
#ifdef __ARM_ARCH
  if (!locateDevice())
  {
    printf("Backlight: no backlight device found under /sys/class/backlight\n"); fflush(stdout);
    return false;
  }
  percent = std::clamp(percent, 0.0f, 100.0f);
  long level = std::lround(percent / 100.0f * (float)maxBrightness);
  std::ofstream f(deviceDir + "/brightness");
  if (!f)
  {
    printf("Backlight: cannot open %s/brightness for writing\n", deviceDir.c_str()); fflush(stdout);
    return false;
  }
  f << level << std::endl;
  return f.good();
#else
  // host/dev build: never touch the development machine's own backlight
  (void)percent;
  return true;
#endif
}

void Backlight::update(bool sensorsOnly, bool refreshVolatileElements)
{
  if (sensorsOnly)
  {
    // inactivity timeout: switch the backlight off after timeoutSeconds without
    // user input, restore it on the next touch
    if (!timeoutPtr->get())
      return;
    uint32_t inactiveMs = lv_display_get_inactive_time(nullptr);
    uint32_t timeoutMs = (uint32_t)(timeoutSecondsPtr->get() * 1000.0f);
    if (!screenOff && inactiveMs >= timeoutMs)
    {
      if (applyPercent(0.0f))
        screenOff = true;
    }
    else if (screenOff && inactiveMs < timeoutMs)
    {
      if (applyPercent(brightnessPtr->get()))
        screenOff = false;
    }
  }
  else // non-sensors
  {
    // note: isModified() with a list clears every listed element's flag
    if (isModified({brightnessPtr, timeoutPtr, timeoutSecondsPtr}))
    {
      // a user-initiated change (or the initial restore) always lights the screen
      // and restarts the inactivity countdown (otherwise enabling the timeout from
      // the webUI would find the on-device idle time already past the limit)
      lv_display_trigger_activity(nullptr);
      if (applyPercent(brightnessPtr->get()))
        screenOff = false;
    }
  }
}
