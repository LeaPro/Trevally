/**
 * @file album_art.h
 * @brief Album art of the track that is playing on the active audio source (see audio_source.h).
 *
 * Every source publishes the file of its album art in trackAlbumArt; the widget scales it to the size of its box.
 *
 * The widget keeps itself current: it polls the elements on a timer of its own, follows the active source when it
 * changes, and shows a plain placeholder while there is no art.
 */

#ifndef ALBUM_ART_H
#define ALBUM_ART_H

#include "lvgl/lvgl.h"
#include <stdbool.h>

/**
 * @brief Creates a @p w x @p h album art box in @p parent.
 *
 * The image is scaled to fit inside the box keeping its aspect ratio (never cropped).
 *
 * @param radius     corner radius of the box (the image is clipped to it)
 * @param align      where the image sits in the box when their aspect ratios differ
 */
lv_obj_t *album_art_create(lv_obj_t *parent, int w, int h, int radius, lv_align_t align = LV_ALIGN_CENTER);

#endif /* ALBUM_ART_H */
