/**
 * @file scroll_pane.h
 * @brief Horizontally-scrolling container of screens.
 *
 * The pane builds itself from the Screens leaf (screenOrder + config); each
 * entry becomes a screen (see screen.h), up to SCROLL_MAX_SCREENS.
 */

#ifndef SCROLL_PANE_H
#define SCROLL_PANE_H

#include "lvgl/lvgl.h"
#include "screen.h"

#define SCROLL_PAGE_W      SCREEN_W
#define SCROLL_PANE_H      SCREEN_H
#define SCROLL_MAX_SCREENS 16

#ifdef __cplusplus
extern "C" {
#endif

/** Create the pane and build its screens from the Screens leaf. */
lv_obj_t *scroll_pane_create(lv_obj_t *parent);

/** Rebuild the screens in place (after Screens / Tiles config changes). */
void scroll_pane_refresh(lv_obj_t *pane);

void scroll_pane_destroy(lv_obj_t *pane);

void scroll_pane_set_screen(lv_obj_t *pane, int screen_idx);
void scroll_pane_set_screen_anim(lv_obj_t *pane, int screen_idx, bool anim);
int  scroll_pane_get_screen(lv_obj_t *pane);

#ifdef __cplusplus
}
#endif

#endif /* SCROLL_PANE_H */
