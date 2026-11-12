#pragma once
#include <object.h>
#include <string>

// Screensaver settings from the web UI. When "enabled", the screensaver starts after "timeoutSeconds" of UI
// inactivity. "type" is "background-logo", "date-time" or "weather"; "showStreamingInfo" adds what's playing.
class Screensaver final : public LeafObject
{
public:
  Screensaver(std::string path, Object *parent);

  std::shared_ptr<BoolControl> enabledPtr;
  std::shared_ptr<UInt32Control> timeoutSecondsPtr;
  std::shared_ptr<StringControl> typePtr;
  std::shared_ptr<BoolControl> showStreamingInfoPtr;
};
