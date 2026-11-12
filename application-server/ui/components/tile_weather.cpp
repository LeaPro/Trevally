/**
 * @file tile_weather.cpp
 * @brief Weather tile: city, condition icon + text and temperature.
 *        Reads /weather config[binding] (binding = postal code; none = the default postal code):
 *        { city, condition, temperature, isDaytime }; the icon follows the condition.
 */

#include "tile.h"
#include "../ui_design_tokens.h"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <string>

#include <object.h>
#include <Weather.h>

/* The device's root object (defined in main.cpp). */
extern std::shared_ptr<RootObject> root;

/* The /weather config entry a tile binding reads: a tile with no binding shows the default postal code's weather. */
static std::string weatherKey(const std::string &binding)
{
    return binding.empty() ? std::string("default") : binding;
}

static std::string jsonString(const json &j, const char *key)
{
    auto it = j.find(key);
    return (it != j.end() && it->is_string()) ? it->get<std::string>() : std::string();
}

static float jsonNumber(const json &j, const char *key)
{
    auto it = j.find(key);
    return (it != j.end() && it->is_number()) ? it->get<float>() : 0.0f;
}

/* Icon for the National Weather Service short forecast ("Mostly Cloudy", "Chance Rain Showers",
 * "Patchy Fog", ...). Anything unrecognised, and the empty text of a tile without data yet, gets the
 * generic partly cloudy icon. */
static ui_icon_type_t weatherIcon(std::string condition, bool day)
{
    for (char &c : condition)
        c = (char)tolower((unsigned char)c);
    auto has = [&](std::initializer_list<const char *> words) {
        for (const char *w : words)
            if (condition.find(w) != std::string::npos)
                return true;
        return false;
    };

    if (has({"thunder"}))
        return UI_ICON_WEATHER_THUNDERSTORM;
    if (has({"snow", "sleet", "flurr", "blizzard", "freezing", "ice", "wintry"}))
        return UI_ICON_WEATHER_SNOW;
    if (has({"rain", "shower", "drizzle"}))
        return UI_ICON_WEATHER_RAIN;
    if (has({"fog", "haze", "smoke", "mist", "dust"}))
        return UI_ICON_WEATHER_FOG;
    if (has({"partly"}))
        return day ? UI_ICON_WEATHER_PARTLY_CLOUDY_DAY : UI_ICON_WEATHER_PARTLY_CLOUDY_NIGHT;
    if (has({"cloud", "overcast"}))
        return UI_ICON_WEATHER_CLOUDY;
    if (has({"sunny", "clear", "fair"}))
        return day ? UI_ICON_WEATHER_SUNNY : UI_ICON_WEATHER_CLEAR_NIGHT;
    return UI_ICON_WEATHER_PARTLY_CLOUDY_DAY;
}

ui_icon_type_t tile_weather_icon(const std::string &binding)
{
    std::string condition;
    bool day = true;
    if (auto weather = std::dynamic_pointer_cast<Weather>(::root->leafFromPath("/weather"))) {
        const json config = weather->configPtr->get();
        auto it = config.find(weatherKey(binding));
        if (it != config.end() && it->is_object()) {
            condition = jsonString(*it, "condition");
            auto d = it->find("isDaytime");
            if (d != it->end() && d->is_boolean()) day = d->get<bool>();
        }
    }
    return weatherIcon(condition, day);
}

std::string tile_weather_degrees(const std::string &binding)
{
    if (auto weather = std::dynamic_pointer_cast<Weather>(::root->leafFromPath("/weather"))) {
        const json config = weather->configPtr->get();
        auto it = config.find(weatherKey(binding));
        if (it != config.end() && it->is_object() && it->contains("temperature")) {
            char number[16];
            snprintf(number, sizeof(number), "%.0f", jsonNumber(*it, "temperature"));
            return number;
        }
    }
    return std::string();
}

/* The icons' artwork fills this many px of their 74px canvas (see ui/icons/gen_weather_icons.py). */
#define ICON_ART_PX 70

/* An icon whose artwork fills @p size px. The 74px source is scaled, but LVGL keeps the unscaled size for layout,
 * so the icon sits centred in a box of @p size. */
lv_obj_t *tile_weather_icon_create(lv_obj_t *parent, ui_icon_type_t type, int size)
{
    lv_obj_t *box = tile_container(parent);
    lv_obj_set_size(box, size, size);
    lv_obj_t *icon = UI_ICON_CREATE_DEFAULT(box, type);
    if (icon) {
        lv_image_set_pivot(icon, LV_PCT(50), LV_PCT(50));
        lv_image_set_scale(icon, 256 * size / ICON_ART_PX);   /* LVGL scale: 256 == 100% */
        lv_obj_align(icon, LV_ALIGN_CENTER, 0, 0);
    }
    return box;
}

/* The temperature: the digits in @p font and a degree sign, which is a ring scaled to the digits with its top
 * level with the top of the digits (the fonts' own degree sign is tiny, and the 140px font has none).
 * Returns nullptr, creating nothing, when that is wider than @p maxW. */
