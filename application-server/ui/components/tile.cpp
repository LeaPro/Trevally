/**
 * @file tile.cpp
 * @brief Tile dispatch and leaf readers (see tile.h). The tiles themselves
 *        are built in tile_<type>.cpp.
 */

#include "tile.h"
#include "../ui_design_tokens.h"
#include "adaptive_text.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <vector>

#include <object.h>

/* The device's root object (defined in main.cpp). */
extern std::shared_ptr<RootObject> root;

bool tile_type_parse(const std::string &s, tile_type_t &out)
{
    static const std::map<std::string, tile_type_t> types = {
        {"output-volume",    OUTPUT_VOLUME},
        {"input",            INPUT},
        {"control",          CONTROL},
        {"sensor",           SENSOR},
        {"bluetooth-volume", BLUETOOTH},
        {"streaming-volume", STREAMING},
        {"weather",          WEATHER},
        {"macro",            MACRO},
    };
    auto it = types.find(s);
    if (it == types.end()) return false;
    out = it->second;
    return true;
}

/* Tiles that open a page when tapped (see tile_create). Dropped with the tiles'
 * screen by tile_live_clear(). */
struct TileTap {
    lv_obj_t   *tile;
    lv_obj_t   *screen;
    tile_type_t type;
    std::string name;
    std::string binding;
};
static std::vector<TileTap> tileTaps;

static void tile_clicked_cb(lv_event_t *e)
{
    lv_obj_t *tile = (lv_obj_t *)lv_event_get_current_target(e);
    for (const TileTap &t : tileTaps) {
        if (t.tile != tile) continue;
        switch (t.type) {
        case BLUETOOTH: tile_bluetooth_open(t.name, t.binding); break;
        default: break;
        }
        return;
    }
}

lv_obj_t *tile_create(lv_obj_t *parent, tile_type_t type,
                      const std::string &name, const std::string &binding,
                      int w, int h)
{
    lv_obj_t *tile = lv_obj_create(parent);
    lv_obj_remove_style_all(tile);
    lv_obj_set_size(tile, w, h);
    lv_obj_add_style(tile, ui_theme_get_style_bg(), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_CLICKABLE);

    switch (type) {
    case OUTPUT_VOLUME: tile_output_volume_build(tile, name, binding, w, h); break;
    case INPUT:         tile_input_build        (tile, name, binding, w, h); break;
    case CONTROL:       tile_control_build      (tile, name, binding, w, h); break;
    case SENSOR:        tile_sensor_build       (tile, name, binding, w, h); break;
    case BLUETOOTH:     tile_bluetooth_build    (tile, name, binding, w, h); break;
    case STREAMING:     tile_streaming_build    (tile, name, binding, w, h); break;
    case WEATHER:       tile_weather_build      (tile, name, binding, w, h); break;
    case MACRO:         tile_macro_build        (tile, name, binding, w, h); break;
    }

    /* Types that have a page open it when tapped. */
    if (type == BLUETOOTH) {
        lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(tile, tile_clicked_cb, LV_EVENT_CLICKED, NULL);
        tileTaps.push_back({tile, lv_obj_get_screen(tile), type, name, binding});
    }
    return tile;
}

/* =========================================================================
 * Widget helpers
 * ====================================================================== */

lv_obj_t *tile_container(lv_obj_t *parent)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_CLICKABLE);
    return c;
}

lv_obj_t *tile_label(lv_obj_t *parent, const std::string &text,
                     const lv_font_t *font, lv_style_t *style)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_add_style(l, style, 0);
    lv_obj_set_style_text_font(l, font, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_label_set_text(l, text.c_str());
    return l;
}

