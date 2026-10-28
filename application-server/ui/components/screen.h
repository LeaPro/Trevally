/**
 * @file screen.h
 * @brief One screen of tiles. The screen decides each tile's size and
 *        position from how many tiles it holds; what is inside a tile is the
 *        tile type's business (see tile.h).
 *
 * Tile count -> layout (screen is ui_content_w() x ui_pane_h(), see ui_design_tokens.h;
 *  a division with a remainder gives the extra pixel(s) to the last row / column):
 *   1  full screen
 *   2  two full-width rows
 *   3  one full-width row on top, two columns below
 *   4  2 x 2
 *   5-6  2 columns x 3 rows
 */

#ifndef SCREEN_COMPONENT_H
#define SCREEN_COMPONENT_H

#include "lvgl/lvgl.h"
#include <object.h>
#include <string>

#define SCREEN_MAX_TILES 6

/**
 * @brief Build a screen from one Screens.config entry
 *        ({ "name": ..., "tileOrder": [tileId, ...] }). Tiles are looked up
 *        in the Tiles leaf; unknown ids/types are skipped and anything past
 *        SCREEN_MAX_TILES is ignored.
 */
lv_obj_t *screen_create(lv_obj_t *parent, const json &screenConfig);

/** The screen's display name ("" if unset). */
std::string screen_name(const json &screenConfig);

#endif /* SCREEN_COMPONENT_H */
