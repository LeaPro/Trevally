/**
 * @file cover_art.h
 * @brief Cover art of the track that is playing on the active audio source (see audio_source.h), in two sizes.
 *
 * Every source publishes trackArtThumbnail (small, for spots such as the home-screen ribbon) and trackArt (native
 * image, for the full-width art on the streaming controls page); a source may publish the same file for both.
 * Bluetooth: the phone sends a 200x200 JPEG thumbnail first, then the native image (see Bluetooth.h).
 *
 * The widget keeps itself current: it polls the elements on a timer of its own, follows the active source when it
 * changes, and shows a plain placeholder while there is no art.
 */

#ifndef COVER_ART_H
#define COVER_ART_H

#include "lvgl/lvgl.h"
#include <stdbool.h>

/**
 * @brief Creates a @p w x @p h cover art box in @p parent.
 *
 * The image fills the box keeping its aspect ratio (centre-cropped).
 *
 * @param thumbnail  true: follow trackArtThumbnail; false: follow trackArt (native)
 * @param radius     corner radius of the box (the image is clipped to it)
 */
lv_obj_t *cover_art_create(lv_obj_t *parent, int w, int h, bool thumbnail, int radius);

#endif /* COVER_ART_H */
