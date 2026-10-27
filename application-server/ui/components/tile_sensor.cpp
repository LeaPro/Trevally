/**
 * @file tile_sensor.cpp
 * @brief Sensor tile: title at the top, read-only bar (no knob) and
 *        reading at the bottom, all in the primary text colour.
 *        The reading is a "-" placeholder until it is read from /leaSensor
 *        config.
 */

#include "tile.h"
#include "../ui_design_tokens.h"

void tile_sensor_build(lv_obj_t *root, const std::string &name,
                       const std::string &binding, int w, int h)
{
    (void)binding;
    const bool tall = h >= 386;
    const bool wide = w >= 480;
    const int  pad  = tall ? 36 : 16;

    TileNumber reading;  /* placeholder: no value yet (fill 0) */

    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(root, pad, 0);

    tile_adaptive_label(root, name, &satoshi_medium_24, ui_theme_get_style_text_primary(),
                        w - 2 * pad);

    lv_obj_t *group = tile_container(root);
    lv_obj_set_layout(group, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(group, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(group, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_size(group, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(group, 12, 0);

    lv_obj_t *bar = lv_bar_create(group);
    lv_obj_remove_style_all(bar);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(bar, w - 2 * pad, 8);
    lv_bar_set_range(bar, 0, 1000);
    lv_bar_set_value(bar, (int32_t)(reading.fill * 1000.0f), LV_ANIM_OFF);
    lv_obj_add_style(bar, ui_theme_get_style_bg_tertiary(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 4, LV_PART_MAIN);
    lv_obj_add_style(bar, ui_theme_get_style_text_primary(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR);

    lv_obj_t *row = tile_container(group);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(row, 4, 0);

    tile_label(row, "-",
               wide ? &satoshi_medium_48 : &satoshi_medium_32, ui_theme_get_style_text_primary());
}
