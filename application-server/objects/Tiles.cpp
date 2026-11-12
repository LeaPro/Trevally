#include "Tiles.h"

/*
"config" is a collection of tile info blobs keyed by UUID, e.g.:
{
  "tile-uuid-lemon": {
    "type": "output-volume",
    "id": "tile-uuid-lemon",
    "name": "Output Volume for LEA Amps",
    "binding": "output-uuid-carrot"
  }
}
Tile ordering within a screen lives in that screen's own tileOrder (see Screens),
not here. This is a generic JsonControl; whole-blob get/set and granular
add/update/remove (via url subpath) are handled entirely by JsonElement::processJson().
*/
Tiles::Tiles(string path, Object *parent) : LeafObject(path, parent)
{
  methods.insert({"add", "update", "remove"});
  string name;
  name = "config"; elements[name] = configPtr = std::make_shared<JsonControl>(path + "/" + name, this, json::object());
}
