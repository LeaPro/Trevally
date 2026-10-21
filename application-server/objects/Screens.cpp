#include "Screens.h"

/*
"config" is a collection of screen info blobs keyed by UUID, e.g.:
{
  "screen-uuid-green": {
    "id": "screen-uuid-green",
    "name": "Controls screen",
    "tileOrder": ["tile-uuid-kiwi", "tile-uuid-banana"]
  }
}
"screenOrder" is an array of the UUIDs in "config", dictating display order.
Both elements are generic JsonControls; whole-blob get/set and granular
add/update/remove (via url subpath) are handled entirely by JsonElement::processJson().
*/
Screens::Screens(string path, Object *parent) : LeafObject(path, parent)
{
  methods.insert({"add", "update", "remove"});
  string name;
  name = "config"; elements[name] = std::make_shared<JsonControl>(path + "/" + name, this, json::object());
  name = "screenOrder"; elements[name] = std::make_shared<JsonControl>(path + "/" + name, this, json::array());
}
