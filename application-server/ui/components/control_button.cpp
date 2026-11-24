/**
 * @file control_button.cpp
 * @brief See control_button.h.
 */

#include "control_button.h"
#include "permissions.h"
#include "tile.h"
#include "../ui_design_tokens.h"

#define CONTROL_BUTTON_POLL_MS 200
#define CONTROL_OFF_OPA        LV_OPA_40

struct ControlButton {
    const AudioControl *control;
    lv_obj_t           *glyph;
    lv_timer_t         *timer;      /* only for toggle controls */
    int                 shownOn = -1;   /* -1: nothing shown yet */
};

static void refresh(lv_timer_t *timer)
{
    ControlButton *b = (ControlButton *)lv_timer_get_user_data(timer);
    const AudioSource *source = audio_source_active();
    const int on = source && tile_read_bool(source->path, b->control->element) ? 1 : 0;
    if (on == b->shownOn) return;
    b->shownOn = on;
    lv_obj_set_style_opa(b->glyph, on ? LV_OPA_COVER : CONTROL_OFF_OPA, 0);
}

static void clicked(lv_event_t *e)
{
    const ControlButton *b = (const ControlButton *)lv_event_get_user_data(e);
    const AudioSource *source = audio_source_active();
    if (source && perm_source_control(source->path) < PERM_FULL) return;   /* view only for the logged-in role */
    audio_source_press(source, b->control);
}

lv_obj_t *control_button_create(lv_obj_t *parent, int h, const AudioControl *control)
{
    lv_obj_t *button = tile_container(parent);
    lv_obj_set_size(button, CONTROL_BUTTON_W, h);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);

    ControlButton *b = new ControlButton{};
    b->control = control;
    b->glyph = UI_ICON_CREATE_DEFAULT(button, control->icon);
    if (b->glyph) lv_obj_align(b->glyph, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_event_cb(button, clicked, LV_EVENT_CLICKED, b);
    lv_obj_add_event_cb(button, [](lv_event_t *e) {
        ControlButton *b = (ControlButton *)lv_event_get_user_data(e);
        if (b->timer) lv_timer_delete(b->timer);
        delete b;
    }, LV_EVENT_DELETE, b);

    if (b->glyph && control->behavior == AUDIO_CONTROL_TOGGLE) {
        b->timer = lv_timer_create(refresh, CONTROL_BUTTON_POLL_MS, b);
        lv_timer_ready(b->timer);   /* show the current state right away */
    }
    return button;
}
