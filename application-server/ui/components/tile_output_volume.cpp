/**
 * @file tile_output_volume.cpp
 * @brief Output volume tile: title, level slider, left-aligned value in dB.
 *        The binding is a /leaOutput config uuid; the level is /outputFaders/<binding> (the amps'
 *        fader, see OutputFaders.h), "-" until the amp reports it or while the uuid isn't configured.
 *        Tapping it opens a page with the header and level gauge, where the encoder changes the level.
 *        An output with subgroups gets a "Subgroups" button there: with one subgroup it opens that
 *        subgroup's page, with more a picker of them (in subgroupOrder, each with its level) whose rows
 *        open theirs. A subgroup page is the same gauge page for /outputFaders/<subgroup uuid>
 *        (the subchannels' faderZoneAdjust).
 */

#include "tile.h"
#include "header.h"
#include "level_gauge.h"
#include "nav.h"
#include "picker_wheel.h"
#include "../ui_design_tokens.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include <object.h>
/* The device's root object (defined in main.cpp). */
extern std::shared_ptr<RootObject> root;

static const char *fadersLeaf = "/outputFaders";

/* An element for the binding exists (tile_read_number() gives zeros for a missing one). */
static bool level_exists(const TileNumber &n)
{
    return n.max > n.min;
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

void tile_output_volume_build(lv_obj_t *root, const std::string &name,
                              const std::string &binding, int w, int h)
{
    const bool tall = h >= ui_pane_h();
    const bool wide = w >= ui_content_w();
    const int  pad  = tall ? 36 : 16;

    TileNumber level = tile_read_number(fadersLeaf, binding.c_str());

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
    lv_obj_set_flex_align(group, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_size(group, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(group, 12, 0);

    lv_obj_t *slider = make_slider(group, w - 2 * pad, level.fill);

    lv_obj_t *row = tile_container(group);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(row, 4, 0);

    lv_obj_t *valueLabel = tile_label(row, level_exists(level) ? tile_format_number(level.value) : std::string("-"),
                                      wide ? &satoshi_medium_48 : &satoshi_medium_32, ui_theme_get_style_brand());
    tile_label(row, "dB", wide ? &satoshi_medium_32 : &satoshi_medium_24, ui_theme_get_style_brand());

    /* Looked up every tick rather than bound once: the element comes and goes with the /leaOutput config. */
    float last = level_exists(level) ? level.value : NAN;
    tile_bind_update(valueLabel, [valueLabel, slider, binding, last]() mutable {
        const TileNumber n = tile_read_number(fadersLeaf, binding.c_str());
        const float value = level_exists(n) ? n.value : NAN;
        if (value == last || (std::isnan(value) && std::isnan(last))) return;
        last = value;
        lv_label_set_text(valueLabel, tile_format_number(value).c_str());
        lv_slider_set_value(slider, (int32_t)(n.fill * 1000.0f), LV_ANIM_OFF);
    });
}

/* ---- Subgroups ---- */

struct Subgroup
{
    std::string id;     /* uuid: also its element in /outputFaders */
    std::string name;
    bool operator==(const Subgroup &other) const { return id == other.id && name == other.name; }
};

/* /leaOutput config[binding].subgroups, in subgroupOrder (then any the order leaves out, in config order). */
static std::vector<Subgroup> read_subgroups(const std::string &binding)
{
    std::vector<Subgroup> result;
    auto element = std::dynamic_pointer_cast<JsonElement>(root->elementFromPath("/leaOutput/config"));
    if (!element) return result;
    const json config = element->get();
    auto entry = config.find(binding);
    if (entry == config.end() || !entry->is_object()) return result;
    auto subgroups = entry->find("subgroups");
    if (subgroups == entry->end() || !subgroups->is_object()) return result;

    std::vector<std::string> ids;
    auto order = entry->find("subgroupOrder");
    if (order != entry->end() && order->is_array())
        for (const auto &id : *order)
            if (id.is_string() && subgroups->contains(id.get<std::string>()) &&
                std::find(ids.begin(), ids.end(), id.get<std::string>()) == ids.end())
                ids.push_back(id.get<std::string>());
    for (auto it = subgroups->begin(); it != subgroups->end(); ++it)
        if (std::find(ids.begin(), ids.end(), it.key()) == ids.end())
            ids.push_back(it.key());

    for (const std::string &id : ids)
    {
        const json &sub = (*subgroups)[id];
        if (!sub.is_object()) continue;
        auto nameIt = sub.find("name");
        result.push_back({id, (nameIt != sub.end() && nameIt->is_string() && !nameIt->get<std::string>().empty())
                                  ? nameIt->get<std::string>() : std::string("Subgroup")});
    }
    return result;
}

/* "-6 dB", or "-" while the level is unknown / the subgroup is gone. */
static std::string level_text(const std::string &element)
{
    const TileNumber n = tile_read_number(fadersLeaf, element.c_str());
    if (!level_exists(n) || std::isnan(n.value)) return "-";
    return tile_format_number(n.value) + " " + n.units;
}

static void level_page_open(const std::string &title, const std::string &element, const std::string &subgroupsOf);

/* Picker of the subgroups (with their levels); choosing one opens its page. Follows the config while open:
 * a new subgroupOrder, renames, added / removed subgroups. */
struct SubgroupsView
{
    std::string              binding;   /* the output's /leaOutput config uuid */
    std::vector<Subgroup>    subgroups;
    std::vector<std::string> shown;   /* level text per row, as last set */
    lv_obj_t                *picker = nullptr;
};

static void subgroups_activated_cb(int index, void *user_data)
{
    const SubgroupsView &v = **static_cast<std::shared_ptr<SubgroupsView> *>(user_data);
    if (index < 0 || index >= (int)v.subgroups.size()) return;
    level_page_open(v.subgroups[index].name, v.subgroups[index].id, std::string());
}

static void subgroups_view_delete_cb(lv_event_t *e)
{
    delete static_cast<std::shared_ptr<SubgroupsView> *>(lv_event_get_user_data(e));
}

static void subgroups_page_key_cb(lv_event_t *e)
{
    lv_obj_t *picker = (lv_obj_t *)lv_event_get_user_data(e);
    const uint32_t key = lv_event_get_key(e);
    if (key == LV_KEY_RIGHT)     picker_wheel_nudge(picker, 1, false);
    else if (key == LV_KEY_LEFT) picker_wheel_nudge(picker, -1, false);
}

static void subgroups_page_click_cb(lv_event_t *e)
{
    picker_wheel_click(static_cast<lv_obj_t *>(lv_event_get_user_data(e)));
}

/* Items for the view's rows (they point into the view, which must outlive their use). */
static std::vector<picker_wheel_item_t> make_subgroup_items(const SubgroupsView &v)
{
    std::vector<picker_wheel_item_t> items(v.subgroups.size());
    for (size_t i = 0; i < items.size(); i++)
    {
        items[i].primary_text         = v.subgroups[i].name.c_str();
        items[i].primary_text_align   = LV_TEXT_ALIGN_LEFT;
        items[i].secondary_text       = v.shown[i].c_str();
        items[i].secondary_text_align = LV_TEXT_ALIGN_RIGHT;
        items[i].border_width         = 1;
        items[i].border_color         = ui_get_border_color();
    }
    return items;
}

/* Rebuilds the rows in place when the subgroups (order, names, membership) changed, keeping the selected
 * subgroup selected; then brings the levels up to date. */
static void refresh_subgroups_view(SubgroupsView &v)
{
    const std::vector<Subgroup> now = read_subgroups(v.binding);
    if (!now.empty() && now != v.subgroups)
    {
        const int selected = picker_wheel_get_selected(v.picker);
        const std::string selectedId = (selected >= 0 && selected < (int)v.subgroups.size()) ? v.subgroups[selected].id
                                                                                             : std::string();
        v.subgroups = now;
        v.shown.clear();
        int newSelected = 0;
        for (size_t i = 0; i < v.subgroups.size(); i++)
        {
            v.shown.push_back(level_text(v.subgroups[i].id));
            if (v.subgroups[i].id == selectedId) newSelected = (int)i;
        }
        const std::vector<picker_wheel_item_t> items = make_subgroup_items(v);
        picker_wheel_set_items(v.picker, items.data(), (int)items.size(), newSelected);
    }

    for (size_t i = 0; i < v.subgroups.size(); i++)
    {
        const std::string level = level_text(v.subgroups[i].id);
        if (level == v.shown[i]) continue;
        v.shown[i] = level;
        picker_wheel_set_secondary_text(v.picker, (int)i, level.c_str());
    }
}

static void subgroups_picker_open(const std::string &binding, const std::vector<Subgroup> &subgroups)
{
    auto view = std::make_shared<SubgroupsView>();
    view->binding   = binding;
    view->subgroups = subgroups;
    for (const Subgroup &sub : subgroups)
        view->shown.push_back(level_text(sub.id));
    auto *holder = new std::shared_ptr<SubgroupsView>(view);

    nav_page_t page = nav_screen_create();
    const header_slot_t back = {true, UI_ICON_ARROW_BACK, nav_back_cb, nullptr};
    const header_slot_t none = {false, UI_ICON_ARROW_BACK, nullptr, nullptr};
    header_create(page.screen, back, "Subgroups", none);

    std::vector<picker_wheel_item_t> items = make_subgroup_items(*view);
    picker_wheel_props_t props = {};
    props.type               = PICKER_WHEEL_TYPE_LEFT_BORDER;
    props.items              = items.data();
    props.item_count         = (int)items.size();
    props.selected_index     = 0;
    props.activated_callback = subgroups_activated_cb;
    props.user_data          = holder;
    view->picker = picker_wheel_create(page.screen, &props);
    lv_obj_t *picker = view->picker;
    lv_obj_align(picker, LV_ALIGN_TOP_LEFT, 0, UI_HEADER_H_PX);
    lv_obj_add_event_cb(picker, subgroups_view_delete_cb, LV_EVENT_DELETE, holder);

    tile_bind_update(picker, [view]() { refresh_subgroups_view(*view); });

    lv_obj_add_event_cb(page.focus, subgroups_page_key_cb, LV_EVENT_KEY, picker);
    lv_obj_add_event_cb(page.focus, subgroups_page_click_cb, LV_EVENT_CLICKED, picker);
    nav_push(page);
}

/* One subgroup: straight to its page; more: the picker. */
static void subgroups_open(const std::string &binding)
{
    const std::vector<Subgroup> subgroups = read_subgroups(binding);
    if (subgroups.size() == 1)
        level_page_open(subgroups[0].name, subgroups[0].id, std::string());
    else if (!subgroups.empty())
        subgroups_picker_open(binding, subgroups);
}

/* ---- Level pages (the output's and each subgroup's) ---- */

struct LevelPageCtx
{
    std::string element;       /* in /outputFaders */
    std::string subgroupsOf;   /* output binding whose Subgroups button the page has; "" for none */
};

/* Encoder rotation changes the level by 1 dB per tick, on every channel behind it; a press is the
 * Subgroups button, if the page has one. */
static void level_page_key_cb(lv_event_t *e)
{
    const LevelPageCtx *ctx = static_cast<const LevelPageCtx *>(lv_event_get_user_data(e));
    const uint32_t key = lv_event_get_key(e);
    const int step = (key == LV_KEY_RIGHT) ? 1 : (key == LV_KEY_LEFT) ? -1 : 0;
    if (step != 0)
        tile_step_number(fadersLeaf, ctx->element.c_str(), step);
}

static void level_page_subgroups_cb(lv_event_t *e)
{
    const LevelPageCtx *ctx = static_cast<const LevelPageCtx *>(lv_event_get_user_data(e));
    subgroups_open(ctx->subgroupsOf);
}

static void level_page_delete_cb(lv_event_t *e)
{
    delete static_cast<LevelPageCtx *>(lv_event_get_user_data(e));
}

/* Outlined "Subgroups" button, centred: under the header in portrait; at the bottom in landscape, where it
 * clears the value and arc of the ellipse and anchored gauges. The floating gauge's arc circles the value
 * and reaches lower, so there the button goes into the space under it. */
static lv_obj_t *subgroups_button_create(lv_obj_t *screen)
{
    const int margin = 24, w = 240, h = 56;
    int y = UI_HEADER_H_PX + margin;
    if (ui_is_landscape())
    {
        const int bottom = UI_HEADER_H_PX + ui_pane_h();
        y = bottom - margin - h;
        auto style = std::dynamic_pointer_cast<StringControl>(root->elementFromPath("/appearance/levelGaugeStyle"));
        if (style && style->get() == "floating")
        {
            const int arcBottom = LEVEL_GAUGE_CX + LEVEL_GAUGE_FLOATING_LAND_RADIUS + LEVEL_GAUGE_ARC_W / 2;
            y = arcBottom + std::max(0, (bottom - arcBottom - h) / 2);
        }
    }

    lv_obj_t *btn = lv_obj_create(screen);
    lv_obj_remove_style_all(btn);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, (ui_content_w() - w) / 2, y);
    lv_obj_add_style(btn, ui_theme_get_style_border(), 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_style(btn, ui_theme_get_style_bg_tertiary(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_PRESSED);

    lv_obj_t *label = tile_label(btn, "Subgroups", &satoshi_medium_24, ui_theme_get_style_text_primary());
    lv_obj_center(label);
    return btn;
}

static void level_page_open(const std::string &title, const std::string &element, const std::string &subgroupsOf)
{
    LevelPageCtx *ctx = new LevelPageCtx{element, subgroupsOf};
    nav_page_t page = nav_screen_create();

    const header_slot_t back = {true, UI_ICON_ARROW_BACK, nav_back_cb, nullptr};
    const header_slot_t none = {false, UI_ICON_ARROW_BACK, nullptr, nullptr};
    header_create(page.screen, back, title.c_str(), none);

    const bool exists = level_exists(tile_read_number(fadersLeaf, element.c_str()));
    level_gauge_create(page.screen, exists ? std::string(fadersLeaf) : std::string(), element.c_str());

    if (!subgroupsOf.empty())
    {
        lv_obj_t *btn = subgroups_button_create(page.screen);
        lv_obj_add_event_cb(btn, level_page_subgroups_cb, LV_EVENT_CLICKED, ctx);
        lv_obj_add_event_cb(page.focus, level_page_subgroups_cb, LV_EVENT_CLICKED, ctx);
    }

    lv_obj_add_event_cb(page.focus, level_page_key_cb, LV_EVENT_KEY, ctx);
    lv_obj_add_event_cb(page.screen, level_page_delete_cb, LV_EVENT_DELETE, ctx);
    nav_push(page);
}

void tile_output_volume_open(const std::string &name, const std::string &binding)
{
    level_page_open(name, binding, read_subgroups(binding).empty() ? std::string() : binding);
}
