/**
 * @file adaptive_text.h
 * @brief Shared adaptive label helpers (wrap-or-marquee) for UI components.
 */

#ifndef ADAPTIVE_TEXT_H
#define ADAPTIVE_TEXT_H

#include "lvgl/lvgl.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Sets label text and applies adaptive behavior:
 * - wraps when text fits within max_wrap_lines
 * - circular marquee when text exceeds max_wrap_lines
 */
void ui_adaptive_text_set(lv_obj_t *label,
                          const char *text,
                          uint8_t max_wrap_lines);

#ifdef __cplusplus
}
#endif

#endif /* ADAPTIVE_TEXT_H */