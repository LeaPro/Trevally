/**
 * @file equalizer.h
 * @brief Reusable equalizer animation UI component.
 */

#ifndef EQUALIZER_H
#define EQUALIZER_H

#include "lvgl/lvgl.h"
#include <stdbool.h>

/** Creates the animation at (@p x, @p y) in @p parent. It is hidden and still until equalizer_set_running(true). */
lv_obj_t *equalizer_create(lv_obj_t *parent, int x, int y);

/** Size of the animation, for placing it. */
void      equalizer_get_size(int *out_w, int *out_h);

/** Shows and runs the animation, or stops and hides it. */
void      equalizer_set_running(lv_obj_t *equalizer, bool running);

/** Stops the animation, leaving the bars as they are. */
void      equalizer_stop(lv_obj_t *equalizer);

#endif /* EQUALIZER_H */
