/**
 * @file header.h
 * @brief Page header: an icon slot on the left, optional centre text and an
 *        icon slot on the right. Icon slots are clickable when given a handler.
 */

#ifndef HEADER_H
#define HEADER_H

#include "lvgl/lvgl.h"
#include "../ui_design_tokens.h"

#define HEADER_H_PX 56   /* header height and icon slot width (touch target) */

typedef struct {
    bool           show;        /* false: empty slot (its width is still reserved) */
    ui_icon_type_t icon;
    lv_event_cb_t  on_click;    /* NULL: placeholder, not clickable */
    void          *user_data;
} header_slot_t;

/**
 * @brief Create a header across the top of @p parent.
 * @param title  Centre text; NULL or "" leaves the centre empty.
 */
lv_obj_t *header_create(lv_obj_t *parent, const header_slot_t &left,
                        const char *title, const header_slot_t &right);

#endif /* HEADER_H */
