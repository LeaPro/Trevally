/**
 * @file header.h
 * @brief Page header: an icon slot on the left, optional centre text and an
 *        icon slot on the right. Icon slots are clickable when given a handler.
 */

#ifndef HEADER_H
#define HEADER_H

#include "lvgl/lvgl.h"
#include "../ui_design_tokens.h"

typedef struct {
    bool           show;        /* false: empty slot (its width is still reserved) */
    ui_icon_type_t icon;        /* ignored when text is set */
    lv_event_cb_t  on_click;    /* NULL: placeholder, not clickable */
    void          *user_data;
    const char    *text = nullptr;   /* non-NULL: show this text (e.g. "Edit") instead of the icon */
} header_slot_t;

/**
 * @brief Create a header across the top of @p parent: UI_HEADER_H_PX tall and
 *        ui_content_w() wide, left-aligned (landscape leaves the streaming panel's column free).
 * @param title  Centre text; NULL or "" leaves the centre empty.
 * @param font   Font of the centre text.
 */
lv_obj_t *header_create(lv_obj_t *parent, const header_slot_t &left,
                        const char *title, const header_slot_t &right,
                        const lv_font_t *font = &satoshi_medium_32);

/** Sets the centre text (adaptive: wraps, or scrolls if it doesn't fit on one line). No-op if unchanged. */
void header_set_title(lv_obj_t *header, const char *title);

#endif /* HEADER_H */
