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
 * The image fills the box keeping its aspect ratio (centre-cropped).
 *
 * @param radius     corner radius of the box (the image is clipped to it)
 */
lv_obj_t *album_art_create(lv_obj_t *parent, int w, int h, int radius);

#endif /* ALBUM_ART_H */
