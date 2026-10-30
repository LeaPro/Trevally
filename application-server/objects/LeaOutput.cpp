#include "LeaOutput.h"

/*
"config" is a collection of output-proxy info blobs keyed by UUID, e.g.:
{
  "output-uuid-carrot": {
    "amps (fader)": { "mac123": {"1": ..., "2": ...}, "mac456": {"1": ..., "2": ...} },
    "subgroups": {
      "output-sub-uuid-kale": {
        "id": "output-sub-uuid-kale",
        "name": ...,
        "subchannels (faderZoneAdjust)": { "mac123": {"1": ..., "2": ...} }
      }
    },
    "subgroupOrder": ["output-sub-uuid-celery", "output-sub-uuid-kale"]
  }
}
This is a generic JsonControl; whole-blob get/set and granular add/update/remove
(via url subpath) are handled entirely by JsonElement::processJson().
*/
LeaOutput::LeaOutput(string path, Object *parent) : LeafObject(path, parent)
{
  methods.insert({"add", "update", "remove"});
  string name;
  name = "config"; elements[name] = std::make_shared<JsonControl>(path + "/" + name, this, json::object());
}
