/**
 * @file scroll_pane.h
 * @brief Horizontally-scrolling container of screens.
 *
 * The pane builds itself from the Screens leaf (screenOrder + config); each
 * entry becomes a screen (see screen.h), up to SCROLL_MAX_SCREENS. The pane is
 * ui_content_w() x ui_pane_h() (ui_design_tokens.h); when there is more than
 * one screen, pagination dots are drawn in the header band above it.
 */

#ifndef SCROLL_PANE_H
#define SCROLL_PANE_H

#include "lvgl/lvgl.h"
#include "screen.h"

#define SCROLL_MAX_SCREENS 16

/** Create the pane and build its screens from the Screens leaf. */
lv_obj_t *scroll_pane_create(lv_obj_t *parent);

/** Rebuild the screens in place (after Screens / Tiles config changes). */
void scroll_pane_refresh(lv_obj_t *pane);

void scroll_pane_destroy(lv_obj_t *pane);

void scroll_pane_set_screen(lv_obj_t *pane, int screen_idx);
void scroll_pane_set_screen_anim(lv_obj_t *pane, int screen_idx, bool anim);
int  scroll_pane_get_screen(lv_obj_t *pane);

/** Name of the current screen ("" if there are none). Valid until the pane is refreshed. */
const char *scroll_pane_get_screen_name(lv_obj_t *pane);

#endif /* SCROLL_PANE_H */
