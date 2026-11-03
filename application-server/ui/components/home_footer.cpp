/**
 * @file home_footer.cpp
 * @brief See home_footer.h.
 */

#include "home_footer.h"
#include "tile.h"
#include "../ui_design_tokens.h"

#include <string>

#include <object.h>
#include <DateTime.h>

/* The device's root object (defined in main.cpp). */
extern std::shared_ptr<RootObject> root;

#define DATE_X          30
#define DATE_Y          27
#define LOGO_W          78
#define LOGO_H          24
#define POLL_MS         1000

struct HomeFooter {
    lv_obj_t   *label;
    lv_timer_t *timer;
    std::string shown;
};

static void refresh(lv_timer_t *t)
{
    HomeFooter *f = (HomeFooter *)lv_timer_get_user_data(t);
    auto dt = std::dynamic_pointer_cast<DateTime>(::root->leafFromPath("/dateTime"));
    if (!dt) return;

    std::string text = dt->displayTimePtr->get() + "\n" + dt->displayDatePtr->get();
    if (text == f->shown) return;
    f->shown = text;
    lv_label_set_text(f->label, text.c_str());
}

lv_obj_t *home_footer_create(lv_obj_t *parent, int w, int logo_bottom)
{
    lv_obj_t *footer = tile_container(parent);
    lv_obj_set_size(footer, w, HOME_FOOTER_H_PX);

    HomeFooter *f = new HomeFooter{};

    f->label = lv_label_create(footer);
    lv_obj_add_style(f->label, ui_theme_get_style_text_secondary(), 0);
    lv_obj_set_style_text_font(f->label, &satoshi_medium_20, 0);
    lv_obj_set_pos(f->label, DATE_X, DATE_Y);

    lv_obj_t *logo = UI_ICON_CREATE_DEFAULT(footer, UI_ICON_LOGO);
    if (logo) lv_obj_set_pos(logo, (w - LOGO_W) / 2, HOME_FOOTER_H_PX - logo_bottom - LOGO_H);

    lv_obj_t *sleep = UI_ICON_CREATE_DEFAULT(footer, UI_ICON_SLEEP);
    if (sleep) lv_obj_align(sleep, LV_ALIGN_TOP_RIGHT, -(HOME_FOOTER_H_PX - 74) / 2, (HOME_FOOTER_H_PX - 74) / 2);

    f->timer = lv_timer_create(refresh, POLL_MS, f);
    lv_obj_add_event_cb(footer, [](lv_event_t *e) {
        HomeFooter *f = (HomeFooter *)lv_event_get_user_data(e);
        lv_timer_delete(f->timer);
        delete f;
    }, LV_EVENT_DELETE, f);

    lv_timer_ready(f->timer);
    return footer;
}
