/**
 * @file tile_output_volume.cpp
 * @brief Output volume tile: title, level slider, left-aligned value in dB or % (Appearance "volumeUnits").
 *        The binding is a /leaOutput config uuid; the level is /outputFaders/<binding> (the amps'
 *        fader, see OutputFaders.h), "-" until the amp reports it or while the uuid isn't configured.
 *        Tapping it opens a page with the header and level gauge, where the encoder changes the level.
 *        An output with subgroups gets a "Subgroups" button there: with one subgroup it opens that
 *        subgroup's page, with more a picker of them (in subgroupOrder, each with its level) whose rows
 *        open theirs. A subgroup page is the same gauge page for /outputFaders/<subgroup uuid>
 *        (the subchannels' faderZoneAdjust).
 */

#include "tile.h"
#include "fader_page.h"
#include "../ui_design_tokens.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include <object.h>
#include <LeaOutput.h>
/* The device's root object (defined in main.cpp). */
extern std::shared_ptr<RootObject> root;

static const char *fadersLeaf = "/outputFaders";

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

    lv_obj_t *slider = make_slider(group, w - 2 * pad, 0.0f);   /* set by refresh() below */

    lv_obj_t *row = tile_container(group);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(row, 4, 0);

    lv_obj_t *valueLabel = tile_label(row, "-", wide ? &satoshi_medium_48 : &satoshi_medium_32,
                                      ui_theme_get_style_brand());
    lv_obj_t *unitsLabel = tile_label(row, "", wide ? &satoshi_medium_32 : &satoshi_medium_24,
                                      ui_theme_get_style_brand());

    /* Looked up every tick rather than bound once: the element comes and goes with the /leaOutput config, and the
     * units with the Appearance "volumeUnits" (see fader_units.h). */
    auto shown = std::make_shared<FaderDisplay>();
    shown->number = "?";   /* differs from anything real: the first refresh sets both labels */
    auto refresh = [valueLabel, unitsLabel, slider, binding, shown]() {
        const float dB = fader_read_db(fadersLeaf, binding);
        const FaderDisplay d = fader_display(dB);
        if (d.number == shown->number && d.units == shown->units) return;
        *shown = d;
        lv_label_set_text(valueLabel, d.number.c_str());
        lv_label_set_text(unitsLabel, d.units.c_str());
        const float fill = std::isnan(dB) ? 0.0f : std::fmin(std::fmax((dB + 80.0f) / 80.0f, 0.0f), 1.0f);
        lv_slider_set_value(slider, (int32_t)(fill * 1000.0f), LV_ANIM_OFF);
    };
    refresh();
    tile_bind_update(valueLabel, refresh);
}

/* ---- Subgroups ---- */

struct Subgroup
{
    std::string id;     /* uuid: also its element in /outputFaders */
    std::string name;
};

/* /leaOutput config[binding].subgroups, in subgroupOrder (then any the order leaves out, in config order). */
static std::vector<Subgroup> read_subgroups(const std::string &binding)
{
    std::vector<Subgroup> result;
    auto output = std::dynamic_pointer_cast<LeaOutput>(root->leafFromPath("/leaOutput"));
    if (!output) return result;
    const json config = output->configPtr->get();
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

/* One subgroup: straight to its page; more: a list of them (following the config while open). */
static void subgroups_open(const std::string &binding)
{
    const std::vector<Subgroup> subgroups = read_subgroups(binding);
    if (subgroups.size() == 1)
    {
        fader_page_open(subgroups[0].name, fadersLeaf, subgroups[0].id);
        return;
    }
    fader_list_open("Subgroups",
        [binding]() {
            std::vector<FaderListRow> rows;
            for (const Subgroup &sub : read_subgroups(binding))
                rows.push_back({sub.id, sub.name, "", fadersLeaf, sub.id});
            return rows;
        },
        [](const FaderListRow &row) { fader_page_open(row.title, fadersLeaf, row.element); });
}

/* The Subgroups button follows the config: it shows while the output has subgroups. */
void tile_output_volume_open(const std::string &name, const std::string &binding)
{
    fader_page_open(name, fadersLeaf, binding, "Subgroups", [binding]() { subgroups_open(binding); },
                    [binding]() { return !read_subgroups(binding).empty(); });
}
