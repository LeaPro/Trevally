/**
 * @file fader_page.cpp
 * @brief See fader_page.h.
 */

#include "fader_page.h"
#include "tile.h"
#include "header.h"
#include "level_gauge.h"
#include "nav.h"
#include "picker_wheel.h"
#include "../ui_design_tokens.h"

#include <algorithm>
#include <memory>

#include <object.h>
/* The device's root object (defined in main.cpp). */
extern std::shared_ptr<RootObject> root;

/* ---- Gauge page ---- */

struct FaderPageCtx
{
    std::string leafPath;
    std::string element;
    std::function<void()> onButton;
    std::function<bool()> showButton;   /* null: always shown */
};

/* Encoder rotation changes the element by one dB / % per tick (see fader_step()); a press is the button, if the page
 * has one. */
static void fader_page_key_cb(lv_event_t *e)
{
    const FaderPageCtx *ctx = static_cast<const FaderPageCtx *>(lv_event_get_user_data(e));
    const uint32_t key = lv_event_get_key(e);
    const int step = (key == LV_KEY_RIGHT) ? 1 : (key == LV_KEY_LEFT) ? -1 : 0;
    if (step != 0)
        fader_step(ctx->leafPath, ctx->element, step);
}

static void fader_page_button_cb(lv_event_t *e)
{
    const FaderPageCtx *ctx = static_cast<const FaderPageCtx *>(lv_event_get_user_data(e));
    if (ctx->showButton && !ctx->showButton()) return;
    if (ctx->onButton) ctx->onButton();
}

static void fader_page_delete_cb(lv_event_t *e)
{
    delete static_cast<FaderPageCtx *>(lv_event_get_user_data(e));
}

/* Outlined button: centred under the header in portrait. In landscape it goes at the bottom, centred under
 * the value (which sits left of the gauge, where depends on the gauge style; see level_gauge.cpp), clear of the
 * value and arc of the ellipse and anchored gauges. The floating gauge's arc circles the value and reaches
 * lower, so there the button goes into the space under it. */
