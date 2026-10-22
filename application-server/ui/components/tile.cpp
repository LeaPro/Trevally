/**
 * @file tile.c
 * @brief Individual data block component – implementation.
 *
 * Figma node: 5259-1973
 *
 * Internal layout uses LV_LAYOUT_FLEX (column, space-between) so the
 * label sits at the top and the value/bar group sits at the bottom.
 * No manual lv_obj_set_pos() calls are needed inside each block.
 *
 * The parent PAGE should use:
 *   lv_obj_set_layout(page, LV_LAYOUT_FLEX);
 *   lv_obj_set_flex_flow(page, LV_FLEX_FLOW_ROW_WRAP);
 * Then simply call tile_create(page, &cfg) for each block and they
 * will flow left-to-right, wrapping automatically.
 */

#include "tile.h"
#include "../ui_design_tokens.h"
#include "adaptive_text.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* =========================================================================
 * Private colours — theme-agnostic constants only.
 * Semantic colors (bg, text, border) come from ui_design_tokens.h styles.
 * ====================================================================== */

#define BLK_COL_BRAND    ui_get_brand_color()

/* Padding inside block (Figma: 16 px sides, 36 px for Full) */
#define BLK_PAD_SM  16
#define BLK_PAD_LG  36
#define BLK_BAR_H    8
#define BLK_BAR_R    4
#define BLK_KNOB_SZ 12
/* Vertical offset so track is centred inside the taller knob container */
#define BLK_BAR_Y   ((BLK_KNOB_SZ - BLK_BAR_H) / 2)   /* = 2 px */

/* =========================================================================
 * Private data stored on each block object
 * ====================================================================== */

typedef struct {
    lv_obj_t *value_label;  /* main value text */
    lv_obj_t *bar_fill;     /* filled portion of progress bar */
    lv_obj_t *bar_knob;     /* knob dot on the bar */
    lv_obj_t *bar_wrap;     /* track container for touch mapping */
} tile_data_t;

/* =========================================================================
 * Helpers
 * ====================================================================== */

static void tile_reset(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
}

/* Create a label with consistent style */
static lv_obj_t *tile_label(lv_obj_t *parent,
                             const char *text,
                             const lv_font_t *font,
                             lv_style_t *text_style)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl, font, 0);
    if (text_style) lv_obj_add_style(lbl, text_style, 0);
    lv_label_set_text(lbl, text ? text : "");
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(lbl, LV_PCT(100));
    return lbl;
}

/* Draw a progress bar inside @p parent.
 * @p parent should be a container sized inner_w × BLK_KNOB_SZ.
 * The track/fill are centred vertically (y = BLK_BAR_Y) so the 12 px knob
 * sits flush with the top of the parent without clipping.
 * @p tertiary_track  – use bg-tertiary (#3a3c40) for the track
 *                      instead of bg-secondary (Output Volume style). */
