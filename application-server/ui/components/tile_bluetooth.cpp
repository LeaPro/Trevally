/**
 * @file tile_bluetooth.cpp
 * @brief Bluetooth tile: title, volume slider above a left-aligned percentage.
 *        Reads /bluetooth/volume. Tapping it opens a page with the header and
 *        level gauge, where the encoder changes the volume.
 */

#include "tile.h"
#include "header.h"
#include "level_gauge.h"
#include "picker_wheel.h"
#include "nav.h"
#include "../ui_design_tokens.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <vector>

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

void tile_bluetooth_build(lv_obj_t *root, const std::string &name,
                          const std::string &binding, int w, int h)
{
    (void)binding;
    const bool tall = h >= ui_pane_h();
    const bool wide = w >= ui_content_w();
    const int  pad  = tall ? 36 : 16;

    TileNumber volume = tile_read_number("/bluetooth", "volume");

    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root,
                          tall ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(root, pad, 0);
    lv_obj_set_style_pad_gap(root, tall ? 24 : 0, 0);

    tile_adaptive_label(root, name, &satoshi_medium_24, ui_theme_get_style_text_primary(),
                        w - 2 * pad);

    lv_obj_t *group = tile_container(root);
    lv_obj_set_layout(group, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(group, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(group, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_size(group, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(group, 12, 0);

    lv_obj_t *slider = make_slider(group, w - 2 * pad, volume.fill);

    lv_obj_t *row = tile_container(group);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(row, 4, 0);

    lv_obj_t *valueLabel = tile_label(row, tile_format_number(volume.value),
               wide ? &satoshi_medium_48 : &satoshi_medium_32, ui_theme_get_style_brand());
    tile_label(row, volume.units.empty() ? "%" : volume.units,
               wide ? &satoshi_medium_32 : &satoshi_medium_24, ui_theme_get_style_brand());

    tile_bind_number(valueLabel, slider, "/bluetooth", "volume");
}

/* Encoder rotation on the page changes the volume by one per tick; a press is unmapped. */
static void bluetooth_page_key_cb(lv_event_t *e)
{
    const uint32_t key = lv_event_get_key(e);
    const int step = (key == LV_KEY_RIGHT) ? 1 : (key == LV_KEY_LEFT) ? -1 : 0;
    if (step != 0)
        tile_step_number("/bluetooth", "volume", step);
}

/* ---- Paired devices page (gear on the volume page) ---- */

struct PairedDevice
{
    std::string name;
    std::string mac;
    bool        connected = false;
};

/* Parses "Name (AA:BB:CC:DD:EE:FF), Name2 (11:22:...)" without splitting on commas inside names. */
static std::vector<PairedDevice> parse_paired_devices(const std::string &list)
{
    std::vector<PairedDevice> devices;
    size_t start = 0;
    for (size_t i = start; i + 19 <= list.size(); i++)
    {
        if (list[i] != '(' || list[i + 18] != ')') continue;
        bool isMac = true;
        for (int k = 0; k < 17 && isMac; k++)
        {
            const char c = list[i + 1 + k];
            isMac = (k % 3 == 2) ? c == ':' : isxdigit((unsigned char)c) != 0;
        }
        if (!isMac) continue;

        std::string name = list.substr(start, i - start);
        while (!name.empty() && name.back() == ' ') name.pop_back();
        devices.push_back({name, list.substr(i + 1, 17), false});

        start = i + 19;
        if (list.compare(start, 2, ", ") == 0) start += 2;
        i = start - 1;   /* loop increment lands on start */
    }
    return devices;
}

static std::string devicesPageTitle;

/* Paired:   list of paired devices; activating a row connects to it (Edit in the header).
 * Edit:     same list; activating a row opens its Forget Device page; plus in the header scans.
 * Discover: devices found by the scan; activating a row connects to it. */
enum class DevicesMode { Paired, Edit, Discover };

/* What the devices page currently shows (rows are in display order). */
struct DevicesView
{
    DevicesMode               mode = DevicesMode::Paired;
    lv_obj_t                 *picker = nullptr;
    std::string               list;         /* raw strings the rows were built from */
    std::string               connected;
    std::vector<PairedDevice> devices;
    bool                      done = false; /* a row was activated and the page is closing */
};

static std::string lower(std::string s)
{
    for (char &c : s) c = (char)tolower((unsigned char)c);
    return s;
}

/* MAC of the "Name (MAC)" a device element holds; empty if none. */
static std::string device_mac(const std::string &device)
{
    const std::vector<PairedDevice> parsed = parse_paired_devices(device);
    return parsed.empty() ? std::string() : parsed[0].mac;
}

static void read_devices_elements(DevicesMode mode, std::string &list, std::string &connected)
{
    list      = tile_read_string("/bluetooth", mode == DevicesMode::Discover ? "discoveredDevices" : "pairedDevices");
    connected = mode == DevicesMode::Discover ? std::string("-") : tile_read_string("/bluetooth", "connectedDevice");
}

/* Connected device first (like a phone's Bluetooth list), the rest alphabetical, so rows
 * don't jump around when bluetoothctl changes its output order. */
static void load_devices(DevicesView &v, const std::string &list, const std::string &connected)
{
    v.list      = list;
    v.connected = connected;
    v.devices   = list == "-" ? std::vector<PairedDevice>{} : parse_paired_devices(list);
    const std::string connectedMac = device_mac(connected);
    for (PairedDevice &d : v.devices)
        d.connected = !connectedMac.empty() && d.mac == connectedMac;
    std::stable_sort(v.devices.begin(), v.devices.end(),
                     [](const PairedDevice &a, const PairedDevice &b)
                     {
                         if (a.connected != b.connected) return a.connected;
                         return lower(a.name) < lower(b.name);
                     });
}

/* Picker rows for the view; the strings point into v.devices and must outlive the call that uses them. */
static std::vector<picker_wheel_item_t> make_device_items(const DevicesView &v)
{
    std::vector<picker_wheel_item_t> items(v.devices.empty() ? 1 : v.devices.size());
    for (size_t i = 0; i < items.size(); i++)
    {
        items[i].primary_text       = !v.devices.empty() ? v.devices[i].name.c_str()
                                    : v.mode == DevicesMode::Discover ? "No devices found"
                                                                      : "No paired devices";
        items[i].primary_text_align = LV_TEXT_ALIGN_CENTER;
        items[i].accessory = (!v.devices.empty() && v.devices[i].connected)
                                 ? PICKER_WHEEL_ITEM_ACCESSORY_CHECK
                                 : PICKER_WHEEL_ITEM_ACCESSORY_NONE;
    }
    return items;
}

/* Each UI tick: rebuild the rows in place if the device list or connected device changed,
 * keeping the same device selected when it is still there. */
static void refresh_devices_view(DevicesView &v)
{
    std::string list, connected;
    read_devices_elements(v.mode, list, connected);
    if (list == v.list && connected == v.connected) return;

    const int selected = picker_wheel_get_selected(v.picker);
    std::string selectedMac;
    if (selected >= 0 && selected < (int)v.devices.size())
        selectedMac = v.devices[selected].mac;

    load_devices(v, list, connected);

    int newSelected = selected < 0 ? 0 : selected;
    for (size_t i = 0; i < v.devices.size(); i++)
        if (v.devices[i].mac == selectedMac) newSelected = (int)i;

    const std::vector<picker_wheel_item_t> items = make_device_items(v);
    picker_wheel_set_items(v.picker, items.data(), (int)items.size(), newSelected);
}

static void devices_page_key_cb(lv_event_t *e)
{
    lv_obj_t *picker = (lv_obj_t *)lv_event_get_user_data(e);
    const uint32_t key = lv_event_get_key(e);
    if (key == LV_KEY_RIGHT)     picker_wheel_nudge(picker, 1, false);
    else if (key == LV_KEY_LEFT) picker_wheel_nudge(picker, -1, false);
}

/* ---- Forget Device page ---- */

struct ForgetCtx
{
    std::string name;
    std::string mac;
    bool        done = false;   /* guards against a second activation while the page animates out */
};

static void forget_cb(lv_event_t *e)
{
    ForgetCtx *ctx = static_cast<ForgetCtx *>(lv_event_get_user_data(e));
    if (ctx->done) return;
    ctx->done = true;
    tile_set_string("/bluetooth", "removeDevice", ctx->mac);
    nav_pop();
}

static void forget_ctx_delete_cb(lv_event_t *e)
{
    delete static_cast<ForgetCtx *>(lv_event_get_user_data(e));
}

/* A single Forget Device button; touch taps it, an encoder press activates it. */
static void forget_page_open(const PairedDevice &dev)
{
    static const lv_color_t errorColor = lv_color_hex(0xF97066);
    ForgetCtx *ctx = new ForgetCtx{dev.name, dev.mac, false};

    nav_page_t page = nav_screen_create();
    const header_slot_t back = {true, UI_ICON_ARROW_BACK, nav_back_cb, nullptr};
    const header_slot_t none = {false, UI_ICON_SETTINGS, nullptr, nullptr};
    header_create(page.screen, back, ctx->name.c_str(), none);

    const int btnH = 72;
    lv_obj_t *btn = lv_obj_create(page.screen);
    lv_obj_remove_style_all(btn);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(btn, ui_content_w() - 48, btnH);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, UI_HEADER_H_PX + (UI_HEADER_H_PX + ui_pane_h() - btnH) / 2);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_color(btn, errorColor, 0);
    lv_obj_set_style_border_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, errorColor, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_20, LV_STATE_PRESSED);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "Forget Device");
    lv_obj_set_style_text_font(label, &satoshi_medium_24, 0);
    lv_obj_set_style_text_color(label, errorColor, 0);
    lv_obj_center(label);

    lv_obj_add_event_cb(btn, forget_cb, LV_EVENT_CLICKED, ctx);
    lv_obj_add_event_cb(page.focus, forget_cb, LV_EVENT_CLICKED, ctx);
    lv_obj_add_event_cb(page.screen, forget_ctx_delete_cb, LV_EVENT_DELETE, ctx);
    nav_push(page);
}

/* ---- Pair New Device pages ---- */

/* "Paired! View devices": shown once the new device is connected; returns to the paired devices page
 * (the pages above it are this one and Edit). */
struct PairedDoneCtx
{
    bool done = false;   /* guards against a second activation while the page animates out */
};

static void paired_done_cb(lv_event_t *e)
{
    PairedDoneCtx *ctx = static_cast<PairedDoneCtx *>(lv_event_get_user_data(e));
    if (ctx->done) return;
    ctx->done = true;
    nav_pop(2);
}

static void paired_done_ctx_delete_cb(lv_event_t *e)
{
    delete static_cast<PairedDoneCtx *>(lv_event_get_user_data(e));
}

static void paired_page_open()
{
    PairedDoneCtx *ctx = new PairedDoneCtx();

    nav_page_t page = nav_screen_create();
    const header_slot_t none = {false, UI_ICON_SETTINGS, nullptr, nullptr};
    header_create(page.screen, none, "Pair New Device", none);

    const int btnH = 72;
    lv_obj_t *btn = lv_obj_create(page.screen);
    lv_obj_remove_style_all(btn);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(btn, ui_theme_get_style_brand(), 0);
    lv_obj_set_size(btn, ui_content_w() - 48, btnH);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, UI_HEADER_H_PX + (UI_HEADER_H_PX + ui_pane_h() - btnH) / 2);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(btn, OPACITY_BRAND_BG_LIGHT, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_opa(btn, LV_OPA_COVER, 0);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "Paired! View devices");
    lv_obj_add_style(label, ui_theme_get_style_brand(), 0);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_font(label, &satoshi_medium_24, 0);
    lv_obj_center(label);

    lv_obj_add_event_cb(btn, paired_done_cb, LV_EVENT_CLICKED, ctx);
    lv_obj_add_event_cb(page.focus, paired_done_cb, LV_EVENT_CLICKED, ctx);
    lv_obj_add_event_cb(page.screen, paired_done_ctx_delete_cb, LV_EVENT_DELETE, ctx);
    nav_replace(page);   /* replaces the code page */
}

