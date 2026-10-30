/**
 * @file tile.h
 * @brief Tiles: the individual data widgets placed on a screen.
 *
 * A tile is only given a size (decided by the screen from how many tiles it
 * holds), a display name and a binding. Everything inside the tile - layout,
 * fonts, which leaf it reads and what it shows - belongs to its type, and
 * each type is built by its own tile_<type>.cpp:
 *
 *   OUTPUT_VOLUME  tile_output_volume.cpp   (value shows "-"; will come from /leaOutput config)
 *   INPUT          tile_input.cpp           (values show "-"; will come from /leaInput config)
 *   CONTROL        tile_control.cpp         (value shows "-"; will come from /leaControl config)
 *   SENSOR         tile_sensor.cpp          (value shows "-"; will come from /leaSensor config)
 *   BLUETOOTH      tile_bluetooth.cpp       /bluetooth/volume
 *   STREAMING      tile_streaming.cpp       /<binding, lower-cased>/volume
 *   WEATHER        tile_weather.cpp         /weather config[binding]
 *   MACRO          tile_macro.cpp           (no data yet)
 *
 * Values are read from the leaves when the tile is built; the screens are
 * rebuilt when the Screens / Tiles config changes.
 */

#ifndef TILE_H
#define TILE_H

#include "lvgl/lvgl.h"
#include <functional>
#include <string>

typedef enum {
    OUTPUT_VOLUME,
    INPUT,
    CONTROL,
    SENSOR,
    BLUETOOTH,
    STREAMING,
    WEATHER,
    MACRO,
} tile_type_t;

/** Maps a Tiles.config "type" string (e.g. "output-volume") to a tile type. */
bool tile_type_parse(const std::string &s, tile_type_t &out);

/**
 * @brief Create a tile of @p type, @p w x @p h pixels, as a child of
 *        @p parent. The caller positions it.
 *
 * @param name     Tile display name (Tiles.config[id].name)
 * @param binding  Tile binding (Tiles.config[id].binding); meaning is per type
 */
lv_obj_t *tile_create(lv_obj_t *parent, tile_type_t type,
                      const std::string &name, const std::string &binding,
                      int w, int h);

/* =========================================================================
 * Per-type builders - fill an empty @p root (already sized and themed)
 * ====================================================================== */

void tile_output_volume_build(lv_obj_t *root, const std::string &name, const std::string &binding, int w, int h);
void tile_input_build        (lv_obj_t *root, const std::string &name, const std::string &binding, int w, int h);
void tile_control_build      (lv_obj_t *root, const std::string &name, const std::string &binding, int w, int h);
void tile_sensor_build       (lv_obj_t *root, const std::string &name, const std::string &binding, int w, int h);
void tile_bluetooth_build    (lv_obj_t *root, const std::string &name, const std::string &binding, int w, int h);
void tile_streaming_build    (lv_obj_t *root, const std::string &name, const std::string &binding, int w, int h);
void tile_weather_build      (lv_obj_t *root, const std::string &name, const std::string &binding, int w, int h);
void tile_macro_build        (lv_obj_t *root, const std::string &name, const std::string &binding, int w, int h);

/* Pages: a tile type may open its own page (see nav.h) when tapped. Types with a
 * page are made clickable by tile_create(). */
void tile_bluetooth_volume_open(const std::string &name, const std::string &binding);

/** Opens the Paired devices page directly (e.g. from the Settings Bluetooth row), titled @p title. */
void tile_bluetooth_devices_open(const std::string &title);

/* Live values: a tile or page that follows an element at runtime binds its widgets to it.
 * tile_live_update() is called every UI tick (see scheduleLvglTick) and only touches
 * widgets on the active screen. tile_live_clear(screen) must run before the widgets of
 * that screen are destroyed (screens rebuilt, pages popped). */

/** Keeps @p label (value text) and/or @p range (a slider or an arc, 0..1000 fill) following
 *  a numeric element. Either widget may be null. Nothing is registered if the element is
 *  missing or not numeric. */
void tile_bind_number(lv_obj_t *label, lv_obj_t *range,
                      const std::string &leafPath, const char *elementName);
void tile_live_update();
/** Calls @p fn on every UI tick while @p anchor's screen is the active one (for widgets
 *  that need more than a number kept in sync, e.g. a list). Dropped by tile_live_clear(). */
void tile_bind_update(lv_obj_t *anchor, std::function<void()> fn);
/** Drops the bindings and tap targets of every widget on @p screen. */
void tile_live_clear(lv_obj_t *screen);

/* =========================================================================
 * Small widget helpers (no styling decisions beyond "transparent" / "clipped")
 * ====================================================================== */

/** Transparent, unpadded, non-scrollable, non-clickable container. */
lv_obj_t *tile_container(lv_obj_t *parent);

/** Single-line clipped label with @p font and the shared theme @p style. */
lv_obj_t *tile_label(lv_obj_t *parent, const std::string &text,
                     const lv_font_t *font, lv_style_t *style);

/** Natural single-line width of @p text in @p font, in pixels. */
int tile_text_width(const std::string &text, const lv_font_t *font);

/**
 * Adaptive label for user/device-supplied text: one line when it fits in
 * @p max_w, otherwise a circular marquee (see adaptive_text.h). The label is
 * sized to its text, capped at @p max_w, so centred/space-between parents
 * keep working for short text.
 */
lv_obj_t *tile_adaptive_label(lv_obj_t *parent, const std::string &text,
                              const lv_font_t *font, lv_style_t *style,
                              int max_w);

/* =========================================================================
 * Leaf readers - the only thing the tile types have in common besides size
 * ====================================================================== */

struct TileNumber {
    float       value = 0.0f;
    float       fill  = 0.0f;   /* value normalised to the element's min..max */
    float       min   = 0.0f;
    float       max   = 0.0f;
    std::string units;          /* "percent" is shown as "%" */
};

/** Reads a numeric element (e.g. "/leaOutput", "volume"); zeros if missing. */
TileNumber tile_read_number(const std::string &leafPath, const char *elementName);

/** Adds @p steps (may be negative) to a numeric element, clamped to its range. */
void tile_step_number(const std::string &leafPath, const char *elementName, int steps);

/** Reads a string element; "-" if it is missing or empty. */
std::string tile_read_string(const std::string &leafPath, const char *elementName);

/** Reads a boolean element; false if missing. */
bool tile_read_bool(const std::string &leafPath, const char *elementName);

/** Writes a string control (e.g. "/bluetooth", "removeDevice"); no-op if missing or not a string. */
void tile_set_string(const std::string &leafPath, const char *elementName, const std::string &value);

/** Writes a boolean control (e.g. "/bluetooth", "scan"); no-op if missing or not a bool. */
void tile_set_bool(const std::string &leafPath, const char *elementName, bool value);

/** "50" for whole numbers, "50.5" otherwise. */
std::string tile_format_number(float value);

#endif /* TILE_H */
