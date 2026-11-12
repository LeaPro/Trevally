/**
 * @file info_list.cpp
 * @brief Label / value rows (see info_list.h).
 */

#include "info_list.h"
#include "adaptive_text.h"
#include "tile.h"
#include "../ui_design_tokens.h"
#include "../ui_icon_data.h"

#include <algorithm>
#include <cstring>

#define LIST_PAD_X_PX 24
#define LABEL_VALUE_GAP_PX 16

#define LABEL_FONT satoshi_medium_24
#define VALUE_FONT satoshi_medium_20

#define DROPDOWN_SYMBOL_GAP_PX 8
#define DROPDOWN_LIST_RADIUS_PX 8
#define DROPDOWN_LIST_PAD_X_PX 20
#define DROPDOWN_ITEM_H_PX INFO_LIST_ROW_H_PX
#define NO_MATCH_TEXT "-"

/* Each row is a container whose children are the label (0) and the value (1): a label,
 * or a dropdown on rows that have options. */
#define VALUE_CHILD 1

/* Kept in the list's user data. */
struct ListData {
    info_list_change_cb_t cb;
    void *user_data;
    int highlight;       /* encoder: the row a press would open, -1 if none */
    uint32_t openStart;  /* encoder: the option that was selected when the dropdown opened */
};

static lv_obj_t *value_widget(lv_obj_t *list, int index)
{
    lv_obj_t *row = lv_obj_get_child(list, index);
    return row ? lv_obj_get_child(row, VALUE_CHILD) : nullptr;
}

static bool is_dropdown(lv_obj_t *obj)
{
    return obj && lv_obj_has_class(obj, &lv_dropdown_class);
}

/* Selects the option with this text; shows "-" if there is none. */
static void select_option_text(lv_obj_t *dropdown, const char *text)
{
    const int32_t index = lv_dropdown_get_option_index(dropdown, text);
    if (index >= 0) {
        lv_dropdown_set_text(dropdown, nullptr);   /* show the selected option */
        lv_dropdown_set_selected(dropdown, (uint32_t)index);
    } else {
        lv_dropdown_set_text(dropdown, NO_MATCH_TEXT);   /* must outlive the dropdown */
    }
}

static void dropdown_changed_cb(lv_event_t *e)
{
    lv_obj_t *dropdown = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t *list = (lv_obj_t *)lv_event_get_user_data(e);
    lv_dropdown_set_text(dropdown, nullptr);   /* the user's pick replaces any "-" */
    const ListData *data = (const ListData *)lv_obj_get_user_data(list);
    if (data && data->cb)
        data->cb((int)lv_obj_get_index(lv_obj_get_parent(dropdown)), (int)lv_dropdown_get_selected(dropdown), data->user_data);
}

/* The open option list is not part of the row, so a press anywhere else on the page closes it. */
static void page_pressed_cb(lv_event_t *e)
{
    lv_obj_t *list = (lv_obj_t *)lv_event_get_user_data(e);
    for (uint32_t i = 0; i < lv_obj_get_child_count(list); i++) {
        lv_obj_t *dropdown = value_widget(list, (int)i);
        if (is_dropdown(dropdown) && lv_dropdown_is_open(dropdown))
            lv_dropdown_close(dropdown);
    }
}

static void list_deleted_cb(lv_event_t *e)
{
    lv_obj_t *list = (lv_obj_t *)lv_event_get_target(e);
    delete (ListData *)lv_obj_get_user_data(list);
    lv_obj_set_user_data(list, nullptr);
    lv_obj_remove_event_cb_with_user_data(lv_obj_get_screen(list), page_pressed_cb, list);
}

/* The option list that drops down from @p dropdown. It lives on the screen, outside the
 * row, so it is styled on its own. All colours come from the shared theme styles. */
