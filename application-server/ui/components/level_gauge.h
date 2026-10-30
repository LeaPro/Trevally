/**
 * @file level_gauge.h
 * @brief Gauge for an adjustable numeric element (volume etc.): a large value
 *        label and an arc, with the units inside the arc and the
 *        element's min / max under its two ends. Placement depends on the
 *        Appearance "levelGaugeStyle" element (see Appearance.h):
 *        ANCHORED rings the physical encoder with the value above the arc;
 *        FLOATING centres the arc on the value label; ELLIPSE is a shallow
 *        arc under the value label. Each style has its own sweep angle.
 *
 * The widgets follow the element live (see tile_bind_number). The gauge does
 * not change the element: what the encoder does is up to the page using it.
 * Positions are absolute on the display (480x854, or 854x480 in landscape), so
 * @p parent must be a full-screen page.
 */

#ifndef LEVEL_GAUGE_H
#define LEVEL_GAUGE_H

#include "lvgl/lvgl.h"
#include <string>

/* Shared by all styles: horizontal centre of the panel and the arc thickness. */
#define LEVEL_GAUGE_CX      240
#define LEVEL_GAUGE_ARC_W   12
#define LEVEL_GAUGE_END_GAP 20   /* between the arc's ends and the min / max labels */

/* Each style's *_SWEEP is the arc's sweep in degrees, centred opposite the
 * bottom gap: smaller = more open at the bottom (max 360). */

/* level_gauge_style_t::ANCHORED: the arc is centred on the encoder (from the
 * Figma "Knob": 282px wide, centred horizontally, top at y=535). */
#define LEVEL_GAUGE_ANCHORED_SWEEP     240
#define LEVEL_GAUGE_ANCHORED_CY        676
#define LEVEL_GAUGE_ANCHORED_RADIUS    200
#define LEVEL_GAUGE_ANCHORED_NUMBER_Y  190   /* top of the value label (satoshi_medium_140, 160 px line) */
#define LEVEL_GAUGE_ANCHORED_UNITS_DY  (-186) /* units' top, relative to the arc centre (just inside its top) */

/* level_gauge_style_t::FLOATING: the arc is centred on the value label. */
#define LEVEL_GAUGE_FLOATING_SWEEP     240
#define LEVEL_GAUGE_FLOATING_RADIUS    200
#define LEVEL_GAUGE_FLOATING_NUMBER_Y  260   /* top of the value label; arc centre follows it */
#define LEVEL_GAUGE_FLOATING_UNITS_DY  60    /* units' top, relative to the arc centre (below the label's centre) */

/* level_gauge_style_t::ELLIPSE: a shallow arc whose top is at
 * y = CY - RADIUS (about 434 here), with the units just under its crown. */
#define LEVEL_GAUGE_ELLIPSE_SWEEP      100
#define LEVEL_GAUGE_ELLIPSE_RADIUS     250
#define LEVEL_GAUGE_ELLIPSE_CY         684
#define LEVEL_GAUGE_ELLIPSE_NUMBER_Y   210   /* top of the value label */
#define LEVEL_GAUGE_ELLIPSE_UNITS_DY   (-229) /* units' top, relative to the arc centre */

/* Landscape (854x480, see ui_is_landscape()): an ANCHORED / ELLIPSE arc keeps its place around
 * the encoder, turned with the panel, so its gap faces the encoder; the value, units and
 * min / max stay upright and move to fit (value left of the arc, min at the bottom end, max at
 * the top). FLOATING is laid out around the value label at (FLOATING_LAND_CX, panel mid-height),
 * with its gap toward the encoder. */
#define LEVEL_GAUGE_FLOATING_LAND_CX     250
#define LEVEL_GAUGE_FLOATING_LAND_RADIUS 170

/** Returns the arc. @p leafPath / @p elementName select the numeric element.
 *  The look is selected by the Appearance "levelGaugeStyle" element. */
lv_obj_t *level_gauge_create(lv_obj_t *parent, const std::string &leafPath, const char *elementName);

#endif /* LEVEL_GAUGE_H */
