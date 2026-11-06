/**
 * @file play_button.cpp
 * @brief See play_button.h.
 */

#include "play_button.h"
#include "audio_source.h"
#include "tile.h"
#include "../ui_design_tokens.h"
#include "../ui_icon_data.h"

#define PLAY_BUTTON_RING    3
#define PLAY_BUTTON_POLL_MS 200

struct PlayButton {
    lv_obj_t   *glyph;           /* play / pause icon inside the button */
    lv_timer_t *timer;
    int         shownPlaying = -1;   /* -1: nothing shown yet */
};

static void refresh(lv_timer_t *timer)
{
    PlayButton *b = (PlayButton *)lv_timer_get_user_data(timer);
    if (!b->glyph) return;
    const AudioSource *source = audio_source_active();
    const int playing = source && tile_read_bool(source->path, "isPlaying") ? 1 : 0;
    if (playing == b->shownPlaying) return;
    b->shownPlaying = playing;
    lv_image_set_src(b->glyph, ui_icon_data[playing ? UI_ICON_PAUSE : UI_ICON_PLAY]);
}

static void clicked(lv_event_t *)
{
    audio_source_toggle_play(audio_source_active());
}

/* Circular outline button: an icon-coloured disc with a background-coloured disc on top. */
lv_obj_t *play_button_create(lv_obj_t *parent)
{
    lv_obj_t *ring = tile_container(parent);
    lv_obj_set_size(ring, PLAY_BUTTON_SIZE_PX, PLAY_BUTTON_SIZE_PX);
    lv_obj_add_style(ring, ui_theme_get_style_bg_icon_primary(), 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_flag(ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(ring, 8);
    lv_obj_add_event_cb(ring, clicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *inner = tile_container(ring);
    lv_obj_set_size(inner, PLAY_BUTTON_SIZE_PX - 2 * PLAY_BUTTON_RING, PLAY_BUTTON_SIZE_PX - 2 * PLAY_BUTTON_RING);
    lv_obj_align(inner, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(inner, ui_theme_get_style_bg(), 0);
    lv_obj_set_style_bg_opa(inner, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(inner, LV_RADIUS_CIRCLE, 0);

    PlayButton *b = new PlayButton{};
    b->glyph = UI_ICON_CREATE_DEFAULT(inner, UI_ICON_PLAY);
    if (b->glyph) lv_obj_center(b->glyph);

    b->timer = lv_timer_create(refresh, PLAY_BUTTON_POLL_MS, b);
    lv_obj_add_event_cb(ring, [](lv_event_t *e) {
        PlayButton *b = (PlayButton *)lv_event_get_user_data(e);
        lv_timer_delete(b->timer);
        delete b;
    }, LV_EVENT_DELETE, b);

    lv_timer_ready(b->timer);   /* show the current state right away */
    return ring;
}
