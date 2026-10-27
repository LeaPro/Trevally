/**
 * @file tile_input.cpp
 * @brief Input tile: title, current source name, and the input's volume
 *        level. Both are "-" placeholders until they are read from
 *        /leaInput config.
 */

#include "tile.h"
#include "../ui_design_tokens.h"

void tile_input_build(lv_obj_t *root, const std::string &name,
                      const std::string &binding, int w, int h)
{
    (void)binding;
    const bool tall  = h >= 386;
    const bool wide  = w >= 480;
    const bool short_ = h < 150;
    const int  pad   = tall ? 36 : 16;
    const int  pad_v = short_ ? 12 : pad;


    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root,
                          tall ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_left(root, pad, 0);
    lv_obj_set_style_pad_right(root, pad, 0);
    lv_obj_set_style_pad_top(root, pad_v, 0);
    lv_obj_set_style_pad_bottom(root, pad_v, 0);
    lv_obj_set_style_pad_gap(root, tall ? 24 : 0, 0);

    tile_adaptive_label(root, name, &satoshi_medium_24, ui_theme_get_style_text_primary(),
                        w - 2 * pad);

    /* Source name: marquee when it does not fit. */
    tile_adaptive_label(root, "-",
                        wide ? &satoshi_medium_48 : &satoshi_medium_32,
                        ui_theme_get_style_brand(), w - 2 * pad);

    lv_obj_t *row = tile_container(root);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(row, 12, 0);

    lv_obj_t *icon = UI_ICON_CREATE_DEFAULT(row, UI_ICON_VOLUME_SMALL);
    if (icon) lv_obj_set_size(icon, 18, 18);
    tile_label(row, "-", &satoshi_medium_24, ui_theme_get_style_text_primary());
}
