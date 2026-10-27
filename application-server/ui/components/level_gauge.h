/**
 * @file level_gauge.h
 * @brief Gauge for an adjustable numeric element (volume etc.): a large value
 *        above a 270-degree arc ringing the physical encoder, with the units
 *        inside the arc and the element's min / max under its two ends.
 *
 * The widgets follow the element live (see tile_bind_number). The gauge does
 * not change the element: what the encoder does is up to the page using it.
 * Positions are absolute on the 480x854 panel, so @p parent must be a
 * full-screen page.
 */

#ifndef LEVEL_GAUGE_H
#define LEVEL_GAUGE_H

#include "lvgl/lvgl.h"
#include <string>

/* The encoder's centre and the arc's outer radius on the panel (from the
 * Figma "Knob": 282px wide, centred horizontally, top at y=535). */
#define LEVEL_GAUGE_CX      240
#define LEVEL_GAUGE_CY      676
#define LEVEL_GAUGE_RADIUS  187
#define LEVEL_GAUGE_ARC_W   12
#define LEVEL_GAUGE_NUMBER_Y 190   /* top of the value label (satoshi_medium_140, 160 px line) */

/** Returns the arc. @p leafPath / @p elementName select the numeric element. */
lv_obj_t *level_gauge_create(lv_obj_t *parent, const std::string &leafPath, const char *elementName);

#endif /* LEVEL_GAUGE_H */
