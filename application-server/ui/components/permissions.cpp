/**
 * @file permissions.cpp
 * @brief See permissions.h.
 */

#include "permissions.h"
#include "../ui_design_tokens.h"

#include <algorithm>
#include <memory>

#include <object.h>
#include <Roles.h>
/* The device's root object (defined in main.cpp). */
extern std::shared_ptr<RootObject> root;

static std::shared_ptr<Roles> roles()
{
    return std::dynamic_pointer_cast<Roles>(root->leafFromPath("/roles"));
}

int perm_global(const char *key)
{
    auto r = roles();
    return r ? r->globalLevel(key) : PERM_FULL;
}

int perm_tile(const std::string &tileId, const char *key)
{
    auto r = roles();
    return r ? r->tileLevel(tileId, key) : PERM_FULL;
}

int perm_source_control(const std::string &leafPath)
{
    return perm_global(leafPath == "/bluetooth" ? "bluetooth-control" : "streaming-control");
}

int perm_tile_level(tile_type_t type, const std::string &tileId)
{
    switch (type) {
    case INPUT:
        return std::max(perm_tile(tileId, "input-source-select"), perm_tile(tileId, "input-volume-control"));
    case BLUETOOTH:
        return std::min(perm_tile(tileId), perm_global("bluetooth-control"));
    case STREAMING:
        return std::min(perm_tile(tileId), perm_global("streaming-control"));
    default:
        return perm_tile(tileId);
    }
}

std::string perm_role_name()
{
    auto r = roles();
    return r ? r->activeRoleName() : std::string();
}

lv_obj_t *perm_lock_icon_create(lv_obj_t *parent, int size)
{
    /* a shackle (arc) over a body (rounded rectangle), drawn rather than an image so it scales and themes */
    const int w = size * 4 / 5, bodyH = size * 3 / 5, shackleW = w * 5 / 8, stroke = std::max(2, size / 8);
    lv_obj_t *lock = lv_obj_create(parent);
    lv_obj_remove_style_all(lock);
    lv_obj_remove_flag(lock, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(lock, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(lock, w, size);

    lv_obj_t *shackle = lv_arc_create(lock);
    lv_obj_remove_style_all(shackle);
    lv_obj_remove_flag(shackle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(shackle, shackleW, shackleW);
    lv_obj_align(shackle, LV_ALIGN_TOP_MID, 0, 0);
    lv_arc_set_bg_angles(shackle, 180, 360);
    lv_arc_set_value(shackle, 0);
    lv_obj_set_style_arc_color(shackle, ui_get_text_secondary_color(), LV_PART_MAIN);
    lv_obj_set_style_arc_width(shackle, stroke, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(shackle, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(shackle, true, LV_PART_MAIN);

    lv_obj_t *legs = lv_obj_create(lock);   /* the shackle's straight sides down into the body */
    lv_obj_remove_style_all(legs);
    lv_obj_set_size(legs, shackleW, size - bodyH - shackleW / 2 + 2);
    lv_obj_align(legs, LV_ALIGN_TOP_MID, 0, shackleW / 2);
    lv_obj_set_style_border_side(legs, (lv_border_side_t)(LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT), 0);
    lv_obj_set_style_border_width(legs, stroke, 0);
    lv_obj_set_style_border_color(legs, ui_get_text_secondary_color(), 0);
    lv_obj_set_style_border_opa(legs, LV_OPA_COVER, 0);

    lv_obj_t *body = lv_obj_create(lock);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, w, bodyH);
    lv_obj_align(body, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(body, std::max(1, size / 8), 0);
    lv_obj_set_style_bg_color(body, ui_get_text_secondary_color(), 0);
    lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
    return lock;
}

lv_obj_t *perm_header_lock_create(lv_obj_t *screen)
{
    const int size = 20;
    lv_obj_t *lock = perm_lock_icon_create(screen, size);
    lv_obj_align(lock, LV_ALIGN_TOP_LEFT, ui_content_w() - UI_HEADER_H_PX / 2 - lv_obj_get_width(lock) / 2,
                 (UI_HEADER_H_PX - size) / 2);
    return lock;
}
