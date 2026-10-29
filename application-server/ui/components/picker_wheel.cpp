/**
 * @file picker_wheel.c
 * @brief Picker Wheel component implementation
 * 
 * Implementation based on Figma design specifications (Node: 5609:12037):
 * - Container: 480x463px (full width, centered vertically)
 * - Selected highlight: 432x72px with 2px cyan border and 10% cyan background
 * - Items: 432x72px with 40px spacing
 * - Font: Satoshi Medium 24px (using Montserrat as fallback)
 * - Selected text: cyan (#5DE9FF)
 * - Normal text: white (#FAFAFA) in dark / black (#000000) in light - theme-aware
 * - Background: black (#000000) in dark / white (#FFFFFF) in light - theme-aware
 * - Top fade gradient: 196px height, background color (opaque) to transparent - theme-aware
 * - Bottom fade gradient: 196px height, transparent to background color (opaque) - theme-aware
 * 
 * Note: Gradients are implemented using LVGL canvas with ARGB8888 format for
 * proper alpha channel support, as standard LVGL gradients don't support
 * opacity transitions. Gradient colors adapt to the current theme.
 */

#include "picker_wheel.h"
#include "adaptive_text.h"
#include "../ui_design_tokens.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FONT_PICKER_WHEEL &satoshi_medium_24

// Layout dimensions (using design tokens where applicable)
// Figma Node 5609:12037 specifies a 480x463px container in portrait. The width
// follows the content area; in landscape the height is capped to the pane
// height so the picker still fits below the header.
#define PICKER_WIDTH            ui_content_w()
#define PICKER_HEIGHT           (ui_is_landscape() ? UI_LANDSCAPE_PANE_H_PX : 463)
#define ITEM_WIDTH              432
#define ITEM_HEIGHT             72
#define ITEM_SPACING            16
#define ITEM_PITCH              (ITEM_HEIGHT + ITEM_SPACING)
#define ITEM_RADIUS             8
#define HIGHLIGHT_BORDER        2
#define FADE_HEIGHT             196
#define VISIBLE_ROWS            10
#define ITEM_TEXT_PAD_X         24
#define ACCESSORY_SLOT_WIDTH    40
#define ACCESSORY_GLYPH_SIZE    28
#define SECONDARY_TEXT_OFFSET_Y 14
#define RIGHT_CONTENT_SHIFT_X   8

// Component data structure
typedef struct {
    picker_wheel_props_t props;
    lv_obj_t* scroll_area;
    lv_obj_t* highlight_box;
    picker_wheel_item_t* items_copy;  // Local copy of items
    lv_obj_t** item_rows;
    lv_obj_t** primary_labels;
    lv_obj_t** secondary_labels;
    lv_obj_t** accessory_objs;
    lv_obj_t* top_fade;       // Top gradient canvas
    lv_obj_t* bottom_fade;    // Bottom gradient canvas
    void* top_fade_buf;
    void* bottom_fade_buf;
    int touch_pressed_index;
    bool external_press_active;
} picker_wheel_data_t;

// Forward declarations
static void picker_wheel_scroll_cb(lv_event_t* e);
static void picker_wheel_item_clicked_cb(lv_event_t* e);
static void picker_wheel_item_press_state_cb(lv_event_t* e);
static bool is_alt_highlight_active(const picker_wheel_data_t* data);
static lv_color_t selected_accent_color(const picker_wheel_data_t* data);
static void refresh_highlight_box_color(picker_wheel_data_t* data);
static void refresh_selected_accent(picker_wheel_data_t* data);
static void update_fade_gradient(lv_obj_t* canvas, int width, int height, bool top_fade);
static void create_fade_gradient(lv_obj_t* canvas, int width, int height, bool top_fade, void** out_buf);
static void theme_changed_cb(lv_event_t* e);
static void picker_wheel_delete_cb(lv_event_t* e);
static lv_text_align_t normalize_text_align(lv_text_align_t align);
static bool accessory_on_left(const picker_wheel_props_t* props);
static int get_selected_index_from_scroll(const picker_wheel_data_t* data);
static void refresh_item_visuals(picker_wheel_data_t* data, int index, bool selected);
static void refresh_all_item_visuals(picker_wheel_data_t* data);
static void set_selected_index(picker_wheel_data_t* data, int index, bool notify);
static void scroll_to_index(picker_wheel_data_t* data, int index, bool anim);
static void emit_item_callback(picker_wheel_data_t* data, int index);
static lv_obj_t* create_item_accessory(lv_obj_t* parent, const picker_wheel_data_t* data,
                                       const picker_wheel_item_t* item);
