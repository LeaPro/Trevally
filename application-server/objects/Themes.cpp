#include "Themes.h"

/*
"config" is a collection of theme info blobs keyed by name/UUID, e.g.:
{
  "dark": {
    "type": "preset",
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
Themes::Themes(string path, Object *parent) : LeafObject(path, parent)
{
  methods.insert({"add", "update", "remove"});
  string name;
  json defaultConfig = {
    {"dark", {
      {"type", "preset"},
      {"screensaver", {{"background", ""}, {"logo", ""}}},
      {"background", ""},
      {"startTime", "19:00"}
    }},
    {"light", {
      {"type", "preset"},
      {"screensaver", {{"background", ""}, {"logo", ""}}},
      {"background", ""},
      {"startTime", "07:00"}
    }}
  };
  name = "config"; elements[name] = configPtr = std::make_shared<JsonControl>(path + "/" + name, this, defaultConfig);
}
