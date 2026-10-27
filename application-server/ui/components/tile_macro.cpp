/**
 * @file tile_macro.cpp
 * @brief Macro tile: title at the top and the macro's state in the brand
 *        colour. No leaf is bound yet, so the state is a placeholder.
 */

#include "tile.h"
#include "../ui_design_tokens.h"

void tile_macro_build(lv_obj_t *root, const std::string &name,
                      const std::string &binding, int w, int h)
{
    (void)binding;
    const bool tall = h >= 386;
    const bool wide = w >= 480;
    const int  pad  = tall ? 36 : 16;

    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(root, pad, 0);

    tile_adaptive_label(root, name, &satoshi_medium_24, ui_theme_get_style_text_primary(),
                        w - 2 * pad);

    tile_adaptive_label(root, "-", wide ? &satoshi_medium_48 : &satoshi_medium_32,
                        ui_theme_get_style_brand(), w - 2 * pad);
}
