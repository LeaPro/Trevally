#include "ui_design_tokens.h"
#include "ui_icon_data.h"

#include <Appearance.h>
#include <Themes.h>

// Theme palette, keyed by the same string stored in Appearance::themePtr.
// "dark"/"light" are the only built-in theme (colors compiled in, via the
// THEME_DARK_*/THEME_LIGHT_* defines in ui_design_tokens.h); any other key
// is a custom theme, whose colors are read at runtime from the theme
// leaf's config[key].colors object - callers never branch on a specific
// theme name, they just read ui_get_*_color().
struct ThemePalette
{
  lv_color_t background;
  lv_color_t textPrimary;
  lv_color_t textSecondary;
  lv_color_t brand;
  lv_color_t border;
  lv_color_t bgSecondary;
  lv_color_t bgTertiary;
  lv_color_t iconPrimary;
};

static ThemePalette darkPalette(void)
{
  return {
    THEME_DARK_BACKGROUND, THEME_DARK_TEXT_PRIMARY, THEME_DARK_TEXT_SECONDARY,
    THEME_DARK_BRAND, THEME_DARK_BORDER, THEME_DARK_BG_SECONDARY,
    THEME_DARK_BG_TERTIARY, THEME_DARK_ICON_PRIMARY,
  };
}

static ThemePalette lightPalette(void)
{
  return {
    THEME_LIGHT_BACKGROUND, THEME_LIGHT_TEXT_PRIMARY, THEME_LIGHT_TEXT_SECONDARY,
    THEME_LIGHT_BRAND, THEME_LIGHT_BORDER, THEME_LIGHT_BG_SECONDARY,
    THEME_LIGHT_BG_TERTIARY, THEME_LIGHT_ICON_PRIMARY,
  };
}

static std::shared_ptr<Appearance> g_appearance;
static std::shared_ptr<Themes> g_themes;

// Falls back to Appearance's own default theme key (rather than ui_design_
// tokens inventing its own default) if no Appearance leaf has been set yet.
static std::string currentThemeKey(void)
{
  return (g_appearance && g_appearance->themePtr) ? g_appearance->themePtr->get() : "dark";
}

// Parses a "#RRGGBB" string as read from a custom theme's colors object;
// returns fallback for anything malformed (missing '#', wrong length, etc).
static lv_color_t parseHexColor(const std::string &hex, lv_color_t fallback)
{
  if (hex.size() != 7 || hex[0] != '#')
  {
    return fallback;
  }
  return lv_color_hex(strtoul(hex.c_str() + 1, nullptr, 16));
}

// Builds the palette for a custom theme (any key other than "dark"/"light")
// by reading config[key].colors from the theme leaf, field by field; any
// field missing or not a "#RRGGBB" string falls back to the dark palette's
// value for that same field. Falls back to the whole dark palette if the
// theme leaf isn't set, or the key/colors object isn't found.
static ThemePalette customPalette(const std::string &key)
{
  ThemePalette palette = darkPalette();
  if (!g_themes || !g_themes->configPtr)
  {
    return palette;
  }

  json config = g_themes->configPtr->get();
  auto themeIt = config.find(key);
  if (themeIt == config.end() || !themeIt->contains("colors"))
  {
    return palette;
  }

  const json &colors = (*themeIt)["colors"];
  auto colorOr = [&](const char *field, lv_color_t fallback)
  {
    auto it = colors.find(field);
    return (it != colors.end() && it->is_string()) ? parseHexColor(it->get<std::string>(), fallback) : fallback;
  };

  palette.background = colorOr("background", palette.background);
  palette.textPrimary = colorOr("textPrimary", palette.textPrimary);
  palette.textSecondary = colorOr("textSecondary", palette.textSecondary);
  palette.brand = colorOr("brand", palette.brand);
  palette.border = colorOr("border", palette.border);
  palette.bgSecondary = colorOr("bgSecondary", palette.bgSecondary);
  palette.bgTertiary = colorOr("bgTertiary", palette.bgTertiary);
  palette.iconPrimary = colorOr("iconPrimary", palette.iconPrimary);
  return palette;
}

// Compiled-in presets, keyed the same way as custom themes (by the string
// stored in Appearance::themePtr). Adding a new preset is just adding a
// THEME_<NAME>_* block to ui_design_tokens.h, a <name>Palette() function
// above, and one row here - currentPalette() below never needs to change.
struct NamedPalette
{
  const char *key;
  ThemePalette (*build)(void);
};

static const NamedPalette presetPalettes[] = {
  {"dark", darkPalette},
  {"light", lightPalette},
};

static ThemePalette currentPalette(void)
{
  std::string key = currentThemeKey();
  for (const NamedPalette &preset : presetPalettes)
  {
    if (key == preset.key)
    {
      return preset.build();
    }
  }
  return customPalette(key);
}

lv_color_t ui_get_background_color(void)
{
  return currentPalette().background;
}

lv_color_t ui_get_brand_color(void)
{
  return currentPalette().brand;
}

lv_color_t ui_get_text_primary_color(void)
{
  return currentPalette().textPrimary;
}

lv_color_t ui_get_text_secondary_color(void)
{
  return currentPalette().textSecondary;
}

lv_color_t ui_get_icon_primary_color(void)
{
  return currentPalette().iconPrimary;
}

lv_color_t ui_get_border_color(void)
{
  return currentPalette().border;
}

lv_color_t ui_get_bg_secondary_color(void)
{
  return currentPalette().bgSecondary;
}

