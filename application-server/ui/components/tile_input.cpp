/**
 * @file tile_input.cpp
 * @brief Input tile: title, the active state's name, and its volume (the state's fader, see InputFaders.h).
 *        The binding is a /leaInput config uuid. "-" while there is no active state / it has no volume control.
 *
 *        Tapping it opens the States page: a picker of the binding's states (stateOrder), the active one (last
 *        applied from here, /leaInput/active) checked. Choosing a state applies it (selects its sources on its
 *        amp channels). The speaker icon in the header shows while the highlighted state has volume control and
 *        opens its volume page:
 *        - "<state> Input Volume": gauge for every volume-controlled fader of the state (primaryFaders, and the
 *          enabled mix faders of Mixer channels). Its button:
 *          - "Inputs" (some channel isn't Mixer): a list of the volume-controlled channels with their own levels
 *            (labelled with the amps' input and device names, see InputStates::sourceLabel() / channelLabel());
 *            a channel opens a gauge for its primaryFader, a Mixer channel its Input Mixer list (its enabled
 *            sources' mix faders).
 *          - "Input Mixer" (every channel is Mixer): a list of the enabled sources, one row per source however
 *            many channels have it; a row's gauge sets that source's mix fader on all of them.
 */

#include "tile.h"
#include "adaptive_text.h"
#include "fader_page.h"
#include "header.h"
#include "nav.h"
#include "picker_wheel.h"
#include "../ui_design_tokens.h"

#include <algorithm>
#include <memory>
#include <vector>

#include <object.h>
#include <InputFaders.h>
#include <LeaInput.h>
/* The device's root object (defined in main.cpp). */
extern std::shared_ptr<RootObject> root;

using InputStates::State;
using InputStates::Channel;
using InputStates::MixSource;

static const char *fadersLeaf = "/inputFaders";

static std::shared_ptr<LeaInput> lea_input()
{
    return std::dynamic_pointer_cast<LeaInput>(root->leafFromPath("/leaInput"));
}

static std::vector<State> read_states(const std::string &binding)
{
    auto input = lea_input();
    return input ? InputStates::read(input->configPtr->get(), binding) : std::vector<State>();
}

/* The state @p stateId of @p binding; false if it's gone. */
static bool find_state(const std::string &binding, const std::string &stateId, State &out)
{
    for (State &s : read_states(binding))
        if (s.id == stateId) { out = std::move(s); return true; }
    return false;
}

static std::string active_state(const std::string &binding)
{
    auto input = lea_input();
    return input ? input->activeState(binding) : std::string();
}

/* ---- Volume pages ---- */

/* A Mixer channel's enabled sources (Inputs page -> a Mixer row). */
static void channel_mixer_open(const std::string &binding, const std::string &stateId,
                               const std::string &mac, const std::string &channel)
{
    fader_list_open("Input Mixer",
        [binding, stateId, mac, channel]() {
            std::vector<FaderListRow> rows;
            State state;
            if (!find_state(binding, stateId, state)) return rows;
            for (const Channel &c : state.channels)
                if (c.mac == mac && c.channel == channel)
                    for (const MixSource &s : c.mixSources)
                        rows.push_back({s.element, InputStates::sourceLabel(c.mac, s.source), "", fadersLeaf,
                                        InputStates::mixFader(state, c, s)});
            return rows;
        },
        [](const FaderListRow &row) { fader_page_open(row.title, fadersLeaf, row.element); });
}

/* All-Mixer state: one row per enabled source, its gauge on every channel that has it. */
static void shared_mixer_open(const std::string &binding, const std::string &stateId)
{
    fader_list_open("Input Mixer",
        [binding, stateId]() {
            std::vector<FaderListRow> rows;
            State state;
            if (!find_state(binding, stateId, state)) return rows;
            for (const MixSource &s : state.mixSources())
            {
                /* named from the first channel that has it */
                std::string mac;
                for (const Channel &c : state.channels)
                    for (const MixSource &cs : c.mixSources)
                        if (mac.empty() && cs.element == s.element) mac = c.mac;
                rows.push_back({s.element, InputStates::sourceLabel(mac, s.source), "", fadersLeaf,
                                InputStates::sharedMixFader(state, s)});
            }
            return rows;
        },
        [](const FaderListRow &row) { fader_page_open(row.title, fadersLeaf, row.element); });
}

