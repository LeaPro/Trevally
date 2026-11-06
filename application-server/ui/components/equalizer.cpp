/**
 * @file equalizer.c
 * @brief Reusable equalizer animation UI component.
 */

#include "equalizer.h"
#include "equalizer_gif_data.h"
#include "../ui_design_tokens.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define EQ_BAR_COUNT EQ_GIF_BAR_COUNT
#define EQ_BAR_MIN_H 3
#define EQ_BAR_MAX_H EQ_GIF_HEIGHT

typedef struct equalizer_data_t equalizer_data_t;

typedef struct {
    equalizer_data_t *owner;
    uint8_t bar_index;
    int current_height;
    int current_bottom;
    uint16_t frame_index;
    bool running;
} equalizer_anim_t;

struct equalizer_data_t {
    lv_obj_t *anchor;
    lv_obj_t *bars[EQ_BAR_COUNT];
    equalizer_anim_t anim[EQ_BAR_COUNT];
};

static equalizer_data_t *equalizer_get_data(lv_obj_t *equalizer)
{
    if (!equalizer) return NULL;
    if (!lv_obj_is_valid(equalizer)) return NULL;
    return (equalizer_data_t *)lv_obj_get_user_data(equalizer);
}

static void equalizer_apply_bar(equalizer_anim_t *anim_state)
{
    if (!anim_state || !anim_state->owner) return;

    lv_obj_t *bar = anim_state->owner->bars[anim_state->bar_index];
    if (!bar) return;
    if (!lv_obj_is_valid(bar)) {
        anim_state->owner->bars[anim_state->bar_index] = NULL;
        return;
    }

    int height = anim_state->current_height;
    int bottom = anim_state->current_bottom;
    if (height < EQ_BAR_MIN_H) height = EQ_BAR_MIN_H;
    if (height > EQ_BAR_MAX_H) height = EQ_BAR_MAX_H;
    if (bottom < height) bottom = height;
    if (bottom > EQ_BAR_MAX_H) bottom = EQ_BAR_MAX_H;

    lv_obj_set_size(bar, EQ_GIF_BAR_WIDTH[anim_state->bar_index], height);
    lv_obj_set_pos(bar, EQ_GIF_BAR_X[anim_state->bar_index], bottom - height);
}

static void equalizer_height_exec(void *var, int32_t value)
{
    equalizer_anim_t *anim_state = (equalizer_anim_t *)var;
    if (!anim_state) return;
    anim_state->current_height = (int)value;
    equalizer_apply_bar(anim_state);
}

static void equalizer_bottom_exec(void *var, int32_t value)
{
    equalizer_anim_t *anim_state = (equalizer_anim_t *)var;
    if (!anim_state) return;
    anim_state->current_bottom = (int)value;
    equalizer_apply_bar(anim_state);
}

static void equalizer_start_bar_segment(equalizer_anim_t *anim_state);

static void equalizer_ready_cb(lv_anim_t *anim)
{
    if (!anim || !anim->var) return;

    equalizer_anim_t *anim_state = (equalizer_anim_t *)anim->var;
    if (!anim_state->owner) return;
    if (!anim_state->running) return;

    anim_state->frame_index = (uint16_t)((anim_state->frame_index + 1) % EQ_GIF_FRAME_COUNT);
    equalizer_start_bar_segment(anim_state);
}

static void equalizer_start_bar_segment(equalizer_anim_t *anim_state)
{
    if (!anim_state || !anim_state->owner || !anim_state->running) return;

    uint16_t from_frame = anim_state->frame_index;
    uint16_t to_frame = (uint16_t)((from_frame + 1) % EQ_GIF_FRAME_COUNT);
    int bar_index = anim_state->bar_index;
    uint16_t dur = EQ_GIF_DURATIONS[from_frame];

    lv_anim_delete(anim_state, equalizer_height_exec);
    lv_anim_delete(anim_state, equalizer_bottom_exec);

    lv_anim_t height_anim;
    lv_anim_init(&height_anim);
    lv_anim_set_var(&height_anim, anim_state);
    lv_anim_set_exec_cb(&height_anim, equalizer_height_exec);
    lv_anim_set_values(&height_anim,
                       EQ_GIF_HEIGHTS[from_frame][bar_index],
                       EQ_GIF_HEIGHTS[to_frame][bar_index]);
    lv_anim_set_time(&height_anim, dur);
    lv_anim_set_path_cb(&height_anim, lv_anim_path_linear);
    lv_anim_start(&height_anim);

    lv_anim_t bottom_anim;
    lv_anim_init(&bottom_anim);
    lv_anim_set_var(&bottom_anim, anim_state);
    lv_anim_set_exec_cb(&bottom_anim, equalizer_bottom_exec);
    lv_anim_set_values(&bottom_anim,
                       EQ_GIF_BOTTOMS[from_frame][bar_index],
                       EQ_GIF_BOTTOMS[to_frame][bar_index]);
    lv_anim_set_time(&bottom_anim, dur);
    lv_anim_set_path_cb(&bottom_anim, lv_anim_path_linear);
    lv_anim_set_ready_cb(&bottom_anim, equalizer_ready_cb);
    lv_anim_start(&bottom_anim);
}

