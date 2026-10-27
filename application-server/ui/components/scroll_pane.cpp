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

typedef struct {
    lv_obj_t *scroll_area;
    int       num_screens;
    int       cur_screen;
} sp_data_t;

static sp_data_t *sp_get(lv_obj_t *pane)
{
    return (sp_data_t *)lv_obj_get_user_data(pane);
}

static void sp_scroll_end_cb(lv_event_t *e)
{
    lv_obj_t  *sa   = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t  *pane = (lv_obj_t *)lv_event_get_user_data(e);
    sp_data_t *sp   = sp_get(pane);
    if (!sp || sp->num_screens <= 0) return;

    int idx = (int)((lv_obj_get_scroll_x(sa) + SCROLL_PAGE_W / 2) / SCROLL_PAGE_W);
    if (idx < 0) idx = 0;
    if (idx >= sp->num_screens) idx = sp->num_screens - 1;
    sp->cur_screen = idx;
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
    lv_obj_set_size(sa, SCROLL_PAGE_W, SCREEN_H);
    lv_obj_align(sa, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_layout(sa, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(sa, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sa, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(sa, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(sa, LV_SCROLL_SNAP_START);
    lv_obj_add_event_cb(sa, sp_scroll_end_cb, LV_EVENT_SCROLL_END, pane);
    sp->scroll_area = sa;

    for (const json &screenConfig : screens)
        screen_create(sa, screenConfig);

    if (sp->cur_screen > 0)
        lv_obj_scroll_to_x(sa, sp->cur_screen * SCROLL_PAGE_W, LV_ANIM_OFF);
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
    lv_obj_set_size(pane, SCROLL_PAGE_W, SCROLL_PANE_H);

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
    lv_obj_scroll_to_x(sp->scroll_area, screen_idx * SCROLL_PAGE_W,
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

void scroll_pane_destroy(lv_obj_t *pane)
{
    sp_data_t *sp = sp_get(pane);
    if (sp) {
        lv_obj_set_user_data(pane, NULL);
        free(sp);
    }
    tile_live_clear(lv_obj_get_screen(pane));
    lv_obj_del(pane);
}
