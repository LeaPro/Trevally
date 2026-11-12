/**
 * @file home_controls.cpp
 * @brief See home_controls.h.
 */

#include "home_controls.h"
#include "audio_source.h"
#include "audio_sources_page.h"
#include "control_button.h"
#include "lock_screen.h"
#include "permissions.h"
#include "tile.h"
#include "../ui_design_tokens.h"

#include <string>
#include <vector>

#include <object.h>
#include <DateTime.h>

/* The device's root object (defined in main.cpp). */
extern std::shared_ptr<RootObject> root;

/* Geometry from the Figma home screens: 98px touch targets holding 74px icons. */
#define CONTROL_W                CONTROL_BUTTON_W
#define PORTRAIT_BUTTON_H        98
#define WEATHER_ICON_PX          40
#define PORTRAIT_BUTTON_Y        164    /* centre at 50% + 17 of the 391px panel */
#define LANDSCAPE_BUTTON_H       78
#define LANDSCAPE_BUTTON_Y       72     /* right under the ribbon */
#define HOME_CONTROLS_POLL_MS    200

/* The footer strip along the bottom of the panel. */
#define FOOTER_H                 98
#define DATE_X                   30
#define DATE_Y                   27
#define LOGO_W                   78
#define LOGO_H                   24
#define PORTRAIT_LOGO_BOTTOM     6      /* gap between the logo and the bottom edge */
#define LANDSCAPE_LOGO_BOTTOM    22

struct HomeControls {
    lv_obj_t   *cast;
    lv_obj_t   *weather;
    ui_icon_type_t weatherIcon;                   /* what the weather area shows now */
    std::string    weatherDegrees;
    lv_obj_t   *row;                              /* the active source's controls */
    std::vector<const AudioControl *> shown;      /* what the row holds now */
    lv_obj_t   *dateTime;                         /* footer: time and date */
    std::string dateTimeShown;
    lv_timer_t *timer;
};