int tile_text_width(const std::string &text, const lv_font_t *font)
{
    lv_point_t size = {0, 0};
    lv_text_get_size(&size, text.c_str(), font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return size.x;
}

lv_obj_t *tile_adaptive_label(lv_obj_t *parent, const std::string &text,
                              const lv_font_t *font, lv_style_t *style,
                              int max_w)
{
    if (max_w < 1) max_w = 1;
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_add_style(l, style, 0);
    lv_obj_set_style_text_font(l, font, 0);
    /* +2px so rounding never makes text that exactly fits wrap into a marquee */
    lv_obj_set_width(l, std::min(tile_text_width(text, font) + 2, max_w));
    ui_adaptive_text_set(l, text.c_str(), 1);
    return l;
}

/* =========================================================================
 * Leaf readers
 * ====================================================================== */

static float normalized(float min, float max, float value)
{
    if (max <= min) return 0.0f;
    return std::min(1.0f, std::max(0.0f, (value - min) / (max - min)));
}

static std::string displayUnits(const char *units)
{
    if (!units) return std::string();
    return strcmp(units, "percent") == 0 ? "%" : units;
}

/* Value, normalised fill and units of a numeric element; zeros for anything else. */
static TileNumber readNumber(Element *element)
{
    TileNumber n;
    if (auto f = dynamic_cast<FloatElement *>(element)) {
        n.value = f->get();
        n.fill  = normalized(f->min, f->max, n.value);
        n.min   = f->min;
        n.max   = f->max;
        n.units = displayUnits(f->units);
    } else if (auto u = dynamic_cast<UInt32Element *>(element)) {
        n.value = (float)u->get();
        n.fill  = normalized((float)u->min, (float)u->max, n.value);
        n.min   = (float)u->min;
        n.max   = (float)u->max;
        n.units = displayUnits(u->units);
    }
    return n;
}

TileNumber tile_read_number(const std::string &leafPath, const char *elementName)
{
    return readNumber(root->elementFromPath(leafPath + "/" + elementName).get());
}

/* Live bindings: label/range widgets that follow a numeric element. Polled by value
 * (not isModified()) so the flag stays available to the leaf that owns the element.
 * Each binding remembers its screen: only the active screen is refreshed, and a
 * screen's bindings are dropped together (tile_live_clear). */
struct LiveNumber {
    std::shared_ptr<Element> element;
    lv_obj_t *label;
    lv_obj_t *range;    /* slider or arc */
    lv_obj_t *screen;
    float     last;
};
static std::vector<LiveNumber> liveNumbers;

void tile_bind_number(lv_obj_t *label, lv_obj_t *range,
                      const std::string &leafPath, const char *elementName)
{
    lv_obj_t *anchor = label ? label : range;
    if (!anchor) return;
    auto element = root->elementFromPath(leafPath + "/" + elementName);
    if (!dynamic_cast<FloatElement *>(element.get()) && !dynamic_cast<UInt32Element *>(element.get()))
        return;
    liveNumbers.push_back({element, label, range, lv_obj_get_screen(anchor),
                           readNumber(element.get()).value});
}

void tile_live_update()
{
    lv_obj_t *active = lv_screen_active();
    for (LiveNumber &b : liveNumbers) {
        if (b.screen != active) continue;   /* refreshed on the first tick after it becomes active */
        TileNumber n = readNumber(b.element.get());
        if (n.value == b.last) continue;
        b.last = n.value;
        if (b.label) lv_label_set_text(b.label, tile_format_number(n.value).c_str());
        if (b.range) {
            const int32_t pos = (int32_t)(n.fill * 1000.0f);
            if (lv_obj_check_type(b.range, &lv_arc_class)) lv_arc_set_value(b.range, pos);
            else                                           lv_slider_set_value(b.range, pos, LV_ANIM_OFF);
        }
    }
}

void tile_live_clear(lv_obj_t *screen)
{
    liveNumbers.erase(std::remove_if(liveNumbers.begin(), liveNumbers.end(),
                                     [screen](const LiveNumber &b) { return b.screen == screen; }),
                      liveNumbers.end());
    tileTaps.erase(std::remove_if(tileTaps.begin(), tileTaps.end(),
                                  [screen](const TileTap &t) { return t.screen == screen; }),
                   tileTaps.end());
}

void tile_step_number(const std::string &leafPath, const char *elementName, int steps)
{
    auto element = root->elementFromPath(leafPath + "/" + elementName);
    if (auto f = dynamic_cast<FloatElement *>(element.get())) {
        f->set(f->get() + (float)steps);                      /* set() clamps to min..max */
    } else if (auto u = dynamic_cast<UInt32Element *>(element.get())) {
        int64_t v = (int64_t)u->get() + steps;
        u->set(v < 0 ? 0u : (uint32_t)v);                     /* set() clamps to min..max */
    }
}

std::string tile_read_string(const std::string &leafPath, const char *elementName)
{
    auto element = root->elementFromPath(leafPath + "/" + elementName);
    if (auto s = dynamic_cast<StringElement *>(element.get()))
        return s->get();
    return std::string();
}

std::string tile_format_number(float value)
{
    char buf[32];
    snprintf(buf, sizeof(buf), (value == (float)(long)value) ? "%.0f" : "%.1f", value);
    return buf;
}
