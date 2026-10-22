/**
 * @file scroll_pane.c
 * @brief Horizontally-scrollable blocks pane for LVGL
 */

#include "scroll_pane.h"
#include "tile.h"
#include "../ui_design_tokens.h"
#include "lvgl/lvgl.h"
#include <stdlib.h>
#include <string.h>

#define SP_MAX_PAGES  16
#define SP_MAX_BLOCKS 64

#define COL_BG        ui_get_background_color()
#define COL_BRAND     ui_get_brand_color()
#define COL_TAB_INACT ui_get_border_color()
#define COL_TAB_TXT   ui_get_text_primary_color()

typedef struct {
    scroll_pane_props_t props;

    lv_obj_t *scroll_area;
    lv_obj_t *screen_panels[SP_MAX_PAGES];
    lv_obj_t *tile_objs[SP_MAX_PAGES][SP_MAX_BLOCKS];

    lv_obj_t *tab_bar;
    lv_obj_t *tab_btns[SP_MAX_PAGES];

    int cur_screen;
    int pane_w;
    int content_h;
} sp_data_t;

static sp_data_t *sp_get(lv_obj_t *pane)
{
    return (sp_data_t *)lv_obj_get_user_data(pane);
}

static void sp_set_tab_active(sp_data_t *sp, int idx)
{
    for (int i = 0; i < sp->props.num_screens && i < SP_MAX_PAGES; i++) {
        if (!sp->tab_btns[i]) continue;
        lv_obj_set_style_border_color(sp->tab_btns[i],
            (i == idx) ? COL_BRAND : COL_TAB_INACT, 0);
    }
}

static void sp_tab_cb(lv_event_t *e)
{
    lv_obj_t  *btn  = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t  *pane = (lv_obj_t *)lv_event_get_user_data(e);
    int        idx  = (int)(intptr_t)lv_obj_get_user_data(btn);
    scroll_pane_set_screen(pane, idx);
}

static void sp_scroll_end_cb(lv_event_t *e)
{
    lv_obj_t  *sa   = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t  *pane = (lv_obj_t *)lv_event_get_user_data(e);
    sp_data_t *sp   = sp_get(pane);
    if (!sp || sp->pane_w <= 0) return;

    lv_coord_t x          = lv_obj_get_scroll_x(sa);
    int        screen_idx = (int)((x + sp->pane_w / 2) / sp->pane_w);
    if (screen_idx < 0) screen_idx = 0;
    if (screen_idx >= sp->props.num_screens) screen_idx = sp->props.num_screens - 1;

    if (screen_idx != sp->cur_screen) {
        sp->cur_screen = screen_idx;
        sp_set_tab_active(sp, screen_idx);
        if (sp->props.screen_changed_cb)
            sp->props.screen_changed_cb(screen_idx, sp->props.user_data);
    }
}

static void sp_scroll_cb(lv_event_t *e)
{
    lv_obj_t  *sa   = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t  *pane = (lv_obj_t *)lv_event_get_user_data(e);
    sp_data_t *sp   = sp_get(pane);
    if (!sp || sp->pane_w <= 0 || !sp->props.screen_scroll_cb) return;

    lv_coord_t x = lv_obj_get_scroll_x(sa);
    float pos = (float)x / (float)sp->pane_w;
    if (pos < 0.0f) pos = 0.0f;
    float max_pos = (float)(sp->props.num_screens - 1);
    if (pos > max_pos) pos = max_pos;

    sp->props.screen_scroll_cb(pos, sp->props.user_data);
}

static void sp_trim_outer_tile_borders(lv_obj_t *panel, int pane_w, int content_h)
{
    if (!panel) return;

    lv_obj_update_layout(panel);

    uint32_t child_count = lv_obj_get_child_count(panel);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *blk = lv_obj_get_child(panel, i);
        if (!blk) continue;

        lv_coord_t x = lv_obj_get_x(blk);
        lv_coord_t y = lv_obj_get_y(blk);
        lv_coord_t w = lv_obj_get_width(blk);
        lv_coord_t h = lv_obj_get_height(blk);

        int on_right_edge = (x + w) >= pane_w;
        int on_bottom_edge = (y + h) >= content_h;

        lv_border_side_t sides = LV_BORDER_SIDE_BOTTOM;
        if (!on_right_edge) {
            sides = (lv_border_side_t)(sides | LV_BORDER_SIDE_RIGHT);
        }
        if (on_bottom_edge) {
            sides = (lv_border_side_t)(sides & (~LV_BORDER_SIDE_BOTTOM));
        }

        lv_obj_set_style_border_side(blk, sides, 0);
    }
}

