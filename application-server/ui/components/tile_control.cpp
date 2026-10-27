/**
 * @file tile_control.cpp
 * @brief Control tile: title at the top, slider (white knob) and value at
 *        the bottom. The value is a "-" placeholder until it is read from
 *        /leaControl config.
 */

#include "tile.h"
#include "../ui_design_tokens.h"

void tile_control_build(lv_obj_t *root, const std::string &name,
                        const std::string &binding, int w, int h)
{
    (void)binding;
    const bool tall = h >= 386;
    const bool wide = w >= 480;
    const int  pad  = tall ? 36 : 16;

    TileNumber value;  /* placeholder: no value yet (fill 0) */

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

    lv_obj_t *s = lv_slider_create(group);
    lv_obj_remove_style_all(s);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(s, w - 2 * pad, 8);
    lv_slider_set_range(s, 0, 1000);
    lv_slider_set_value(s, (int32_t)(value.fill * 1000.0f), LV_ANIM_OFF);

    lv_obj_add_style(s, ui_theme_get_style_bg_secondary(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s, 4, LV_PART_MAIN);

    lv_obj_add_style(s, ui_theme_get_style_brand(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s, 4, LV_PART_INDICATOR);

    lv_obj_add_style(s, ui_theme_get_style_text_primary(), LV_PART_KNOB);
    lv_obj_add_style(s, ui_theme_get_style_knob_border(), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s, 2, LV_PART_KNOB);
    lv_obj_set_style_border_width(s, 2, LV_PART_KNOB);
    lv_obj_set_style_border_opa(s, LV_OPA_COVER, LV_PART_KNOB);

    lv_obj_t *row = tile_container(group);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(row, 4, 0);

    tile_label(row, "-",
               wide ? &satoshi_medium_48 : &satoshi_medium_32, ui_theme_get_style_brand());
}
