/**
 * @file control_button.h
 * @brief A button for one of the optional controls of the active audio source (see audio_source.h).
 */

#ifndef CONTROL_BUTTON_H
#define CONTROL_BUTTON_H

#include "audio_source.h"
#include "lvgl/lvgl.h"

/* Figma: 98px touch targets holding 74px icons. */
#define CONTROL_BUTTON_W 98

/** Creates a CONTROL_BUTTON_W x @p h button with the icon of @p control, centred. Pressing it presses the control
 *  on the active source. A toggle control shows its state: dimmed while off. The caller positions it.
 *  @p control must stay valid (the pointers audio_source_controls() returns do). */
lv_obj_t *control_button_create(lv_obj_t *parent, int h, const AudioControl *control);

#endif /* CONTROL_BUTTON_H */