lv_obj_t *scroll_pane_create(lv_obj_t *parent,
                               const scroll_pane_props_t *props)
{
    if (!props || props->num_screens <= 0) return NULL;
    if (props->pane_w <= 0 || props->pane_h <= 0) return NULL;

    int tab_h     = (props->show_tab_bar) ? SCROLL_TAB_H : 0;
    int content_h = props->pane_h - tab_h;
    int pane_w    = props->pane_w;

    /* ---- Outer container ---- */
    lv_obj_t *pane = lv_obj_create(parent);
    lv_obj_remove_style_all(pane);
    lv_obj_add_style(pane, ui_theme_get_style_bg(), 0);
    lv_obj_set_style_bg_opa(pane, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pane, 0, 0);
    lv_obj_set_style_pad_all(pane, 0, 0);
    lv_obj_clear_flag(pane, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(pane, pane_w, props->pane_h);

    sp_data_t *sp = (sp_data_t *)calloc(1, sizeof(sp_data_t));
    sp->props      = *props;
    sp->pane_w     = pane_w;
    sp->content_h  = content_h;
    sp->cur_screen = (props->initial_screen >= 0 &&
                      props->initial_screen < props->num_screens)
                     ? props->initial_screen : 0;
    lv_obj_set_user_data(pane, sp);

    /* ---- Scroll area ---- */
    lv_obj_t *sa = lv_obj_create(pane);
    lv_obj_remove_style_all(sa);
    lv_obj_set_style_bg_opa(sa, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(sa, 0, 0);
    lv_obj_set_style_pad_gap(sa, 0, 0);
    lv_obj_set_size(sa, pane_w, content_h);
    lv_obj_align(sa, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_layout(sa, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(sa, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sa, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(sa, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(sa, LV_SCROLL_SNAP_START);
    lv_obj_add_event_cb(sa, sp_scroll_cb, LV_EVENT_SCROLL, pane);
    lv_obj_add_event_cb(sa, sp_scroll_end_cb, LV_EVENT_SCROLL_END, pane);
    sp->scroll_area = sa;

    /* ---- Screens ---- */
    int n = props->num_screens;
    for (int pi = 0; pi < n && pi < SP_MAX_PAGES; pi++) {
        const scroll_screen_t *pg = &props->screens[pi];

        /* Create and size the page panel BEFORE adding blocks */
        lv_obj_t *panel = lv_obj_create(sa);
        lv_obj_remove_style_all(panel);
        lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(panel, 0, 0);
        lv_obj_set_style_pad_gap(panel, 0, 0);
        lv_obj_set_size(panel, pane_w, content_h);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(panel, LV_OBJ_FLAG_OVERFLOW_VISIBLE);   /* allow blocks to render naturally */
        lv_obj_set_style_clip_corner(panel, true, 0);
        lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        sp->screen_panels[pi] = panel;

        /* Add blocks now that panel is sized */
        int nb = pg->num_blocks < SP_MAX_BLOCKS ? pg->num_blocks : SP_MAX_BLOCKS;
        for (int bi = 0; bi < nb; bi++) {
            /* If the page has a tap handler and the individual block does not,
             * inject it via a temporary copy of the config. */
            tile_config_t cfg = pg->blocks[bi];
            if (!cfg.tapped_cb && pg->tile_tapped_cb) {
                cfg.tapped_cb  = pg->tile_tapped_cb;
                cfg.user_data  = pg->tile_user_data;
            }
            lv_obj_t *blk = tile_create(panel, &cfg);
            sp->tile_objs[pi][bi] = blk;
        }

        /* Hide dividers on the page perimeter so display-edge borders are not visible. */
        sp_trim_outer_tile_borders(panel, pane_w, content_h);
    }

    /* ---- Tab bar ---- */
    if (props->show_tab_bar) {
        lv_obj_t *bar = lv_obj_create(pane);
        lv_obj_remove_style_all(bar);
        lv_obj_add_style(bar, ui_theme_get_style_bg(), 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
        lv_obj_add_style(bar, ui_theme_get_style_border(), 0);
        lv_obj_set_style_border_width(bar, 1, 0);
        lv_obj_set_size(bar, pane_w, SCROLL_TAB_H);
        lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_set_layout(bar, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_EVENLY,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(bar, 0, 0);
        sp->tab_bar = bar;

        int btn_w = pane_w / (n > 0 ? n : 1);
        for (int i = 0; i < n && i < SP_MAX_PAGES; i++) {
            lv_obj_t *btn = lv_obj_create(bar);
            lv_obj_remove_style_all(btn);
            lv_obj_add_style(btn, ui_theme_get_style_bg(), 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
            lv_obj_set_size(btn, btn_w, SCROLL_TAB_H);
            lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_BOTTOM, 0);
            lv_obj_set_style_border_width(btn, 2, 0);
            lv_obj_add_style(btn, ui_theme_get_style_border(), 0);
            lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);

            lv_obj_t *lbl = lv_label_create(btn);
            lv_obj_add_style(lbl, ui_theme_get_style_text_primary(), 0);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
            lv_label_set_text(lbl,
                props->screens[i].tab_label ? props->screens[i].tab_label : "");
            lv_obj_center(lbl);

            lv_obj_add_event_cb(btn, sp_tab_cb, LV_EVENT_CLICKED, pane);
            lv_obj_set_user_data(btn, (void *)(intptr_t)i);
            sp->tab_btns[i] = btn;
        }
    }

    /* Activate initial screen */
    sp_set_tab_active(sp, sp->cur_screen);
    if (sp->cur_screen > 0)
        lv_obj_scroll_to_x(sa, sp->cur_screen * pane_w, LV_ANIM_OFF);

    return pane;
}

void scroll_pane_set_screen_anim(lv_obj_t *pane, int screen_idx, bool anim)
{
    sp_data_t *sp = sp_get(pane);
    if (!sp) return;
    if (screen_idx < 0) screen_idx = 0;
    if (screen_idx >= sp->props.num_screens) screen_idx = sp->props.num_screens - 1;

    sp->cur_screen = screen_idx;
    lv_obj_scroll_to_x(sp->scroll_area, screen_idx * sp->pane_w,
                       anim ? LV_ANIM_ON : LV_ANIM_OFF);
    sp_set_tab_active(sp, screen_idx);

    if (sp->props.screen_changed_cb)
        sp->props.screen_changed_cb(screen_idx, sp->props.user_data);
}

void scroll_pane_set_screen(lv_obj_t *pane, int screen_idx)
{
    scroll_pane_set_screen_anim(pane, screen_idx, true);
}

int scroll_pane_get_screen(lv_obj_t *pane)
{
    sp_data_t *sp = sp_get(pane);
    return sp ? sp->cur_screen : 0;
}

void scroll_pane_update_block(lv_obj_t *pane,
                               int screen_idx, int tile_idx,
                               const char *value_str, float bar_fill)
{
    sp_data_t *sp = sp_get(pane);
    if (!sp) return;
    if (screen_idx < 0 || screen_idx >= SP_MAX_PAGES)  return;
    if (tile_idx   < 0 || tile_idx   >= SP_MAX_BLOCKS) return;
    lv_obj_t *blk = sp->tile_objs[screen_idx][tile_idx];
    if (blk) tile_update_value(blk, value_str, bar_fill);
}

void scroll_pane_destroy(lv_obj_t *pane)
{
    sp_data_t *sp = sp_get(pane);
    if (sp) {
        lv_obj_set_user_data(pane, NULL);
        free(sp);
    }
    lv_obj_del(pane);
}
