#include "LeaSensor.h"

/*
"config" is a collection of sensor-proxy info blobs keyed by UUID, e.g.:
{
  "sensor-uuid-tennis": {
    "impedanceMeter": { "mac123": {"1": ...} }
  }
}
Unlike control, a single sensor tile cannot span multiple mac/channel combos.
This is a generic JsonControl; whole-blob get/set and granular add/update/remove
(via url subpath) are handled entirely by JsonElement::processJson().
*/
LeaSensor::LeaSensor(string path, Object *parent) : LeafObject(path, parent)
{
  methods.insert({"add", "update", "remove"});
  string name;
  name = "config"; elements[name] = std::make_shared<JsonControl>(path + "/" + name, this, json::object());
}
