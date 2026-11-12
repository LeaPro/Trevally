/**
 * @file lock_screen.cpp
 * @brief See lock_screen.h.
 */

#include "lock_screen.h"
#include "header.h"
#include "nav.h"
#include "screensaver.h"
#include "tile.h"
#include "../ui_design_tokens.h"

#include <algorithm>
#include <memory>
#include <string>

#include <object.h>
#include <Backlight.h>
#include <Roles.h>
/* The device's root object (defined in main.cpp). */
extern std::shared_ptr<RootObject> root;

LV_FONT_DECLARE(lv_font_montserrat_32);   /* has LV_SYMBOL_BACKSPACE */

static std::shared_ptr<Roles> roles;
static std::shared_ptr<Backlight> backlight;

/* ---- Wake guard: while the screen is dark or the screensaver shows, input only wakes it ---- */

static lv_obj_t *shield = nullptr;
static bool shieldPressed = false;
static lv_obj_t *focusBeforeShield = nullptr;
static bool shieldIsScreensaver = false;
static std::string screensaverShown;    /* screensaver_signature() of what the shield shows */
static bool screensaverRequested = false; /* the sleep icon, with the screensaver enabled */
static uint32_t signatureCheckedAt = 0;
#define SIGNATURE_CHECK_MS 1000

/* Waking the screen while the screensaver is up only shows it again: that touch (to its release) and knob turns for
 * a moment after are swallowed, and the next input dismisses it. lock_screen_tick() runs before LVGL reads the
 * input, so darkAtLastTick is whether the screen was dark when the input came. */
static bool darkAtLastTick = false;
static bool wakeGesture = false;   /* the current touch / knob press woke the screen */
static uint32_t wokeAt = 0;
#define WAKE_GRACE_MS 500

/* The screensaver is hidden (the shield is black) while the screen is dark, so it also looks off in the simulator. */
static void shield_show_content(bool show)
{
    if (lv_obj_t *content = shield ? lv_obj_get_child(shield, 0) : nullptr)
        lv_obj_set_hidden(content, !show);
}

static void shield_remove()
{
    if (!shield) return;
    lv_group_t *group = lv_group_get_default();
    if (group && lv_group_get_focused(group) == shield && focusBeforeShield && lv_obj_is_valid(focusBeforeShield)) {
        lv_group_focus_obj(focusBeforeShield);
        lv_group_set_editing(group, true);
    }
    lv_obj_delete_async(shield);
    shield = nullptr;
    shieldPressed = false;
    wakeGesture = false;
    focusBeforeShield = nullptr;
}

static void shield_event_cb(lv_event_t *e)
{
    switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
        shieldPressed = true;   /* the backlight wakes on this input; the release removes the guard */
        wakeGesture = shieldIsScreensaver && darkAtLastTick;
        if (wakeGesture) wokeAt = lv_tick_get();
        break;
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
    case LV_EVENT_CLICKED:      /* encoder press */
        if (wakeGesture) shieldPressed = false;   /* woke the screen: the screensaver stays */
        else shield_remove();
        break;
    case LV_EVENT_KEY:          /* encoder turn */
        if (shieldIsScreensaver && (darkAtLastTick || lv_tick_elaps(wokeAt) < WAKE_GRACE_MS)) {
            if (darkAtLastTick) wokeAt = lv_tick_get();
        } else {
            shield_remove();
        }
        break;
    default:
        break;
    }
}

