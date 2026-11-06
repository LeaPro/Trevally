#include "Weather.h"

#include <cmath>

#include "Appearance.h"
#include "DeviceInfo.h"
#include "Tiles.h"

/*
"config" is the binding target of weather tiles: a collection of weather
conditions keyed by the tile's binding (a postal code: a US one as is, another country's
with its country code in front, "GB:SW1A 1AA"), e.g.:
{
  "98102": {
    "city": "Seattle",
    "condition": "Cloudy",
    "temperature": 52,
    "temperatureUnits": "f",
    "isDaytime": true
  }
}
The device fills it in itself: the postal codes of the weather tiles (see Tiles)
are looked up by WeatherLookup (Zippopotam.us + National Weather Service in the US,
MET Norway elsewhere), and
"temperature" follows /appearance temperatureUnits ("f" or "c") and is stored with the
unit it is in ("temperatureUnits"). Entries of
postal codes that no weather tile is bound to are removed. A weather tile whose
binding has no entry (yet) falls back to the tile's own name and empty values.
The "default" entry always exists, like the preset themes: it is the weather at
/deviceInfo defaultPostalCode, with that code in "postalCode", and a weather tile
bound to "default" shows it:
{
  "default": {
    "postalCode": "98102",
    "city": "Seattle",
    "condition": "Cloudy",
    "temperature": 52
  }
}
This is a generic JsonControl; whole-blob get/set and granular
add/update/remove (via url subpath) are handled entirely by JsonElement::processJson().
*/
Weather::Weather(string path, Object *parent) : LeafObject(path, parent)
{
  methods.insert({"add", "update", "remove"});
  string name;
  name = "config"; elements[name] = configPtr = std::make_shared<JsonControl>(path + "/" + name, this, json::object());
}

// Bindings (postal codes) of the tiles of type "weather".
std::set<std::string> Weather::weatherTileBindings()
{
  std::set<std::string> bindings;
  auto tiles = std::dynamic_pointer_cast<Tiles>(getRoot()->leafFromPath("/tiles"));
  if (!tiles)
    return bindings;
  const json config = tiles->configPtr->get();
  if (!config.is_object())
    return bindings;
  for (const auto &[id, tile] : config.items())
  {
    if (!tile.is_object())
      continue;
    const auto type = tile.find("type");
    const auto binding = tile.find("binding");
    if (type != tile.end() && type->is_string() && type->get<std::string>() == "weather" &&
        binding != tile.end() && binding->is_string() && WeatherLookup::isPostalCode(binding->get<std::string>()))
      bindings.insert(binding->get<std::string>());
  }
  return bindings;
}

void Weather::update(bool sensorsOnly, bool refreshVolatileElements)
{
  const auto now = std::chrono::steady_clock::now();
  if (now - lastSync_ < std::chrono::seconds(1))
    return;
  lastSync_ = now;

  if (!getRoot()->leafFromPath("/tiles"))
    return; // the tiles are not there (yet): nothing to base the wanted postal codes on
  const std::set<std::string> tileCodes = weatherTileBindings();
  std::string defaultCode;
  if (auto deviceInfo = std::dynamic_pointer_cast<DeviceInfo>(getRoot()->leafFromPath("/deviceInfo")))
    if (WeatherLookup::isPostalCode(deviceInfo->defaultPostalCodePtr->get()))
      defaultCode = deviceInfo->defaultPostalCodePtr->get();
  std::set<std::string> wanted = tileCodes;
  if (!defaultCode.empty())
    wanted.insert(defaultCode);
  lookup_.setLocations(wanted);

  bool celsius = false;
  if (auto appearance = std::dynamic_pointer_cast<Appearance>(getRoot()->leafFromPath("/appearance")))
    celsius = appearance->temperatureUnitsPtr->get() == "c";

  const json config = configPtr->get();
  json next = json::object();
  if (config.is_object())
    for (const auto &[code, entry] : config.items())
      if (tileCodes.count(code) != 0)
        next[code] = entry; // last known conditions stay while a refresh fails

  // The default location always exists. Its last known conditions stay only while its postal code is unchanged.
  next["default"] = {{"postalCode", defaultCode}};
  if (config.is_object() && config.contains("default") && config["default"].is_object() &&
      config["default"].value("postalCode", std::string()) == defaultCode)
    next["default"] = config["default"];

  for (const auto &[code, found] : lookup_.conditions())
  {
    const double degrees = celsius ? (found.temperatureF - 32.0) * 5.0 / 9.0 : found.temperatureF;
    const json conditions = {{"city", found.city}, {"condition", found.condition}, {"temperature", std::lround(degrees)}, {"temperatureUnits", celsius ? "c" : "f"}, {"isDaytime", found.isDaytime}};
    if (tileCodes.count(code) != 0)
      next[code] = conditions;
    if (code == defaultCode)
      next["default"].update(conditions);
  }
  configPtr->set(next);
}
