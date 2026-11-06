/**
 * @file nav.h
 * @brief Navigation between destinations and pages.
 *
 *        A destination is a top-level screen that lives for the whole run
 *        (home, settings); it is created once and never deleted. A page is a
 *        transient screen created by a tile type and pushed over the current
 *        destination; popping slides back and deletes it. Pages can push
 *        further pages.
 *
 * Encoder input: pushing a page moves focus (in edit mode, as scroll_pane
 * does) to the page's focus object, so rotation arrives there as
 * LV_KEY_LEFT / LV_KEY_RIGHT. Popping or going to a destination refocuses
 * that destination's focus object.
 */

#ifndef NAV_H
#define NAV_H

#include "lvgl/lvgl.h"

typedef struct {
    lv_obj_t *screen;   /* parent for the page's widgets */
    lv_obj_t *focus;    /* invisible; attach an LV_EVENT_KEY handler here to use the encoder */
} nav_page_t;

/* Same shape as a page, but never deleted. */
typedef nav_page_t nav_dest_t;

/** Registers the main screen (the active one) as the home destination, with the object that takes encoder focus on it. */
void nav_init(lv_obj_t *home_focus);

/** Creates an empty themed screen. Build its widgets on .screen, then reach it with nav_goto() (destination) or nav_push() (page). */
nav_page_t nav_screen_create();

void nav_push(const nav_page_t &page);   /* pages (push / replace / pop) switch instantly; only nav_goto animates */

/** Swaps the top page for @p page (deleting the page it replaces); back from @p page returns to the page below. */
void nav_replace(const nav_page_t &page);

/** Back @p count pages (default one). The popped page is deleted; pages under it that are also closed are deleted too.
 *  With no page open, goes home from any other destination. */
void nav_pop(size_t count = 1);

/** Switches to a destination, closing any open pages. Slides right when going home, left otherwise. */
void nav_goto(const nav_dest_t &dest, bool animate = true);

/** Closes all open pages and returns to the current destination. */
void nav_close_pages(bool animate);

/** LV_EVENT_CLICKED handler that calls nav_pop(); for a header's back slot. */
void nav_back_cb(lv_event_t *e);

#endif /* NAV_H */
