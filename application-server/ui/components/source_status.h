/**
 * @file source_status.h
 * @brief The audio source widgets: a status dot (gray / yellow / green, see audio_source.h) and, on a source's
 *        volume page, the enable switch with its dot.
 */

#ifndef SOURCE_STATUS_H
#define SOURCE_STATUS_H

#include "lvgl/lvgl.h"
#include <functional>
#include <string>

#define SOURCE_DOT_PX 16

/**
 * A dot that follows the state of the audio source at @p leafPath (gray for a path that is not a source).
 * The caller positions it. The parent must already be on its screen: the dot's binding is dropped with
 * that screen by tile_live_clear().
 */
lv_obj_t *source_status_dot_create(lv_obj_t *parent, const std::string &leafPath);

/**
 * Dims @p tile (the whole tile, children included) while the audio source at @p leafPath is not enabled; an
 * empty path (a binding that names no source) is always dimmed. Same screen requirement as
 * source_status_dot_create().
 */
void source_tile_dim_bind(lv_obj_t *tile, const std::string &leafPath);

/**
 * The enable switch and its dot, in the upper left below the header (portrait) or the lower left (landscape).
 * The switch follows the source's enable element; using it enables this source and disables all others
 * (audio_source_set_enabled()). An encoder press on @p focus (the page's focus object) toggles it. The switch
 * is disabled when @p leafPath is empty (a binding that names no source, see audio_source_leaf_path()).
 * @p nameFn, when given, supplies a name (the connected device / the service) shown to the right of the dot; it is
 * polled, so it can change while the page is open, and an empty result hides the name.
 */
void source_enable_control_create(lv_obj_t *screen, lv_obj_t *focus, const std::string &leafPath,
                                  std::function<std::string()> nameFn = nullptr);

/**
 * A row just below the header with "On" / "Off" on the left and a switch on the right, bound to the bool element
 * @p path / @p element: the switch follows the element (it can also change from the websocket) and touching it
 * writes it. Same screen requirement as source_status_dot_create(). It overlays the top of the page's content.
 */
lv_obj_t *toggle_row_create(lv_obj_t *screen, const std::string &path, const std::string &element);

#endif /* SOURCE_STATUS_H */
