/**
 * @file nav.h
 * @brief Page navigation. A page is a full LVGL screen created by a tile
 *        type and pushed over the main screen; popping slides back and
 *        deletes it. Pages can push further pages.
 *
 * Encoder input: pushing a page moves focus (in edit mode, as scroll_pane
 * does) to the page's focus object, so rotation arrives there as
 * LV_KEY_LEFT / LV_KEY_RIGHT. Popping to the main screen refocuses its pane.
 */

#ifndef NAV_H
#define NAV_H

#include "lvgl/lvgl.h"

typedef struct {
    lv_obj_t *screen;   /* parent for the page's widgets */
    lv_obj_t *focus;    /* invisible; attach an LV_EVENT_KEY handler here to use the encoder */
} nav_page_t;

/** Registers the main screen (the active one) and the object that takes encoder focus on it. */
void nav_init(lv_obj_t *home_focus);

/** Creates an empty themed page. Build its widgets on page.screen, then nav_push() it. */
nav_page_t nav_page_create();

void nav_push(const nav_page_t &page);

/** Back one page (slides right and deletes the popped page). */
void nav_pop();

/** Back to the main screen from any depth. */
void nav_home(bool animate);

/** LV_EVENT_CLICKED handler that calls nav_pop(); for a header's back slot. */
void nav_back_cb(lv_event_t *e);

#endif /* NAV_H */
