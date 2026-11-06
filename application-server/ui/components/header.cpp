/**
 * @file header.cpp
 * @brief See header.h.
 */

#include "header.h"
#include "adaptive_text.h"

#include <string.h>

#define HEADER_TITLE_MAX_LINES 1
static lv_obj_t *make_slot(lv_obj_t *header, const header_slot_t &slot)
{
    lv_obj_t *s = lv_obj_create(header);
    lv_obj_remove_style_all(s);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(s, UI_HEADER_H_PX, UI_HEADER_H_PX);
    if (!slot.show) return s;

    if (slot.text) {
        lv_obj_t *label = lv_label_create(s);
        lv_obj_add_style(label, ui_theme_get_style_text_primary(), 0);
        lv_obj_set_style_text_font(label, &satoshi_medium_20, 0);
        lv_label_set_text(label, slot.text);
        lv_obj_center(label);
    } else {
        lv_obj_t *icon = UI_ICON_CREATE_DEFAULT(s, slot.icon);
        if (icon) lv_obj_center(icon);
    }

    if (slot.on_click) {
        lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s, slot.on_click, LV_EVENT_CLICKED, slot.user_data);
    }
    return s;
}

lv_obj_t *header_create(lv_obj_t *parent, const header_slot_t &left,
                        const char *title, const header_slot_t &right, const lv_font_t *font)
{
    lv_obj_t *h = lv_obj_create(parent);
    lv_obj_remove_style_all(h);
    lv_obj_clear_flag(h, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(h, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(h, ui_content_w(), UI_HEADER_H_PX);
    lv_obj_align(h, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_layout(h, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(h, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(h, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    make_slot(h, left);

    lv_obj_t *t = lv_label_create(h);
    lv_obj_add_style(t, ui_theme_get_style_text_primary(), 0);
    lv_obj_set_style_text_font(t, font, 0);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_flex_grow(t, 1);

    make_slot(h, right);

    /* Adaptive text needs the label's final width, so lay out first. */
    lv_obj_update_layout(h);
    ui_adaptive_text_set(t, title, HEADER_TITLE_MAX_LINES);
    return h;
}

void header_set_title(lv_obj_t *header, const char *title)
{
    if (!header) return;
    lv_obj_t *t = lv_obj_get_child(header, 1);   /* children: left slot, title, right slot */
    const char *text = title ? title : "";
    if (strcmp(lv_label_get_text(t), text) != 0)
        ui_adaptive_text_set(t, text, HEADER_TITLE_MAX_LINES);
}
