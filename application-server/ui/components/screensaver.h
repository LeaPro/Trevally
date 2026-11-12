/**
 * @file screensaver.h
 * @brief The screensaver's content (Figma "Screensaver ..." frames), per /screensaver (see Screensaver.h).
 *
 * Types: "weather" (city, conditions, temperature), "date-time" (date and time; the clock hands are not drawn
 * yet) and "background-logo" (the active theme's screensaver background image and logo, from
 * /mnt/data/image-files; the theme's background colour without a background image). With showStreamingInfo the
 * active audio source's album art, title and artist are shown above the knob while it has a track.
 *
 * Everything follows the active theme's colours (custom themes too): weather and date-time draw the Figma glow
 * in its background / bg-secondary, and text uses its text colours, except over a background image, where it is
 * white. The content is laid out clear of the physical knob: in portrait the column above it, in landscape the
 * column to its left. When and how it is shown is up to lock_screen.cpp.
 */

#ifndef SCREENSAVER_H
#define SCREENSAVER_H

#include "lvgl/lvgl.h"
#include <cstdint>
#include <string>

/** /screensaver "enabled". */
bool screensaver_enabled();

/** /screensaver "timeoutSeconds" in ms. */
uint32_t screensaver_timeout_ms();

/** Builds the screensaver in @p parent, a full-display object; it keeps itself current until deleted. */
void screensaver_build(lv_obj_t *parent);

/** Changes whenever screensaver_build() would build something different (type, theme images, orientation). */
std::string screensaver_signature();

#endif /* SCREENSAVER_H */
