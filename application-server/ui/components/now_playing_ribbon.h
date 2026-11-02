/**
 * @file now_playing_ribbon.h
 * @brief The now-playing ribbon of the active audio source (Bluetooth or a streaming provider): album art,
 *        equalizer and track title, and a play/pause button.
 *
 * It is the top strip of the streaming controls panel (Figma "Streaming Controls"): in
 * portrait it sits under the screens and is as wide as the display, in landscape it is
 * the top of the panel on the right and 356px wide. Only the width differs; the content
 * is placed from the left (art, equalizer, title) and from the right (play/pause).
 *
 * The ribbon follows the active audio source (see audio_source.h) by itself (timer of its own): it is hidden
 * while no source has a track, the equalizer runs while it plays, and the button shows pause while it plays and
 * play while it is paused. Tapping the button toggles that source's playback.
 */

#ifndef NOW_PLAYING_RIBBON_H
#define NOW_PLAYING_RIBBON_H

#include "lvgl/lvgl.h"

#define NOW_PLAYING_RIBBON_H_PX 72

/** Creates the ribbon, @p w pixels wide and NOW_PLAYING_RIBBON_H_PX high, in @p parent. The caller positions it.
 *  @p on_art_tap, if given, is called (LV_EVENT_CLICKED) when the album art is tapped. */
lv_obj_t *now_playing_ribbon_create(lv_obj_t *parent, int w, lv_event_cb_t on_art_tap = nullptr);

#endif /* NOW_PLAYING_RIBBON_H */
