/**
 * @file ui_design_tokens.h
 * @brief Design tokens (fonts + colors) used by the ui/components/* widgets
 *        ported from the old lv_port_linux prototype.
 *
 * Live dark/light theme switching: ui_design_tokens_init() stores the
 * device's Appearance leaf and polls Appearance::themePtr for changes. On a
 * change, every uiGet*Color() getter below starts returning the other
 * theme's palette, the 7 shared lv_style_t singletons are updated in place,
 * and lv_obj_report_style_change() is called for each - any widget that
 * applied one of these styles via lv_obj_add_style() (rather than baking a
 * one-off color in with lv_obj_set_style_*_color()) repaints automatically.
 * A widget that can't be driven by a style at all (e.g. picker_wheel's
 * hand-drawn canvas gradients) instead applies one of these styles to its
 * own top-level container purely so LV_EVENT_STYLE_CHANGED fires on it,
 * and reacts to that event by redoing its manual drawing - see
 * picker_wheel.cpp's theme_changed_cb(), ported as-is from the old
 * prototype's identical pattern.
 */

#pragma once

#include <lvgl.h>
#include <memory>

class Appearance;
class Themes;

LV_FONT_DECLARE(satoshi_medium_14);
LV_FONT_DECLARE(satoshi_medium_20);
LV_FONT_DECLARE(satoshi_medium_24);
LV_FONT_DECLARE(satoshi_medium_32);
LV_FONT_DECLARE(satoshi_medium_48);
LV_FONT_DECLARE(satoshi_medium_140);

#define OPACITY_BRAND_BG_LIGHT LV_OPA_10

/* Physical panel size (Densitron DMT049W1NTCMI-1A, portrait). Also used as the
 * simulator window size so the host build matches the real device. */
#define UI_PANEL_W_PX 480
#define UI_PANEL_H_PX 854

/* Layout, per orientation (Figma: LEA CS Touch Wall Panel UX/UI).
 *
 * The "content area" is the column that holds the header and the pane below it.
 * Portrait it is the full panel width. Landscape it is the left 498px; the
 * remaining 356px on the right is the streaming controls panel (not built yet).
 * The header height is the same in both orientations. Which orientation is
 * active follows the display: it is landscape when the display is wider than
 * tall, which is what lv_display_set_rotation(90 / 270) gives. */
#define UI_HEADER_H_PX                72    /* header height and icon slot size (touch target) */

#define UI_PORTRAIT_CONTENT_W_PX      480
#define UI_PORTRAIT_PANE_H_PX         391   /* the scroll pane, below the header */
#define UI_LANDSCAPE_CONTENT_W_PX     498
#define UI_LANDSCAPE_PANE_H_PX        408

/* Screen pagination dots, centred on the content area. The top edge is measured
 * from the top of the header. */
#define UI_PAGINATION_TOP_PX          58
#define UI_PAGINATION_DOT_PX          6
#define UI_PAGINATION_ACTIVE_W_PX     14
#define UI_PAGINATION_GAP_PX          4

static inline bool ui_is_landscape(void)
{
  return lv_display_get_horizontal_resolution(nullptr) > lv_display_get_vertical_resolution(nullptr);
}

static inline int ui_content_w(void)
{
  return ui_is_landscape() ? UI_LANDSCAPE_CONTENT_W_PX : UI_PORTRAIT_CONTENT_W_PX;
}

/* Height of the scroll pane (the screens), i.e. of the content area minus the header. */
static inline int ui_pane_h(void)
{
  return ui_is_landscape() ? UI_LANDSCAPE_PANE_H_PX : UI_PORTRAIT_PANE_H_PX;
}

/* Level gauge styles (see components/level_gauge.h). The look used by every
 * level_gauge_create() call is the Appearance "levelGaugeStyle" element:
 *  - ANCHORED: arc centred on the physical encoder, value label above it.
 *  - FLOATING: arc centred on the centre of the value label.
 *  - ELLIPSE: shallow arc bowing over the units, with the value label above it. */
enum class level_gauge_style_t
{
  ANCHORED,
  FLOATING,
  ELLIPSE
};

/* Status text colors (Figma text-success / text-error). Not part of the theme palettes: the
 * same in every theme. */
#define UI_COLOR_TEXT_SUCCESS     lv_color_hex(0xAEE076)
#define UI_COLOR_TEXT_ERROR       lv_color_hex(0xFDA29B)

/* Built-in "dark"/"light" palettes - the only two theme with colors
 * compiled into firmware. Any other theme key is a custom theme and its
 * colors are read at runtime from the theme leaf's config[key].colors
 * object (see ui_design_tokens.cpp's currentPalette()); fields missing
 * from a custom theme's colors fall back to the dark values below. */
#define THEME_DARK_BACKGROUND     lv_color_hex(0x000000)
#define THEME_DARK_TEXT_PRIMARY   lv_color_hex(0xFAFAFA)
#define THEME_DARK_TEXT_SECONDARY lv_color_hex(0xB0B0B0)
#define THEME_DARK_BRAND          lv_color_hex(0x5DE9FF)
#define THEME_DARK_BORDER         lv_color_hex(0x676A6F)
#define THEME_DARK_BG_SECONDARY   lv_color_hex(0x3A3C40)
#define THEME_DARK_BG_TERTIARY    lv_color_hex(0x3A3C40)
#define THEME_DARK_ICON_PRIMARY   lv_color_hex(0xF5F5F5)

