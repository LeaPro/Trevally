/**
 * @file tile.h
 * @brief Individual data block component for the scroll pane.
 *
 * Figma node: 5259-1973 (Blocks component set)
 *
 * A Block is a self-contained tile that displays one data point.
 * It knows its own size; the parent page container uses
 * LV_LAYOUT_FLEX + LV_FLEX_FLOW_ROW_WRAP so blocks flow automatically
 * without manual coordinate placement.
 *
 * Sizes and pixel dimensions (from Figma):
 *   TILE_SIZE_FULL    480 × 386 px  – full width, full height
 *   TILE_SIZE_HALF    480 × 193 px  – full width, half height
 *   TILE_SIZE_QUARTER 240 × 193 px  – half width, half height (2 per row)
 *   TILE_SIZE_SIXTH   240 × 129 px  – half width, third height (2 per row)
 *
 * Types:
 *   OUTPUT_VOLUME  – editable slider, brand colour value
 *   CONTROL        – editable slider (white knob), brand value
 *   SENSOR         – read-only bar, text-primary value
 *   SOURCE_SELECT  – enumeration state + volume level
 *   MACRO          – enumeration state in brand colour
 *   WEATHER        – city / icon / condition / large temperature
 *
 * Sub-types (for Control / Sensor / Macro / Source Select):
 *   NUMBER         – progress bar + numeric value
 *   BOOLEAN        – On / Off text
 *   ENUMERATION    – state name text
 */

#ifndef TILE_H
#define TILE_H

#include "lvgl/lvgl.h"
#include <stdbool.h>

/* =========================================================================
 * Enumerations
 * ====================================================================== */

typedef enum {
    TILE_TYPE_OUTPUT_VOLUME,
    TILE_TYPE_SOURCE_SELECT,
    TILE_TYPE_MACRO,
    TILE_TYPE_WEATHER,
    TILE_TYPE_CONTROL,
    TILE_TYPE_SENSOR,
} tile_type_t;

typedef enum {
    TILE_SIZE_FULL,      /* 480 × 386 */
    TILE_SIZE_HALF,      /* 480 × 193 */
    TILE_SIZE_QUARTER,   /* 240 × 193 */
    TILE_SIZE_SIXTH,     /* 240 × 129 */
} tile_size_t;

typedef enum {
    TILE_SUB_NUMBER,
    TILE_SUB_BOOLEAN,
    TILE_SUB_ENUMERATION,
} tile_subtype_t;

/* =========================================================================
 * Configuration struct – pass to tile_create()
 * ====================================================================== */

typedef struct {
    tile_type_t    type;
    tile_size_t    size;
    tile_subtype_t subtype;

    /* Common */
    const char     *label;         /* block title, e.g. "Output Volume" */
    const char     *value_str;     /* numeric string, state name, or On/Off */
    const char     *unit_str;      /* "%" / "dB" / NULL */
    float           bar_fill;      /* 0.0 – 1.0 for progress bar */

    /* Source Select extras */
    const char     *source_level;       /* e.g. "50%" */
    const char     *volume_icon_path;   /* A:path/to/volume.svg  or NULL */

    /* Weather extras */
    const char     *city;
    const char     *weather_icon_path;  /* A:path/to/weather.svg or NULL */
    const char     *condition;          /* "Clear", "Cloudy", … */
    float           temperature;        /* numeric value */
    const char     *temp_unit;          /* "°F" / "°C" or NULL (defaults to °) */
    const char     *degrees_icon_path;  /* small degree-symbol image or NULL */

    /* Interaction */
    lv_event_cb_t  tapped_cb;
    void          *user_data;

} tile_config_t;

/* =========================================================================
 * Pixel dimensions helpers
 * ====================================================================== */

/** Return pixel width for a given block size. */
static inline int tile_size_w(tile_size_t s)
{
    return (s == TILE_SIZE_FULL || s == TILE_SIZE_HALF) ? 480 : 240;
}

/** Return pixel height for a given block size. */
static inline int tile_size_h(tile_size_t s)
{
    switch (s) {
        case TILE_SIZE_FULL:    return 386;
        case TILE_SIZE_HALF:    return 193;
        case TILE_SIZE_QUARTER: return 193;
        case TILE_SIZE_SIXTH:   return 129;
        default:                 return 129;
    }
}

/* =========================================================================
 * Public API
 * ====================================================================== */

/**
 * @brief Create a block tile as a child of @p parent.
 *
 * The block sets its own width/height from cfg->size.
 * Simply add it to a page container that has:
 *   lv_obj_set_layout(page, LV_LAYOUT_FLEX);
 *   lv_obj_set_flex_flow(page, LV_FLEX_FLOW_ROW_WRAP);
 * and it will flow automatically.
 *
 * @return The root lv_obj_t* of the block.
 */
lv_obj_t *tile_create(lv_obj_t *parent, const tile_config_t *cfg);

/**
 * @brief Live-update a block's displayed value and bar fill.
 * @param block  Object returned by tile_create().
 */
void tile_update_value(lv_obj_t *block, const char *value_str, float bar_fill);

#endif /* TILE_H */