static lv_obj_t *page_button_create(lv_obj_t *screen, const std::string &label)
{
    const int margin = 24, w = 240, h = 56;
    int x = (ui_content_w() - w) / 2;
    int y = UI_HEADER_H_PX + margin;
    if (ui_is_landscape())
    {
        const int bottom = UI_HEADER_H_PX + ui_pane_h();
        y = bottom - margin - h;
        auto styleElement = std::dynamic_pointer_cast<StringControl>(root->elementFromPath("/appearance/levelGaugeStyle"));
        const std::string style = styleElement ? styleElement->get() : std::string();
        int numberX;   /* the value's centre: midway between the panel's left edge and the arc */
        if (style == "floating")
        {
            numberX = LEVEL_GAUGE_FLOATING_LAND_CX;
            const int arcBottom = LEVEL_GAUGE_CX + LEVEL_GAUGE_FLOATING_LAND_RADIUS + LEVEL_GAUGE_ARC_W / 2;
            y = arcBottom + std::max(0, (bottom - arcBottom - h) / 2);
        }
        else if (style == "anchored")
            numberX = (LEVEL_GAUGE_ANCHORED_CY - LEVEL_GAUGE_ANCHORED_RADIUS) / 2;
        else
            numberX = (LEVEL_GAUGE_ELLIPSE_CY - LEVEL_GAUGE_ELLIPSE_RADIUS) / 2;
        x = std::max(0, numberX - w / 2);
    }

    lv_obj_t *btn = lv_obj_create(screen);
    lv_obj_remove_style_all(btn);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_add_style(btn, ui_theme_get_style_border(), 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_style(btn, ui_theme_get_style_bg_tertiary(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_PRESSED);

    lv_obj_t *text = tile_label(btn, label, &satoshi_medium_24, ui_theme_get_style_text_primary());
    lv_obj_center(text);
    return btn;
}

void fader_page_open(const std::string &title, const std::string &leafPath, const std::string &element,
                     const std::string &buttonLabel, std::function<void()> onButton,
                     std::function<bool()> showButton)
{
    FaderPageCtx *ctx = new FaderPageCtx{leafPath, element, std::move(onButton), std::move(showButton)};
    nav_page_t page = nav_screen_create();

    const header_slot_t back = {true, UI_ICON_ARROW_BACK, nav_back_cb, nullptr};
    const header_slot_t none = {false, UI_ICON_ARROW_BACK, nullptr, nullptr};
    header_create(page.screen, back, title.c_str(), none);

    level_gauge_create(page.screen, leafPath, element.c_str(), true /*faderUnits*/);

    if (!buttonLabel.empty())
    {
        lv_obj_t *btn = page_button_create(page.screen, buttonLabel);
        lv_obj_add_event_cb(btn, fader_page_button_cb, LV_EVENT_CLICKED, ctx);
        lv_obj_add_event_cb(page.focus, fader_page_button_cb, LV_EVENT_CLICKED, ctx);
        if (ctx->showButton)
        {
            auto update = [btn, show = ctx->showButton]() {
                const bool visible = show();
                if (visible == !lv_obj_has_flag(btn, LV_OBJ_FLAG_HIDDEN)) return;
                if (visible) lv_obj_remove_flag(btn, LV_OBJ_FLAG_HIDDEN);
                else         lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
            };
            update();
            tile_bind_update(btn, update);
        }
    }

    lv_obj_add_event_cb(page.focus, fader_page_key_cb, LV_EVENT_KEY, ctx);
    lv_obj_add_event_cb(page.screen, fader_page_delete_cb, LV_EVENT_DELETE, ctx);
    nav_push(page);
}

/* ---- List page ---- */

struct FaderListView
{
    std::function<std::vector<FaderListRow>()> rowsFn;
    std::function<void(const FaderListRow &)> onChoose;
    std::vector<FaderListRow> rows;
    std::vector<std::string>  shown;   /* level text per row, as last set */
    lv_obj_t                 *picker = nullptr;
};

/* Items for the view's rows (they point into the view, which must outlive their use). */
static std::vector<picker_wheel_item_t> make_items(const FaderListView &v)
{
    std::vector<picker_wheel_item_t> items(v.rows.size());
    for (size_t i = 0; i < items.size(); i++)
    {
        items[i].primary_text         = v.rows[i].title.c_str();
        items[i].primary_text_align   = LV_TEXT_ALIGN_LEFT;
        items[i].subtitle_text        = v.rows[i].subtitle.c_str();
        items[i].secondary_text       = v.shown[i].c_str();
        items[i].secondary_text_align = LV_TEXT_ALIGN_RIGHT;
        items[i].border_width         = 1;
        items[i].border_color         = ui_get_border_color();
    }
    return items;
}

static void set_rows(FaderListView &v, std::vector<FaderListRow> rows)
{
    v.rows = std::move(rows);
    v.shown.clear();
    for (const FaderListRow &r : v.rows)
        v.shown.push_back(r.leafPath.empty() ? std::string() : fader_level_text(r.leafPath, r.element));
}

/* Rebuilds the rows in place when they changed, keeping the selected row selected; then brings the levels
 * up to date. */
static void refresh_list(FaderListView &v)
{
    std::vector<FaderListRow> now = v.rowsFn();
    if (!now.empty() && now != v.rows)
    {
        const int selected = picker_wheel_get_selected(v.picker);
        const std::string selectedKey = (selected >= 0 && selected < (int)v.rows.size()) ? v.rows[selected].key
                                                                                         : std::string();
        set_rows(v, std::move(now));
        int newSelected = 0;
        for (size_t i = 0; i < v.rows.size(); i++)
            if (v.rows[i].key == selectedKey) newSelected = (int)i;
        const std::vector<picker_wheel_item_t> items = make_items(v);
        picker_wheel_set_items(v.picker, items.data(), (int)items.size(), newSelected);
    }

    for (size_t i = 0; i < v.rows.size(); i++)
    {
        if (v.rows[i].leafPath.empty()) continue;   /* created without a level label */
        const std::string level = fader_level_text(v.rows[i].leafPath, v.rows[i].element);
        if (level == v.shown[i]) continue;
        v.shown[i] = level;
        picker_wheel_set_secondary_text(v.picker, (int)i, level.c_str());
    }
}

static void list_activated_cb(int index, void *user_data)
{
    const FaderListView &v = **static_cast<std::shared_ptr<FaderListView> *>(user_data);
    if (index < 0 || index >= (int)v.rows.size() || !v.onChoose) return;
    const FaderListRow row = v.rows[index];   /* a copy: onChoose may change the rows */
    v.onChoose(row);
}

static void list_view_delete_cb(lv_event_t *e)
{
    delete static_cast<std::shared_ptr<FaderListView> *>(lv_event_get_user_data(e));
}

void fader_list_open(const std::string &title, std::function<std::vector<FaderListRow>()> rows,
                     std::function<void(const FaderListRow &)> onChoose)
{
    auto view = std::make_shared<FaderListView>();
    view->rowsFn   = std::move(rows);
    view->onChoose = std::move(onChoose);
    set_rows(*view, view->rowsFn());
    if (view->rows.empty()) return;
    auto *holder = new std::shared_ptr<FaderListView>(view);

    nav_page_t page = nav_screen_create();
    const header_slot_t back = {true, UI_ICON_ARROW_BACK, nav_back_cb, nullptr};
    const header_slot_t none = {false, UI_ICON_ARROW_BACK, nullptr, nullptr};
    header_create(page.screen, back, title.c_str(), none);

    std::vector<picker_wheel_item_t> items = make_items(*view);
    picker_wheel_props_t props = {};
    props.type               = PICKER_WHEEL_TYPE_LEFT_BORDER;
    props.items              = items.data();
    props.item_count         = (int)items.size();
    props.selected_index     = 0;
    props.activated_callback = list_activated_cb;
    props.user_data          = holder;
    view->picker = picker_wheel_create(page.screen, &props);
    lv_obj_t *picker = view->picker;
    lv_obj_align(picker, LV_ALIGN_TOP_LEFT, 0, UI_HEADER_H_PX);
    lv_obj_add_event_cb(picker, list_view_delete_cb, LV_EVENT_DELETE, holder);

    tile_bind_update(picker, [view]() { refresh_list(*view); });

    picker_wheel_attach_encoder(picker, page.focus);
    nav_push(page);
}
