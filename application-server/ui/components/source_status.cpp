/**
 * @file source_status.cpp
 * @brief See source_status.h.
 */

#include "source_status.h"
#include "audio_source.h"
#include "tile.h"
#include "../ui_design_tokens.h"

#include <memory>

#define SWITCH_W_PX   64
#define SWITCH_H_PX   36
#define DOT_GAP_PX    16   /* between the switch and its dot */
#define MARGIN_PX     24

/* The colors are fixed (the same in every theme), see ui_design_tokens.h. */
static void apply_state(lv_obj_t *dot, AudioSourceState state)
{
    switch (state) {
    case AUDIO_SOURCE_GREEN:  lv_obj_set_style_bg_color(dot, UI_COLOR_TEXT_SUCCESS, 0); break;
    case AUDIO_SOURCE_YELLOW: lv_obj_set_style_bg_color(dot, UI_COLOR_STATUS_WARNING, 0); break;
    default:                  lv_obj_set_style_bg_color(dot, UI_COLOR_STATUS_INACTIVE, 0); break;
    }
}

lv_obj_t *source_status_dot_create(lv_obj_t *parent, const std::string &leafPath)
{
    lv_obj_t *dot = tile_container(parent);
    lv_obj_set_size(dot, SOURCE_DOT_PX, SOURCE_DOT_PX);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);

    auto shown = std::make_shared<AudioSourceState>(audio_source_state(leafPath));
    apply_state(dot, *shown);
    /* The state lives behind a shared_ptr: tile_live_update() runs a copy of the callback, so a captured value
     * would never see its own update. */
    tile_bind_update(dot, [dot, leafPath, shown]() {
        const AudioSourceState now = audio_source_state(leafPath);
        if (now == *shown) return;
        *shown = now;
        apply_state(dot, now);
    });
    return dot;
}

#define DISABLED_TILE_OPA LV_OPA_40

void source_tile_dim_bind(lv_obj_t *tile, const std::string &leafPath)
{
    if (leafPath.empty()) {   /* a binding that names no source: always dimmed */
        lv_obj_set_style_opa(tile, DISABLED_TILE_OPA, 0);
        return;
    }

    auto shown = std::make_shared<bool>(tile_read_bool(leafPath, "enable"));
    lv_obj_set_style_opa(tile, *shown ? LV_OPA_COVER : DISABLED_TILE_OPA, 0);
    tile_bind_update(tile, [tile, leafPath, shown]() {   /* shared_ptr: see source_status_dot_create() */
        const bool now = tile_read_bool(leafPath, "enable");
        if (now == *shown) return;
        *shown = now;
        lv_obj_set_style_opa(tile, now ? LV_OPA_COVER : DISABLED_TILE_OPA, 0);
    });
}

struct EnableCtx {
    std::string path;
    lv_obj_t   *sw;
};

/* Touch or an encoder press changed the switch: enable this source (and so disable the others) or disable it. */
static void switch_changed_cb(lv_event_t *e)
{
    const EnableCtx *ctx = static_cast<const EnableCtx *>(lv_event_get_user_data(e));
    audio_source_set_enabled(ctx->path, lv_obj_has_state(ctx->sw, LV_STATE_CHECKED));
}

static void page_click_cb(lv_event_t *e)
{
    const EnableCtx *ctx = static_cast<const EnableCtx *>(lv_event_get_user_data(e));
    if (lv_obj_has_state(ctx->sw, LV_STATE_DISABLED)) return;
    if (lv_obj_has_state(ctx->sw, LV_STATE_CHECKED)) lv_obj_remove_state(ctx->sw, LV_STATE_CHECKED);
    else                                             lv_obj_add_state(ctx->sw, LV_STATE_CHECKED);
    lv_obj_send_event(ctx->sw, LV_EVENT_VALUE_CHANGED, nullptr);
}

static void ctx_delete_cb(lv_event_t *e)
{
    delete static_cast<EnableCtx *>(lv_event_get_user_data(e));
}

static lv_obj_t *make_switch(lv_obj_t *parent)
{
    lv_obj_t *sw = lv_switch_create(parent);
    lv_obj_remove_style_all(sw);
    lv_group_remove_obj(sw);   /* encoder rotation and presses go to the page's focus object, not the switch */
    lv_obj_set_size(sw, SWITCH_W_PX, SWITCH_H_PX);

    /* The track is gray while the source is off (the same fixed gray as the status dot) and covered by the
     * brand colored indicator while it is on. */
    lv_obj_set_style_bg_color(sw, UI_COLOR_STATUS_INACTIVE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_MAIN);

    lv_obj_add_style(sw, ui_theme_get_style_brand(), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);

    lv_obj_add_style(sw, ui_theme_get_style_bg_icon_primary(), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(sw, -3, LV_PART_KNOB);   /* a knob a little smaller than the track */
    return sw;
}

void source_enable_control_create(lv_obj_t *screen, lv_obj_t *focus, const std::string &leafPath)
{
    lv_obj_t *row = tile_container(screen);
    lv_obj_set_size(row, SWITCH_W_PX + DOT_GAP_PX + SOURCE_DOT_PX, SWITCH_H_PX);
    if (ui_is_landscape())
        lv_obj_set_pos(row, MARGIN_PX, UI_HEADER_H_PX + ui_pane_h() - MARGIN_PX - SWITCH_H_PX);
    else
        lv_obj_set_pos(row, MARGIN_PX, UI_HEADER_H_PX + MARGIN_PX);

    lv_obj_t *sw = make_switch(row);
    lv_obj_align(sw, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *dot = source_status_dot_create(row, leafPath);
    lv_obj_align(dot, LV_ALIGN_RIGHT_MID, 0, 0);

    if (leafPath.empty()) lv_obj_add_state(sw, LV_STATE_DISABLED);   /* a binding that names no source */

    if (tile_read_bool(leafPath, "enable")) lv_obj_add_state(sw, LV_STATE_CHECKED);

    EnableCtx *ctx = new EnableCtx{leafPath, sw};
    lv_obj_add_event_cb(sw, switch_changed_cb, LV_EVENT_VALUE_CHANGED, ctx);
    lv_obj_add_event_cb(focus, page_click_cb, LV_EVENT_CLICKED, ctx);
    lv_obj_add_event_cb(screen, ctx_delete_cb, LV_EVENT_DELETE, ctx);

    /* follow the element: it can also change from the websocket or from another source being enabled */
    tile_bind_update(sw, [sw, leafPath]() {
        const bool enabled = tile_read_bool(leafPath, "enable");
        if (enabled == lv_obj_has_state(sw, LV_STATE_CHECKED)) return;
        if (enabled) lv_obj_add_state(sw, LV_STATE_CHECKED);
        else         lv_obj_remove_state(sw, LV_STATE_CHECKED);
    });
}
