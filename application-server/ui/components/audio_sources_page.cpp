/**
 * @file audio_sources_page.cpp
 * @brief See audio_sources_page.h.
 */

#include "audio_sources_page.h"
#include "audio_source.h"
#include "header.h"
#include "nav.h"
#include "picker_wheel.h"
#include "tile.h"
#include "../ui_design_tokens.h"

#include <memory>
#include <string>
#include <vector>

#define BLUETOOTH_ROW_NAME "Bluetooth"
#define BLUETOOTH_PATH     "/bluetooth"

/* What the picker shows. A null source is the Bluetooth row. */
struct SourcesView {
    lv_obj_t                        *picker = nullptr;
    bool                             streamingOnly = false;   /* the Streaming settings page: no Bluetooth row */
    std::vector<const AudioSource *> rows;
    std::string                      checked;   /* leaf path of the enabled source, "" if none */
};

/* Bluetooth first (unless @p streamingOnly), then the streaming services that are paired or configured (yellow or green). */
static std::vector<const AudioSource *> current_rows(bool streamingOnly)
{
    std::vector<const AudioSource *> rows;
    if (!streamingOnly) rows.push_back(nullptr);
    for (const AudioSource *s : audio_source_streaming_sources())
        if (audio_source_state(s->path) != AUDIO_SOURCE_GRAY) rows.push_back(s);
    return rows;
}

static std::string enabled_path()
{
    const AudioSource *active = audio_source_active();
    return active ? active->path : std::string();
}

static std::vector<picker_wheel_item_t> make_items(const SourcesView &v)
{
    std::vector<picker_wheel_item_t> items(v.rows.empty() ? 1 : v.rows.size());
    if (v.rows.empty()) {   /* only on the Streaming page with nothing configured */
        items[0].primary_text       = "No streaming services";
        items[0].primary_text_align = LV_TEXT_ALIGN_CENTER;
        return items;
    }
    for (size_t i = 0; i < items.size(); i++) {
        const AudioSource *s = v.rows[i];
        items[i].primary_text       = s ? s->name : BLUETOOTH_ROW_NAME;
        items[i].primary_text_align = LV_TEXT_ALIGN_CENTER;
        items[i].accessory = v.checked == (s ? std::string(s->path) : std::string(BLUETOOTH_PATH))
                           ? PICKER_WHEEL_ITEM_ACCESSORY_CHECK
                           : PICKER_WHEEL_ITEM_ACCESSORY_NONE;
    }
    return items;
}

/* Rebuilds the rows in place when the services or the enabled source changed, keeping the selection. */
static void refresh_view(SourcesView &v)
{
    const std::vector<const AudioSource *> rows = current_rows(v.streamingOnly);
    const std::string checked = enabled_path();
    if (rows == v.rows && checked == v.checked) return;

    const int selected = picker_wheel_get_selected(v.picker);
    const AudioSource *selectedSource = (selected >= 0 && selected < (int)v.rows.size()) ? v.rows[selected] : nullptr;
    const bool hadSelection = selected >= 0 && selected < (int)v.rows.size();

    v.rows    = rows;
    v.checked = checked;

    int newSelected = selected < 0 ? 0 : selected;
    if (hadSelection)
        for (size_t i = 0; i < v.rows.size(); i++)
            if (v.rows[i] == selectedSource) newSelected = (int)i;

    const std::vector<picker_wheel_item_t> items = make_items(v);
    picker_wheel_set_items(v.picker, items.data(), (int)items.size(), newSelected);
}

/* "Couldn't connect" page: no Bluetooth device is connected. One button goes to the paired devices page;
 * back from there returns to the sources page. Touch taps the button, an encoder press activates it. */
struct NoDeviceCtx {
    bool done = false;   /* guards against a second activation */
};

static void no_device_go_cb(lv_event_t *e)
{
    NoDeviceCtx *ctx = static_cast<NoDeviceCtx *>(lv_event_get_user_data(e));
    if (ctx->done) return;
    ctx->done = true;
    nav_pop();
    tile_bluetooth_devices_open(BLUETOOTH_ROW_NAME);
}

static void no_device_ctx_delete_cb(lv_event_t *e)
{
    delete static_cast<NoDeviceCtx *>(lv_event_get_user_data(e));
}