static void create_picker_item(lv_obj_t* parent, picker_wheel_data_t* data, int index);

/* Empty block above the first / below the last row so those rows can scroll to the centre. */
static void add_scroll_spacer(lv_obj_t* scroll_area) {
    lv_obj_t* spacer = lv_obj_create(scroll_area);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, ITEM_WIDTH, FADE_HEIGHT);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_CLICKABLE);
}

/**
 * Create the picker wheel component
 */
lv_obj_t* picker_wheel_create(lv_obj_t* parent, const picker_wheel_props_t* props) {
    if (!props || !props->items || props->item_count <= 0) {
        return NULL;
    }
    
    // Main container
    lv_obj_t* container = lv_obj_create(parent);
    lv_obj_set_size(container, PICKER_WIDTH, PICKER_HEIGHT);
    lv_obj_add_style(container, ui_theme_get_style_bg(), 0);  // Theme-aware background
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_set_style_radius(container, 0, 0);
    // lv_obj_center(container);
    
    // Allocate and store component data
    picker_wheel_data_t* data = (picker_wheel_data_t*)malloc(sizeof(picker_wheel_data_t));
    if (!data) {
        lv_obj_delete(container);
        return NULL;
    }
    memcpy(&data->props, props, sizeof(picker_wheel_props_t));
    data->scroll_area = NULL;
    data->highlight_box = NULL;
    data->top_fade = NULL;
    data->bottom_fade = NULL;
    data->top_fade_buf = NULL;
    data->bottom_fade_buf = NULL;
    data->items_copy = NULL;
    data->item_rows = NULL;
    data->primary_labels = NULL;
    data->secondary_labels = NULL;
    data->accessory_objs = NULL;
    data->touch_pressed_index = -1;
    data->external_press_active = false;
    
    // Copy items array
    data->items_copy = (picker_wheel_item_t*)malloc(sizeof(picker_wheel_item_t) * props->item_count);
    if (!data->items_copy) {
        free(data);
        lv_obj_delete(container);
        return NULL;
    }
    memcpy(data->items_copy, props->items, sizeof(picker_wheel_item_t) * props->item_count);
    data->props.items = data->items_copy;

    data->item_rows = (lv_obj_t**)calloc((size_t)props->item_count, sizeof(lv_obj_t*));
    data->primary_labels = (lv_obj_t**)calloc((size_t)props->item_count, sizeof(lv_obj_t*));
    data->secondary_labels = (lv_obj_t**)calloc((size_t)props->item_count, sizeof(lv_obj_t*));
    data->accessory_objs = (lv_obj_t**)calloc((size_t)props->item_count, sizeof(lv_obj_t*));
    if (!data->item_rows || !data->primary_labels || !data->secondary_labels || !data->accessory_objs) {
        free(data->accessory_objs);
        free(data->secondary_labels);
        free(data->primary_labels);
        free(data->item_rows);
        free(data->items_copy);
        free(data);
        lv_obj_delete(container);
        return NULL;
    }

    lv_obj_set_user_data(container, data);
    
    // Create highlight box (selected item indicator)
    data->highlight_box = lv_obj_create(container);
    lv_obj_set_size(data->highlight_box, ITEM_WIDTH, ITEM_HEIGHT);
    lv_obj_align(data->highlight_box, LV_ALIGN_TOP_MID, 0, FADE_HEIGHT);
    // Use theme-aware brand color (cyan for dark mode, indigo for light mode)
    lv_obj_add_style(data->highlight_box, ui_theme_get_style_brand(), 0);
    lv_obj_set_style_bg_opa(data->highlight_box, OPACITY_BRAND_BG_LIGHT, 0);  // 10% opacity
    lv_obj_set_style_border_width(data->highlight_box, HIGHLIGHT_BORDER, 0);
    lv_obj_set_style_radius(data->highlight_box, ITEM_RADIUS, 0);
    lv_obj_add_flag(data->highlight_box, LV_OBJ_FLAG_FLOATING);  // Don't affect layout
    lv_obj_clear_flag(data->highlight_box, LV_OBJ_FLAG_CLICKABLE);

    data->scroll_area = lv_obj_create(container);
    lv_obj_remove_style_all(data->scroll_area);
    lv_obj_set_size(data->scroll_area, ITEM_WIDTH, PICKER_HEIGHT);
    lv_obj_align(data->scroll_area, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_scroll_dir(data->scroll_area, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(data->scroll_area, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(data->scroll_area, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(data->scroll_area, 0, 0);
    lv_obj_set_style_bg_opa(data->scroll_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(data->scroll_area, 0, 0);
    lv_obj_set_layout(data->scroll_area, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(data->scroll_area, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(data->scroll_area, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(data->scroll_area, picker_wheel_scroll_cb, LV_EVENT_SCROLL, data);
    lv_obj_add_event_cb(data->scroll_area, picker_wheel_scroll_cb, LV_EVENT_SCROLL_END, data);

    lv_obj_t* top_spacer = lv_obj_create(data->scroll_area);
    lv_obj_remove_style_all(top_spacer);
    lv_obj_set_size(top_spacer, ITEM_WIDTH, FADE_HEIGHT);
    lv_obj_clear_flag(top_spacer, LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i < props->item_count; i++) {
        create_picker_item(data->scroll_area, data, i);
    }

    lv_obj_t* bottom_spacer = lv_obj_create(data->scroll_area);
    lv_obj_remove_style_all(bottom_spacer);
    lv_obj_set_size(bottom_spacer, ITEM_WIDTH, FADE_HEIGHT);
    lv_obj_clear_flag(bottom_spacer, LV_OBJ_FLAG_CLICKABLE);
    
    // Create top fade gradient canvas (background color to transparent, top 196px)
    data->top_fade = lv_canvas_create(container);
    lv_obj_align(data->top_fade, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_flag(data->top_fade, LV_OBJ_FLAG_FLOATING);  // Don't affect layout
    lv_obj_clear_flag(data->top_fade, LV_OBJ_FLAG_CLICKABLE);  // Allow clicks to pass through
    create_fade_gradient(data->top_fade, PICKER_WIDTH, FADE_HEIGHT, true, &data->top_fade_buf);
    
    // Create bottom fade gradient canvas (transparent to background color, bottom 196px)
    data->bottom_fade = lv_canvas_create(container);
    lv_obj_align(data->bottom_fade, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(data->bottom_fade, LV_OBJ_FLAG_FLOATING);  // Don't affect layout
    lv_obj_clear_flag(data->bottom_fade, LV_OBJ_FLAG_CLICKABLE);  // Allow clicks to pass through
    create_fade_gradient(data->bottom_fade, PICKER_WIDTH, FADE_HEIGHT, false, &data->bottom_fade_buf);

    lv_obj_update_layout(data->scroll_area);
    set_selected_index(data, props->selected_index, false);
    scroll_to_index(data, data->props.selected_index, false);
    refresh_all_item_visuals(data);
    refresh_highlight_box_color(data);
    
    // Listen for theme changes to update gradients automatically. This
    // fires because container itself has ui_theme_get_style_bg() applied
    // above - lv_obj_report_style_change(&styleBg) targets every object
    // using that exact style object, container included.
    lv_obj_add_event_cb(container, theme_changed_cb, LV_EVENT_STYLE_CHANGED, NULL);
    
    return container;
}

/**
 * Set the selected item
 */
void picker_wheel_set_selected(lv_obj_t* picker, int index, bool anim) {
    if (!picker) return;
    
    picker_wheel_data_t* data = (picker_wheel_data_t*)lv_obj_get_user_data(picker);
    if (!data || index < 0 || index >= data->props.item_count) return;
    
    set_selected_index(data, index, true);
    scroll_to_index(data, index, anim);
}

/**
 * Replace every item in place
 */
bool picker_wheel_set_items(lv_obj_t* picker, const picker_wheel_item_t* items,
                            int item_count, int selected_index) {
    if (!picker || !items || item_count <= 0) return false;

    picker_wheel_data_t* data = (picker_wheel_data_t*)lv_obj_get_user_data(picker);
    if (!data || !data->scroll_area) return false;

    size_t n = (size_t)item_count;
    picker_wheel_item_t* new_items = (picker_wheel_item_t*)malloc(sizeof(picker_wheel_item_t) * n);
    lv_obj_t** new_rows = (lv_obj_t**)calloc(n, sizeof(lv_obj_t*));
    lv_obj_t** new_primary = (lv_obj_t**)calloc(n, sizeof(lv_obj_t*));
    lv_obj_t** new_secondary = (lv_obj_t**)calloc(n, sizeof(lv_obj_t*));
    lv_obj_t** new_accessory = (lv_obj_t**)calloc(n, sizeof(lv_obj_t*));
    if (!new_items || !new_rows || !new_primary || !new_secondary || !new_accessory) {
        free(new_items);
        free(new_rows);
        free(new_primary);
        free(new_secondary);
        free(new_accessory);
        return false;
    }
    memcpy(new_items, items, sizeof(picker_wheel_item_t) * n);

    /* Install the new (still empty) arrays before touching the widgets, so scroll events
     * fired while the rows are replaced only ever see a consistent state. */
    free(data->items_copy);
    free(data->item_rows);
    free(data->primary_labels);
    free(data->secondary_labels);
    free(data->accessory_objs);
    data->items_copy = new_items;
    data->item_rows = new_rows;
    data->primary_labels = new_primary;
    data->secondary_labels = new_secondary;
    data->accessory_objs = new_accessory;
    data->props.items = new_items;
    data->props.item_count = item_count;
    data->props.selected_index = selected_index < 0 ? 0
                               : selected_index >= item_count ? item_count - 1 : selected_index;
    data->touch_pressed_index = -1;

    lv_obj_clean(data->scroll_area);
    add_scroll_spacer(data->scroll_area);
    for (int i = 0; i < item_count; i++) {
        create_picker_item(data->scroll_area, data, i);
    }
    add_scroll_spacer(data->scroll_area);

    lv_obj_update_layout(data->scroll_area);
    scroll_to_index(data, data->props.selected_index, false);
    refresh_all_item_visuals(data);
    refresh_highlight_box_color(data);
    return true;
}

/**
 * Get the currently selected item
 */
int picker_wheel_get_selected(lv_obj_t* picker) {
    if (!picker) return -1;
    
    picker_wheel_data_t* data = (picker_wheel_data_t*)lv_obj_get_user_data(picker);
    if (!data) return -1;
    
    return data->props.selected_index;
}

/**
 * Update item's checkmark state
 */
void picker_wheel_update_item_check(lv_obj_t* picker, int index, bool show_check) {
    if (!picker) return;

    picker_wheel_data_t* data = (picker_wheel_data_t*)lv_obj_get_user_data(picker);
    if (!data || index < 0 || index >= data->props.item_count) return;

    data->items_copy[index].accessory = show_check
        ? PICKER_WHEEL_ITEM_ACCESSORY_CHECK
        : PICKER_WHEEL_ITEM_ACCESSORY_NONE;

    if (data->item_rows[index]) {
        if (data->accessory_objs[index]) {
            lv_obj_delete(data->accessory_objs[index]);
            data->accessory_objs[index] = NULL;
        }
        data->accessory_objs[index] = create_item_accessory(data->item_rows[index], data,
                                                            &data->items_copy[index]);
        refresh_item_visuals(data, index, index == data->props.selected_index);
    }
}

/**
 * Replace an item's secondary text. The item must have been created with non-empty secondary text.
 */
void picker_wheel_set_secondary_text(lv_obj_t* picker, int index, const char* text) {
    if (!picker) return;

    picker_wheel_data_t* data = (picker_wheel_data_t*)lv_obj_get_user_data(picker);
    if (!data || index < 0 || index >= data->props.item_count) return;

    if (data->secondary_labels[index]) {
        ui_adaptive_text_set(data->secondary_labels[index], text ? text : "", 1);
    }
}

static lv_text_align_t normalize_text_align(lv_text_align_t align) {
    switch (align) {
        case LV_TEXT_ALIGN_LEFT:
        case LV_TEXT_ALIGN_CENTER:
        case LV_TEXT_ALIGN_RIGHT:
            return align;
        case LV_TEXT_ALIGN_AUTO:
        default:
            return LV_TEXT_ALIGN_CENTER;
    }
}

static bool accessory_on_left(const picker_wheel_props_t* props) {
    return props->type == PICKER_WHEEL_TYPE_LEFT_ARROW ||
           props->type == PICKER_WHEEL_TYPE_LEFT_BORDER;
}

static int get_selected_index_from_scroll(const picker_wheel_data_t* data) {
    int32_t scroll_y = lv_obj_get_scroll_y(data->scroll_area);
    int index = (int)((scroll_y + (ITEM_PITCH / 2)) / ITEM_PITCH);

    if (index < 0) index = 0;
    if (index >= data->props.item_count) index = data->props.item_count - 1;
    return index;
}

static void refresh_item_visuals(picker_wheel_data_t* data, int index, bool selected) {
    if (!data || index < 0 || index >= data->props.item_count) return;

    picker_wheel_item_t* item = &data->items_copy[index];
    lv_color_t accent_color = selected_accent_color(data);
    lv_color_t primary_color = selected ? accent_color : ui_get_text_primary_color();
    lv_color_t secondary_color = selected ? accent_color : ui_get_text_secondary_color();
    lv_obj_t* row = data->item_rows[index];

    if (!row) return;

    /* The selected row sits under the highlight box, which already draws its
     * own accent-colored border - suppress the item's own border there so
     * the two don't visually stack. */
    uint16_t row_border_width = selected ? 0 : item->border_width;
    lv_obj_set_style_border_width(row, row_border_width, 0);
    if (row_border_width > 0) {
        lv_obj_set_style_border_color(row, item->border_color, 0);
    }

    if (data->primary_labels[index]) {
        lv_obj_set_style_text_color(data->primary_labels[index], primary_color, 0);
    }

    if (data->secondary_labels[index]) {
        lv_obj_set_style_text_color(data->secondary_labels[index], secondary_color, 0);
    }

    if (data->accessory_objs[index]) {
        lv_obj_set_style_text_color(data->accessory_objs[index], primary_color, 0);
    }
}

static void refresh_all_item_visuals(picker_wheel_data_t* data) {
    if (!data) return;

    for (int i = 0; i < data->props.item_count; i++) {
        refresh_item_visuals(data, i, i == data->props.selected_index);
    }
}

static void emit_item_callback(picker_wheel_data_t* data, int index) {
    if (index < 0 || index >= data->props.item_count) return;

    picker_wheel_item_t* item = &data->items_copy[index];
    if (item->callback) {
        item->callback(item->user_data);
    }
}

static void set_selected_index(picker_wheel_data_t* data, int index, bool notify) {
    if (!data) return;
    if (index < 0) index = 0;
    if (index >= data->props.item_count) index = data->props.item_count - 1;

    if (index == data->props.selected_index) {
        refresh_all_item_visuals(data);
        refresh_highlight_box_color(data);
        return;
    }

    int previous_index = data->props.selected_index;
    data->props.selected_index = index;

    if (previous_index >= 0 && previous_index < data->props.item_count) {
        refresh_item_visuals(data, previous_index, false);
    }
    refresh_item_visuals(data, index, true);
    refresh_highlight_box_color(data);

    if (notify) {
        if (data->props.value_changed_callback) {
            data->props.value_changed_callback(index, data->props.user_data);
        }
        emit_item_callback(data, index);
    }
}

static void refresh_highlight_box_color(picker_wheel_data_t* data) {
    if (!data || !data->highlight_box) return;

    lv_color_t color = selected_accent_color(data);
    lv_obj_set_style_border_color(data->highlight_box, color, 0);
    lv_obj_set_style_bg_color(data->highlight_box, color, 0);
}

static void refresh_selected_accent(picker_wheel_data_t* data) {
    if (!data) return;

    refresh_highlight_box_color(data);
    if (data->props.selected_index >= 0 && data->props.selected_index < data->props.item_count) {
        refresh_item_visuals(data, data->props.selected_index, true);
    }
}

static bool is_alt_highlight_active(const picker_wheel_data_t* data) {
    if (!data) return false;

    return data->props.pressed_alt_enabled &&
           (data->props.pressed_alt_index >= 0) &&
           (data->props.selected_index == data->props.pressed_alt_index) &&
           (data->external_press_active ||
            data->touch_pressed_index == data->props.selected_index);
}

static lv_color_t selected_accent_color(const picker_wheel_data_t* data) {
    if (is_alt_highlight_active(data)) {
        return data->props.pressed_alt_color;
    }
    return ui_get_brand_color();
}

static void scroll_to_index(picker_wheel_data_t* data, int index, bool anim) {
    if (!data || !data->scroll_area) return;
    lv_obj_scroll_to_y(data->scroll_area, index * ITEM_PITCH, anim ? LV_ANIM_ON : LV_ANIM_OFF);
}

static lv_obj_t* create_item_accessory(lv_obj_t* parent, const picker_wheel_data_t* data,
                                       const picker_wheel_item_t* item) {
    if (!item || item->accessory == PICKER_WHEEL_ITEM_ACCESSORY_NONE) {
        return NULL;
    }

    bool left = accessory_on_left(&data->props);
    ui_icon_type_t icon_type = UI_ICON_CHECK_SMALL;
    const char* fallback_symbol = LV_SYMBOL_OK;

    if (item->accessory == PICKER_WHEEL_ITEM_ACCESSORY_ARROW) {
        icon_type = left ? UI_ICON_ARROW_BACK : UI_ICON_CHEVRON_FORWARD;
        fallback_symbol = left ? LV_SYMBOL_LEFT : LV_SYMBOL_RIGHT;
    }

    lv_obj_t* icon = ui_icon_create(parent, ui_icon_config_t{
        icon_type, fallback_symbol
    });
    if (!icon) return NULL;

    lv_obj_set_size(icon, ACCESSORY_GLYPH_SIZE, ACCESSORY_GLYPH_SIZE);
    int x_ofs = left ? ITEM_TEXT_PAD_X : -(ITEM_TEXT_PAD_X - RIGHT_CONTENT_SHIFT_X);
    lv_obj_align(icon, left ? LV_ALIGN_LEFT_MID : LV_ALIGN_RIGHT_MID,
                 x_ofs, 0);
    return icon;
}

static void create_picker_item(lv_obj_t* parent, picker_wheel_data_t* data, int index) {
    picker_wheel_item_t* item = &data->items_copy[index];
    bool left = accessory_on_left(&data->props);
    bool has_accessory = item->accessory != PICKER_WHEEL_ITEM_ACCESSORY_NONE;
    bool has_secondary = item->secondary_text && item->secondary_text[0] != '\0';
    int text_x = ITEM_TEXT_PAD_X;
    int text_width = ITEM_WIDTH - (ITEM_TEXT_PAD_X * 2);
    int primary_width = text_width;
    int secondary_x = text_x;
    int secondary_width = text_width;

    if (has_accessory) {
        text_width -= ACCESSORY_SLOT_WIDTH;
        if (left) {
            text_x += ACCESSORY_SLOT_WIDTH;
        }
    }
    if (data->props.type == PICKER_WHEEL_TYPE_CENTER_CHECK) {
        /* Centered text stays put whether or not a row has its check: keep both sides clear. */
        text_x = ITEM_TEXT_PAD_X + ACCESSORY_SLOT_WIDTH;
        text_width = ITEM_WIDTH - (ITEM_TEXT_PAD_X + ACCESSORY_SLOT_WIDTH) * 2;
        primary_width = text_width;
        secondary_x = text_x;
        secondary_width = text_width;
    }

    if (has_secondary) {
        primary_width = text_width / 2;
        secondary_x = text_x + primary_width;
        secondary_width = text_width - primary_width;
        if (!left) {
            secondary_x += RIGHT_CONTENT_SHIFT_X;
        }
    }

    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, ITEM_WIDTH, ITEM_HEIGHT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(row, ITEM_RADIUS, 0);
    /* The selected row sits under the highlight box, which already draws its
     * own accent-colored border - suppress the item's own border there so
     * the two don't visually stack. */
    bool is_selected_row = (index == data->props.selected_index);
    uint16_t row_border_width = is_selected_row ? 0 : item->border_width;
    lv_obj_set_style_border_width(row, row_border_width, 0);
    if (row_border_width > 0) {
        lv_obj_set_style_border_color(row, item->border_color, 0);
    }
    if (index < data->props.item_count - 1) {
        lv_obj_set_style_margin_bottom(row, ITEM_SPACING, 0);
    }
    lv_obj_add_flag(row, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SNAPPABLE));
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(row, picker_wheel_item_press_state_cb, LV_EVENT_PRESSED, data);
    lv_obj_add_event_cb(row, picker_wheel_item_press_state_cb, LV_EVENT_RELEASED, data);
    lv_obj_add_event_cb(row, picker_wheel_item_press_state_cb, LV_EVENT_PRESS_LOST, data);
    lv_obj_add_event_cb(row, picker_wheel_item_clicked_cb, LV_EVENT_CLICKED, data);

    lv_obj_t* primary = lv_label_create(row);
    lv_obj_set_width(primary, primary_width);
    lv_obj_set_style_text_font(primary, FONT_PICKER_WHEEL, 0);
    lv_obj_set_style_text_align(primary, normalize_text_align(item->primary_text_align), 0);
    ui_adaptive_text_set(primary, item->primary_text ? item->primary_text : "", 1);   // marquee when it doesn't fit
    lv_obj_align(primary, LV_ALIGN_LEFT_MID, text_x, 0);

    lv_obj_t* secondary = NULL;
    if (has_secondary) {
        secondary = lv_label_create(row);
        lv_obj_set_width(secondary, secondary_width);
        lv_obj_set_style_text_font(secondary, FONT_PICKER_WHEEL, 0);
        lv_obj_set_style_text_align(secondary, normalize_text_align(item->secondary_text_align), 0);
        ui_adaptive_text_set(secondary, item->secondary_text, 1);   // marquee when it doesn't fit
        lv_obj_align(secondary, LV_ALIGN_LEFT_MID, secondary_x, 0);
    }

    data->item_rows[index] = row;
    data->primary_labels[index] = primary;
    data->secondary_labels[index] = secondary;
    data->accessory_objs[index] = create_item_accessory(row, data, item);
}

/**
 * Update fade gradient pixels with current theme color
 * @param canvas Canvas object to update
 * @param width Width of the gradient
 * @param height Height of the gradient
 * @param top_fade true for top fade (opaque to transparent), false for bottom fade (transparent to opaque)
 */
static void update_fade_gradient(lv_obj_t* canvas, int width, int height, bool top_fade) {
    if (!canvas) return;
    
    // Get the existing canvas buffer
    lv_draw_buf_t* draw_buf = lv_canvas_get_draw_buf(canvas);
    if (!draw_buf || !draw_buf->data) return;
    
    uint32_t* pixels = (uint32_t*)draw_buf->data;
    
    // Get current theme background color (full RGB: custom themes aren't grayscale)
    lv_color_t bg_color = ui_get_background_color();
    uint32_t bg_rgb = ((uint32_t)bg_color.red << 16) | ((uint32_t)bg_color.green << 8) | bg_color.blue;  // 0x00RRGGBB
    
    // Update gradient pixel by pixel with new color
    for (int y = 0; y < height; y++) {
        // Calculate alpha based on position
        uint8_t alpha;
        if (top_fade) {
            // Top fade: opaque (255) at top to transparent (0) at bottom
            alpha = (uint8_t)(255 * (height - y) / height);
        } else {
            // Bottom fade: transparent (0) at top to opaque (255) at bottom
            alpha = (uint8_t)(255 * y / height);
        }
        
        // Create ARGB8888 pixel value (background color with varying alpha)
        uint32_t pixel = ((uint32_t)alpha << 24) | bg_rgb;  // AARRGGBB format
        
        // Fill the entire row with this pixel
        for (int x = 0; x < width; x++) {
            pixels[y * width + x] = pixel;
        }
    }
    
    // Refresh the canvas
    lv_obj_invalidate(canvas);
}

/**
 * Create a fade gradient using canvas (initial setup)
 * @param canvas Canvas object to draw on
 * @param width Width of the gradient
 * @param height Height of the gradient
 * @param top_fade true for top fade (opaque to transparent), false for bottom fade (transparent to opaque)
 */
static void create_fade_gradient(lv_obj_t* canvas, int width, int height, bool top_fade, void** out_buf) {
    // Allocate buffer for canvas (ARGB8888 format for full alpha support)
    // Allocate new buffer (4 bytes per pixel for ARGB8888)
    size_t buf_size = width * height * 4;
    lv_color_t* cbuf = (lv_color_t*)malloc(buf_size);
    
    if (!cbuf) {
        return;  // Allocation failed
    }

    if (out_buf) {
        *out_buf = cbuf;
    }
    
    // Set canvas buffer with ARGB8888 format
    lv_canvas_set_buffer(canvas, cbuf, width, height, LV_COLOR_FORMAT_ARGB8888);
    
    // Use update function to fill the pixels (reuse logic)
    update_fade_gradient(canvas, width, height, top_fade);
}

/**
 * Roller value changed callback
 */
static void picker_wheel_scroll_cb(lv_event_t* e) {
    picker_wheel_data_t* data = (picker_wheel_data_t*)lv_event_get_user_data(e);
    if (!data) return;

    set_selected_index(data, get_selected_index_from_scroll(data), true);
}

static void picker_wheel_item_clicked_cb(lv_event_t* e) {
    picker_wheel_data_t* data = (picker_wheel_data_t*)lv_event_get_user_data(e);
    if (!data) return;

    lv_obj_t* target = (lv_obj_t *)lv_event_get_target(e);
    int selected_index = -1;
    for (int i = 0; i < data->props.item_count; i++) {
        if (data->item_rows[i] == target) {
            selected_index = i;
            break;
        }
    }

    if (selected_index < 0) return;

    set_selected_index(data, selected_index, true);
    scroll_to_index(data, selected_index, true);

    if (data->props.activated_callback) {
        data->props.activated_callback(selected_index, data->props.user_data);
    }
}

static void picker_wheel_item_press_state_cb(lv_event_t* e) {
    picker_wheel_data_t* data = (picker_wheel_data_t*)lv_event_get_user_data(e);
    if (!data) return;

    lv_obj_t* target = (lv_obj_t *)lv_event_get_target(e);
    int index = -1;
    for (int i = 0; i < data->props.item_count; i++) {
        if (data->item_rows[i] == target) {
            index = i;
            break;
        }
    }

    if (index < 0) return;

    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        data->touch_pressed_index = index;
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        data->touch_pressed_index = -1;
    }

    refresh_selected_accent(data);
}

void picker_wheel_nudge(lv_obj_t* picker, int delta, bool anim) {
    if (!picker) return;
    picker_wheel_data_t* data = (picker_wheel_data_t*)lv_obj_get_user_data(picker);
    if (!data || data->props.item_count <= 0) return;

    int cur = picker_wheel_get_selected(picker);
    int next = cur + delta;
    if (next < 0) next = 0;
    if (next >= data->props.item_count) next = data->props.item_count - 1;
    picker_wheel_set_selected(picker, next, anim);
}

void picker_wheel_click(lv_obj_t* picker) {
    if (!picker) return;
    picker_wheel_data_t* data = (picker_wheel_data_t*)lv_obj_get_user_data(picker);
    if (!data) return;

    int sel = data->props.selected_index;

    if (data->props.activated_callback)
        data->props.activated_callback(sel, data->props.user_data);
    if (sel >= 0 && sel < data->props.item_count) {
        emit_item_callback(data, sel);
    }
}

void picker_wheel_set_press_active(lv_obj_t* picker, bool active) {
    if (!picker) return;

    picker_wheel_data_t* data = (picker_wheel_data_t*)lv_obj_get_user_data(picker);
    if (!data) return;

    data->external_press_active = active;
    refresh_selected_accent(data);
}

/**
 * Delete event callback - releases the component data and gradient buffers.
 * LV_EVENT_DELETE is sent before the children are deleted, which is the point
 * LVGL expects user-owned resources (such as canvas buffers) to be freed.
 */
static void picker_wheel_delete_cb(lv_event_t* e) {
    lv_obj_t* container = (lv_obj_t *)lv_event_get_target(e);
    picker_wheel_data_t* data = (picker_wheel_data_t*)lv_obj_get_user_data(container);
    if (!data) return;

    lv_obj_set_user_data(container, NULL);
    free(data->top_fade_buf);
    free(data->bottom_fade_buf);
    free(data->accessory_objs);
    free(data->secondary_labels);
    free(data->primary_labels);
    free(data->item_rows);
    free(data->items_copy);
    free(data);
}

/**
 * Theme changed event callback - updates gradients when theme changes
 */
static void theme_changed_cb(lv_event_t* e) {
    lv_obj_t* container = (lv_obj_t *)lv_event_get_target(e);
    if (!container) return;
    
    picker_wheel_data_t* data = (picker_wheel_data_t*)lv_obj_get_user_data(container);
    if (!data) return;
    
    // Update both gradient canvases with the new theme color
    // Reuses existing buffer - no memory allocation/deallocation needed
    if (data->top_fade) {
        update_fade_gradient(data->top_fade, PICKER_WIDTH, FADE_HEIGHT, true);
    }
    if (data->bottom_fade) {
        update_fade_gradient(data->bottom_fade, PICKER_WIDTH, FADE_HEIGHT, false);
    }

    refresh_all_item_visuals(data);
    refresh_highlight_box_color(data);
}
