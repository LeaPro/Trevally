/**
 * @file ui_icon_data.h
 * @brief Image asset registry for the themed icon set, ported from the old
 *        lv_port_linux prototype's ui_icon_data.h/.c. Entries are in the
 *        same order as ui_icon_type_t (see ui_design_tokens.h) - index
 *        ui_icon_data[] with a ui_icon_type_t value to get the matching
 *        image descriptor. The underlying generated image data lives in
 *        ui/icons/*.c (one file per icon, untouched copies of the
 *        lv_img_conv output from the old prototype).
 */

#pragma once

#include <lvgl.h>

extern const lv_image_dsc_t *const ui_icon_data[];
