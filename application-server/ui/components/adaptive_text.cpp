/**
 * @file adaptive_text.c
 * @brief Shared adaptive label helpers (wrap-or-marquee) for UI components.
 */

#include "adaptive_text.h"

#include <string.h>

#define ADAPTIVE_TEXT_MARQUEE_DURATION_MS 9000
#define ADAPTIVE_TEXT_MARQUEE_HOLD_MS 2600

static lv_anim_t s_marquee_anim;
static bool s_marquee_anim_initialized = false;

static const lv_anim_t *ui_adaptive_text_get_marquee_anim(void)
{
    if (!s_marquee_anim_initialized) {
        lv_anim_init(&s_marquee_anim);
        lv_anim_set_repeat_count(&s_marquee_anim, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_delay(&s_marquee_anim, ADAPTIVE_TEXT_MARQUEE_HOLD_MS);
        lv_anim_set_repeat_delay(&s_marquee_anim, ADAPTIVE_TEXT_MARQUEE_HOLD_MS);
        s_marquee_anim_initialized = true;
    }

    return &s_marquee_anim;
}

static void ui_adaptive_text_apply(lv_obj_t *label,
                                   const char *text,
                                   bool set_text,
                                   uint8_t max_wrap_lines)
{
    if (!label) return;

    const char *current = lv_label_get_text(label);
    if (!current) current = "";

    const char *safe = text ? text : current;
    bool text_changed = set_text && strcmp(current, safe) != 0;

    lv_coord_t width = lv_obj_get_width(label);
    const lv_font_t *font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    int32_t letter_space = lv_obj_get_style_text_letter_space(label, LV_PART_MAIN);
    int32_t line_space = lv_obj_get_style_text_line_space(label, LV_PART_MAIN);

    lv_coord_t one_line_h = font ? lv_font_get_line_height(font) : lv_obj_get_height(label);
    if (one_line_h <= 0) one_line_h = 1;

    bool use_scroll = true;
    lv_coord_t target_h = one_line_h;

    if (width > 0 && font && max_wrap_lines > 0) {
        lv_point_t text_size = {0, 0};
        lv_text_get_size(&text_size, safe, font, letter_space, line_space, width, LV_TEXT_FLAG_NONE);

        lv_coord_t max_wrap_h = (lv_coord_t)(max_wrap_lines * one_line_h);
        if (max_wrap_lines > 1) {
            max_wrap_h += (lv_coord_t)((max_wrap_lines - 1) * line_space);
        }

        if (text_size.y <= max_wrap_h) {
            use_scroll = false;
            target_h = text_size.y > 0 ? text_size.y : one_line_h;
        }
    }

    lv_label_long_mode_t desired_mode = use_scroll
        ? LV_LABEL_LONG_MODE_SCROLL_CIRCULAR
        : LV_LABEL_LONG_MODE_WRAP;
    lv_label_long_mode_t current_mode = lv_label_get_long_mode(label);
    lv_coord_t current_h = lv_obj_get_height(label);

    if (!text_changed && current_mode == desired_mode && current_h == target_h) {
        return;
    }

    if (current_mode != desired_mode) {
        lv_label_set_long_mode(label, desired_mode);
    }

    if (use_scroll) {
        lv_obj_set_style_base_dir(label, LV_BASE_DIR_RTL, LV_PART_MAIN);
        lv_obj_set_style_anim(label, ui_adaptive_text_get_marquee_anim(), LV_PART_MAIN);
        lv_obj_set_style_anim_duration(label, ADAPTIVE_TEXT_MARQUEE_DURATION_MS, LV_PART_MAIN);
    } else {
        lv_obj_set_style_base_dir(label, LV_BASE_DIR_AUTO, LV_PART_MAIN);
        lv_obj_set_style_anim(label, NULL, LV_PART_MAIN);
    }

    if (current_h != target_h) {
        lv_obj_set_height(label, target_h);
    }

    if (text_changed) {
        lv_label_set_text(label, safe);
    }
}

void ui_adaptive_text_set(lv_obj_t *label,
                          const char *text,
                          uint8_t max_wrap_lines)
{
    ui_adaptive_text_apply(label, text ? text : "", true, max_wrap_lines);
}

void ui_adaptive_text_refresh(lv_obj_t *label,
                              uint8_t max_wrap_lines)
{
    ui_adaptive_text_apply(label, NULL, false, max_wrap_lines);
}