/* The state's volume-controlled channels, each with its own level (a Mixer channel: none; it opens its sources). */
static void inputs_open(const std::string &binding, const std::string &stateId, const std::string &title)
{
    fader_list_open(title,
        [binding, stateId]() {
            std::vector<FaderListRow> rows;
            State state;
            if (!find_state(binding, stateId, state)) return rows;
            for (const Channel &c : state.channels)
            {
                if (!c.volume) continue;
                const std::string key = InputStates::channelFader(state, c);
                if (c.mixer)
                    rows.push_back({key, "Mixer", InputStates::channelLabel(c), "", ""});
                else
                    rows.push_back({key, InputStates::sourceLabel(c.mac, c.source), InputStates::channelLabel(c),
                                    fadersLeaf, key});
            }
            return rows;
        },
        [binding, stateId](const FaderListRow &row) {
            if (!row.leafPath.empty())
            {
                fader_page_open(row.title + " - " + row.subtitle, fadersLeaf, row.element);
                return;
            }
            State state;
            if (!find_state(binding, stateId, state)) return;
            for (const Channel &c : state.channels)
                if (InputStates::channelFader(state, c) == row.key)
                    channel_mixer_open(binding, stateId, c.mac, c.channel);
        });
}

static void state_volume_open(const std::string &binding, const State &state)
{
    const std::string title = state.name + " Input Volume";
    const std::string stateId = state.id;
    if (state.allMixer())
        fader_page_open(title, fadersLeaf, InputStates::stateFader(state), "Input Mixer",
                        [binding, stateId]() { shared_mixer_open(binding, stateId); });
    else
        fader_page_open(title, fadersLeaf, InputStates::stateFader(state), "Inputs",
                        [binding, stateId, title]() { inputs_open(binding, stateId, title); });
}

/* ---- States page ---- */

struct StatesView
{
    std::string              binding;
    std::vector<State>       states;
    std::string              active;
    lv_obj_t                *picker = nullptr;
    lv_obj_t                *header = nullptr;
};

static std::vector<picker_wheel_item_t> make_state_items(const StatesView &v)
{
    std::vector<picker_wheel_item_t> items(v.states.size());
    for (size_t i = 0; i < items.size(); i++)
    {
        items[i].primary_text       = v.states[i].name.c_str();
        items[i].primary_text_align = LV_TEXT_ALIGN_CENTER;
        items[i].accessory = v.states[i].id == v.active ? PICKER_WHEEL_ITEM_ACCESSORY_CHECK
                                                        : PICKER_WHEEL_ITEM_ACCESSORY_NONE;
    }
    return items;
}

/* The speaker icon follows the highlighted state. */
static void refresh_volume_icon(const StatesView &v)
{
    const int selected = picker_wheel_get_selected(v.picker);
    const bool volume = selected >= 0 && selected < (int)v.states.size() && v.states[selected].hasVolume();
    header_set_right_visible(v.header, volume);
}

static bool same_states(const std::vector<State> &a, const std::vector<State> &b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (a[i].id != b[i].id || a[i].name != b[i].name || a[i].hasVolume() != b[i].hasVolume()) return false;
    return true;
}

/* Rebuilds the rows in place when the states or the active one changed, keeping the highlighted state. */
static void refresh_states_view(StatesView &v)
{
    std::vector<State> states = read_states(v.binding);
    const std::string active = active_state(v.binding);
    if (!states.empty() && (!same_states(states, v.states) || active != v.active))
    {
        const int selected = picker_wheel_get_selected(v.picker);
        const std::string selectedId = (selected >= 0 && selected < (int)v.states.size()) ? v.states[selected].id
                                                                                          : std::string();
        v.states = std::move(states);
        v.active = active;
        int newSelected = 0;
        for (size_t i = 0; i < v.states.size(); i++)
            if (v.states[i].id == selectedId) newSelected = (int)i;
        const std::vector<picker_wheel_item_t> items = make_state_items(v);
        picker_wheel_set_items(v.picker, items.data(), (int)items.size(), newSelected);
    }
    else if (!states.empty())
    {
        v.states = std::move(states);   /* same rows; channels/volume settings may still have changed */
    }
    refresh_volume_icon(v);
}

static void states_activated_cb(int index, void *user_data)
{
    StatesView &v = **static_cast<std::shared_ptr<StatesView> *>(user_data);
    if (index < 0 || index >= (int)v.states.size()) return;
    if (auto input = lea_input())
        input->applyState(v.binding, v.states[index].id);
    refresh_states_view(v);
}

static void states_changed_cb(int, void *user_data)
{
    refresh_volume_icon(**static_cast<std::shared_ptr<StatesView> *>(user_data));
}

static void states_volume_cb(lv_event_t *e)
{
    const StatesView &v = **static_cast<std::shared_ptr<StatesView> *>(lv_event_get_user_data(e));
    const int selected = picker_wheel_get_selected(v.picker);
    if (selected < 0 || selected >= (int)v.states.size() || !v.states[selected].hasVolume()) return;
    state_volume_open(v.binding, v.states[selected]);
}