lv_color_t ui_get_bg_tertiary_color(void)
{
  return currentPalette().bgTertiary;
}

static lv_style_t styleBg;
static lv_style_t styleBgSecondary;
static lv_style_t styleBgTertiary;
static lv_style_t styleBrand;
static lv_style_t styleTextPrimary;
static lv_style_t styleTextSecondary;
static lv_style_t styleBorder;
static lv_style_t styleIconPrimary;
static lv_style_t styleKnobBorder;

// Icon recolor style, applied to lv_image icons (not labels) via
// ui_icon_create() - image_recolor is the lv_image equivalent of a label's
// text_color. Follows the theme, same as styleIconPrimary for label-based
// fallback icons.
static lv_style_t styleImageIconPrimary;

// Re-applies every token's current (theme-resolved) color to its shared
// style, then notifies every object using one of those styles - plus any
// widget that registered via ui_theme_add_listener() - that it needs to
// repaint. Safe to call repeatedly (e.g. on first init, and again on every
// live theme change).
static void applyTheme(void)
{
  lv_style_set_bg_color(&styleBg, ui_get_background_color());

  lv_style_set_bg_color(&styleBgSecondary, ui_get_bg_secondary_color());
  lv_style_set_bg_color(&styleBgTertiary, ui_get_bg_tertiary_color());
  lv_style_set_arc_color(&styleBgTertiary, ui_get_bg_tertiary_color());

  lv_color_t brand = ui_get_brand_color();
  lv_style_set_bg_color(&styleBrand, brand);
  lv_style_set_text_color(&styleBrand, brand);
  lv_style_set_border_color(&styleBrand, brand);
  lv_style_set_arc_color(&styleBrand, brand);

  lv_style_set_text_color(&styleTextPrimary, ui_get_text_primary_color());
  lv_style_set_text_color(&styleTextSecondary, ui_get_text_secondary_color());
  lv_style_set_border_color(&styleBorder, ui_get_border_color());
  lv_style_set_text_color(&styleIconPrimary, ui_get_icon_primary_color());
  lv_style_set_border_color(&styleKnobBorder, ui_get_background_color());
  lv_style_set_image_recolor(&styleImageIconPrimary, ui_get_icon_primary_color());

  lv_obj_report_style_change(&styleBg);
  lv_obj_report_style_change(&styleBgSecondary);
  lv_obj_report_style_change(&styleBgTertiary);
  lv_obj_report_style_change(&styleBrand);
  lv_obj_report_style_change(&styleTextPrimary);
  lv_obj_report_style_change(&styleTextSecondary);
  lv_obj_report_style_change(&styleBorder);
  lv_obj_report_style_change(&styleIconPrimary);
  lv_obj_report_style_change(&styleKnobBorder);
  lv_obj_report_style_change(&styleImageIconPrimary);
}

lv_style_t *ui_theme_get_style_bg(void)
{
  return &styleBg;
}

lv_style_t *ui_theme_get_style_bg_secondary(void)
{
  return &styleBgSecondary;
}

lv_style_t *ui_theme_get_style_bg_tertiary(void)
{
  return &styleBgTertiary;
}

lv_style_t *ui_theme_get_style_brand(void)
{
  return &styleBrand;
}

lv_style_t *ui_theme_get_style_text_primary(void)
{
  return &styleTextPrimary;
}

lv_style_t *ui_theme_get_style_text_secondary(void)
{
  return &styleTextSecondary;
}

lv_style_t *ui_theme_get_style_border(void)
{
  return &styleBorder;
}

lv_style_t *ui_theme_get_style_icon_primary(void)
{
  return &styleIconPrimary;
}

lv_style_t *ui_theme_get_style_knob_border(void)
{
  return &styleKnobBorder;
}

// Creates an lv_image showing the icon's PNG asset, with the recolor style
// attached so it repaints automatically on theme change. Falls back to a
// plain symbol label if the image failed to decode - in practice this
// shouldn't happen since every icon is compiled-in, not loaded from disk.
lv_obj_t *ui_icon_create(lv_obj_t *parent, ui_icon_config_t config)
{
  if (config.type >= UI_ICON_TYPE_COUNT)
  {
    return nullptr;
  }

  lv_obj_t *icon = lv_image_create(parent);
  lv_image_set_src(icon, ui_icon_data[config.type]);
  lv_obj_add_style(icon, &styleImageIconPrimary, 0);

  if (lv_image_get_src(icon) == nullptr)
  {
    lv_obj_delete(icon);
    if (!config.fallback_symbol)
    {
      return nullptr;
    }
    icon = lv_label_create(parent);
    lv_label_set_text(icon, config.fallback_symbol);
    lv_obj_add_style(icon, &styleIconPrimary, 0);
  }

  return icon;
}

void ui_design_tokens_refresh(void)
{
  applyTheme();
}

void ui_design_tokens_init(std::shared_ptr<Appearance> appearance, std::shared_ptr<Themes> themes)
{
  g_appearance = appearance;
  g_themes = themes;

  lv_style_init(&styleBg);
  lv_style_init(&styleBgSecondary);
  lv_style_init(&styleBgTertiary);
  lv_style_init(&styleBrand);
  lv_style_init(&styleTextPrimary);
  lv_style_init(&styleTextSecondary);
  lv_style_init(&styleBorder);
  lv_style_init(&styleIconPrimary);
  lv_style_init(&styleKnobBorder);

  lv_style_init(&styleImageIconPrimary);
  lv_style_set_image_recolor_opa(&styleImageIconPrimary, LV_OPA_COVER);

  applyTheme();
}

