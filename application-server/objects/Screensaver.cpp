#include "objects.h"

using std::string;

Screensaver::Screensaver(string path, Object *parent) :
  LeafObject(path, parent)
{
  string name;
  name = "enabled"; elements[name] = enabledPtr = std::make_shared<BoolControl>(path + "/" + name, this, false/*default*/);
  // an integer: the web UI's model has an Int here and fails to decode 120.0
  name = "timeoutSeconds"; elements[name] = timeoutSecondsPtr = std::make_shared<UInt32Control>(path + "/" + name, this, 5/*min*/, 3600/*max*/, 120/*default*/, "seconds"/*units*/);
  name = "type"; elements[name] = typePtr = std::make_shared<StringControl>(path + "/" + name, this, "background-logo"/*default*/, 32);
  name = "showStreamingInfo"; elements[name] = showStreamingInfoPtr = std::make_shared<BoolControl>(path + "/" + name, this, true/*default*/);
}
