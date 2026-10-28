/**
 * @file nav.cpp
 * @brief See nav.h.
 */

#include "nav.h"
#include "tile.h"
#include "ui_design_tokens.h"

#include <vector>

static const uint32_t NAV_ANIM_MS = 250;

static nav_dest_t homeDest    = {nullptr, nullptr};
static nav_dest_t currentDest = {nullptr, nullptr};
static std::vector<nav_page_t> stack;   /* transient pages above currentDest */

static void take_focus(lv_obj_t *obj)
{
    lv_group_t *group = lv_group_get_default();
    if (!group || !obj) return;
    lv_group_add_obj(group, obj);
    lv_group_focus_obj(obj);
    lv_group_set_editing(group, true);
}

void nav_init(lv_obj_t *home_focus)
{
    homeDest    = {lv_screen_active(), home_focus};
    currentDest = homeDest;
}

nav_page_t nav_screen_create()
{
    nav_page_t page;
    page.screen = lv_obj_create(nullptr);
    lv_obj_add_style(page.screen, ui_theme_get_style_bg(), 0);
    lv_obj_set_style_bg_opa(page.screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(page.screen, LV_OBJ_FLAG_SCROLLABLE);

    page.focus = lv_obj_create(page.screen);
    lv_obj_remove_style_all(page.focus);
    lv_obj_clear_flag(page.focus, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(page.focus, 1, 1);
    return page;
}

void nav_push(const nav_page_t &page)
{
    stack.push_back(page);
    lv_screen_load_anim(page.screen, LV_SCREEN_LOAD_ANIM_MOVE_LEFT, NAV_ANIM_MS, 0, false);
    take_focus(page.focus);
}

void nav_pop()
{
    if (stack.empty()) {
        nav_goto(homeDest);   /* no-op when already home */
        return;
    }

    const nav_page_t leaving = stack.back();
    stack.pop_back();
    tile_live_clear(leaving.screen);

    const nav_page_t &target = stack.empty() ? currentDest : stack.back();
    lv_screen_load_anim(target.screen, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, NAV_ANIM_MS, 0, true);   /* deletes the popped page */
    take_focus(target.focus);
}

void nav_goto(const nav_dest_t &dest, bool animate)
{
    if (stack.empty() && dest.screen == currentDest.screen) return;

    /* Open pages are transient. The active screen is the top page if there is
     * one, otherwise the current destination, which must survive. */
    const bool deleteActive = !stack.empty();
    for (size_t i = 0; i + 1 < stack.size(); i++) {
        tile_live_clear(stack[i].screen);
        lv_obj_delete(stack[i].screen);
    }
    if (deleteActive) tile_live_clear(stack.back().screen);
    stack.clear();

    currentDest = dest;
    const bool goingHome = dest.screen == homeDest.screen;
    lv_screen_load_anim(dest.screen,
                        !animate ? LV_SCREEN_LOAD_ANIM_NONE
                                 : goingHome ? LV_SCREEN_LOAD_ANIM_MOVE_RIGHT : LV_SCREEN_LOAD_ANIM_MOVE_LEFT,
                        animate ? NAV_ANIM_MS : 0, 0, deleteActive);
    take_focus(dest.focus);
}

void nav_close_pages(bool animate)
{
    nav_goto(currentDest, animate);
}

void nav_back_cb(lv_event_t *)
{
    nav_pop();
}