static lv_obj_t *temperature_create(lv_obj_t *parent, const std::string &number, const lv_font_t *font,
                                    lv_style_t *style, int maxW)
{
    lv_font_glyph_dsc_t zero = {};
    const bool haveZero = lv_font_get_glyph_dsc(font, &zero, '0', 0);
    const int digitH   = haveZero ? (int)zero.box_h : font->line_height * 7 / 10;
    const int digitTop = haveZero ? (int)(font->line_height - font->base_line) - (zero.ofs_y + (int)zero.box_h) : 0;
    const int ringD      = std::max(6, digitH * 30 / 100);
    const int ringStroke = std::max(2, digitH * 7 / 100);
    const int ringGap    = std::max(2, digitH / 25);
    if (tile_text_width(number, font) + ringGap + ringD > maxW)
        return nullptr;

    lv_obj_t *row = tile_container(parent);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(row, ringGap, 0);
    tile_label(row, number, font, style);

    lv_color_t color = lv_color_white();
    lv_style_value_t textColor;
    if (lv_style_get_prop(style, LV_STYLE_TEXT_COLOR, &textColor) == LV_STYLE_RES_FOUND)
        color = textColor.color;
    lv_obj_t *ring = tile_container(row);
    lv_obj_set_size(ring, ringD, ringD);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ring, ringStroke, 0);
    lv_obj_set_style_border_opa(ring, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(ring, color, 0);
    lv_obj_set_style_margin_top(ring, std::max(0, digitTop), 0);
    return row;
}

lv_obj_t *tile_weather_degrees_create(lv_obj_t *parent, const std::string &degrees, const lv_font_t *font,
                                      lv_style_t *style)
{
    return temperature_create(parent, degrees, font, style, LV_COORD_MAX);
}

void tile_weather_build(lv_obj_t *tile, const std::string &name,
                        const std::string &binding, int w, int h)
{
    const bool tall = h >= ui_pane_h();
    const bool wide = w >= ui_content_w();

    std::string city      = name;
    std::string condition;
    float       temperature = 0.0f;
    bool        day = true;
    if (auto weather = std::dynamic_pointer_cast<Weather>(::root->leafFromPath("/weather"))) {
        json config = weather->configPtr->get();
        auto it = config.find(weatherKey(binding));
        if (it != config.end() && it->is_object()) {
            std::string c = jsonString(*it, "city");
            if (!c.empty()) city = c;
            condition   = jsonString(*it, "condition");
            temperature = jsonNumber(*it, "temperature");
            auto d = it->find("isDaytime");
            if (d != it->end() && d->is_boolean()) day = d->get<bool>();
        }
    }

    char number[16];
    snprintf(number, sizeof(number), "%.0f", temperature);
    const std::string degree = "\xC2\xB0";   /* degree sign */

    lv_obj_set_layout(tile, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);

    /* Icon sized by scaling the 74px source. */
    auto weatherIconSized = [&](lv_obj_t *parent, int size) {
        return tile_weather_icon_create(parent, weatherIcon(condition, day), size);
    };

    if (!tall) {
        /* Name top left; icon and temperature (brand colour) bottom left. Half tiles: 48px icon and number,
         * quarter and sixth tiles: 36px icon, 32px number; smaller custom tiles get a compact version. */
        const bool large = wide && h >= 100;
        const bool small = h < 100;
        const int padX = small ? 12 : 16;
        const int padY = small ? 12 : h < 150 ? 12 : 16;
        lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_hor(tile, padX, 0);
        lv_obj_set_style_pad_ver(tile, padY, 0);

        const lv_font_t *tempFont = large ? &satoshi_medium_48 : small ? &satoshi_medium_24 : &satoshi_medium_32;
        const int iconSize = large ? 48 : small ? 24 : 36;
        const int gap      = large ? 24 : small ? 8 : 16;

        tile_adaptive_label(tile, city, small ? &satoshi_medium_20 : &satoshi_medium_24,
                            ui_theme_get_style_text_primary(), w - 2 * padX);

        lv_obj_t *row = tile_container(tile);
        lv_obj_set_layout(row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_gap(row, gap, 0);
        weatherIconSized(row, iconSize);
        if (!temperature_create(row, number, tempFont, ui_theme_get_style_brand(),
                                w - 2 * padX - iconSize - gap))
            tile_label(row, number + degree, tempFont, ui_theme_get_style_brand());
        return;
    }

    /* Full height tile: city, icon and condition, then the big temperature. */
    const int padX = 36;
    lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(tile, padX, 0);
    lv_obj_set_style_pad_ver(tile, 24, 0);

    tile_adaptive_label(tile, city, &satoshi_medium_24, ui_theme_get_style_text_secondary(), w - 2 * padX);

    lv_obj_t *row = tile_container(tile);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(row, 16, 0);
    const int iconSize = wide ? 48 : 36;
    weatherIconSized(row, iconSize);
    tile_adaptive_label(row, condition, &satoshi_medium_24, ui_theme_get_style_text_secondary(),
                        w - 2 * padX - iconSize - 16);

    /* When the number is too wide for the tile, it falls back to the 48px font. */
    if (!temperature_create(tile, number, &satoshi_medium_140, ui_theme_get_style_text_primary(), w - 2 * padX))
        tile_label(tile, number + degree, &satoshi_medium_48, ui_theme_get_style_text_primary());
}
