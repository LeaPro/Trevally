/**
 * @file level_gauge.cpp
 * @brief See level_gauge.h.
 */

#include "level_gauge.h"
#include "tile.h"
#include "../ui_design_tokens.h"

#include <math.h>
#include <object.h>

/* The device's root object (defined in main.cpp). */
extern std::shared_ptr<RootObject> root;

/* The style chosen by the Appearance "levelGaugeStyle" element; ELLIPSE if it is missing or unknown. */
static level_gauge_style_t gauge_style(void)
{
    auto element = std::dynamic_pointer_cast<StringControl>(root->elementFromPath("/appearance/levelGaugeStyle"));
    const std::string value = element ? element->get() : std::string();
    if (value == "anchored") return level_gauge_style_t::ANCHORED;
    if (value == "floating") return level_gauge_style_t::FLOATING;
    return level_gauge_style_t::ELLIPSE;
}

/* Per-style placement, from the LEVEL_GAUGE_<STYLE>_* constants in level_gauge.h. */
struct GaugeGeometry
{
    int sweep;     /* arc sweep, degrees */
    int radius;
    int cy;        /* arc centre y */
    int numberY;   /* top of the value label */
    int unitsDY;   /* units' top relative to cy */
    bool rotateArc; /* landscape: the arc switches orientation with the labels (laid out afresh);
                       otherwise it stays on the encoder and only the labels move */
};

static GaugeGeometry gauge_geometry(void)
{
    switch (gauge_style())
    {
    case level_gauge_style_t::FLOATING:
    {
        const int numberY = LEVEL_GAUGE_FLOATING_NUMBER_Y;
        const int cy = numberY + lv_font_get_line_height(&satoshi_medium_140) / 2;
        return { LEVEL_GAUGE_FLOATING_SWEEP, LEVEL_GAUGE_FLOATING_RADIUS, cy, numberY,
                 LEVEL_GAUGE_FLOATING_UNITS_DY, true };
    }
    case level_gauge_style_t::ELLIPSE:
        return { LEVEL_GAUGE_ELLIPSE_SWEEP, LEVEL_GAUGE_ELLIPSE_RADIUS, LEVEL_GAUGE_ELLIPSE_CY,
                 LEVEL_GAUGE_ELLIPSE_NUMBER_Y, LEVEL_GAUGE_ELLIPSE_UNITS_DY, false };
    case level_gauge_style_t::ANCHORED:
    default:
        return { LEVEL_GAUGE_ANCHORED_SWEEP, LEVEL_GAUGE_ANCHORED_RADIUS, LEVEL_GAUGE_ANCHORED_CY,
                 LEVEL_GAUGE_ANCHORED_NUMBER_Y, LEVEL_GAUGE_ANCHORED_UNITS_DY, false };
    }
}

/* A label's spot on the screen: its centre, or its left edge and vertical centre when @c left. */
struct Spot
{
    int x;
    int y;
    bool left;
};

struct GaugeLayout
{
    int sweep;
    int radius;
    int rotation;   /* lv_arc_set_rotation */
    int cx, cy;     /* arc centre */
    Spot number, units, minLabel, maxLabel;
};

/* The encoder is fixed on the panel, so in landscape (panel turned 90 degrees) an encoder-anchored
 * arc is the portrait arc turned with it: it keeps its place relative to the encoder and its gap
 * faces the encoder, while the text stays upright and moves to where it fits. An arc with
 * rotateArc set is instead laid out afresh around the value label, gap toward the encoder as well. */
