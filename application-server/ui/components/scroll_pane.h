/**
 * @file scroll_pane.h
 * @brief Horizontally-scrollable blocks pane for LVGL
 *
 * Figma node: 5489-26087 (BlocksScroll)
 *
 * The pane holds N screens side-by-side; swipe horizontally to move
 * between them.  An optional tab bar at the bottom lets users jump
 * directly to any screen.
 *
 * Each screen is a flex-wrap row container.  Populate it with an array
 * of tile_config_t (from tile.h).  Blocks declare their own size
 * (Full / Half / Quarter / Sixth) and the flex layout arranges them
 * automatically – no manual coordinates needed.
 *
 *   Quarter + Quarter  → side-by-side row  (240 + 240 = 480 px)
 *   Sixth   + Sixth    → side-by-side row  (240 + 240 = 480 px)
 *   Half               → full-width row    (480 px)
 *   Full               → full-width row    (480 px)
 *
 * Example:
 *
 *   static const tile_config_t blks[] = {
 *       { .type=TILE_TYPE_OUTPUT_VOLUME, .size=TILE_SIZE_FULL,
 *         .subtype=TILE_SUB_NUMBER, .label="Output Volume",
 *         .value_str="50", .unit_str="%", .bar_fill=0.5f },
 *   };
 *   static scroll_screen_t screens[] = {
 *       { .tab_label="Output", .blocks=blks, .num_blocks=1 },
 *   };
 *   scroll_pane_props_t props = {
 *       .screens=screens, .num_screens=1, .show_tab_bar=true,
 *   };
 *   lv_obj_t *pane = scroll_pane_create(parent, &props);
 *   lv_obj_set_size(pane, 480, 386 + SCROLL_TAB_H);
 */

#ifndef SCROLL_PANE_H
#define SCROLL_PANE_H

#include "tile.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Dimensions
 * ====================================================================== */

#define SCROLL_PAGE_W  480   /* screen width – matches display width */
#define SCROLL_TAB_H    48   /* height of the bottom tab bar       */

/* =========================================================================
 * Screen definition
 * ====================================================================== */

typedef struct {
    const char          *tab_label;   /* text in the tab bar button    */
    const tile_config_t *blocks;      /* array of tile_config_t        */
    int                   num_blocks; /* length of blocks[]            */

    /* Optional: if set, every block on this screen gets this tap handler.
     * The block's own tapped_cb is used when non-NULL; this is the
     * per-screen fallback applied to all blocks that have no individual cb. */
    lv_event_cb_t  tile_tapped_cb;
    void          *tile_user_data;
} scroll_screen_t;

/* =========================================================================
 * Pane properties
 * ====================================================================== */

typedef struct {
    scroll_screen_t *screens;
    int              num_screens;
    int              initial_screen;  /* 0-based index of screen shown on start */
    bool             show_tab_bar;

    /* Pixel dimensions of the pane (REQUIRED – set these before calling
     * scroll_pane_create so the layout can be applied immediately).
     * pane_h should include the tab bar height when show_tab_bar=true.
     * Content height = pane_h - (show_tab_bar ? SCROLL_TAB_H : 0).
     * pane_w is also used as the screen width (typically 480). */
    int  pane_w;
    int  pane_h;

    /* Called when the visible screen changes (swipe or tab tap) */
    void (*screen_changed_cb)(int screen_idx, void *user_data);

    /* Called continuously while horizontally scrolling.
     * screen_pos is in [0, num_screens-1], where 0.0=screen0, 1.0=screen1, ... */
    void (*screen_scroll_cb)(float screen_pos, void *user_data);

    void  *user_data;
} scroll_pane_props_t;

/* =========================================================================
 * Public API
 * ====================================================================== */

/**
 * @brief Create the scroll pane as a child of @p parent.
 *
 * Call lv_obj_set_size() after creation.
 * Recommended height: content_h + (show_tab_bar ? SCROLL_TAB_H : 0)
 * where content_h is the height of your tallest page.
 */
lv_obj_t *scroll_pane_create(lv_obj_t *parent,
                               const scroll_pane_props_t *props);

/** @brief Destroy the pane and free internal state. */
void scroll_pane_destroy(lv_obj_t *pane);

/** @brief Programmatically jump to @p screen_idx (animated). */
void scroll_pane_set_screen(lv_obj_t *pane, int screen_idx);

/** @brief Programmatically jump to @p screen_idx with explicit animation control. */
void scroll_pane_set_screen_anim(lv_obj_t *pane, int screen_idx, bool anim);

/** @brief Return the index of the currently visible screen. */
int  scroll_pane_get_screen(lv_obj_t *pane);

/**
 * @brief Live-update a block's value text and bar fill.
 *
 * @param screen_idx  Screen index (0-based)
 * @param tile_idx    Tile index within that screen (0-based)
 * @param value_str  New value string, or NULL to leave unchanged
 * @param bar_fill   New fill ratio 0.0–1.0 (ignored when block has no bar)
 */
void scroll_pane_update_block(lv_obj_t *pane,
                               int page_idx, int tile_idx,
                               const char *value_str, float bar_fill);

#ifdef __cplusplus
}
#endif

#endif /* SCROLL_PANE_H */
