#include "LeaControl.h"

/*
"config" is a collection of control-proxy info blobs keyed by UUID, e.g.:
{
  "control-uuid-japan": {
    "outputEqFilters": { "frequency": { "mac123": {"1": ..., "2": ...} } }
  },
  "control-uuid-france": {
    "inputSelector": { "signalGeneratorEnable": { "mac123": {"1": ..., "2": ..., "3": ..., "4": ...} } }
  }
}
A single control tile can span multiple mac/channel combos. This is a generic
JsonControl; whole-blob get/set and granular add/update/remove (via url
subpath) are handled entirely by JsonElement::processJson().
*/
LeaControl::LeaControl(string path, Object *parent) : LeafObject(path, parent)
{
  methods.insert({"add", "update", "remove"});
  string name;
  name = "config"; elements[name] = std::make_shared<JsonControl>(path + "/" + name, this, json::object());
}
