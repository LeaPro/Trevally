/**
 * @file scroll_pane.cpp
 * @brief See scroll_pane.h.
 */

#include "scroll_pane.h"
#include "screen.h"
#include "tile.h"
#include "../ui_design_tokens.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <Screens.h>

/* The device's root object (defined in main.cpp). */
extern std::shared_ptr<RootObject> root;

#define SP_NAME_LEN 64

typedef struct {
    lv_obj_t *scroll_area;
    lv_obj_t *dots;
    int       dots_active;
    int       num_screens;
    int       cur_screen;
    char      names[SCROLL_MAX_SCREENS][SP_NAME_LEN];
} sp_data_t;

static sp_data_t *sp_get(lv_obj_t *pane)
{
    return (sp_data_t *)lv_obj_get_user_data(pane);
}

/* Highlights dot @p active (the wide, icon-primary one); the rest are narrow and border-colored. */
static void sp_update_dots(sp_data_t *sp, int active)
{
    if (!sp->dots || active == sp->dots_active) return;
    sp->dots_active = active;

    const uint32_t count = lv_obj_get_child_count(sp->dots);
    for (uint32_t i = 0; i < count; i++) {
        lv_obj_t *dot = lv_obj_get_child(sp->dots, (int32_t)i);
        const bool on = ((int)i == active);
        lv_obj_remove_style(dot, ui_theme_get_style_bg_icon_primary(), 0);
        lv_obj_remove_style(dot, ui_theme_get_style_bg_border(), 0);
        lv_obj_add_style(dot, on ? ui_theme_get_style_bg_icon_primary()
                                 : ui_theme_get_style_bg_border(), 0);
        lv_obj_set_size(dot, on ? UI_PAGINATION_ACTIVE_W_PX : UI_PAGINATION_DOT_PX,
                        UI_PAGINATION_DOT_PX);
    }
}

static int sp_index_from_scroll(lv_obj_t *sa, int num_screens)
{
    const int w = ui_content_w();
    int idx = (int)((lv_obj_get_scroll_x(sa) + w / 2) / w);
    if (idx < 0) idx = 0;
    if (idx >= num_screens) idx = num_screens - 1;
    return idx;
}

static void sp_scroll_cb(lv_event_t *e)
{
    lv_obj_t  *sa   = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t  *pane = (lv_obj_t *)lv_event_get_user_data(e);
    sp_data_t *sp   = sp_get(pane);
    if (!sp || sp->num_screens <= 0) return;

    sp_update_dots(sp, sp_index_from_scroll(sa, sp->num_screens));
}

static void sp_scroll_end_cb(lv_event_t *e)
{
    lv_obj_t  *sa   = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t  *pane = (lv_obj_t *)lv_event_get_user_data(e);
    sp_data_t *sp   = sp_get(pane);
    if (!sp || sp->num_screens <= 0) return;

    sp->cur_screen = sp_index_from_scroll(sa, sp->num_screens);
    sp_update_dots(sp, sp->cur_screen);
}

/* Encoder: one tick = one screen, snapped with no animation. */
static void sp_key_cb(lv_event_t *e)
{
    lv_obj_t *pane = (lv_obj_t *)lv_event_get_target(e);
    uint32_t  key  = lv_event_get_key(e);
    int       step = (key == LV_KEY_RIGHT) ? 1 : (key == LV_KEY_LEFT) ? -1 : 0;
    if (step == 0) return;
    scroll_pane_set_screen_anim(pane, scroll_pane_get_screen(pane) + step, false);
}

/* Screens.screenOrder -> Screens.config entries (max SCROLL_MAX_SCREENS). */
static std::vector<json> sp_read_screens()
{
    std::vector<json> out;
    auto screens = std::dynamic_pointer_cast<Screens>(::root->leafFromPath("/screens"));
    if (!screens) return out;

    json config = screens->configPtr->get();
    json order  = screens->screenOrderPtr->get();
    if (!order.is_array()) return out;

    for (const auto &id : order) {
        if (!id.is_string()) continue;
        auto it = config.find(id.get<std::string>());
        if (it == config.end() || !it->is_object()) {
            fprintf(stderr, "scroll_pane: screenOrder entry %s has no config\n",
                    id.get<std::string>().c_str());
            continue;
        }
        if ((int)out.size() >= SCROLL_MAX_SCREENS) {
            fprintf(stderr, "scroll_pane: more than %d screens configured, ignoring the rest\n",
                    SCROLL_MAX_SCREENS);
            break;
        }
        out.push_back(*it);
    }
    return out;
}

