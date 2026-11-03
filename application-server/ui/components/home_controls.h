/**
 * @file home_controls.h
 * @brief The home panel's control icons, above the footer and under the now-playing ribbon.
 *
 * Follows audio_source_active():
 *   - no source enabled: the music-cast and weather icons (not linked to anything yet).
 *   - a source enabled: one button per optional control the source supports (audio_source_controls(): Bluetooth
 *     has previous and next, the streaming services only next), spread across the panel. The ribbon with the
 *     play button is the ribbon's own business.
 */

#ifndef HOME_CONTROLS_H
#define HOME_CONTROLS_H

#include "lvgl/lvgl.h"

/** Creates the controls area, @p w x @p h, with the top-left at (0, 0) of @p parent; the caller positions it.
 *  Its children are placed for the current orientation. The area itself is not clickable. */
lv_obj_t *home_controls_create(lv_obj_t *parent, int w, int h);

#endif /* HOME_CONTROLS_H */
