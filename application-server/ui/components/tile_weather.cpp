/**
 * @file tile_weather.cpp
 * @brief Weather tile: city, condition icon + text and temperature.
 *        Reads /weather config[binding] (binding = zip code):
 *        { city, condition, temperature }.
 */

#include "tile.h"
#include "../ui_design_tokens.h"

#include <cstdio>

#include <object.h>
#include <Weather.h>

/* The device's root object (defined in main.cpp). */
extern std::shared_ptr<RootObject> root;

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

void tile_weather_build(lv_obj_t *tile, const std::string &name,
                        const std::string &binding, int w, int h)
{
    const bool tall    = h >= ui_pane_h();
    const bool compact = h < 193;
    const bool wide    = w >= ui_content_w();
    const int  pad     = (tall || wide) ? 36 : 16;

    std::string city      = name;
    std::string condition;
    float       temperature = 0.0f;
    if (auto weather = std::dynamic_pointer_cast<Weather>(::root->leafFromPath("/weather"))) {
        json config = weather->configPtr->get();
        auto it = config.find(binding);
        if (it != config.end() && it->is_object()) {
            std::string c = jsonString(*it, "city");
            if (!c.empty()) city = c;
            condition   = jsonString(*it, "condition");
            temperature = jsonNumber(*it, "temperature");
        }
    }

    char temp[16];
    snprintf(temp, sizeof(temp), "%.0f\xC2\xB0", temperature);   /* degree sign */

    lv_obj_set_layout(tile, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    const int pad_h = compact ? 12 : pad;
    lv_obj_set_style_pad_all(tile, pad_h, 0);

    if (compact) {
        /* Short tile: city on top, temperature and condition on one line. */
        lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

        tile_adaptive_label(tile, city, &satoshi_medium_20, ui_theme_get_style_text_secondary(),
                            w - 2 * pad_h);

        lv_obj_t *row = tile_container(tile);
        lv_obj_set_layout(row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        tile_label(row, temp, &satoshi_medium_32, ui_theme_get_style_brand());
        tile_adaptive_label(row, condition, &satoshi_medium_20, ui_theme_get_style_text_secondary(),
                            w - 2 * pad_h - tile_text_width(temp, &satoshi_medium_32) - 12);
        return;
    }

    lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(tile, 24, 0);
    lv_obj_set_style_pad_gap(tile, 24, 0);

    tile_adaptive_label(tile, city, &satoshi_medium_24, ui_theme_get_style_text_secondary(),
                        w - 2 * pad);

    lv_obj_t *row = tile_container(tile);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(row, 16, 0);

    const int icon_sz = wide ? 48 : 36;
    lv_obj_t *icon = UI_ICON_CREATE_DEFAULT(row, UI_ICON_WEATHER);
    if (icon) lv_obj_set_size(icon, icon_sz, icon_sz);
    tile_adaptive_label(row, condition, &satoshi_medium_24, ui_theme_get_style_text_secondary(),
                        w - 2 * pad - icon_sz - 16);

    tile_label(tile, temp, wide ? &satoshi_medium_48 : &satoshi_medium_32,
               tall ? ui_theme_get_style_text_primary() : ui_theme_get_style_brand());
}