/* Builds the scroll area and its screens inside an empty @p pane. */
static void sp_build(lv_obj_t *pane, sp_data_t *sp, int initial_screen)
{
    std::vector<json> screens = sp_read_screens();
    const int  n = (int)screens.size();

    memset(sp, 0, sizeof(*sp));
    sp->num_screens = n;
    sp->cur_screen  = (initial_screen >= 0 && initial_screen < n) ? initial_screen : 0;
    if (n == 0) return;

    /* ---- Scroll area holding the screens side by side ---- */
    lv_obj_t *sa = lv_obj_create(pane);
    lv_obj_remove_style_all(sa);
    lv_obj_set_style_bg_opa(sa, LV_OPA_TRANSP, 0);
    lv_obj_set_size(sa, ui_content_w(), ui_pane_h());
    lv_obj_align(sa, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_layout(sa, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(sa, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sa, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(sa, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(sa, LV_SCROLL_SNAP_START);
    lv_obj_add_event_cb(sa, sp_scroll_cb, LV_EVENT_SCROLL, pane);
    lv_obj_add_event_cb(sa, sp_scroll_end_cb, LV_EVENT_SCROLL_END, pane);
    sp->scroll_area = sa;

    for (int i = 0; i < n; i++) {
        screen_create(sa, screens[i]);
        snprintf(sp->names[i], SP_NAME_LEN, "%s", screen_name(screens[i]).c_str());
    }

    if (sp->cur_screen > 0)
        lv_obj_scroll_to_x(sa, sp->cur_screen * ui_content_w(), LV_ANIM_OFF);

    /* ---- Pagination dots: sit in the header band, i.e. above the pane. A child can't be
     * drawn outside its parent (overflow-visible only extends the parent's clip by its
     * shadow/outline size), so they are siblings of the pane, over its left-aligned
     * column, and are removed with it. ---- */
    if (n > 1) {
        lv_obj_t *dots = lv_obj_create(lv_obj_get_parent(pane));
        lv_obj_remove_style_all(dots);
        lv_obj_remove_flag(dots, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(dots, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(dots, ui_content_w(), UI_PAGINATION_DOT_PX);
        lv_obj_align(dots, LV_ALIGN_TOP_LEFT, 0, UI_PAGINATION_TOP_PX);
        lv_obj_set_layout(dots, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(dots, UI_PAGINATION_GAP_PX, 0);

        for (int i = 0; i < n; i++) {
            lv_obj_t *dot = lv_obj_create(dots);
            lv_obj_remove_style_all(dot);
            lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_radius(dot, UI_PAGINATION_DOT_PX / 2, 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
            lv_obj_set_size(dot, UI_PAGINATION_DOT_PX, UI_PAGINATION_DOT_PX);
            lv_obj_add_style(dot, ui_theme_get_style_bg_border(), 0);
        }

        sp->dots        = dots;
        sp->dots_active = -1;
        sp_update_dots(sp, sp->cur_screen);
    }
}

/* =========================================================================
 * Public API
 * ====================================================================== */

lv_obj_t *scroll_pane_create(lv_obj_t *parent)
{
    lv_obj_t *pane = lv_obj_create(parent);
    lv_obj_remove_style_all(pane);
    lv_obj_add_style(pane, ui_theme_get_style_bg(), 0);
    lv_obj_set_style_bg_opa(pane, LV_OPA_COVER, 0);
    lv_obj_clear_flag(pane, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(pane, ui_content_w(), ui_pane_h());

    sp_data_t *sp = (sp_data_t *)calloc(1, sizeof(sp_data_t));
    lv_obj_set_user_data(pane, sp);

    /* Encoder input: the pane itself is the focused object, in edit mode, so
     * rotation arrives as LV_KEY_LEFT / LV_KEY_RIGHT. The pane is not
     * scrollable, so LVGL's own (animated) key scrolling doesn't apply. */
    lv_obj_add_event_cb(pane, sp_key_cb, LV_EVENT_KEY, NULL);
    lv_group_t *group = lv_group_get_default();
    if (group) {
        lv_group_add_obj(group, pane);
        lv_group_focus_obj(pane);
        lv_group_set_editing(group, true);
    }

    sp_build(pane, sp, 0);
    return pane;
}

void scroll_pane_refresh(lv_obj_t *pane)
{
    sp_data_t *sp = sp_get(pane);
    if (!sp) return;

    int keep = sp->cur_screen;
    tile_live_clear(lv_obj_get_screen(pane));
    if (sp->dots) lv_obj_delete(sp->dots);
    lv_obj_clean(pane);
    sp_build(pane, sp, keep);
}

void scroll_pane_set_screen_anim(lv_obj_t *pane, int screen_idx, bool anim)
{
    sp_data_t *sp = sp_get(pane);
    if (!sp || sp->num_screens <= 0) return;
    if (screen_idx < 0) screen_idx = 0;
    if (screen_idx >= sp->num_screens) screen_idx = sp->num_screens - 1;

    sp->cur_screen = screen_idx;
    sp_update_dots(sp, screen_idx);
    lv_obj_scroll_to_x(sp->scroll_area, screen_idx * ui_content_w(),
                       anim ? LV_ANIM_ON : LV_ANIM_OFF);
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

const char *scroll_pane_get_screen_name(lv_obj_t *pane)
{
    sp_data_t *sp = sp_get(pane);
    if (!sp || sp->cur_screen < 0 || sp->cur_screen >= sp->num_screens) return "";
    return sp->names[sp->cur_screen];
}

void scroll_pane_destroy(lv_obj_t *pane)
{
    sp_data_t *sp = sp_get(pane);
    if (sp) {
        if (sp->dots) lv_obj_delete(sp->dots);
        lv_obj_set_user_data(pane, NULL);
        free(sp);
    }
    tile_live_clear(lv_obj_get_screen(pane));
    lv_obj_del(pane);
}