static void equalizer_apply_frame(equalizer_data_t *data, uint16_t frame_index)
{
    if (!data) return;
    frame_index = (uint16_t)(frame_index % EQ_GIF_FRAME_COUNT);

    for (int i = 0; i < EQ_BAR_COUNT; i++) {
        data->anim[i].frame_index = frame_index;
        data->anim[i].current_height = EQ_GIF_HEIGHTS[frame_index][i];
        data->anim[i].current_bottom = EQ_GIF_BOTTOMS[frame_index][i];
        equalizer_apply_bar(&data->anim[i]);
    }
}

static void equalizer_stop_data(equalizer_data_t *data)
{
    if (!data) return;

    for (int i = 0; i < EQ_BAR_COUNT; i++) {
        data->anim[i].running = false;
        data->anim[i].frame_index = 0;
        lv_anim_delete(&data->anim[i], equalizer_height_exec);
        lv_anim_delete(&data->anim[i], equalizer_bottom_exec);
    }
}

static void equalizer_set_running_data(equalizer_data_t *data, bool running)
{
    if (!data) return;

    if (running) {
        equalizer_stop_data(data);
        equalizer_apply_frame(data, 0);
        for (int i = 0; i < EQ_BAR_COUNT; i++) {
            if (!data->bars[i] || !lv_obj_is_valid(data->bars[i])) continue;
            lv_obj_clear_flag(data->bars[i], LV_OBJ_FLAG_HIDDEN);
            data->anim[i].running = true;
            data->anim[i].frame_index = 0;
            equalizer_start_bar_segment(&data->anim[i]);
        }
    } else {
        equalizer_stop_data(data);
        for (int i = 0; i < EQ_BAR_COUNT; i++) {
            if (!data->bars[i] || !lv_obj_is_valid(data->bars[i])) continue;
            lv_obj_add_flag(data->bars[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void equalizer_delete_cb(lv_event_t *e)
{
    lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);
    if (!obj) return;

    equalizer_data_t *data = equalizer_get_data(obj);
    if (!data) return;

    equalizer_stop_data(data);
    for (int i = 0; i < EQ_BAR_COUNT; i++) {
        data->bars[i] = NULL;
        data->anim[i].owner = NULL;
    }
    lv_obj_set_user_data(obj, NULL);

    /* Keep data allocated to avoid UAF from any late LVGL animation callback. */
}

void equalizer_get_size(int *out_w, int *out_h)
{
    if (out_w) *out_w = EQ_GIF_WIDTH;
    if (out_h) *out_h = EQ_BAR_MAX_H;
}

lv_obj_t *equalizer_create(lv_obj_t *parent, int x, int y)
{
    if (!parent) return NULL;

    int eq_w = 1;
    int eq_h = 1;
    equalizer_get_size(&eq_w, &eq_h);

    lv_obj_t *anchor = lv_obj_create(parent);
    lv_obj_remove_style_all(anchor);
    lv_obj_set_style_pad_all(anchor, 0, 0);
    lv_obj_set_style_border_width(anchor, 0, 0);
    lv_obj_set_style_bg_opa(anchor, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(anchor, 0, 0);
    lv_obj_clear_flag(anchor, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(anchor, eq_w, eq_h);
    lv_obj_set_pos(anchor, x, y);
    lv_obj_add_flag(anchor, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_clear_flag(anchor, LV_OBJ_FLAG_CLICKABLE);

    equalizer_data_t *data = (equalizer_data_t *)calloc(1, sizeof(equalizer_data_t));
    if (!data) {
        lv_obj_delete(anchor);
        return NULL;
    }
    data->anchor = anchor;
    lv_obj_set_user_data(anchor, data);
    lv_obj_add_event_cb(anchor, equalizer_delete_cb, LV_EVENT_DELETE, NULL);

    for (int i = 0; i < EQ_BAR_COUNT; i++) {
        lv_obj_t *bar = lv_obj_create(anchor);
        lv_obj_remove_style_all(bar);
        lv_obj_set_style_pad_all(bar, 0, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_add_style(bar, ui_theme_get_style_bg_icon_primary(), 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(bar, 0, 0);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(bar, 8, 0);
        lv_obj_set_size(bar, EQ_GIF_BAR_WIDTH[i], EQ_BAR_MIN_H);
        lv_obj_set_pos(bar, EQ_GIF_BAR_X[i], EQ_BAR_MAX_H - EQ_BAR_MIN_H);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
        data->bars[i] = bar;

        data->anim[i].owner = data;
        data->anim[i].bar_index = (uint8_t)i;
        data->anim[i].frame_index = 0;
        data->anim[i].current_height = EQ_GIF_HEIGHTS[0][i];
        data->anim[i].current_bottom = EQ_GIF_BOTTOMS[0][i];
        data->anim[i].running = false;
    }

    equalizer_apply_frame(data, 0);
    return anchor;
}

void equalizer_set_running(lv_obj_t *equalizer, bool running)
{
    equalizer_data_t *data = equalizer_get_data(equalizer);
    equalizer_set_running_data(data, running);
}

void equalizer_stop(lv_obj_t *equalizer)
{
    equalizer_data_t *data = equalizer_get_data(equalizer);
    equalizer_stop_data(data);
}