static void states_view_delete_cb(lv_event_t *e)
{
    delete static_cast<std::shared_ptr<StatesView> *>(lv_event_get_user_data(e));
}

void tile_input_open(const std::string &name, const std::string &binding)
{
    (void)name;
    auto view = std::make_shared<StatesView>();
    view->binding = binding;
    view->states  = read_states(binding);
    view->active  = active_state(binding);
    if (view->states.empty()) return;
    auto *holder = new std::shared_ptr<StatesView>(view);

    nav_page_t page = nav_screen_create();
    const header_slot_t back   = {true, UI_ICON_ARROW_BACK, nav_back_cb, nullptr};
    const header_slot_t volume = {true, UI_ICON_VOLUME_UP, states_volume_cb, holder};
    view->header = header_create(page.screen, back, "States", volume);

    int selected = 0;
    for (size_t i = 0; i < view->states.size(); i++)
        if (view->states[i].id == view->active) selected = (int)i;

    std::vector<picker_wheel_item_t> items = make_state_items(*view);
    picker_wheel_props_t props = {};
    props.type                   = PICKER_WHEEL_TYPE_CENTER_CHECK;
    props.items                  = items.data();
    props.item_count             = (int)items.size();
    props.selected_index         = selected;
    props.activated_callback     = states_activated_cb;
    props.value_changed_callback = states_changed_cb;
    props.user_data              = holder;
    view->picker = picker_wheel_create(page.screen, &props);
    lv_obj_t *picker = view->picker;
    lv_obj_align(picker, LV_ALIGN_TOP_LEFT, 0, UI_HEADER_H_PX);
    lv_obj_add_event_cb(picker, states_view_delete_cb, LV_EVENT_DELETE, holder);
    refresh_volume_icon(*view);

    tile_bind_update(picker, [view]() { refresh_states_view(*view); });

    picker_wheel_attach_encoder(picker, page.focus);
    nav_push(page);
}

/* ---- Tile ---- */

void tile_input_build(lv_obj_t *root, const std::string &name,
                      const std::string &binding, int w, int h)
{
    const bool tall  = h >= ui_pane_h();
    const bool wide  = w >= ui_content_w();
    const bool short_ = h < 150;
    const int  pad   = tall ? 36 : 16;
    const int  pad_v = short_ ? 12 : pad;

    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root,
                          tall ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_left(root, pad, 0);
    lv_obj_set_style_pad_right(root, pad, 0);
    lv_obj_set_style_pad_top(root, pad_v, 0);
    lv_obj_set_style_pad_bottom(root, pad_v, 0);
    lv_obj_set_style_pad_gap(root, tall ? 24 : 0, 0);

    tile_adaptive_label(root, name, &satoshi_medium_24, ui_theme_get_style_text_primary(),
                        w - 2 * pad);

    /* Active state name: marquee when it does not fit. */
    const lv_font_t *stateFont = wide ? &satoshi_medium_48 : &satoshi_medium_32;
    const int stateMaxW = w - 2 * pad;
    lv_obj_t *stateLabel = tile_adaptive_label(root, "-", stateFont, ui_theme_get_style_brand(), stateMaxW);

    lv_obj_t *row = tile_container(root);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(row, 12, 0);

    lv_obj_t *icon = UI_ICON_CREATE_DEFAULT(row, UI_ICON_VOLUME_SMALL);
    if (icon) lv_obj_set_size(icon, 18, 18);
    lv_obj_t *levelLabel = tile_label(row, "-", &satoshi_medium_24, ui_theme_get_style_text_primary());

    /* Follows the active state, its name and level (the config and /leaInput/active can change any time). */
    auto shownState = std::make_shared<std::string>("-");
    auto shownLevel = std::make_shared<std::string>("-");
    auto refresh = [stateLabel, stateFont, stateMaxW, levelLabel, binding, shownState, shownLevel]() {
        std::string stateText = "-", levelText = "-";
        State state;
        if (find_state(binding, active_state(binding), state))
        {
            stateText = state.name;
            if (state.hasVolume())
                levelText = fader_level_text(fadersLeaf, InputStates::stateFader(state));
        }
        if (stateText != *shownState)
        {
            *shownState = stateText;
            /* sized to its text, like tile_adaptive_label() (+2px so an exact fit doesn't marquee) */
            lv_obj_set_width(stateLabel, std::min(tile_text_width(stateText, stateFont) + 2, stateMaxW));
            ui_adaptive_text_set(stateLabel, stateText.c_str(), 1);
        }
        if (levelText != *shownLevel)
        {
            *shownLevel = levelText;
            lv_label_set_text(levelLabel, levelText.c_str());
        }
    };
    refresh();
    tile_bind_update(levelLabel, refresh);
}
