/**
 * @file nav.cpp
 * @brief See nav.h.
 */

#include "nav.h"
#include "theme_images.h"
#include "tile.h"
#include "ui_design_tokens.h"
#include "ui_icon_data.h"

#include <vector>

static const uint32_t NAV_ANIM_MS = 250;

static nav_dest_t homeDest    = {nullptr, nullptr};
static nav_dest_t currentDest = {nullptr, nullptr};
static std::vector<nav_page_t> stack;   /* transient pages above currentDest */

/* Must run before the screen being left is deleted: deleting the focused object makes LVGL refocus
 * the group's next object, and focusing an object scrolls it into view (e.g. the home pane to
 * another screen). */
static void take_focus(lv_obj_t *obj)
{
    lv_group_t *group = lv_group_get_default();
    if (!group || !obj) return;
    /* re-adding removes the object first, which refocuses the group if it is focused */
    if (lv_obj_get_group(obj) != group) lv_group_add_obj(group, obj);
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
    lv_obj_add_style(page.screen, theme_background_style(), 0);   /* the theme's background image, if any */
    lv_obj_set_style_bg_opa(page.screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(page.screen, LV_OBJ_FLAG_SCROLLABLE);

    page.focus = lv_obj_create(page.screen);
    lv_obj_remove_style_all(page.focus);
    lv_obj_clear_flag(page.focus, LV_OBJ_FLAG_CLICKABLE);
    /* A scrollable focus object makes LVGL toggle the group's edit mode on an encoder long press;
     * leaving edit mode turns rotation into focus navigation, so KEY events stop arriving. */
    lv_obj_clear_flag(page.focus, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(page.focus, 1, 1);

    /* The LEA logo beneath the knob, where the home screen has it (home_controls.cpp); created first, so the page's
     * widgets are drawn over it */
    if (lv_obj_t *logo = UI_ICON_CREATE_DEFAULT(page.screen, UI_ICON_LOGO)) {
        const lv_area_t area = nav_logo_area();
        lv_obj_set_pos(logo, area.x1, area.y1);
    }
    return page;
}

lv_area_t nav_logo_area(void)
{
    const int displayW = lv_display_get_horizontal_resolution(nullptr);
    const int displayH = lv_display_get_vertical_resolution(nullptr);
    lv_area_t area;
    area.x1 = ui_is_landscape() ? UI_LANDSCAPE_KNOB_X_PX - UI_LOGO_W_PX / 2 : (displayW - UI_LOGO_W_PX) / 2;
    area.y1 = displayH - (ui_is_landscape() ? UI_LANDSCAPE_LOGO_BOTTOM_PX : UI_PORTRAIT_LOGO_BOTTOM_PX) - UI_LOGO_H_PX;
    area.x2 = area.x1 + UI_LOGO_W_PX - 1;
    area.y2 = area.y1 + UI_LOGO_H_PX - 1;
    return area;
}

void nav_hide_logo(lv_obj_t *screen)
{
    for (uint32_t i = 0; screen && i < lv_obj_get_child_count(screen); i++) {
        lv_obj_t *child = lv_obj_get_child(screen, (int32_t)i);
        if (lv_obj_check_type(child, &lv_image_class) && lv_image_get_src(child) == ui_icon_data[UI_ICON_LOGO]) {
            lv_obj_delete(child);
            return;
        }
    }
}

void nav_push(const nav_page_t &page)
{
    stack.push_back(page);
    lv_screen_load_anim(page.screen, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, false);
    take_focus(page.focus);
}

void nav_replace(const nav_page_t &page)
{
    if (stack.empty()) {
        nav_push(page);
        return;
    }
    tile_live_clear(stack.back().screen);
    stack.back() = page;
    take_focus(page.focus);
    lv_screen_load_anim(page.screen, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);   /* deletes the replaced page */
}

void nav_pop(size_t count)
{
    if (stack.empty()) {
        nav_goto(homeDest);   /* no-op when already home */
        return;
    }

    /* pages under the one on screen are not shown, so closing them needs no animation */
    for (; count > 1 && stack.size() > 1; count--) {
        const nav_page_t hidden = stack[stack.size() - 2];
        stack.erase(stack.end() - 2);
        tile_live_clear(hidden.screen);
        lv_obj_delete(hidden.screen);
    }

    const nav_page_t leaving = stack.back();
    stack.pop_back();
    tile_live_clear(leaving.screen);

    const nav_page_t &target = stack.empty() ? currentDest : stack.back();
    take_focus(target.focus);
    lv_screen_load_anim(target.screen, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);   /* deletes the popped page */
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
    take_focus(dest.focus);
    lv_screen_load_anim(dest.screen,
                        !animate ? LV_SCREEN_LOAD_ANIM_NONE
                                 : goingHome ? LV_SCREEN_LOAD_ANIM_MOVE_RIGHT : LV_SCREEN_LOAD_ANIM_MOVE_LEFT,
                        animate ? NAV_ANIM_MS : 0, 0, deleteActive);
}

void nav_home(bool animate)
{
    nav_goto(homeDest, animate);
}

void nav_close_pages(bool animate)
{
    nav_goto(currentDest, animate);
}

void nav_back_cb(lv_event_t *)
{
    nav_pop();
}
