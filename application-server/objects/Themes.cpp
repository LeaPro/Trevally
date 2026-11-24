#include "Themes.h"

/*
"config" is a collection of theme info blobs keyed by name/UUID, e.g.:
{
  "dark": {
    "type": "preset",
    "name": "Dark",
    "screensaver": { "background": "", "logo": "" },
    "background": "",
    "startTime": "19:00"
  },
  "theme-uuid-cyberpunk": {
    "id": "theme-uuid-cyberpunk",
    "type": "custom",
    "colors": {
      "background": "#0D0221",
      "textPrimary": "#F5F5F5",
      "textSecondary": "#B0B0B0",
      "brand": "#FF2E63",
      "border": "#252A34",
      "bgSecondary": "#252A34",
      "bgTertiary": "#08D9D6",
      "iconPrimary": "#F5F5F5"
    },
    "screensaver": { "background": "", "logo": "" },
    "background": "",
    "startTime": "18:30"
  }
}
"background"/"screensaver.background"/"screensaver.logo" are file-hash
references into the "files" leaf's image-files map (wallpaper/logo images),
not colors. "colors" is only present on "custom" theme and supplies the
palette (keyed to match ui_design_tokens.cpp's ThemePalette fields) used in
place of the built-in "dark"/"light" palettes; Appearance::themePtr selects
the active entry by key. This is a generic JsonControl; whole-blob get/set
and granular add/update/remove (via url subpath) are handled entirely by
JsonElement::processJson().
*/
static json defaultConfig()
{
  return {
    {"dark", {
      {"type", "preset"},
      {"name", "Dark"},
      {"screensaver", {{"background", ""}, {"logo", ""}}},
      {"background", ""},
      {"startTime", "19:00"}
    }},
    {"light", {
      {"type", "preset"},
      {"name", "Light"},
      {"screensaver", {{"background", ""}, {"logo", ""}}},
      {"background", ""},
      {"startTime", "07:00"}
    }}
  };
}

Themes::Themes(string path, Object *parent) : LeafObject(path, parent)
{
  methods.insert({"add", "update", "remove"});
  string name;
  name = "config"; elements[name] = configPtr = std::make_shared<JsonControl>(path + "/" + name, this, defaultConfig());
}

// The presets can be edited (startTime, background, screensaver) but never removed or
// turned into something else. update() runs after every set/add/update/remove by an
// external client, a restore from persistent storage, and a reset. Here a preset that is
// missing, isn't an object, or has no "type" is replaced with its default blob, a preset
// whose "type" isn't "preset" gets it set back, and a preset without a string "name" (e.g.
// a config persisted before presets had one) gets its default name.
void Themes::update(bool sensorsOnly, bool refreshVolatileElements)
{
  json config = configPtr->get();
  if (!config.is_object())
    config = json::object();
  bool changed = false;
  const json defaults = defaultConfig();
  for (auto it = defaults.begin(); it != defaults.end(); ++it)
  {
    json &preset = config[it.key()];   // creates a null entry if missing
    if (!preset.is_object() || !preset.contains("type"))
    {
      preset = it.value();
      changed = true;
    }
    else if (preset["type"] != "preset")
    {
      preset["type"] = "preset";
      changed = true;
    }
    if (!preset.contains("name") || !preset["name"].is_string())
    {
      preset["name"] = it.value()["name"];
      changed = true;
    }
  }
  if (changed)
    configPtr->set(config);
}
