/**
 * @file now_playing_ribbon.cpp
 * @brief See now_playing_ribbon.h.
 */

#include "now_playing_ribbon.h"
#include "audio_source.h"
#include "album_art.h"
#include "equalizer.h"
#include "play_button.h"
#include "tile.h"
#include "../ui_design_tokens.h"

#include <memory>
#include <object.h>
#include <string>

/* The device's root object (defined in main.cpp). */
extern std::shared_ptr<RootObject> root;

/* Geometry from the Figma "Streaming Controls" ribbon. */
#define RIBBON_ART_SIZE        64
#define RIBBON_ART_RADIUS      8
#define RIBBON_ART_X           10     /* the art sits in a 74px slot, aligned right */
#define RIBBON_ART_Y           (NOW_PLAYING_RIBBON_H_PX - RIBBON_ART_SIZE)
#define RIBBON_TEXT_X          90     /* art slot (74) + gap (16) */
#define RIBBON_EQUALIZER_H     18     /* 17.5 in the design */
#define RIBBON_PLAY_SLOT_W     80     /* the button's slot; the button is 8px in from the right edge */
#define RIBBON_PLAY_RIGHT_PAD  8
#define RIBBON_ART_TAP_MARGIN  8      /* the art's touch target extends this far beyond it */
#define RIBBON_POLL_MS         200

struct Ribbon {
    lv_obj_t   *root;
    lv_obj_t   *title;       /* replaced whenever the track changes */
    lv_obj_t   *equalizer;
    lv_timer_t *timer;
    int         titleMaxW;
    std::string shownTitle;
    int         shownPlaying = -1;   /* -1: nothing shown yet */
};

static void set_title(Ribbon *r, const std::string &title)
{
    if (r->title) lv_obj_delete(r->title);
    r->title = tile_adaptive_label(r->root, title, &satoshi_medium_24,
                                   ui_theme_get_style_text_primary(), r->titleMaxW);
    lv_obj_set_pos(r->title, RIBBON_TEXT_X, RIBBON_ART_Y + RIBBON_EQUALIZER_H);
    lv_obj_clear_flag(r->title, LV_OBJ_FLAG_CLICKABLE);
}

static void refresh(lv_timer_t *timer)
{
    Ribbon *r = (Ribbon *)lv_timer_get_user_data(timer);

    const AudioSource *source = audio_source_active();
    const std::string title = audio_source_text(source, "trackTitle");
    if (title.empty()) {
        if (!lv_obj_has_flag(r->root, LV_OBJ_FLAG_HIDDEN)) {
            equalizer_set_running(r->equalizer, false);
            r->shownPlaying = -1;
            lv_obj_add_flag(r->root, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }
    if (lv_obj_has_flag(r->root, LV_OBJ_FLAG_HIDDEN)) lv_obj_clear_flag(r->root, LV_OBJ_FLAG_HIDDEN);

    if (title != r->shownTitle) {
        r->shownTitle = title;
        set_title(r, title);
    }

    const int playing = tile_read_bool(source->path, "isPlaying") ? 1 : 0;
    if (playing != r->shownPlaying) {
        r->shownPlaying = playing;
        equalizer_set_running(r->equalizer, playing);
    }
}

lv_obj_t *now_playing_ribbon_create(lv_obj_t *parent, int w, lv_event_cb_t on_art_tap)
{
    lv_obj_t *rootObj = tile_container(parent);
    lv_obj_set_size(rootObj, w, NOW_PLAYING_RIBBON_H_PX);
    lv_obj_add_flag(rootObj, LV_OBJ_FLAG_HIDDEN);   /* until the first tick finds a track */

    Ribbon *r = new Ribbon{};
    r->root      = rootObj;
    r->titleMaxW = w - RIBBON_TEXT_X - RIBBON_PLAY_SLOT_W;

    lv_obj_t *art = album_art_create(rootObj, RIBBON_ART_SIZE, RIBBON_ART_SIZE, RIBBON_ART_RADIUS);
    lv_obj_set_pos(art, RIBBON_ART_X, RIBBON_ART_Y);
    if (on_art_tap) {
        lv_obj_add_flag(art, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(art, RIBBON_ART_TAP_MARGIN);
        lv_obj_add_event_cb(art, on_art_tap, LV_EVENT_CLICKED, nullptr);
    }

    r->equalizer = equalizer_create(rootObj, RIBBON_TEXT_X, RIBBON_ART_Y);

    lv_obj_t *play = play_button_create(rootObj);
    lv_obj_set_pos(play, w - RIBBON_PLAY_RIGHT_PAD - PLAY_BUTTON_SIZE_PX, RIBBON_ART_Y);

    r->timer = lv_timer_create(refresh, RIBBON_POLL_MS, r);
    lv_obj_add_event_cb(rootObj, [](lv_event_t *e) {
        Ribbon *r = (Ribbon *)lv_event_get_user_data(e);
        equalizer_stop(r->equalizer);
        lv_timer_delete(r->timer);
        delete r;
    }, LV_EVENT_DELETE, r);

    lv_timer_ready(r->timer);
    return rootObj;
}