static void style_option_list(lv_obj_t *dropdown)
{
    lv_obj_t *opts = lv_dropdown_get_list(dropdown);
    const int line_space = DROPDOWN_ITEM_H_PX - (int)lv_font_get_line_height(&VALUE_FONT);

    lv_obj_add_style(opts, ui_theme_get_style_bg_secondary(), 0);
    lv_obj_add_style(opts, ui_theme_get_style_text_primary(), 0);
    lv_obj_set_style_bg_opa(opts, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(opts, 0, 0);
    lv_obj_set_style_outline_width(opts, 0, 0);
    lv_obj_set_style_shadow_width(opts, 0, 0);
    lv_obj_set_style_radius(opts, DROPDOWN_LIST_RADIUS_PX, 0);
    lv_obj_set_style_pad_hor(opts, DROPDOWN_LIST_PAD_X_PX, 0);
    lv_obj_set_style_pad_ver(opts, line_space / 2, 0);   /* room for the first / last option's row */
    lv_obj_set_style_text_font(opts, &VALUE_FONT, 0);
    lv_obj_set_style_text_line_space(opts, line_space, 0);

    /* The selected / pressed option's row: drawn with the SELECTED part. */
    lv_obj_set_style_text_font(opts, &VALUE_FONT, LV_PART_SELECTED);
    lv_obj_set_style_text_line_space(opts, line_space, LV_PART_SELECTED);
    lv_obj_set_style_pad_hor(opts, DROPDOWN_LIST_PAD_X_PX, LV_PART_SELECTED);
    lv_obj_add_style(opts, ui_theme_get_style_brand(), LV_PART_SELECTED | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(opts, LV_OPA_20, LV_PART_SELECTED | LV_STATE_CHECKED);
    lv_obj_add_style(opts, ui_theme_get_style_bg_tertiary(), LV_PART_SELECTED | LV_STATE_PRESSED);
    lv_obj_add_style(opts, ui_theme_get_style_text_primary(), LV_PART_SELECTED | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(opts, LV_OPA_COVER, LV_PART_SELECTED | LV_STATE_PRESSED);
}

/* A dropdown that looks like a value label: brand text, right-aligned, with a chevron
 * pointing down. */
static lv_obj_t *create_dropdown(lv_obj_t *row, lv_obj_t *list, const char *options, const char *value, int width)
{
    lv_obj_t *dropdown = lv_dropdown_create(row);
    lv_group_remove_obj(dropdown);   /* touching it must not take the encoder away from the page, see info_list_attach_encoder() */
    lv_obj_remove_style_all(dropdown);
    lv_obj_add_style(dropdown, ui_theme_get_style_brand(), 0);
    lv_obj_set_style_text_font(dropdown, &VALUE_FONT, 0);
    lv_obj_set_style_text_align(dropdown, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_pad_column(dropdown, DROPDOWN_SYMBOL_GAP_PX, 0);
    lv_obj_set_size(dropdown, width, INFO_LIST_ROW_H_PX);   /* the whole row height is the touch target */

    lv_dropdown_set_options(dropdown, options);
    lv_dropdown_set_symbol(dropdown, ui_icon_data[UI_ICON_CHEVRON_FORWARD]);
    lv_obj_add_style(dropdown, ui_theme_get_style_image_icon_primary(), LV_PART_INDICATOR);
    lv_obj_set_style_transform_rotation(dropdown, 900, LV_PART_INDICATOR);   /* forward -> down */
    select_option_text(dropdown, value ? value : NO_MATCH_TEXT);

    style_option_list(dropdown);
    lv_obj_add_event_cb(dropdown, dropdown_changed_cb, LV_EVENT_VALUE_CHANGED, list);
    return dropdown;
}

lv_obj_t *info_list_create(lv_obj_t *parent, const info_row_t *rows, int count)
{
    /* The value label takes whatever the widest label leaves. */
    int widest = 0;
    for (int i = 0; i < count; i++)
        widest = std::max(widest, tile_text_width(rows[i].label, &LABEL_FONT));
    const int value_w = ui_content_w() - 2 * LIST_PAD_X_PX - widest - LABEL_VALUE_GAP_PX;

    lv_obj_t *list = tile_container(parent);
    lv_obj_set_size(list, ui_content_w(), LV_SIZE_CONTENT);
    lv_obj_align(list, LV_ALIGN_TOP_LEFT, 0, UI_HEADER_H_PX);
    lv_obj_set_layout(list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_hor(list, LIST_PAD_X_PX, 0);

    bool has_dropdown = false;
    for (int i = 0; i < count; i++) {
        lv_obj_t *row = tile_container(list);
        lv_obj_set_size(row, LV_PCT(100), INFO_LIST_ROW_H_PX);
        lv_obj_set_layout(row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        tile_label(row, rows[i].label, &LABEL_FONT, ui_theme_get_style_text_primary());

        if (rows[i].options) {
            create_dropdown(row, list, rows[i].options, rows[i].value, value_w);
            has_dropdown = true;
            continue;
        }
        lv_obj_t *value = lv_label_create(row);
        lv_obj_add_style(value, ui_theme_get_style_brand(), 0);
        lv_obj_set_style_text_font(value, &VALUE_FONT, 0);
        lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_width(value, value_w);
        ui_adaptive_text_set(value, rows[i].value, 1);
    }

    lv_obj_set_user_data(list, new ListData{nullptr, nullptr, -1, 0});
    lv_obj_add_event_cb(list, list_deleted_cb, LV_EVENT_DELETE, nullptr);
    if (has_dropdown)
        lv_obj_add_event_cb(lv_obj_get_screen(list), page_pressed_cb, LV_EVENT_PRESSED, list);
    return list;
}

void info_list_set_value(lv_obj_t *list, int index, const char *value)
{
    lv_obj_t *widget = value_widget(list, index);
    if (!widget) return;
    const char *text = value ? value : "";
    if (is_dropdown(widget)) {
        if (!lv_dropdown_is_open(widget))
            select_option_text(widget, text);
        return;
    }
    if (strcmp(lv_label_get_text(widget), text) != 0)
        ui_adaptive_text_set(widget, text, 1);
}

void info_list_set_options(lv_obj_t *list, int index, const char *options)
{
    lv_obj_t *dropdown = value_widget(list, index);
    if (!is_dropdown(dropdown) || !options || lv_dropdown_is_open(dropdown)) return;
    if (strcmp(lv_dropdown_get_options(dropdown), options) != 0)
        lv_dropdown_set_options(dropdown, options);
}

void info_list_set_change_cb(lv_obj_t *list, info_list_change_cb_t cb, void *user_data)
{
    ListData *data = (ListData *)lv_obj_get_user_data(list);
    if (!data) return;
    data->cb = cb;
    data->user_data = user_data;
}

/* ---- Encoder ---- */

/* The first dropdown row after @p from in direction @p step (+1 / -1); -1 if there is none. */
static int next_dropdown_row(lv_obj_t *list, int from, int step)
{
    const int count = (int)lv_obj_get_child_count(list);
    for (int i = from + step; i >= 0 && i < count; i += step)
        if (is_dropdown(value_widget(list, i)))
            return i;
    return -1;
}

static int open_row(lv_obj_t *list)
{
    for (int i = 0; i < (int)lv_obj_get_child_count(list); i++) {
        lv_obj_t *dropdown = value_widget(list, i);
        if (is_dropdown(dropdown) && lv_dropdown_is_open(dropdown))
            return i;
    }
    return -1;
}

static void set_highlight(lv_obj_t *list, ListData *data, int row)
{
    if (row == data->highlight) return;
    if (lv_obj_t *old = data->highlight >= 0 ? lv_obj_get_child(list, data->highlight) : nullptr) {
        lv_obj_remove_style(old, ui_theme_get_style_bg_tertiary(), 0);
        lv_obj_remove_local_style_prop(old, LV_STYLE_BG_OPA, 0);
        lv_obj_remove_local_style_prop(old, LV_STYLE_RADIUS, 0);
    }
    data->highlight = row;
    if (lv_obj_t *now = row >= 0 ? lv_obj_get_child(list, row) : nullptr) {
        lv_obj_add_style(now, ui_theme_get_style_bg_tertiary(), 0);
        lv_obj_set_style_bg_opa(now, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(now, DROPDOWN_LIST_RADIUS_PX, 0);
    }
}

/* Rotation moves the highlight between the dropdown rows; a press opens the highlighted
 * one. While one is open, rotation moves its selection and a press confirms it. */
static void encoder_cb(lv_event_t *e)
{
    lv_obj_t *list = (lv_obj_t *)lv_event_get_user_data(e);
    ListData *data = (ListData *)lv_obj_get_user_data(list);
    if (!data) return;

    const int open = open_row(list);
    lv_obj_t *dropdown = open >= 0 ? value_widget(list, open) : nullptr;

    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if (dropdown) {
            const uint32_t selected = lv_dropdown_get_selected(dropdown);
            lv_dropdown_close(dropdown);
            if (selected != data->openStart)
                lv_obj_send_event(dropdown, LV_EVENT_VALUE_CHANGED, nullptr);
        } else if (lv_obj_t *target = data->highlight >= 0 ? value_widget(list, data->highlight) : nullptr) {
            data->openStart = lv_dropdown_get_selected(target);
            lv_dropdown_open(target);
        }
        return;
    }

    const uint32_t key = lv_event_get_key(e);
    if (key != LV_KEY_RIGHT && key != LV_KEY_LEFT) return;
    const int step = key == LV_KEY_RIGHT ? 1 : -1;

    if (dropdown) {
        const int32_t count = (int32_t)lv_dropdown_get_option_count(dropdown);
        const int32_t selected = (int32_t)lv_dropdown_get_selected(dropdown) + step;
        if (selected >= 0 && selected < count)
            lv_dropdown_set_selected(dropdown, (uint32_t)selected);
    } else {
        const int next = next_dropdown_row(list, data->highlight, step);
        if (next >= 0)
            set_highlight(list, data, next);
    }
}

void info_list_attach_encoder(lv_obj_t *list, lv_obj_t *focus)
{
    ListData *data = (ListData *)lv_obj_get_user_data(list);
    if (!data || !focus) return;
    lv_obj_add_event_cb(focus, encoder_cb, LV_EVENT_KEY, list);
    lv_obj_add_event_cb(focus, encoder_cb, LV_EVENT_CLICKED, list);
    set_highlight(list, data, next_dropdown_row(list, -1, 1));
}