static void set_shown(lv_obj_t *obj, bool shown)
{
    if (shown) lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else       lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

/* A CONTROL_W x h area holding @p icon centred. */
static lv_obj_t *icon_area_create(lv_obj_t *parent, int x, int y, int h, ui_icon_type_t icon)
{
    lv_obj_t *area = tile_container(parent);
    lv_obj_set_size(area, CONTROL_W, h);
    lv_obj_set_pos(area, x, y);

    lv_obj_t *glyph = UI_ICON_CREATE_DEFAULT(area, icon);
    if (glyph) lv_obj_align(glyph, LV_ALIGN_CENTER, 0, 0);
    return area;
}

/* Rebuilds the row for @p controls. The row spreads them across the panel; a lone control sits at the right. */
static void rebuild_row(HomeControls *c, std::vector<const AudioControl *> controls)
{
    lv_obj_clean(c->row);
    const int h = lv_obj_get_height(c->row);
    for (const AudioControl *control : controls)
        control_button_create(c->row, h, control);
    lv_obj_set_flex_align(c->row, controls.size() > 1 ? LV_FLEX_ALIGN_SPACE_BETWEEN : LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    c->shown = std::move(controls);
}

/* Rebuilds the weather area: the temperature (once known) to the left of the icon. */
static void weather_area_fill(HomeControls *c, ui_icon_type_t icon, const std::string &degrees)
{
    lv_obj_clean(c->weather);
    if (!degrees.empty()) {
        lv_obj_t *temp = tile_weather_degrees_create(c->weather, degrees, &satoshi_medium_20,
                                                     ui_theme_get_style_text_primary());
        /* the icon's slot has empty margins: tuck the temperature to 8px from the icon without moving the layout */
        if (temp) lv_obj_set_style_translate_x(temp, (CONTROL_W - WEATHER_ICON_PX) / 2 - 8, 0);
    }
    lv_obj_t *slot = tile_container(c->weather);
    lv_obj_set_size(slot, CONTROL_W, PORTRAIT_BUTTON_H);
    lv_obj_t *glyph = tile_weather_icon_create(slot, icon, WEATHER_ICON_PX);
    lv_obj_align(glyph, LV_ALIGN_CENTER, 0, 0);
    c->weatherIcon    = icon;
    c->weatherDegrees = degrees;
}

static void refresh(lv_timer_t *timer)
{
    HomeControls *c = (HomeControls *)lv_timer_get_user_data(timer);
    const AudioSource *source = audio_source_active();
    /* a source the logged-in role has no access to is shown as none (its controls are hidden) */
    const bool controlsHidden = source && perm_source_control(source->path) == PERM_HIDDEN;

    set_shown(c->cast,    source == nullptr);
    set_shown(c->weather, source == nullptr || controlsHidden);
    set_shown(c->row,     source != nullptr && !controlsHidden);

    /* The weather area shows the conditions at the default postal code. */
    const ui_icon_type_t weatherIcon = tile_weather_icon("default");
    const std::string    degrees     = tile_weather_degrees("default");
    if (weatherIcon != c->weatherIcon || degrees != c->weatherDegrees)
        weather_area_fill(c, weatherIcon, degrees);

    std::vector<const AudioControl *> controls;
    for (const AudioControl *control : audio_source_controls(source))
        if (control->onHome) controls.push_back(control);
    if (controls != c->shown) rebuild_row(c, std::move(controls));

    auto dt = std::dynamic_pointer_cast<DateTime>(::root->leafFromPath("/dateTime"));
    if (dt) {
        std::string text = dt->displayTimePtr->get() + "\n" + dt->displayDatePtr->get();
        if (text != c->dateTimeShown) {
            c->dateTimeShown = text;
            lv_label_set_text(c->dateTime, text.c_str());
        }
    }
}

lv_obj_t *home_controls_create(lv_obj_t *parent, int w, int h)
{
    lv_obj_t *area = tile_container(parent);
    lv_obj_set_size(area, w, h);

    const bool landscape = ui_is_landscape();

    HomeControls *c = new HomeControls{};
    c->cast    = icon_area_create(area, 0,            0, PORTRAIT_BUTTON_H, UI_ICON_MUSIC_CAST);
    lv_obj_add_flag(c->cast, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(c->cast, [](lv_event_t *) { audio_sources_page_open(); }, LV_EVENT_CLICKED, nullptr);
    c->weather = tile_container(area);
    lv_obj_set_size(c->weather, LV_SIZE_CONTENT, PORTRAIT_BUTTON_H);
    lv_obj_align(c->weather, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_layout(c->weather, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(c->weather, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c->weather, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    weather_area_fill(c, tile_weather_icon("default"), tile_weather_degrees("default"));

    c->row = tile_container(area);
    lv_obj_set_size(c->row, w, landscape ? LANDSCAPE_BUTTON_H : PORTRAIT_BUTTON_H);
    lv_obj_set_pos(c->row, 0, landscape ? LANDSCAPE_BUTTON_Y : PORTRAIT_BUTTON_Y);
    lv_obj_set_flex_flow(c->row, LV_FLEX_FLOW_ROW);

    const int footerY = h - FOOTER_H;
    c->dateTime = lv_label_create(area);
    lv_obj_add_style(c->dateTime, ui_theme_get_style_text_secondary(), 0);
    lv_obj_set_style_text_font(c->dateTime, &satoshi_medium_20, 0);
    lv_obj_set_pos(c->dateTime, DATE_X, footerY + DATE_Y);

    lv_obj_t *logo = UI_ICON_CREATE_DEFAULT(area, UI_ICON_LOGO);
    if (logo) lv_obj_set_pos(logo, (w - LOGO_W) / 2,
                             h - (landscape ? LANDSCAPE_LOGO_BOTTOM : PORTRAIT_LOGO_BOTTOM) - LOGO_H);

    lv_obj_t *sleep = UI_ICON_CREATE_DEFAULT(area, UI_ICON_SLEEP);
    if (sleep) {
        lv_obj_align(sleep, LV_ALIGN_TOP_RIGHT, -(FOOTER_H - 74) / 2, footerY + (FOOTER_H - 74) / 2);
        /* sleep: log the role out and switch the screen off (see lock_screen.h) */
        lv_obj_add_flag(sleep, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(sleep, [](lv_event_t *) { lock_screen_sleep(); }, LV_EVENT_CLICKED, nullptr);
    }

    c->timer = lv_timer_create(refresh, HOME_CONTROLS_POLL_MS, c);
    lv_obj_add_event_cb(area, [](lv_event_t *e) {
        HomeControls *c = (HomeControls *)lv_event_get_user_data(e);
        lv_timer_delete(c->timer);
        delete c;
    }, LV_EVENT_DELETE, c);

    lv_timer_ready(c->timer);
    return area;
}
