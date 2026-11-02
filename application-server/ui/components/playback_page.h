/**
 * @file playback_page.h
 * @brief The playback page: the track that is playing on the active audio source (see audio_source.h) with its cover art.
 *
 * Figma: "Streaming Controls" (portrait 5455:9472, landscape 5552:27240).
 *   Portrait:  the cover art across the top, then album, title and artist with the play / pause button, then the progress
 *              bar with the elapsed and remaining time, and the logo.
 *   Landscape: the cover art on the left; title and artist with the play / pause button at the top of the right-hand
 *              panel, the progress bar with the times at its bottom.
 * A back button sits over the top left of the art. The progress bar is hidden while the source reports no track length.
 * Not built yet: the buttons along the top other than back, the previous / next / shuffle / repeat buttons, the knob.
 *
 * The page is a navigation destination (see nav.h): build it on a nav_screen_create() screen and reach it with
 * nav_goto(). It follows the active source by itself (timer of its own). The layout depends on the orientation, so it
 * has to be rebuilt when that changes.
 */

#ifndef PLAYBACK_PAGE_H
#define PLAYBACK_PAGE_H

#include "lvgl/lvgl.h"

/** Builds the page's widgets on @p screen. */
void playback_page_create(lv_obj_t *screen);

#endif /* PLAYBACK_PAGE_H */
