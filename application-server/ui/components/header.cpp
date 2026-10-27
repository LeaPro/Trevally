/**
 * @file header.cpp
 * @brief See header.h.
 */

#include "header.h"

static lv_obj_t *make_slot(lv_obj_t *header, const header_slot_t &slot)
{
    lv_obj_t *s = lv_obj_create(header);
    lv_obj_remove_style_all(s);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(s, HEADER_H_PX, HEADER_H_PX);
    if (!slot.show) return s;

    lv_obj_t *icon = UI_ICON_CREATE_DEFAULT(s, slot.icon);
    if (icon) lv_obj_center(icon);

    if (slot.on_click) {
        lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s, slot.on_click, LV_EVENT_CLICKED, slot.user_data);
    }
    return s;
}

lv_obj_t *header_create(lv_obj_t *parent, const header_slot_t &left,
                        const char *title, const header_slot_t &right)
{
    lv_obj_t *h = lv_obj_create(parent);
    lv_obj_remove_style_all(h);
    lv_obj_clear_flag(h, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(h, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(h, LV_PCT(100), HEADER_H_PX);
    lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_layout(h, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(h, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(h, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    make_slot(h, left);

    lv_obj_t *t = lv_label_create(h);
    lv_obj_add_style(t, ui_theme_get_style_text_primary(), 0);
    lv_obj_set_style_text_font(t, &satoshi_medium_24, 0);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(t, LV_LABEL_LONG_CLIP);
    lv_label_set_text(t, title ? title : "");
    lv_obj_set_flex_grow(t, 1);

    make_slot(h, right);
    return h;
}
