/**
 * @file level_gauge.cpp
 * @brief See level_gauge.h.
 */

#include "level_gauge.h"
#include "tile.h"
#include "../ui_design_tokens.h"

lv_obj_t *level_gauge_create(lv_obj_t *parent, const std::string &leafPath, const char *elementName)
{
    const TileNumber n = tile_read_number(leafPath, elementName);
    const int R = LEVEL_GAUGE_RADIUS;

    /* Arc: 270 degrees open at the bottom, range 0..1000 as the live binding expects. */
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_remove_style_all(arc);
    lv_group_remove_obj(arc);   /* keep encoder rotation off the decorative widget */
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(arc, 2 * R, 2 * R);
    lv_obj_set_pos(arc, LEVEL_GAUGE_CX - R, LEVEL_GAUGE_CY - R);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_range(arc, 0, 1000);
    lv_arc_set_value(arc, (int32_t)(n.fill * 1000.0f));

    lv_obj_add_style(arc, ui_theme_get_style_bg_tertiary(), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, LEVEL_GAUGE_ARC_W, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    lv_obj_add_style(arc, ui_theme_get_style_brand(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, LEVEL_GAUGE_ARC_W, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);

    /* Value above the arc, units just inside its top. */
    lv_obj_t *number = tile_label(parent, tile_format_number(n.value), &satoshi_medium_140,
                                  ui_theme_get_style_text_primary());
    lv_obj_align(number, LV_ALIGN_TOP_MID, 0, LEVEL_GAUGE_NUMBER_Y);

    lv_obj_t *units = tile_label(parent, n.units, &satoshi_medium_24, ui_theme_get_style_text_secondary());
    lv_obj_align(units, LV_ALIGN_TOP_MID, 0, LEVEL_GAUGE_CY - R + 14);

    /* Min / max under the arc's two ends (135 and 45 degrees from 3 o'clock). */
    const int dx = (int)(R * 0.7071f);
    const int ey = LEVEL_GAUGE_CY + dx + 6;
    lv_obj_t *minLabel = tile_label(parent, tile_format_number(n.min), &satoshi_medium_20,
                                    ui_theme_get_style_text_secondary());
    lv_obj_align(minLabel, LV_ALIGN_TOP_MID, -dx, ey);
    lv_obj_t *maxLabel = tile_label(parent, tile_format_number(n.max), &satoshi_medium_20,
                                    ui_theme_get_style_text_secondary());
    lv_obj_align(maxLabel, LV_ALIGN_TOP_MID, dx, ey);

    tile_bind_number(number, arc, leafPath, elementName);
    return arc;
}
