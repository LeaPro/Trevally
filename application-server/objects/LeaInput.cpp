#include "LeaInput.h"

/*
"config" is a collection of input-proxy info blobs keyed by UUID, e.g.:
{
  "input-uuid-kansas": {
    "states": {
      "state-uuid-iowa": {
        "id": "state-uuid-iowa",
        "name": ...,
        "inputSelector": { "mac123": {"1": {"source (primary)": ..., "volumeControl": {...}}, "2": {...}} }
      }
    },
    "stateOrder": ["state-uuid-utah", "state-uuid-iowa"]
  }
}
This is a generic JsonControl; whole-blob get/set and granular add/update/remove
(via url subpath) are handled entirely by JsonElement::processJson().
*/
LeaInput::LeaInput(string path, Object *parent) : LeafObject(path, parent)
{
  methods.insert({"add", "update", "remove"});
  string name;
  name = "config"; elements[name] = std::make_shared<JsonControl>(path + "/" + name, this, json::object());
}
