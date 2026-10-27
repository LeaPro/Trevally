/**
 * @file nav.cpp
 * @brief See nav.h.
 */

#include "nav.h"
#include "tile.h"
#include "ui_design_tokens.h"

#include <vector>

static const uint32_t NAV_ANIM_MS = 250;

static lv_obj_t *homeScreen = nullptr;
static lv_obj_t *homeFocus  = nullptr;
static std::vector<nav_page_t> stack;

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
    homeScreen = lv_screen_active();
    homeFocus  = home_focus;
}

nav_page_t nav_page_create()
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
    if (stack.empty() || !homeScreen) return;

    const nav_page_t leaving = stack.back();
    stack.pop_back();
    tile_live_clear(leaving.screen);

    lv_obj_t *target      = stack.empty() ? homeScreen : stack.back().screen;
    lv_obj_t *targetFocus = stack.empty() ? homeFocus  : stack.back().focus;
    lv_screen_load_anim(target, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, NAV_ANIM_MS, 0, true);   /* deletes the popped page */
    take_focus(targetFocus);
}

void nav_home(bool animate)
{
    if (stack.empty() || !homeScreen) return;

    for (size_t i = 0; i + 1 < stack.size(); i++) {
        tile_live_clear(stack[i].screen);
        lv_obj_delete(stack[i].screen);
    }
    tile_live_clear(stack.back().screen);
    stack.clear();

    lv_screen_load_anim(homeScreen, animate ? LV_SCREEN_LOAD_ANIM_MOVE_RIGHT : LV_SCREEN_LOAD_ANIM_NONE,
                        animate ? NAV_ANIM_MS : 0, 0, true);
    take_focus(homeFocus);
}

void nav_back_cb(lv_event_t *e)
{
    (void)e;
    nav_pop();
}