/* Black, so the screen also looks off where there is no backlight to switch (the simulator), or the screensaver. */
static void shield_create(bool withScreensaver)
{
    shield = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(shield);
    lv_obj_set_size(shield, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(shield, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(shield, LV_OPA_COVER, 0);
    lv_obj_add_flag(shield, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(shield, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(shield, shield_event_cb, LV_EVENT_ALL, nullptr);
    shieldIsScreensaver = withScreensaver;
    screensaverRequested = false;
    if (withScreensaver) {
        screensaverShown = screensaver_signature();
        screensaver_build(shield);   /* nothing in it is clickable, so input reaches the shield */
    }

    lv_group_t *group = lv_group_get_default();
    if (group) {
        focusBeforeShield = lv_group_get_focused(group);
        lv_group_add_obj(group, shield);
        lv_group_focus_obj(shield);
        lv_group_set_editing(group, true);
    }
}

/* ---- Enter PIN page ---- */

#define PIN_MAX_DIGITS     12
#define PIN_MESSAGE_MS     1500
#define PIN_BOX_MARGIN_PX  34
#define PIN_BOX_TOP_PX     26     /* below the header */
#define PIN_BOX_H_PX       78
#define PIN_ROW_PITCH_MAX  78
#define PIN_BOTTOM_MARGIN_PX 30

static nav_page_t pinPage = {nullptr, nullptr};
static std::string pinDigits;
static lv_obj_t *pinDisplay = nullptr;
static lv_obj_t *pinBox = nullptr;
static lv_timer_t *pinMessageTimer = nullptr;

static void pin_show_digits()
{
    if (!pinDisplay) return;
    lv_obj_set_style_text_color(pinDisplay, ui_get_brand_color(), 0);
    lv_label_set_text(pinDisplay, pinDigits.c_str());
}

static void pin_message_done_cb(lv_timer_t *)
{
    pinMessageTimer = nullptr;
    pin_show_digits();
}

static void pin_show_message(const char *text)
{
    if (!pinDisplay) return;
    lv_obj_set_style_text_color(pinDisplay, UI_COLOR_STATUS_ERROR, 0);
    lv_label_set_text(pinDisplay, text);
    if (pinMessageTimer) lv_timer_delete(pinMessageTimer);
    pinMessageTimer = lv_timer_create(pin_message_done_cb, PIN_MESSAGE_MS, nullptr);
    lv_timer_set_repeat_count(pinMessageTimer, 1);
}

static void pin_close()
{
    if (!pinPage.screen) return;
    if (lv_screen_active() == pinPage.screen) nav_pop();
    else nav_home(false);   /* closes it wherever it is in the page stack */
}

static void pin_submit()
{
    const std::string role = roles ? roles->roleForPasscode(pinDigits) : std::string();
    pinDigits.clear();
    if (role.empty()) {
        pin_show_message("Incorrect PIN");
        return;
    }
    roles->login(role);
    nav_home(true);   /* slides home like leaving Settings; the PIN page is deleted when the slide ends */
}

static void pin_key_cb(lv_event_t *e)
{
    const char key = (char)(intptr_t)lv_event_get_user_data(e);
    if (pinMessageTimer) {   /* a key press ends the "Incorrect PIN" message */
        lv_timer_delete(pinMessageTimer);
        pinMessageTimer = nullptr;
    }
    if (key == '<') {
        if (!pinDigits.empty()) pinDigits.pop_back();
    } else if (key == 'K') {
        pin_submit();
        return;
    } else if (pinDigits.size() < PIN_MAX_DIGITS) {
        pinDigits += key;
    }
    pin_show_digits();
}

static void pin_back_cb(lv_event_t *)
{
    lock_screen_sleep();   /* nobody gets past the PIN page without one */
}

static void pin_page_deleted_cb(lv_event_t *)
{
    pinPage = {nullptr, nullptr};
    pinDisplay = nullptr;
    pinBox = nullptr;
    pinDigits.clear();
    if (pinMessageTimer) {
        lv_timer_delete(pinMessageTimer);
        pinMessageTimer = nullptr;
    }
}

static lv_obj_t *pin_key_create(lv_obj_t *parent, char key, int w, int h)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_style(btn, ui_theme_get_style_bg_tertiary(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn, pin_key_cb, LV_EVENT_CLICKED, (void *)(intptr_t)key);

    lv_obj_t *label = lv_label_create(btn);
    lv_obj_add_style(label, ui_theme_get_style_text_primary(), 0);
    if (key == '<') {
        lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);
        lv_label_set_text(label, LV_SYMBOL_BACKSPACE);
    } else if (key == 'K') {
        lv_obj_set_style_text_font(label, &satoshi_medium_24, 0);
        lv_label_set_text(label, "OK");
    } else {
        lv_obj_set_style_text_font(label, &satoshi_medium_48, 0);
        const char text[2] = {key, '\0'};
        lv_label_set_text(label, text);
    }
    lv_obj_center(label);
    return btn;
}

static void pin_open()
{
    nav_home(false);   /* whatever was open belongs to the role that logged out */
    pinPage = nav_screen_create();
    pinDigits.clear();

    const header_slot_t back = {true, UI_ICON_ARROW_BACK, pin_back_cb, nullptr};
    const header_slot_t none = {false, UI_ICON_ARROW_BACK, nullptr, nullptr};
    header_create(pinPage.screen, back, "Enter PIN", none);

    const int w = ui_content_w();
    pinBox = lv_obj_create(pinPage.screen);
    lv_obj_remove_style_all(pinBox);
    lv_obj_remove_flag(pinBox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(pinBox, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(pinBox, w - 2 * PIN_BOX_MARGIN_PX, PIN_BOX_H_PX);
    lv_obj_set_pos(pinBox, PIN_BOX_MARGIN_PX, UI_HEADER_H_PX + PIN_BOX_TOP_PX);
    lv_obj_add_style(pinBox, ui_theme_get_style_brand(), 0);
    lv_obj_set_style_bg_opa(pinBox, OPACITY_BRAND_BG_LIGHT, 0);
    lv_obj_set_style_border_width(pinBox, 2, 0);
    lv_obj_set_style_border_opa(pinBox, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pinBox, 8, 0);

    pinDisplay = lv_label_create(pinBox);
    lv_obj_set_style_text_font(pinDisplay, &satoshi_medium_48, 0);
    lv_obj_center(pinDisplay);
    pin_show_digits();

    /* keypad: 1-9, then backspace / 0 / OK, filling the rest of the pane */
    const int top = UI_HEADER_H_PX + PIN_BOX_TOP_PX + PIN_BOX_H_PX + 8;
    const int displayH = lv_display_get_vertical_resolution(nullptr);
    const int pitch = std::min(PIN_ROW_PITCH_MAX, (displayH - top - PIN_BOTTOM_MARGIN_PX) / 4);
    const int colW = (w - 2 * PIN_BOX_MARGIN_PX) / 3;
    static const char keys[12] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '<', '0', 'K'};
    for (int i = 0; i < 12; i++) {
        lv_obj_t *key = pin_key_create(pinPage.screen, keys[i], colW, pitch);
        lv_obj_set_pos(key, PIN_BOX_MARGIN_PX + (i % 3) * colW, top + (i / 3) * pitch);
    }

    lv_obj_add_event_cb(pinPage.screen, pin_page_deleted_cb, LV_EVENT_DELETE, nullptr);
    nav_push(pinPage);
}

/* ---- Public ---- */

void lock_screen_init()
{
    roles = std::dynamic_pointer_cast<Roles>(root->leafFromPath("/roles"));
    backlight = std::dynamic_pointer_cast<Backlight>(root->leafFromPath("/backlight"));
    if (roles) roles->logout();   /* power-on: locked (if there are roles) */
}

void lock_screen_sleep()
{
    if (roles) roles->logout();
    if (screensaver_enabled()) screensaverRequested = true;
    else if (backlight) backlight->sleepNow();
}

void lock_screen_tick()
{
    if (!roles || !backlight) return;

    /* The screensaver logs the role out when it starts (on its timeout, or the sleep icon, which already did); the
     * backlight timeout only darkens the screen. */
    const bool dark = backlight->isScreenOff();
    darkAtLastTick = dark;
    const bool saver = screensaver_enabled();
    const bool saverDue = saver && (screensaverRequested
                                    || lv_display_get_inactive_time(nullptr) >= screensaver_timeout_ms());
    if (!shield) {
        if (saverDue) {
            shield_create(true);
            roles->logout();
        } else if (dark) {
            shield_create(false);
        }
    } else if (!shieldPressed) {
        if (!shieldIsScreensaver && saverDue) {
            /* dark before the screensaver was due: waking shows the screensaver, not what was under it */
            shieldIsScreensaver = true;
            screensaverShown = screensaver_signature();
            screensaver_build(shield);
            roles->logout();
        }
        if (shieldIsScreensaver ? !saver : !dark) {
            shield_remove();   /* turned off, or woken some other way (e.g. a brightness change from the web UI) */
        } else if (shieldIsScreensaver && lv_tick_elaps(signatureCheckedAt) >= SIGNATURE_CHECK_MS) {
            signatureCheckedAt = lv_tick_get();
            const std::string signature = screensaver_signature();
            if (signature != screensaverShown) {   /* its settings, the theme images or the orientation changed */
                screensaverShown = signature;
                lv_obj_clean(shield);
                screensaver_build(shield);
            }
        }
    }
    if (shield && shieldIsScreensaver) shield_show_content(!dark);
    screensaverRequested = false;

    const bool locked = roles->locked();
    if (locked && !pinPage.screen) pin_open();
    else if (!locked && pinPage.screen) pin_close();   /* logged in, or the roles were removed */

    /* Pages opened under the shield (the Enter PIN page, a rebuild) take the encoder focus: give it back, so a
     * knob turn or press still only wakes, and remember the page's focus for when the shield goes. */
    lv_group_t *group = lv_group_get_default();
    if (shield && group && lv_group_get_focused(group) != shield) {
        focusBeforeShield = lv_group_get_focused(group);
        if (lv_obj_get_group(shield) != group) lv_group_add_obj(group, shield);
        lv_group_focus_obj(shield);
        lv_group_set_editing(group, true);
    }
}
