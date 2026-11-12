#include "LeaOutput.h"
#include "OutputFaders.h"

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
The live fader values of these entries are elements of /outputFaders (see OutputFaders.h), kept in
step with the config by update().
*/
LeaOutput::LeaOutput(string path, Object *parent) : LeafObject(path, parent)
{
  methods.insert({"add", "update", "remove"});
  string name;
  name = "config"; elements[name] = configPtr = std::make_shared<JsonControl>(path + "/" + name, this, json::object());
}

void LeaOutput::update(bool sensorsOnly, bool refreshVolatileElements)
{
  if (sensorsOnly)
    return;
  auto faders = std::dynamic_pointer_cast<OutputFaders>(getRoot()->leafFromPath("/outputFaders"));
  if (!faders)
    return;
  json current = configPtr->get();
  if (current == appliedConfig)
    return;
  appliedConfig = current;
  faders->apply(current);
}
