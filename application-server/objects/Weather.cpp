#include "Weather.h"

/*
"config" is the binding target of weather tiles: a collection of weather
conditions keyed by the tile's binding (a zip code), e.g.:
{
  "98102": {
    "city": "Seattle",
    "condition": "Cloudy",
    "temperature": 52
  }
}
Nothing populates this yet; a weather tile whose binding has no entry falls back
to the tile's own name and empty values. This is a generic JsonControl;
whole-blob get/set and granular add/update/remove (via url subpath) are handled
entirely by JsonElement::processJson().
*/
Weather::Weather(string path, Object *parent) : LeafObject(path, parent)
{
  methods.insert({"add", "update", "remove"});
  string name;
  name = "config"; elements[name] = configPtr = std::make_shared<JsonControl>(path + "/" + name, this, json::object());
}
