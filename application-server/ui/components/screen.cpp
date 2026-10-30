/**
 * @file screen.cpp
 * @brief See screen.h.
 */

#include "screen.h"
#include "tile.h"
#include "../ui_design_tokens.h"

#include <cstdio>
#include <vector>

#include <Tiles.h>

/* The device's root object (defined in main.cpp). */
extern std::shared_ptr<RootObject> root;

namespace {

struct Rect { int x, y, w, h; };

struct TileSpec {
    tile_type_t type;
    std::string name;
    std::string binding;
};

std::string jsonString(const json &j, const char *key)
{
    auto it = j.find(key);
    return (it != j.end() && it->is_string()) ? it->get<std::string>() : std::string();
}

/* Tile rectangles for @p n tiles sharing one screen. */
std::vector<Rect> layoutFor(size_t n)
{
    const int W = ui_content_w(), H = ui_pane_h();
    const int half = H / 2, third = H / 3, col = W / 2;

    std::vector<Rect> r;
    switch (n) {
    case 0:
        break;
    case 1:
        r = {{0, 0, W, H}};
        break;
    case 2:
        r = {{0, 0, W, half}, {0, half, W, H - half}};
        break;
    case 3:
        r = {{0, 0, W, half}, {0, half, col, H - half}, {col, half, W - col, H - half}};
        break;
    case 4:
        r = {{0, 0, col, half},        {col, 0, W - col, half},
             {0, half, col, H - half}, {col, half, W - col, H - half}};
        break;
    default:
        for (size_t i = 0; i < n; i++) {
            int row = (int)(i / 2), c = (int)(i % 2);
            int y   = row * third;
            int h   = (row == 2) ? H - y : third;
            r.push_back({c * col, y, c == 0 ? col : W - col, h});
        }
        break;
    }
    return r;
}

std::vector<TileSpec> readTiles(const json &screenConfig)
{
    std::vector<TileSpec> specs;
    auto tiles = std::dynamic_pointer_cast<Tiles>(::root->leafFromPath("/tiles"));
    auto orderIt = screenConfig.find("tileOrder");
    if (!tiles || orderIt == screenConfig.end() || !orderIt->is_array()) return specs;

    json tilesConfig = tiles->configPtr->get();
    for (const auto &tileId : *orderIt) {
        if (!tileId.is_string()) continue;
        auto tileIt = tilesConfig.find(tileId.get<std::string>());
        if (tileIt == tilesConfig.end() || !tileIt->is_object()) {
            fprintf(stderr, "screen: tileOrder entry %s has no config\n",
                    tileId.get<std::string>().c_str());
            continue;
        }
        TileSpec spec;
        std::string typeName = jsonString(*tileIt, "type");
        if (!tile_type_parse(typeName, spec.type)) {
            fprintf(stderr, "screen: unknown tile type '%s'\n", typeName.c_str());
            continue;
        }
        if ((int)specs.size() >= SCREEN_MAX_TILES) {
            fprintf(stderr, "screen: more than %d tiles on screen '%s', ignoring the rest\n",
                    SCREEN_MAX_TILES, screen_name(screenConfig).c_str());
            break;
        }
        spec.name    = jsonString(*tileIt, "name");
        spec.binding = jsonString(*tileIt, "binding");
        specs.push_back(std::move(spec));
    }
    return specs;
}

} // namespace

std::string screen_name(const json &screenConfig)
{
    return jsonString(screenConfig, "name");
}

lv_obj_t *screen_create(lv_obj_t *parent, const json &screenConfig)
{
    lv_obj_t *screen = lv_obj_create(parent);
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_opa(screen, LV_OPA_TRANSP, 0);
    lv_obj_set_size(screen, ui_content_w(), ui_pane_h());
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);   /* let swipes reach the pane */
    lv_obj_add_flag(screen, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    std::vector<TileSpec> specs = readTiles(screenConfig);
    std::vector<Rect>     rects = layoutFor(specs.size());

    for (size_t i = 0; i < specs.size(); i++) {
        const Rect &r = rects[i];
        lv_obj_t *tile = tile_create(screen, specs[i].type, specs[i].name, specs[i].binding, r.w, r.h);
        lv_obj_set_pos(tile, r.x, r.y);

        /* Dividers between tiles only: none on the screen's outer edges. */
        int sides = LV_BORDER_SIDE_NONE;
        if (r.y + r.h < ui_pane_h()) sides |= LV_BORDER_SIDE_BOTTOM;
        if (r.x + r.w < ui_content_w()) sides |= LV_BORDER_SIDE_RIGHT;
        lv_obj_add_style(tile, ui_theme_get_style_border(), 0);
        lv_obj_set_style_border_width(tile, 1, 0);
        lv_obj_set_style_border_side(tile, (lv_border_side_t)sides, 0);
    }
    return screen;
}