/* ---- Pair New Device page (code confirmation) ---- */

static const uint32_t PAIRING_PAGE_TIMEOUT_MS   = 60000;   /* give up waiting for the connection */
static const uint32_t PAIRING_CODE_GONE_WAIT_MS = 15000;   /* after the code disappears, wait this long for the connection */

struct PairingCtx
{
    std::string mac;
    std::string shownCode;
    uint32_t    openedAt = 0;
    uint32_t    codeGoneAt = 0;
    bool        seenCode = false;
    bool        codeGone = false;
    bool        done = false;
};

/* A code too wide for one line (6 digits at the big font) is split in two halves. */
static std::string pairing_code_text(const std::string &code)
{
    if (code.size() < 4 || tile_text_width(code, &satoshi_medium_140) <= ui_content_w() - 72)
        return code;
    const size_t half = (code.size() + 1) / 2;
    return code.substr(0, half) + "\n" + code.substr(half);
}

/* Shows the code the device being paired shows too, until the connection is up (then closes). */
static void pairing_page_open(const PairedDevice &dev)
{
    auto ctx = std::make_shared<PairingCtx>();
    ctx->mac      = dev.mac;
    ctx->openedAt = lv_tick_get();

    nav_page_t page = nav_screen_create();
    const header_slot_t back = {true, UI_ICON_ARROW_BACK, nav_back_cb, nullptr};
    const header_slot_t none = {false, UI_ICON_SETTINGS, nullptr, nullptr};
    header_create(page.screen, back, "Pair New Device", none);

    const int textW = ui_content_w() - 72;
    lv_obj_t *headline = lv_label_create(page.screen);
    lv_obj_add_style(headline, ui_theme_get_style_text_primary(), 0);
    lv_obj_set_style_text_font(headline, &satoshi_medium_24, 0);
    lv_obj_set_style_text_align(headline, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(headline, textW);
    lv_label_set_text(headline, "Connecting...");
    lv_obj_align(headline, LV_ALIGN_TOP_MID, 0, UI_HEADER_H_PX + 24);

    /* the code is centred in the space below the headline */
    const int regionTop = UI_HEADER_H_PX + 24 + lv_font_get_line_height(&satoshi_medium_24);
    lv_obj_t *region = tile_container(page.screen);
    lv_obj_set_size(region, ui_content_w(), UI_HEADER_H_PX + ui_pane_h() - regionTop);
    lv_obj_align(region, LV_ALIGN_TOP_MID, 0, regionTop);

    lv_obj_t *code = lv_label_create(region);
    lv_obj_add_style(code, ui_theme_get_style_text_primary(), 0);
    lv_obj_set_style_text_font(code, &satoshi_medium_140, 0);
    lv_obj_set_style_text_align(code, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(code, ui_content_w());
    lv_label_set_text(code, "");
    lv_obj_center(code);

    tile_bind_update(code, [ctx, headline, code]()
    {
        const std::string current = tile_read_string("/bluetooth", "pairingCode");
        if (current != ctx->shownCode)
        {
            ctx->shownCode = current;
            lv_label_set_text(headline, current.empty() ? "Connecting..." : "Confirm the code on your device");
            lv_label_set_text(code, pairing_code_text(current).c_str());
        }
        if (ctx->done) return;

        if (!current.empty())
        {
            ctx->seenCode = true;
            ctx->codeGone = false;
        }
        else if (ctx->seenCode && !ctx->codeGone)
        {
            ctx->codeGone   = true;
            ctx->codeGoneAt = lv_tick_get();
        }
        const bool connected = device_mac(tile_read_string("/bluetooth", "connectedDevice")) == ctx->mac;
        const bool timedOut  = lv_tick_elaps(ctx->openedAt) > PAIRING_PAGE_TIMEOUT_MS;
        const bool gone      = ctx->codeGone && lv_tick_elaps(ctx->codeGoneAt) > PAIRING_CODE_GONE_WAIT_MS;
        if (connected || timedOut || gone)
        {
            ctx->done = true;
            if (connected) paired_page_open();
            else           nav_pop();   /* pairing failed or was refused: back to the device list */
        }
    });

    nav_replace(page);   /* replaces the Discover page, so closing returns to where the device list now includes the new device */
}

/* ---- Devices pages ---- */

static void devices_activated_cb(int index, void *user_data)
{
    DevicesView &v = **static_cast<std::shared_ptr<DevicesView> *>(user_data);
    if (v.done || index < 0 || index >= (int)v.devices.size()) return;
    const PairedDevice dev = v.devices[index];

    if (v.mode == DevicesMode::Paired)
    {
        /* stays on the page; the check follows connectedDevice once the connection is up (or goes away on disconnect) */
        if (device_mac(tile_read_string("/bluetooth", "connectedDevice")) == dev.mac)
            tile_set_bool("/bluetooth", "disconnectDevice", true);
        else
            tile_set_string("/bluetooth", "requestConnect", dev.mac);
    }
    else if (v.mode == DevicesMode::Edit)
    {
        forget_page_open(dev);
    }
    else if (v.mode == DevicesMode::Discover)
    {
        v.done = true;
        tile_set_bool("/bluetooth", "scan", false);
        tile_set_string("/bluetooth", "requestConnect", dev.mac);
        pairing_page_open(dev);
    }
}

static void devices_view_delete_cb(lv_event_t *e)
{
    delete static_cast<std::shared_ptr<DevicesView> *>(lv_event_get_user_data(e));
}

/* Encoder press on a devices page activates the selected row. */
static void devices_page_click_cb(lv_event_t *e)
{
    picker_wheel_click(static_cast<lv_obj_t *>(lv_event_get_user_data(e)));
}

/* Scanning lasts as long as the Discover page is open (back, choosing a device, or any other way out). */
static void discover_stop_scan_cb(lv_event_t *)
{
    tile_set_bool("/bluetooth", "scan", false);
}

static void bluetooth_devices_edit_open(lv_event_t *);
static void bluetooth_devices_discover_open(lv_event_t *);

/* Picker of devices (see DevicesMode); follows the elements while open. */
static void bluetooth_devices_open(DevicesMode mode)
{
    auto view = std::make_shared<DevicesView>();
    view->mode = mode;
    std::string list, connected;
    read_devices_elements(mode, list, connected);
    load_devices(*view, list, connected);

    nav_page_t page = nav_screen_create();
    const header_slot_t back = {true, UI_ICON_ARROW_BACK, nav_back_cb, nullptr};
    header_slot_t right = {false, UI_ICON_SETTINGS, nullptr, nullptr};
    const char *title = devicesPageTitle.c_str();
    if (mode == DevicesMode::Paired)
    {
        right = {true, UI_ICON_SETTINGS, bluetooth_devices_edit_open, nullptr, "Edit"};
    }
    else if (mode == DevicesMode::Edit)
    {
        right = {true, UI_ICON_ADD, bluetooth_devices_discover_open, nullptr};
        title = "Edit";
    }
    else
    {
        title = "Add Device";
    }
    header_create(page.screen, back, title, right);

    std::vector<picker_wheel_item_t> items = make_device_items(*view);

    auto *holder = new std::shared_ptr<DevicesView>(view);
    picker_wheel_props_t props = {};
    props.type               = PICKER_WHEEL_TYPE_CENTER_CHECK;
    props.items              = items.data();
    props.item_count         = (int)items.size();
    props.selected_index     = 0;
    props.activated_callback = devices_activated_cb;
    props.user_data          = holder;
    view->picker = picker_wheel_create(page.screen, &props);
    lv_obj_t *picker = view->picker;
    lv_obj_align(picker, LV_ALIGN_TOP_LEFT, 0, UI_HEADER_H_PX);
    lv_obj_add_event_cb(picker, devices_view_delete_cb, LV_EVENT_DELETE, holder);

    tile_bind_update(picker, [view]() { refresh_devices_view(*view); });

    lv_obj_add_event_cb(page.focus, devices_page_key_cb, LV_EVENT_KEY, picker);
    lv_obj_add_event_cb(page.focus, devices_page_click_cb, LV_EVENT_CLICKED, picker);
    if (mode == DevicesMode::Discover)
    {
        lv_obj_add_event_cb(page.screen, discover_stop_scan_cb, LV_EVENT_SCREEN_UNLOAD_START, nullptr);
        lv_obj_add_event_cb(page.screen, discover_stop_scan_cb, LV_EVENT_DELETE, nullptr);
    }
    nav_push(page);
}

static void bluetooth_devices_paired_open(lv_event_t *) { bluetooth_devices_open(DevicesMode::Paired); }
static void bluetooth_devices_edit_open(lv_event_t *)   { bluetooth_devices_open(DevicesMode::Edit); }

static void bluetooth_devices_discover_open(lv_event_t *)
{
    tile_set_bool("/bluetooth", "scan", true);
    bluetooth_devices_open(DevicesMode::Discover);
}

void tile_bluetooth_devices_open(const std::string &title)
{
    devicesPageTitle = title;
    bluetooth_devices_open(DevicesMode::Paired);
}

void tile_bluetooth_volume_open(const std::string &name, const std::string &binding)
{
    (void)binding;
    devicesPageTitle = name;
    nav_page_t page = nav_screen_create();

    const header_slot_t back = {true, UI_ICON_ARROW_BACK, nav_back_cb, nullptr};
    const header_slot_t gear = {true, UI_ICON_SETTINGS, bluetooth_devices_paired_open, nullptr};
    header_create(page.screen, back, name.c_str(), gear);

    level_gauge_create(page.screen, "/bluetooth", "volume");

    lv_obj_add_event_cb(page.focus, bluetooth_page_key_cb, LV_EVENT_KEY, NULL);
    nav_push(page);
}
