/**
 * @file play_button.h
 * @brief Circular play / pause button of the active audio source (see audio_source.h).
 *
 * The button shows pause while the source plays and play while it is paused, following the source by itself (timer of its
 * own). Tapping it toggles that source's playback.
 */

#ifndef PLAY_BUTTON_H
#define PLAY_BUTTON_H

#include "lvgl/lvgl.h"

#define PLAY_BUTTON_SIZE_PX 64

/** Creates the button, PLAY_BUTTON_SIZE_PX square, in @p parent. The caller positions it. */
lv_obj_t *play_button_create(lv_obj_t *parent);

#endif /* PLAY_BUTTON_H */
