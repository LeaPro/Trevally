/**
 * @file tile_streaming.cpp
 * @brief Streaming tile: title, volume slider, left-aligned percentage (same
 *        layout as the output-volume tile) and a status dot in the top right.
 *        Reads /<source named by the binding>/volume (e.g. binding "Soundtrack" ->
 *        /soundtrack/volume, see audio_source_leaf_path()). Tapping it opens a page with
 *        the header, level gauge and the source's enable switch, where the encoder
 *        changes the volume. The gear opens a picker of the paired / configured services;
 *        choosing one binds the tile to it.
 */

#include "tile.h"
#include "header.h"
#include "level_gauge.h"
#include "nav.h"
#include "permissions.h"
#include "picker_wheel.h"
#include "audio_source.h"
#include "source_status.h"
#include "../ui_design_tokens.h"

#include <memory>
#include <vector>

static std::string streaming_leaf_path(const std::string &binding)
{
    return audio_source_leaf_path(binding);
}

static lv_obj_t *make_slider(lv_obj_t *parent, int w, float fill)
{
    lv_obj_t *s = lv_slider_create(parent);
    lv_obj_remove_style_all(s);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(s, w, 8);
    lv_slider_set_range(s, 0, 1000);
    lv_slider_set_value(s, (int32_t)(fill * 1000.0f), LV_ANIM_OFF);

    lv_obj_add_style(s, ui_theme_get_style_bg_tertiary(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s, 4, LV_PART_MAIN);

    lv_obj_add_style(s, ui_theme_get_style_brand(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s, 4, LV_PART_INDICATOR);

    lv_obj_add_style(s, ui_theme_get_style_brand(), LV_PART_KNOB);
    lv_obj_add_style(s, ui_theme_get_style_knob_border(), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s, 2, LV_PART_KNOB);
    lv_obj_set_style_border_width(s, 2, LV_PART_KNOB);
    lv_obj_set_style_border_opa(s, LV_OPA_COVER, LV_PART_KNOB);
    return s;
}

void tile_streaming_build(lv_obj_t *root, const std::string &name,
                          const std::string &binding, int w, int h)
{
    const bool tall = h >= ui_pane_h();
    const bool wide = w >= ui_content_w();
    const int  pad  = tall ? 36 : 16;

    const std::string leafPath = streaming_leaf_path(binding);
    TileNumber volume = tile_read_number(leafPath, "volume");

    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root,
                          tall ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(root, pad, 0);
    lv_obj_set_style_pad_gap(root, tall ? 24 : 0, 0);

    tile_adaptive_label(root, name, &satoshi_medium_24, ui_theme_get_style_text_primary(),
                        w - 2 * pad - SOURCE_DOT_PX - 8);

    lv_obj_t *dot = source_status_dot_create(root, leafPath);
    source_tile_dim_bind(root, leafPath);
    lv_obj_add_flag(dot, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_align(dot, LV_ALIGN_TOP_RIGHT, 0, (lv_font_get_line_height(&satoshi_medium_24) - SOURCE_DOT_PX) / 2);

    lv_obj_t *group = tile_container(root);
    lv_obj_set_layout(group, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(group, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(group, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_size(group, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(group, 12, 0);

    lv_obj_t *slider = make_slider(group, w - 2 * pad, volume.fill);

    lv_obj_t *row = tile_container(group);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(row, 4, 0);

    const bool unbound = leafPath.empty();   /* a binding that names no source (or Default with no service enabled) */
    lv_obj_t *valueLabel = tile_label(row, unbound ? std::string("-") : tile_format_number(volume.value),
               wide ? &satoshi_medium_48 : &satoshi_medium_32, ui_theme_get_style_brand());
    if (!unbound)
        tile_label(row, volume.units.empty() ? "%" : volume.units,
                   wide ? &satoshi_medium_32 : &satoshi_medium_24, ui_theme_get_style_brand());

    tile_bind_number(valueLabel, slider, leafPath, "volume");
}

/* ---- Service page (gear on the volume page) ----
 *
 * A picker of "Default" (the tile follows whichever streaming service is enabled on the cast or Streaming
 * settings page) and the streaming services that are paired or configured (yellow or green). The row the
 * tile is bound to has a check. Choosing a row binds the tile to it. The volume page below was built for
 * the old binding, so leaving this page after a change rebuilds it. */

struct ServicesView
{
    std::string                      name;           /* tile name, for the rebuilt volume page */
    std::string                      tileId;
    std::string                      openedBinding;  /* the binding the volume page below was built for */
    lv_obj_t                        *picker = nullptr;
    std::vector<const AudioSource *> rows;           /* in display order; nullptr is the Default row */
    const AudioSource               *checked = nullptr;   /* the checked row's source, nullptr for Default */
};

static std::vector<const AudioSource *> service_rows()
{
    std::vector<const AudioSource *> rows = {nullptr};
    for (const AudioSource *s : audio_source_streaming_sources())
        if (audio_source_state(s->path) != AUDIO_SOURCE_GRAY) rows.push_back(s);
    return rows;
}

/* The source the binding names; nullptr for the default binding (and for one that names nothing). */
static const AudioSource *bound_source(const std::string &binding)
{
    if (audio_source_binding_is_default(binding)) return nullptr;
    const std::string path = audio_source_leaf_path(binding);
    for (const AudioSource *s : audio_source_streaming_sources())
        if (path == s->path) return s;
    return nullptr;
}

static std::vector<picker_wheel_item_t> make_service_items(const ServicesView &v)
{
    std::vector<picker_wheel_item_t> items(v.rows.size());
    for (size_t i = 0; i < items.size(); i++)
    {
        items[i].primary_text       = v.rows[i] ? v.rows[i]->name : "<default>";
        items[i].primary_text_align = LV_TEXT_ALIGN_CENTER;
        items[i].accessory = v.rows[i] == v.checked ? PICKER_WHEEL_ITEM_ACCESSORY_CHECK
                                                    : PICKER_WHEEL_ITEM_ACCESSORY_NONE;
    }
    return items;
}

/* Rebuilds the rows in place when the services or the tile's binding changed, keeping the selection. */
static void refresh_services_view(ServicesView &v)
{
    const std::vector<const AudioSource *> rows = service_rows();
    const AudioSource *checked = bound_source(tile_binding_read(v.tileId));
    if (rows == v.rows && checked == v.checked) return;

    const int selected = picker_wheel_get_selected(v.picker);
    const bool hadSelection = selected >= 0 && selected < (int)v.rows.size();
    const AudioSource *selectedSource = hadSelection ? v.rows[selected] : nullptr;

    v.rows    = rows;
    v.checked = checked;

    int newSelected = hadSelection ? selected : 0;
    if (hadSelection)
        for (size_t i = 0; i < v.rows.size(); i++)
            if (v.rows[i] == selectedSource) newSelected = (int)i;

    const std::vector<picker_wheel_item_t> items = make_service_items(v);
    picker_wheel_set_items(v.picker, items.data(), (int)items.size(), newSelected);
}

static void services_activated_cb(int index, void *user_data)
{
    ServicesView &v = **static_cast<std::shared_ptr<ServicesView> *>(user_data);
    if (index < 0 || index >= (int)v.rows.size()) return;
    if (perm_global("streaming-admin") < PERM_FULL) return;   /* view only for the logged-in role */
    const AudioSource *source = v.rows[index];
    tile_binding_set(v.tileId, source ? std::string(source->path + 1)   /* the leaf's name, without the slash */
                                      : std::string(AUDIO_SOURCE_DEFAULT_BINDING));
    refresh_services_view(v);
}

/* Back: when the binding changed, the volume page below is replaced by one for the new service. */
static void services_back_cb(lv_event_t *e)
{
    const ServicesView &v = **static_cast<std::shared_ptr<ServicesView> *>(lv_event_get_user_data(e));
    const std::string name = v.name, tileId = v.tileId, current = tile_binding_read(v.tileId);
    if (current == v.openedBinding)
    {
        nav_pop();
        return;
    }
    nav_pop(2);   /* deletes this page and its view */
    tile_streaming_volume_open(name, current, tileId);
}

static void services_view_delete_cb(lv_event_t *e)
{
    delete static_cast<std::shared_ptr<ServicesView> *>(lv_event_get_user_data(e));
}

static void streaming_services_open(const std::string &name, const std::string &tileId,
                                    const std::string &openedBinding)
{
    auto view = std::make_shared<ServicesView>();
    view->name          = name;
    view->tileId        = tileId;
    view->openedBinding = openedBinding;
    view->rows          = service_rows();
    view->checked       = bound_source(tile_binding_read(tileId));

    auto *holder = new std::shared_ptr<ServicesView>(view);

    nav_page_t page = nav_screen_create();
    const header_slot_t back = {true, UI_ICON_ARROW_BACK, services_back_cb, holder};
    const header_slot_t none = {false, UI_ICON_SETTINGS, nullptr, nullptr};
    header_create(page.screen, back, "Services", none);

    std::vector<picker_wheel_item_t> items = make_service_items(*view);
    int selected = 0;
    for (size_t i = 0; i < view->rows.size(); i++)
        if (view->rows[i] == view->checked) selected = (int)i;

    picker_wheel_props_t props = {};
    props.type               = PICKER_WHEEL_TYPE_CENTER_CHECK;
    props.items              = items.data();
    props.item_count         = (int)items.size();
    props.selected_index     = selected;
    props.activated_callback = services_activated_cb;
    props.user_data          = holder;
    view->picker = picker_wheel_create(page.screen, &props);
    lv_obj_t *picker = view->picker;
    lv_obj_align(picker, LV_ALIGN_TOP_LEFT, 0, UI_HEADER_H_PX);
    lv_obj_add_event_cb(picker, services_view_delete_cb, LV_EVENT_DELETE, holder);

    tile_bind_update(picker, [view]() { refresh_services_view(*view); });

    picker_wheel_attach_encoder(picker, page.focus);
    nav_push(page);
}

/* ---- Volume page ---- */

struct StreamingPageCtx
{
    std::string leafPath;
    std::string name;
    std::string tileId;
    std::string binding;   /* what the page was built for */
};

/* Encoder rotation on the page changes the volume by one per tick; a press toggles the enable switch
 * (source_enable_control_create()). */
static void streaming_page_key_cb(lv_event_t *e)
{
    const StreamingPageCtx *ctx = static_cast<const StreamingPageCtx *>(lv_event_get_user_data(e));
    const uint32_t key = lv_event_get_key(e);
    const int step = (key == LV_KEY_RIGHT) ? 1 : (key == LV_KEY_LEFT) ? -1 : 0;
    if (step != 0)
        tile_step_number(ctx->leafPath, "volume", step);
}

static void streaming_gear_cb(lv_event_t *e)
{
    const StreamingPageCtx *ctx = static_cast<const StreamingPageCtx *>(lv_event_get_user_data(e));
    streaming_services_open(ctx->name, ctx->tileId, ctx->binding);
}

static void streaming_page_ctx_delete_cb(lv_event_t *e)
{
    delete static_cast<StreamingPageCtx *>(lv_event_get_user_data(e));
}

void tile_streaming_volume_open(const std::string &name, const std::string &binding,
                                const std::string &tileId)
{
    StreamingPageCtx *ctx = new StreamingPageCtx{streaming_leaf_path(binding), name, tileId, binding};

    nav_page_t page = nav_screen_create();

    const header_slot_t back = {true, UI_ICON_ARROW_BACK, nav_back_cb, nullptr};
    const header_slot_t gear = {perm_global("streaming-admin") != PERM_HIDDEN, UI_ICON_SETTINGS, streaming_gear_cb, ctx};
    header_create(page.screen, back, name.c_str(), gear);

    level_gauge_create(page.screen, ctx->leafPath, "volume");
    source_enable_control_create(page.screen, page.focus, ctx->leafPath, [path = ctx->leafPath]() {
        for (const AudioSource *s : audio_source_streaming_sources())
            if (path == s->path) return std::string(s->name);
        return std::string();
    });

    lv_obj_add_event_cb(page.focus, streaming_page_key_cb, LV_EVENT_KEY, ctx);
    lv_obj_add_event_cb(page.screen, streaming_page_ctx_delete_cb, LV_EVENT_DELETE, ctx);
    nav_push(page);
}