static void no_device_page_open()
{
    NoDeviceCtx *ctx = new NoDeviceCtx();

    nav_page_t page = nav_screen_create();
    const header_slot_t back = {true, UI_ICON_ARROW_BACK, nav_back_cb, nullptr};
    const header_slot_t none = {false, UI_ICON_SETTINGS, nullptr, nullptr};
    header_create(page.screen, back, BLUETOOTH_ROW_NAME, none);

    const int btnH = 72;
    const int gap  = 16;
    const int lineH = lv_font_get_line_height(&satoshi_medium_24);
    const int blockH = lineH + gap + btnH;
    const int top = UI_HEADER_H_PX + (UI_HEADER_H_PX + ui_pane_h() - blockH) / 2;

    lv_obj_t *headline = lv_label_create(page.screen);
    lv_label_set_text(headline, "Couldn't connect");
    lv_obj_set_style_text_font(headline, &satoshi_medium_24, 0);
    lv_obj_set_style_text_color(headline, UI_COLOR_STATUS_ERROR, 0);
    lv_obj_set_style_text_align(headline, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(headline, ui_content_w());
    lv_obj_align(headline, LV_ALIGN_TOP_LEFT, 0, top);

    lv_obj_t *btn = lv_obj_create(page.screen);
    lv_obj_remove_style_all(btn);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(btn, ui_theme_get_style_brand(), 0);
    lv_obj_set_size(btn, ui_content_w() - 48, btnH);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 24, top + lineH + gap);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(btn, OPACITY_BRAND_BG_LIGHT, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_opa(btn, LV_OPA_COVER, 0);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "Go to Bluetooth pairing");
    lv_obj_add_style(label, ui_theme_get_style_brand(), 0);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_font(label, &satoshi_medium_24, 0);
    lv_obj_center(label);

    lv_obj_add_event_cb(btn, no_device_go_cb, LV_EVENT_CLICKED, ctx);
    lv_obj_add_event_cb(page.focus, no_device_go_cb, LV_EVENT_CLICKED, ctx);
    lv_obj_add_event_cb(page.screen, no_device_ctx_delete_cb, LV_EVENT_DELETE, ctx);
    nav_push(page);
}

static void activated_cb(int index, void *user_data)
{
    SourcesView &v = **static_cast<std::shared_ptr<SourcesView> *>(user_data);
    if (index < 0 || index >= (int)v.rows.size()) return;

    const AudioSource *source = v.rows[index];
    if (!source) {
        if (v.checked == BLUETOOTH_PATH) {   /* tapping the enabled source disables it */
            audio_source_set_enabled(BLUETOOTH_PATH, false);
        } else if (!tile_read_string(BLUETOOTH_PATH, "connectedDevice").empty()) {
            audio_source_set_enabled(BLUETOOTH_PATH, true);
        } else {
            no_device_page_open();
            return;
        }
        refresh_view(v);
        return;
    }
    audio_source_set_enabled(source->path, v.checked != source->path);   /* tapping the enabled service disables it */
    refresh_view(v);
}

static void view_delete_cb(lv_event_t *e)
{
    delete static_cast<std::shared_ptr<SourcesView> *>(lv_event_get_user_data(e));
}

static void page_key_cb(lv_event_t *e)
{
    lv_obj_t *picker = (lv_obj_t *)lv_event_get_user_data(e);
    const uint32_t key = lv_event_get_key(e);
    if (key == LV_KEY_RIGHT)     picker_wheel_nudge(picker, 1, false);
    else if (key == LV_KEY_LEFT) picker_wheel_nudge(picker, -1, false);
}

static void page_click_cb(lv_event_t *e)
{
    picker_wheel_click(static_cast<lv_obj_t *>(lv_event_get_user_data(e)));
}

static void sources_page_open(bool streamingOnly)
{
    auto view = std::make_shared<SourcesView>();
    view->streamingOnly = streamingOnly;
    view->rows    = current_rows(streamingOnly);
    view->checked = enabled_path();

    auto *holder = new std::shared_ptr<SourcesView>(view);

    nav_page_t page = nav_screen_create();
    const header_slot_t back = {true, UI_ICON_ARROW_BACK, nav_back_cb, nullptr};
    const header_slot_t none = {false, UI_ICON_SETTINGS, nullptr, nullptr};
    header_create(page.screen, back, streamingOnly ? "Streaming" : "Bluetooth & Streaming", none);

    std::vector<picker_wheel_item_t> items = make_items(*view);

    picker_wheel_props_t props = {};
    props.type               = PICKER_WHEEL_TYPE_CENTER_CHECK;
    props.items              = items.data();
    props.item_count         = (int)items.size();
    props.selected_index     = 0;
    props.activated_callback = activated_cb;
    props.user_data          = holder;
    view->picker = picker_wheel_create(page.screen, &props);
    lv_obj_t *picker = view->picker;
    lv_obj_align(picker, LV_ALIGN_TOP_LEFT, 0, UI_HEADER_H_PX);
    lv_obj_add_event_cb(picker, view_delete_cb, LV_EVENT_DELETE, holder);

    tile_bind_update(picker, [view]() { refresh_view(*view); });

    lv_obj_add_event_cb(page.focus, page_key_cb, LV_EVENT_KEY, picker);
    lv_obj_add_event_cb(page.focus, page_click_cb, LV_EVENT_CLICKED, picker);
    nav_push(page);
}

void audio_sources_page_open()           { sources_page_open(false); }
void audio_sources_streaming_page_open() { sources_page_open(true); }