static GaugeLayout gauge_layout(void)
{
    const GaugeGeometry g = gauge_geometry();
    const bool landscape = ui_is_landscape();
    const int R = (landscape && g.rotateArc) ? LEVEL_GAUGE_FLOATING_LAND_RADIUS : g.radius;

    const int numberH = lv_font_get_line_height(&satoshi_medium_140);
    const int unitsH = lv_font_get_line_height(&satoshi_medium_32);
    const int endH = lv_font_get_line_height(&satoshi_medium_24);

    /* Min / max sit under the arc's two ends (half a gap either side of the gap's centre). */
    const int halfGap = (360 - g.sweep) / 2;
    const float halfGapRad = halfGap * (float)M_PI / 180.0f;
    const int dx = (int)(R * sinf(halfGapRad));
    const int endDY = (int)(R * cosf(halfGapRad)) + LEVEL_GAUGE_END_GAP;   /* from the arc centre to the labels' near edge */

    GaugeLayout L;
    L.sweep = g.sweep;
    L.radius = R;

    if (!landscape)
    {
        L.cx = LEVEL_GAUGE_CX;
        L.cy = g.cy;
        L.rotation = 90 + halfGap;   /* LVGL angles run clockwise from 3 o'clock: gap at 6 o'clock */
        L.number = { LEVEL_GAUGE_CX, g.numberY + numberH / 2, false };
        L.units = { LEVEL_GAUGE_CX, g.cy + g.unitsDY + unitsH / 2, false };
        L.minLabel = { LEVEL_GAUGE_CX - dx, g.cy + endDY + endH / 2, false };
        L.maxLabel = { LEVEL_GAUGE_CX + dx, g.cy + endDY + endH / 2, false };
        return L;
    }

    /* Landscape: the portrait x axis becomes the vertical one (the panel's short side is 2 * LEVEL_GAUGE_CX). */
    L.cy = LEVEL_GAUGE_CX;
    L.rotation = halfGap;            /* gap at 3 o'clock, toward the encoder */
    if (g.rotateArc)
    {
        L.cx = LEVEL_GAUGE_FLOATING_LAND_CX;
        L.number = { L.cx, L.cy, false };
        L.units = { L.cx, L.cy + g.unitsDY + unitsH / 2, false };
    }
    else
    {
        L.cx = g.cy;   /* the portrait arc centre's y is its distance from the panel's top, now its x */
        L.number = { (L.cx - R) / 2, L.cy, false };   /* centred in the space left of the arc */
        L.units = { L.cx + g.unitsDY + unitsH / 2, L.cy, false };
    }
    L.minLabel = { L.cx + endDY, L.cy + dx, true };   /* min at the bottom end, max at the top */
    L.maxLabel = { L.cx + endDY, L.cy - dx, true };
    return L;
}

static void place(lv_obj_t *obj, const Spot &s)
{
    const int midX = lv_display_get_horizontal_resolution(nullptr) / 2;
    const int midY = lv_display_get_vertical_resolution(nullptr) / 2;
    if (s.left)
        lv_obj_align(obj, LV_ALIGN_LEFT_MID, s.x, s.y - midY);
    else
        lv_obj_align(obj, LV_ALIGN_CENTER, s.x - midX, s.y - midY);
}

lv_obj_t *level_gauge_create(lv_obj_t *parent, const std::string &leafPath, const char *elementName)
{
    const TileNumber n = tile_read_number(leafPath, elementName);

    const GaugeLayout L = gauge_layout();
    const int R = L.radius;

    /* Arc: L.sweep degrees, range 0..1000 as the live binding expects. */
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_remove_style_all(arc);
    lv_group_remove_obj(arc);   /* keep encoder rotation off the decorative widget */
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(arc, 2 * R, 2 * R);
    lv_obj_set_pos(arc, L.cx - R, L.cy - R);
    lv_arc_set_rotation(arc, L.rotation);
    lv_arc_set_bg_angles(arc, 0, L.sweep);
    lv_arc_set_range(arc, 0, 1000);
    lv_arc_set_value(arc, (int32_t)(n.fill * 1000.0f));

    lv_obj_add_style(arc, ui_theme_get_style_bg_tertiary(), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, LEVEL_GAUGE_ARC_W, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    lv_obj_add_style(arc, ui_theme_get_style_brand(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, LEVEL_GAUGE_ARC_W, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);

    /* Value label and units, positioned per style (see level_gauge.h). */
    lv_obj_t *number = tile_label(parent, tile_format_number(n.value), &satoshi_medium_140,
                                  ui_theme_get_style_text_primary());
    place(number, L.number);

    lv_obj_t *units = tile_label(parent, n.units, &satoshi_medium_32, ui_theme_get_style_text_secondary());
    place(units, L.units);

    lv_obj_t *minLabel = tile_label(parent, tile_format_number(n.min), &satoshi_medium_24,
                                    ui_theme_get_style_text_secondary());
    place(minLabel, L.minLabel);
    lv_obj_t *maxLabel = tile_label(parent, tile_format_number(n.max), &satoshi_medium_24,
                                    ui_theme_get_style_text_secondary());
    place(maxLabel, L.maxLabel);

    tile_bind_number(number, arc, leafPath, elementName);
    return arc;
}