static void tile_draw_bar(lv_obj_t *parent, int bar_w,
                          float fill, bool editable, bool white_knob,
                          bool tertiary_track,
                          lv_obj_t **fill_out, lv_obj_t **knob_out)
{
    /* Track */
    lv_obj_t *track = lv_obj_create(parent);
    tile_reset(track);
    lv_obj_set_size(track, bar_w, BLK_BAR_H);
    lv_obj_set_style_radius(track, BLK_BAR_R, 0);
    if (tertiary_track) {
        lv_obj_add_style(track, ui_theme_get_style_bg_tertiary(), 0);
    } else {
        lv_obj_add_style(track, ui_theme_get_style_bg_secondary(), 0);
    }
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(track, 0, BLK_BAR_Y);

    int fw = (int)(fill * (float)bar_w);
    if (fw < 0) fw = 0;
    if (fw > bar_w) fw = bar_w;

    /* Fill — brand for editable, text-primary for sensor */
    lv_obj_t *bar_fill = lv_obj_create(parent);
    tile_reset(bar_fill);
    lv_obj_set_size(bar_fill, fw > 0 ? fw : 1, BLK_BAR_H);
    if (editable) {
        lv_obj_add_style(bar_fill, ui_theme_get_style_brand(), 0);
    } else {
        lv_obj_add_style(bar_fill, ui_theme_get_style_text_primary(), 0);
    }
    lv_obj_set_style_bg_opa(bar_fill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar_fill, BLK_BAR_R, 0);
    lv_obj_clear_flag(bar_fill, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(bar_fill, 0, BLK_BAR_Y);
    if (fill_out) *fill_out = bar_fill;

    /* Knob — sits at y=0, centred on the bar by BLK_BAR_Y geometry */
    lv_obj_t *knob = lv_obj_create(parent);
    tile_reset(knob);
    lv_obj_set_size(knob, BLK_KNOB_SZ, BLK_KNOB_SZ);
    if (white_knob) {
        lv_obj_add_style(knob, ui_theme_get_style_text_primary(), 0);
    } else {
        lv_obj_add_style(knob, ui_theme_get_style_brand(), 0);
    }
    lv_obj_set_style_bg_opa(knob, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(knob, BLK_KNOB_SZ / 2, 0);
    /* Dark border to visually float the knob above the bar */
    lv_obj_add_style(knob, ui_theme_get_style_knob_border(), 0);
    lv_obj_set_style_border_width(knob, 2, 0);
    lv_obj_set_style_border_opa(knob, LV_OPA_COVER, 0);
    lv_obj_clear_flag(knob, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(knob, fw - BLK_KNOB_SZ / 2, 0);
    if (knob_out) *knob_out = knob;
}

/* =========================================================================
 * Per-type content builders
 * All receive:  @p inner  – the bottom content container (flex col)
 *               @p cfg    – block config
 *               @p d      – widget references for later update
 *               @p inner_w – usable pixel width inside padding
 * ====================================================================== */

/* Fonts by size (Figma maps Satoshi to lv_font_montserrat) */
static const lv_font_t *F14 = &satoshi_medium_14;
static const lv_font_t *F20 = &satoshi_medium_20;
static const lv_font_t *F24 = &satoshi_medium_24;
static const lv_font_t *F32 = &satoshi_medium_32;
static const lv_font_t *F48 = &satoshi_medium_48;

/* Number value font depends on block size */
static const lv_font_t *val_font(tile_size_t s)
{
    return (s == TILE_SIZE_FULL || s == TILE_SIZE_HALF) ? F48 : F32;
}

static const lv_font_t *percent_font(tile_size_t s)
{
    return (s == TILE_SIZE_FULL || s == TILE_SIZE_HALF) ? F32 : F24;
}

/* --- Output Volume (centered bar + centered value) --- */
static void build_output_volume(lv_obj_t *root, int bw,
                                const tile_config_t *cfg, tile_data_t *d)
{
    const bool is_full = (cfg->size == TILE_SIZE_FULL);
    const bool is_wide = (cfg->size == TILE_SIZE_FULL || cfg->size == TILE_SIZE_HALF);
    const int pad = is_full ? BLK_PAD_LG : BLK_PAD_SM;
    const int inner_w = bw - 2 * pad;
    const lv_font_t *value_font = is_wide ? F48 : F32;

    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root,
                          is_full ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(root, pad, 0);
    lv_obj_set_style_pad_gap(root, is_full ? 24 : 0, 0);

    if (is_full) {
        tile_label(root, cfg->label, F24, ui_theme_get_style_text_primary());

        lv_obj_t *bar_wrap = lv_obj_create(root);
        tile_reset(bar_wrap);
        lv_obj_set_size(bar_wrap, inner_w, BLK_KNOB_SZ);
        lv_obj_clear_flag(bar_wrap, LV_OBJ_FLAG_CLICKABLE);
        d->bar_wrap = bar_wrap;
        tile_draw_bar(bar_wrap, inner_w, cfg->bar_fill,
                      true, false, true,
                      &d->bar_fill, &d->bar_knob);

        lv_obj_t *val_row = lv_obj_create(root);
        tile_reset(val_row);
        lv_obj_set_layout(val_row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(val_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(val_row, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_width(val_row, LV_SIZE_CONTENT);
        lv_obj_set_height(val_row, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_gap(val_row, 4, 0);

        lv_obj_t *vlbl = lv_label_create(val_row);
        lv_obj_set_style_text_font(vlbl, value_font, 0);
        lv_obj_add_style(vlbl, ui_theme_get_style_brand(), 0);
        lv_obj_set_style_text_letter_space(vlbl, -1, 0);
        lv_label_set_text(vlbl,
            (cfg->value_str && cfg->value_str[0]) ? cfg->value_str : "0");
        d->value_label = vlbl;

        {
            const char *unit = (cfg->unit_str && cfg->unit_str[0]) ? cfg->unit_str : "%";
            lv_obj_t *ulbl = lv_label_create(val_row);
            lv_obj_set_style_text_font(ulbl, percent_font(cfg->size), 0);
            lv_obj_add_style(ulbl, ui_theme_get_style_brand(), 0);
            lv_label_set_text(ulbl, unit);
        }

        return;
    }

    lv_obj_t *top = lv_obj_create(root);
    tile_reset(top);
    lv_obj_set_layout(top, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_width(top, LV_PCT(100));
    lv_obj_set_height(top, LV_SIZE_CONTENT);
    tile_label(top, cfg->label, F24, ui_theme_get_style_text_primary());

    lv_obj_t *label_spacer = lv_obj_create(top);
    tile_reset(label_spacer);
    lv_obj_set_width(label_spacer, LV_PCT(100));
    lv_obj_set_height(label_spacer, 29);

    lv_obj_t *bottom = lv_obj_create(root);
    tile_reset(bottom);
    lv_obj_set_layout(bottom, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(bottom, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(bottom, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_width(bottom, LV_PCT(100));
    lv_obj_set_height(bottom, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(bottom, 12, 0);

    lv_obj_t *bar_wrap = lv_obj_create(bottom);
    tile_reset(bar_wrap);
    lv_obj_set_size(bar_wrap, inner_w, BLK_KNOB_SZ);
    lv_obj_clear_flag(bar_wrap, LV_OBJ_FLAG_CLICKABLE);
    d->bar_wrap = bar_wrap;
    tile_draw_bar(bar_wrap, inner_w, cfg->bar_fill,
                  true, false, true,
                  &d->bar_fill, &d->bar_knob);

    lv_obj_t *val_row = lv_obj_create(bottom);
    tile_reset(val_row);
    lv_obj_set_layout(val_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(val_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(val_row, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(val_row, LV_SIZE_CONTENT);
    lv_obj_set_height(val_row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(val_row, 4, 0);

    lv_obj_t *vlbl = lv_label_create(val_row);
    lv_obj_set_style_text_font(vlbl, value_font, 0);
    lv_obj_add_style(vlbl, ui_theme_get_style_brand(), 0);
    lv_label_set_text(vlbl,
        (cfg->value_str && cfg->value_str[0]) ? cfg->value_str : "0");
    d->value_label = vlbl;

    {
        const char *unit = (cfg->unit_str && cfg->unit_str[0]) ? cfg->unit_str : "%";
        lv_obj_t *ulbl = lv_label_create(val_row);
        lv_obj_set_style_text_font(ulbl, percent_font(cfg->size), 0);
        lv_obj_add_style(ulbl, ui_theme_get_style_brand(), 0);
        lv_label_set_text(ulbl, unit);
    }
}

/* --- Number block (Output Volume / Control / Sensor) --- */
static void build_number(lv_obj_t *inner, int inner_w,
                          const tile_config_t *cfg, tile_data_t *d)
{
    bool is_control = (cfg->type == TILE_TYPE_CONTROL);
    bool is_sensor  = (cfg->type == TILE_TYPE_SENSOR);
    bool editable   = !is_sensor;
    bool white_knob = is_control;

    /* Bar container — tall enough for the 12 px knob (no clipping) */
    lv_obj_t *bar_wrap = lv_obj_create(inner);
    tile_reset(bar_wrap);
    lv_obj_set_size(bar_wrap, inner_w, BLK_KNOB_SZ);
    lv_obj_set_style_radius(bar_wrap, BLK_BAR_R, 0);
    lv_obj_clear_flag(bar_wrap, LV_OBJ_FLAG_CLICKABLE);

    bool tertiary_track = (cfg->type == TILE_TYPE_OUTPUT_VOLUME);
    tile_draw_bar(bar_wrap, inner_w, cfg->bar_fill,
                 editable, white_knob, tertiary_track,
                 &d->bar_fill, &d->bar_knob);

    /* Value row: number + unit side by side */
    lv_obj_t *val_row = lv_obj_create(inner);
    tile_reset(val_row);
    lv_obj_set_layout(val_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(val_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(val_row, LV_FLEX_ALIGN_START,
                           LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START);
    lv_obj_set_width(val_row, inner_w);
    lv_obj_set_height(val_row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(val_row, 4, 0);

    lv_obj_t *vlbl = lv_label_create(val_row);
    lv_obj_set_style_text_font(vlbl, val_font(cfg->size), 0);
    if (is_sensor) {
        lv_obj_add_style(vlbl, ui_theme_get_style_text_primary(), 0);
    } else {
        lv_obj_add_style(vlbl, ui_theme_get_style_brand(), 0);
    }
    lv_label_set_text(vlbl,
        (cfg->value_str && cfg->value_str[0]) ? cfg->value_str : "0");
    d->value_label = vlbl;

    if (cfg->unit_str && cfg->unit_str[0]) {
        lv_obj_t *ulbl = lv_label_create(val_row);
        lv_obj_set_style_text_font(ulbl, F20, 0);
        if (is_sensor) {
            lv_obj_add_style(ulbl, ui_theme_get_style_text_primary(), 0);
        } else {
            lv_obj_add_style(ulbl, ui_theme_get_style_brand(), 0);
        }
        lv_label_set_text(ulbl, cfg->unit_str);
    }
}

/* --- Boolean / Enumeration block (Control / Sensor) --- */
static void build_bool_enum(lv_obj_t *inner, int inner_w,
                              const tile_config_t *cfg, tile_data_t *d)
{
    bool is_sensor = (cfg->type == TILE_TYPE_SENSOR);

    const char *txt = (cfg->value_str && cfg->value_str[0]) ? cfg->value_str : "-";
    lv_obj_t *vlbl = tile_label(inner, txt, val_font(cfg->size),
        is_sensor ? ui_theme_get_style_text_primary() : ui_theme_get_style_brand());
    lv_obj_set_style_text_letter_space(vlbl, cfg->size == TILE_SIZE_HALF
                                        ? -1 : 0, 0);
    d->value_label = vlbl;
}

/* --- Source Select (Enumeration) --- */
static void build_source_select(lv_obj_t *root, int bw,
                                  const tile_config_t *cfg, tile_data_t *d)
{
    (void)bw;
    const bool is_full = (cfg->size == TILE_SIZE_FULL);
    const bool is_wide = (cfg->size == TILE_SIZE_FULL || cfg->size == TILE_SIZE_HALF);
    const int pad_all = is_full ? 36 : 16;
    const int pad_top_bottom = (cfg->size == TILE_SIZE_SIXTH) ? 12 : pad_all;
    const lv_font_t *state_font = is_wide ? F48 : F32;

    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root,
        is_full ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_SPACE_BETWEEN,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_left(root,  pad_all, 0);
    lv_obj_set_style_pad_right(root, pad_all, 0);
    lv_obj_set_style_pad_top(root, pad_top_bottom, 0);
    lv_obj_set_style_pad_bottom(root, pad_top_bottom, 0);
    lv_obj_set_style_pad_gap(root, is_full ? 24 : 0, 0);

    tile_label(root, cfg->label, F24, ui_theme_get_style_text_primary());

    lv_obj_t *state_lbl = lv_label_create(root);
    lv_obj_set_style_text_font(state_lbl, state_font, 0);
    lv_obj_add_style(state_lbl, ui_theme_get_style_brand(), 0);
    lv_obj_set_style_text_letter_space(state_lbl, is_wide ? -1 : 0, 0);
    int state_lbl_w = bw - 2 * pad_all;
    lv_obj_set_width(state_lbl, state_lbl_w > 0 ? state_lbl_w : LV_SIZE_CONTENT);
    ui_adaptive_text_set(state_lbl,
        (cfg->value_str && cfg->value_str[0]) ? cfg->value_str : "-", 1);
    d->value_label = state_lbl;

    lv_obj_t *vol_row = lv_obj_create(root);
    tile_reset(vol_row);
    lv_obj_set_layout(vol_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(vol_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(vol_row, LV_FLEX_ALIGN_START,
                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_width(vol_row, LV_PCT(100));
    lv_obj_set_height(vol_row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(vol_row, 12, 0);

    if (cfg->volume_icon_path && cfg->volume_icon_path[0]) {
        lv_obj_t *icon = lv_image_create(vol_row);
        lv_image_set_src(icon, cfg->volume_icon_path);
        lv_obj_set_size(icon, 18, 18);
    } else {
        lv_obj_t *icon = UI_ICON_CREATE_DEFAULT(vol_row, UI_ICON_VOLUME_SMALL);
        if (icon) {
            lv_obj_set_size(icon, 18, 18);
        }
    }

    if (cfg->source_level && cfg->source_level[0]) {
        lv_obj_t *lvlbl = lv_label_create(vol_row);
        lv_obj_set_style_text_font(lvlbl, F24, 0);
        lv_obj_add_style(lvlbl, ui_theme_get_style_text_primary(), 0);
        lv_label_set_text(lvlbl, cfg->source_level);
    }
}

/* --- Macro (Enumeration) --- */
static void build_macro(lv_obj_t *inner, int inner_w,
                          const tile_config_t *cfg, tile_data_t *d)
{
    lv_obj_t *vlbl = tile_label(inner,
                                 (cfg->value_str && cfg->value_str[0]) ? cfg->value_str : "-",
                                 val_font(cfg->size),
                                 ui_theme_get_style_brand());
    d->value_label = vlbl;
}

/* --- Weather --- */
static void build_weather(lv_obj_t *root, const tile_config_t *cfg,
                            tile_data_t *d)
{
    /* Weather uses its own layout so build directly in root */
    int pad = (cfg->size == TILE_SIZE_FULL || cfg->size == TILE_SIZE_HALF)
              ? BLK_PAD_LG : BLK_PAD_SM;
    int w   = tile_size_w(cfg->size) - 2 * pad;

    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root, LV_FLEX_ALIGN_CENTER,
                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(root, pad, 0);
    lv_obj_set_style_pad_top(root, 24, 0);
    lv_obj_set_style_pad_gap(root, 24, 0);

    /* City */
    tile_label(root, cfg->city, F24, ui_theme_get_style_text_secondary());

    /* Weather row: icon + condition */
    lv_obj_t *wx_row = lv_obj_create(root);
    tile_reset(wx_row);
    lv_obj_set_layout(wx_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(wx_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(wx_row, LV_FLEX_ALIGN_START,
                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_width(wx_row, LV_PCT(100));
    lv_obj_set_height(wx_row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(wx_row, 16, 0);

    int icon_sz = (cfg->size == TILE_SIZE_FULL || cfg->size == TILE_SIZE_HALF)
                  ? 48 : 36;
    if (cfg->weather_icon_path && cfg->weather_icon_path[0]) {
        lv_obj_t *wicon = lv_image_create(wx_row);
        lv_image_set_src(wicon, cfg->weather_icon_path);
        lv_obj_set_size(wicon, icon_sz, icon_sz);
    } else {
        lv_obj_t *ph = lv_obj_create(wx_row);
        tile_reset(ph);
        lv_obj_set_size(ph, icon_sz, icon_sz);
        lv_obj_add_style(ph, ui_theme_get_style_text_secondary(), 0);
        lv_obj_set_style_bg_opa(ph, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(ph, LV_RADIUS_CIRCLE, 0);
        lv_obj_clear_flag(ph, LV_OBJ_FLAG_CLICKABLE);
    }
    tile_label(wx_row, cfg->condition, F24, ui_theme_get_style_text_secondary());

    /* Temperature row */
    if (cfg->size == TILE_SIZE_FULL) {
        /* Full: huge 140px-ish temperature – use montserrat_48 as largest
         * available; for real device a larger font can be registered */
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "%.0f", cfg->temperature);

        lv_obj_t *tmp_row = lv_obj_create(root);
        tile_reset(tmp_row);
        lv_obj_set_layout(tmp_row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(tmp_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(tmp_row, LV_FLEX_ALIGN_CENTER,
                               LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_width(tmp_row, LV_PCT(100));
        lv_obj_set_height(tmp_row, LV_SIZE_CONTENT);

        lv_obj_t *t = lv_label_create(tmp_row);
        lv_obj_set_style_text_font(t, F48, 0);
        lv_obj_add_style(t, ui_theme_get_style_text_primary(), 0);
        lv_label_set_text(t, tmp);
        d->value_label = t;

        /* Degree symbol */
        const char *deg = cfg->temp_unit ? cfg->temp_unit : "°";
        if (cfg->degrees_icon_path && cfg->degrees_icon_path[0]) {
            lv_obj_t *dimg = lv_image_create(tmp_row);
            lv_image_set_src(dimg, cfg->degrees_icon_path);
            lv_obj_set_size(dimg, 24, 56);
        } else {
            lv_obj_t *dlbl = lv_label_create(tmp_row);
            lv_obj_set_style_text_font(dlbl, F20, 0);
            lv_obj_add_style(dlbl, ui_theme_get_style_text_primary(), 0);
            lv_label_set_text(dlbl, deg);
        }
    } else {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "%.0f%s",
                 cfg->temperature,
                 cfg->temp_unit ? cfg->temp_unit : "°");
        lv_obj_t *vlbl = tile_label(root, tmp, val_font(cfg->size),
                                     ui_theme_get_style_brand());
        d->value_label = vlbl;
    }
}

/* =========================================================================
 * tile_create
 * ====================================================================== */

lv_obj_t *tile_create(lv_obj_t *parent, const tile_config_t *cfg)
{
    if (!cfg) return NULL;

    int bw  = tile_size_w(cfg->size);
    int bh  = tile_size_h(cfg->size);
    int pad = (cfg->size == TILE_SIZE_FULL || cfg->size == TILE_SIZE_HALF)
              ? BLK_PAD_LG : BLK_PAD_SM;

    /* ---- Root container ---- */
    lv_obj_t *root = lv_obj_create(parent);
    tile_reset(root);
    lv_obj_set_size(root, bw, bh);
    lv_obj_add_style(root, ui_theme_get_style_bg(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    /* Bottom border on all blocks */
    lv_obj_set_style_border_side(root, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_add_style(root, ui_theme_get_style_border(), 0);
    lv_obj_set_style_border_width(root, 1, 0);
    lv_obj_set_style_border_opa(root, LV_OPA_COVER, 0);
    /* Right border on half-width blocks (Sixth / Quarter) */
    if (cfg->size == TILE_SIZE_SIXTH || cfg->size == TILE_SIZE_QUARTER) {
        lv_obj_set_style_border_side(root,
            (lv_border_side_t)(LV_BORDER_SIDE_BOTTOM | LV_BORDER_SIDE_RIGHT), 0);
    }
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    /* Allocate widget ref storage on root */
    tile_data_t *d = (tile_data_t *)calloc(1, sizeof(tile_data_t));
    lv_obj_set_user_data(root, d);

    /* Weather handles its own full layout */
    if (cfg->type == TILE_TYPE_WEATHER) {
        build_weather(root, cfg, d);
        return root;
    }

    /* Source Select has a dedicated layout to match the Figma states. */
    if (cfg->type == TILE_TYPE_SOURCE_SELECT) {
        build_source_select(root, bw, cfg, d);
        goto done;
    }

    /* Output Volume has dedicated centered layout from Figma */
    if (cfg->type == TILE_TYPE_OUTPUT_VOLUME) {
        build_output_volume(root, bw, cfg, d);
        goto done;
    }

    /* ---- Common: flex column layout (label top, content bottom) ---- */
    {
        lv_obj_set_layout(root, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(root, LV_FLEX_ALIGN_SPACE_BETWEEN,
                               LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_all(root, pad, 0);
        lv_obj_set_style_pad_gap(root, 0, 0);

        /* ---- Top label ---- */
        tile_label(root, cfg->label, F24, ui_theme_get_style_text_primary());

        /* ---- Bottom content container ---- */
        lv_obj_t *inner = lv_obj_create(root);
        tile_reset(inner);
        lv_obj_set_layout(inner, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(inner, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(inner, LV_FLEX_ALIGN_END,
                               LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_width(inner, LV_PCT(100));
        lv_obj_set_height(inner, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_gap(inner, 12, 0);

        int inner_w = bw - 2 * pad;

        switch (cfg->type) {
            case TILE_TYPE_CONTROL:
                if (cfg->subtype == TILE_SUB_NUMBER)
                    build_number(inner, inner_w, cfg, d);
                else
                    build_bool_enum(inner, inner_w, cfg, d);
                break;

            case TILE_TYPE_SENSOR:
                if (cfg->subtype == TILE_SUB_NUMBER)
                    build_number(inner, inner_w, cfg, d);
                else
                    build_bool_enum(inner, inner_w, cfg, d);
                break;

            case TILE_TYPE_SOURCE_SELECT:
                build_source_select(inner, inner_w, cfg, d);
                break;

            case TILE_TYPE_MACRO:
                build_macro(inner, inner_w, cfg, d);
                break;

            default:
                break;
        }
    }

done:

    /* Tap callback */
    if (cfg->tapped_cb) {
        lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(root,
            cfg->tapped_cb,
            LV_EVENT_CLICKED,
            cfg->user_data);
    }

    return root;
}

/* =========================================================================
 * tile_update_value
 * ====================================================================== */

void tile_update_value(lv_obj_t *block,
                         const char *value_str, float bar_fill)
{
    if (!block) return;
    tile_data_t *d = (tile_data_t *)lv_obj_get_user_data(block);
    if (!d) return;

    if (d->value_label && value_str)
        lv_label_set_text(d->value_label, value_str);

    if (d->bar_fill) {
        /* bar_fill's parent is bar_wrap – width = inner_w */
        int bar_w = lv_obj_get_width(lv_obj_get_parent(d->bar_fill));
        int fw = (int)(bar_fill * (float)bar_w);
        if (fw < 0) fw = 0;
        if (fw > bar_w) fw = bar_w;
        lv_obj_set_width(d->bar_fill, fw > 0 ? fw : 1);

        if (d->bar_knob)
            lv_obj_set_x(d->bar_knob, fw - BLK_KNOB_SZ / 2);
    }
}
