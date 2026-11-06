/**
 * @file info_list.h
 * @brief List of "label ... value" rows: the label on the left, the value on
 *        the right in the brand colour. A row is either read-only (a value
 *        that doesn't fit on one line scrolls, see adaptive_text.h) or a
 *        dropdown that lets the user pick one of several options.
 */

#ifndef INFO_LIST_H
#define INFO_LIST_H

#include "lvgl/lvgl.h"

#define INFO_LIST_ROW_H_PX 52

typedef struct {
    const char *label;
    const char *value;     /* read-only row: the text shown. Dropdown row: the selected option's text. */
    const char *options;   /* NULL: read-only row. Otherwise a dropdown; the option texts, '\n' separated. */
} info_row_t;

/** Creates the list below the header of @p parent. Rows are numbered in @p rows order. */
lv_obj_t *info_list_create(lv_obj_t *parent, const info_row_t *rows, int count);

/**
 * Sets the value of row @p index. No-op if the text is unchanged. On a dropdown row it
 * selects the option with that text ("-" is shown if none matches); it is ignored
 * while the dropdown is open so the user's choice isn't taken away.
 */
void info_list_set_value(lv_obj_t *list, int index, const char *value);

/** Replaces the options of dropdown row @p index (same format as info_row_t::options).
 *  Selects nothing afterwards: follow with info_list_set_value(). No-op if unchanged
 *  or while the dropdown is open. */
void info_list_set_options(lv_obj_t *list, int index, const char *options);

/** Called when the user picks an option of a dropdown row (never for programmatic changes). */
typedef void (*info_list_change_cb_t)(int row, int option, void *user_data);

void info_list_set_change_cb(lv_obj_t *list, info_list_change_cb_t cb, void *user_data);

/**
 * Lets the encoder drive the dropdown rows, via the page's encoder-focused object @p focus
 * (nav_page_t::focus). Rotating moves a highlight between the rows; a press opens the
 * highlighted dropdown; while it is open, rotating moves its selection and a press confirms.
 */
void info_list_attach_encoder(lv_obj_t *list, lv_obj_t *focus);

#endif /* INFO_LIST_H */
