/**
 * @file permissions.h
 * @brief What the logged-in role may do (see Roles.h): 0 = hidden, 1 = view only, 2 = full control.
 *        With no roles configured everything is 2.
 */

#ifndef PERMISSIONS_H
#define PERMISSIONS_H

#include "tile.h"
#include <string>

#define PERM_HIDDEN 0
#define PERM_VIEW   1
#define PERM_FULL   2

/** A global permission (display, advanced, reboot, bluetooth-admin, bluetooth-control, streaming-admin,
 *  streaming-control). */
int perm_global(const char *key);

/** One key of a tile's permissions ("default", or e.g. "input-source-select"); unlisted keys fall back to
 *  "default". */
int perm_tile(const std::string &tileId, const char *key = "default");

/** The level of a whole tile, for showing it: hidden at 0; view only (lock icon, no taps) at 1.
 *  An input tile is the higher of its two controls; Bluetooth / streaming tiles are also limited by the
 *  bluetooth-control / streaming-control global. */
int perm_tile_level(tile_type_t type, const std::string &tileId);

/** bluetooth-control for "/bluetooth", streaming-control for the streaming sources' leaves. */
int perm_source_control(const std::string &leafPath);

/** The logged-in role's name, "" if none. */
std::string perm_role_name();

/** A small padlock (@p size px tall) in the icon colour, for view-only controls. The caller positions it. */
lv_obj_t *perm_lock_icon_create(lv_obj_t *parent, int size);

/** A padlock in the (empty) right slot of a page's header (see header.h): the page is view only. */
lv_obj_t *perm_header_lock_create(lv_obj_t *screen);

#endif /* PERMISSIONS_H */
