/**
 * @file picker_wheel.h
 * @brief Picker Wheel component for LVGL
 * 
 * Component based on Figma design: LEA CS Touch Wall Panel UX/UI
 * Node: 5552:23537 - "Picker Wheel"
 * 
 * A vertically scrolling picker with:
 * - Central highlight box with brand color border
 * - Top and bottom fade gradients
 * - Checkmarks for checked items
 * - Center-aligned text
 * - Configurable options
 */

#ifndef PICKER_WHEEL_H
#define PICKER_WHEEL_H

#include "lvgl/lvgl.h"

typedef enum {
    PICKER_WHEEL_TYPE_CENTER_CHECK = 0,
    PICKER_WHEEL_TYPE_CENTER_ARROW,
    PICKER_WHEEL_TYPE_LEFT_ARROW,
    PICKER_WHEEL_TYPE_LEFT_BORDER,
} picker_wheel_type_t;

typedef enum {
    PICKER_WHEEL_ITEM_ACCESSORY_NONE = 0,
    PICKER_WHEEL_ITEM_ACCESSORY_CHECK,
    PICKER_WHEEL_ITEM_ACCESSORY_ARROW,
} picker_wheel_item_accessory_t;

/**
 * Picker wheel item structure
 */
typedef struct {
    const char* primary_text;                    // Main item text
    lv_text_align_t primary_text_align;         // LV_TEXT_ALIGN_LEFT/CENTER/RIGHT/AUTO
    const char* secondary_text;                 // Optional secondary text line
    lv_text_align_t secondary_text_align;       // LV_TEXT_ALIGN_LEFT/CENTER/RIGHT/AUTO
    picker_wheel_item_accessory_t accessory;    // Optional item accessory
    uint16_t border_width;                      // 0 disables the item border
    lv_color_t border_color;                    // Border color when border_width > 0
    void (*callback)(void*);                    // Callback when item is selected
    void* user_data;                            // User data passed to callback
} picker_wheel_item_t;

/**
 * Picker wheel component properties
 */
typedef struct {
    picker_wheel_type_t type;     // Visual/interaction variant
    picker_wheel_item_t* items;  // Array of picker items
    int item_count;              // Number of items
    int selected_index;          // Currently selected item index (0-based)

    /** Called every time the roller scrolls to a new item (index changed).
     *  Suitable for syncing a control block position or a live readout.
     *  Do NOT wire navigation here — it fires while dragging. */
    void (*value_changed_callback)(int selected_index, void* user_data);

    /** Called only on an explicit short tap/click on the roller, or via
     *  picker_wheel_click().  Safe to use for navigation / confirm actions. */
    void (*activated_callback)(int selected_index, void* user_data);

    /** Enables alternate pressed highlight behavior when true. */
    bool pressed_alt_enabled;

    /** Optional: row index that uses pressed_alt_color while selected and
     *  actively pressed (touch hold or external press-active state). */
    int pressed_alt_index;

    /** Alternate highlight color used with pressed_alt_index while pressed. */
    lv_color_t pressed_alt_color;

    void* user_data;             // User data passed to both callbacks
} picker_wheel_props_t;

/**
 * @brief Create a picker wheel component
 * @param parent Parent object (usually lv_screen_active())
 * @param props Component properties and callbacks
 * @return Pointer to the created picker wheel container object
 */
lv_obj_t* picker_wheel_create(lv_obj_t* parent, const picker_wheel_props_t* props);

/**
 * @brief Set the selected item index
 * @param picker Picker wheel object
 * @param index Index of the item to select (0-based)
 * @param anim Enable animation (true/false)
 */
void picker_wheel_set_selected(lv_obj_t* picker, int index, bool anim);

/**
 * @brief Get the currently selected item index
 * @param picker Picker wheel object
 * @return Currently selected item index (0-based)
 */
int picker_wheel_get_selected(lv_obj_t* picker);

/**
 * @brief Update item's accessory to a checkmark or remove it
 * @param picker Picker wheel object
 * @param index Index of the item to update
 * @param show_check Show or hide checkmark accessory
 */
void picker_wheel_update_item_check(lv_obj_t* picker, int index, bool show_check);

/**
 * @brief Replace an item's secondary text (e.g. a live value)
 * @note The item must have been created with non-empty secondary_text.
 */
void picker_wheel_set_secondary_text(lv_obj_t* picker, int index, const char* text);

void picker_wheel_nudge(lv_obj_t* picker, int delta, bool anim);

/**
 * @brief Replace all items in place (same picker object, no visible reset).
 * @param items Array of @p item_count items (copied; string pointers only need to
 *              stay valid for the duration of this call)
 * @param selected_index Row to select afterwards (clamped to the new range)
 * @return false (nothing changed) on bad arguments or allocation failure
 */
bool picker_wheel_set_items(lv_obj_t* picker, const picker_wheel_item_t* items,
                            int item_count, int selected_index);

void picker_wheel_click(lv_obj_t* picker);

void picker_wheel_set_press_active(lv_obj_t* picker, bool active);

#endif // PICKER_WHEEL_H
