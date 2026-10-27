/**
 * @file tile_bluetooth.cpp
 * @brief Bluetooth tile: title, volume slider above a left-aligned percentage.
 *        Reads /bluetooth/volume. Tapping it opens a page with the header and
 *        level gauge, where the encoder changes the volume.
 */

#include "tile.h"
#include "header.h"
#include "level_gauge.h"
#include "nav.h"
#include "../ui_design_tokens.h"

static lv_obj_t *make_slider(lv_obj_t *parent, int w, float fill)
{
    lv_obj_t *s = lv_slider_create(parent);
    lv_obj_remove_style_all(s);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(s, w, 8);
    lv_slider_set_range(s, 0, 1000);
    lv_slider_set_value(s, (int32_t)(fill * 1000.0f), LV_ANIM_OFF);

    lv_obj_add_style(s, ui_theme_get_style_bg_tertiary(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s, 4, LV_PART_MAIN);

    lv_obj_add_style(s, ui_theme_get_style_brand(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s, 4, LV_PART_INDICATOR);

    lv_obj_add_style(s, ui_theme_get_style_brand(), LV_PART_KNOB);
    lv_obj_add_style(s, ui_theme_get_style_knob_border(), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s, 2, LV_PART_KNOB);
    lv_obj_set_style_border_width(s, 2, LV_PART_KNOB);
    lv_obj_set_style_border_opa(s, LV_OPA_COVER, LV_PART_KNOB);
    return s;
}

void tile_bluetooth_build(lv_obj_t *root, const std::string &name,
                          const std::string &binding, int w, int h)
{
    (void)binding;
    const bool tall = h >= 386;
    const bool wide = w >= 480;
    const int  pad  = tall ? 36 : 16;

    TileNumber volume = tile_read_number("/bluetooth", "volume");

    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root,
                          tall ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(root, pad, 0);
    lv_obj_set_style_pad_gap(root, tall ? 24 : 0, 0);

    tile_adaptive_label(root, name, &satoshi_medium_24, ui_theme_get_style_text_primary(),
                        w - 2 * pad);

    lv_obj_t *group = tile_container(root);
    lv_obj_set_layout(group, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(group, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(group, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_size(group, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(group, 12, 0);

    lv_obj_t *slider = make_slider(group, w - 2 * pad, volume.fill);

    lv_obj_t *row = tile_container(group);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(row, 4, 0);

    lv_obj_t *valueLabel = tile_label(row, tile_format_number(volume.value),
               wide ? &satoshi_medium_48 : &satoshi_medium_32, ui_theme_get_style_brand());
    tile_label(row, volume.units.empty() ? "%" : volume.units,
               wide ? &satoshi_medium_32 : &satoshi_medium_24, ui_theme_get_style_brand());

    tile_bind_number(valueLabel, slider, "/bluetooth", "volume");
}

/* Encoder rotation on the page changes the volume by one per tick; a press is unmapped. */
static void bluetooth_page_key_cb(lv_event_t *e)
{
    const uint32_t key = lv_event_get_key(e);
    const int step = (key == LV_KEY_RIGHT) ? 1 : (key == LV_KEY_LEFT) ? -1 : 0;
    if (step != 0)
        tile_step_number("/bluetooth", "volume", step);
}

void tile_bluetooth_open(const std::string &name, const std::string &binding)
{
    (void)binding;
    nav_page_t page = nav_page_create();

    const header_slot_t back = {true, UI_ICON_ARROW_BACK, nav_back_cb, nullptr};
    const header_slot_t gear = {true, UI_ICON_SETTINGS, nullptr, nullptr};   /* placeholder */
    header_create(page.screen, back, name.c_str(), gear);

    level_gauge_create(page.screen, "/bluetooth", "volume");

    lv_obj_add_event_cb(page.focus, bluetooth_page_key_cb, LV_EVENT_KEY, NULL);
    nav_push(page);
}