#define THEME_LIGHT_BACKGROUND     lv_color_hex(0xF5F5F5)
#define THEME_LIGHT_TEXT_PRIMARY   lv_color_hex(0x000000)
#define THEME_LIGHT_TEXT_SECONDARY lv_color_hex(0x666666)
#define THEME_LIGHT_BRAND          lv_color_hex(0x4F46E5)
#define THEME_LIGHT_BORDER        lv_color_hex(0xC4C4C9)
#define THEME_LIGHT_BG_SECONDARY  lv_color_hex(0xDEDEE3)
#define THEME_LIGHT_BG_TERTIARY   lv_color_hex(0x3A3C40)
#define THEME_LIGHT_ICON_PRIMARY  lv_color_hex(0x111111)

lv_color_t ui_get_background_color(void);
lv_color_t ui_get_brand_color(void);
lv_color_t ui_get_text_primary_color(void);
lv_color_t ui_get_text_secondary_color(void);
lv_color_t ui_get_icon_primary_color(void);
lv_color_t ui_get_border_color(void);
lv_color_t ui_get_bg_secondary_color(void);
lv_color_t ui_get_bg_tertiary_color(void);

/* Lazily-initialized, shared lv_style_t objects - avoids every widget
 * needing its own style struct. Widgets MUST apply these via
 * lv_obj_add_style() rather than reading a uiGet*Color() value into a
 * one-off lv_obj_set_style_*_color() call, otherwise they will not repaint
 * when the theme changes live. */
lv_style_t *ui_theme_get_style_bg(void);
lv_style_t *ui_theme_get_style_bg_secondary(void);
lv_style_t *ui_theme_get_style_bg_tertiary(void);
lv_style_t *ui_theme_get_style_brand(void);
lv_style_t *ui_theme_get_style_text_primary(void);
lv_style_t *ui_theme_get_style_text_secondary(void);
lv_style_t *ui_theme_get_style_border(void);
lv_style_t *ui_theme_get_style_icon_primary(void);
/* Background fills for small solid shapes (e.g. pagination dots): icon-primary and border colors. */
lv_style_t *ui_theme_get_style_bg_icon_primary(void);
lv_style_t *ui_theme_get_style_bg_border(void);
/* image_recolor == icon colour; for lv_image-style icons that aren't made by ui_icon_create()
 * (e.g. a dropdown's symbol, via its LV_PART_INDICATOR). */
lv_style_t *ui_theme_get_style_image_icon_primary(void);
/* border_color == background color; used for the thin "cut-out" border
 * around elements like a slider knob that floats above a filled track. */
lv_style_t *ui_theme_get_style_knob_border(void);

/* Re-resolves every token's color from the current theme key (Appearance::
 * themePtr) and, for custom theme, the theme leaf's config, then
 * re-applies them to the shared styles above and notifies every widget
 * using one of those styles to repaint. ui_design_tokens_init() does NOT
 * poll for changes on its own - the caller (lvgl_ui.cpp) is responsible
 * for detecting when Appearance::themePtr or the theme leaf's config has
 * changed and calling this. */
void ui_design_tokens_refresh(void);

/* ========================================================================
 * ICONS - themed PNG icon set, ported from the old lv_port_linux
 * prototype's ui_icon_create()/ui_icon_swap(). Image data lives in
 * ui_icon_data[] (see ui_icon_data.h) - order must match this enum.
 * ======================================================================== */

enum ui_icon_type_t
{
  UI_ICON_DECREASE,
  UI_ICON_INCREASE,
  UI_ICON_SHUFFLE,
  UI_ICON_PLAY,
  UI_ICON_PAUSE,
  UI_ICON_SKIP_NEXT,
  UI_ICON_SKIP_PREVIOUS,
  UI_ICON_REPEAT,
  UI_ICON_SLEEP,
  UI_ICON_WEATHER,
  UI_ICON_MUSIC_CAST,
  UI_ICON_ADD,
  UI_ICON_ARROW_BACK,
  UI_ICON_CHECK_SMALL,
  UI_ICON_CHEVRON_FORWARD,
  UI_ICON_HOME,
  UI_ICON_QUEUE_MUSIC,
  UI_ICON_VOLUME_UP,
  UI_ICON_VOLUME_SMALL,
  UI_ICON_SETTINGS,
  UI_ICON_ELLIPSE,
  UI_ICON_LOGO,
  UI_ICON_EQUALIZER_1,
  UI_ICON_EQUALIZER_2,
  UI_ICON_EQUALIZER_3,
  UI_ICON_TYPE_COUNT
};

/* Icons always follow ui_get_icon_primary_color(), repainting live via
 * lv_obj_report_style_change() on every theme change, same as the other
 * tokens above. */
struct ui_icon_config_t
{
  ui_icon_type_t type;
  const char *fallback_symbol;
};

lv_obj_t *ui_icon_create(lv_obj_t *parent, ui_icon_config_t config);

#define UI_ICON_CREATE_DEFAULT(parent, icon_type) \
  ui_icon_create((parent), ui_icon_config_t{ (icon_type), nullptr })

// Stores the device's Appearance and Themes leaves and applies the initial
// theme. "dark"/"light" always use the built-in THEME_*_* colors above;
// any other theme key is looked up in the Themes leaf's config at that
// key's "colors" object. Does NOT poll for changes itself - call
// ui_design_tokens_refresh() whenever Appearance::themePtr or the Themes
// leaf's config changes (lvgl_ui.cpp owns that polling). Call once, before
// creating any UI that reads the tokens above.
void ui_design_tokens_init(std::shared_ptr<Appearance> appearance, std::shared_ptr<Themes> themes);

