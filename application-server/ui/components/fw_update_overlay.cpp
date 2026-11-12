/**
 * @file fw_update_overlay.cpp
 * @brief See fw_update_overlay.h.
 */

#include "fw_update_overlay.h"
#include "../ui_design_tokens.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

/* Same look as the playback page's volume overlay: white text over black, the number in the big font with a smaller
 * % sign beside it. The overlay covers the whole UI rather than just the art, so it dims a little more. */
#define OVERLAY_OPA           LV_OPA_80
#define OVERLAY_SIDE_PAD      32
#define TEXT_GAP              16     /* between the rows of the column */
#define PERCENT_VALUE_GAP     11     /* between the number and the % sign */
#define HINT_OPA              LV_OPA_60

enum class Step { Hidden, Downloading, Decrypting, Installing, Restarting, Complete, Failed };

struct StepText {
    Step        step;
    const char *title;
    const char *detail;   /* nullptr: none */
};

static lv_obj_t   *overlay;
static lv_obj_t   *column;        /* the text, in the content area */
static lv_obj_t   *title;
static lv_obj_t   *detail;
static lv_obj_t   *percentRow;
static lv_obj_t   *percentValue;
static lv_obj_t   *hint;
static std::string shownKey;        /* status the overlay shows now; "" while hidden */
static int         shownPercent = -1;
static std::string dismissedKey;    /* end state the user tapped away; "" when none */

static StepText step_for(const std::string &status, float progress)
{
    if (status == "Downloading" || (status == "None" && progress > 0.0f))
        return {Step::Downloading, "Downloading firmware", nullptr};
    if (status == "Decrypting")
        return {Step::Decrypting, "Decrypting firmware", nullptr};
    if (status == "Extraction")
        return {Step::Installing, "Installing firmware", "Do not power off the device."};
    if (status == "ExtractionComplete")
        return {Step::Restarting, "Firmware installed", "Restarting..."};
    if (status == "FinishedFlash" || status == "Finished")
        return {Step::Complete, "Firmware update complete", nullptr};
    if (status == "DownloadingFail")
        return {Step::Failed, "Firmware update failed", "The file could not be downloaded."};
    if (status == "DecryptingFail")
        return {Step::Failed, "Firmware update failed", "The file could not be decrypted."};
    if (status == "ExtractionFail")
        return {Step::Failed, "Firmware update failed", "The file is not a firmware update for this device."};
    if (status == "HwNotSupportedInFw")
        return {Step::Failed, "Firmware update failed", "This hardware is not supported by the firmware. Try a newer version."};
    return {Step::Hidden, nullptr, nullptr};
}

static bool is_end_state(Step step) { return step == Step::Complete || step == Step::Failed; }

/* FinishedFlash and Finished are the same step to the user: dismissing one keeps the other away too. */
static std::string dismiss_key(const std::string &status)
{
    return (status == "FinishedFlash" || status == "Finished") ? "Finished" : status;
}

static void overlay_clicked_cb(lv_event_t *)
{
    dismissedKey = dismiss_key(shownKey);
    lv_obj_set_hidden(overlay, true);
}

static lv_obj_t *white_label(lv_obj_t *parent, const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

void fw_update_overlay_create(void)
{
    overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, OVERLAY_OPA, 0);
    lv_obj_set_scrollable(overlay, false);
    lv_obj_set_clickable(overlay, false);
    lv_obj_add_event_cb(overlay, overlay_clicked_cb, LV_EVENT_CLICKED, nullptr);

    /* the dimming covers the whole display; the text is centred in the content area, clear of the knob in landscape */
    column = lv_obj_create(overlay);
    lv_obj_remove_style_all(column);
    lv_obj_set_size(column, ui_content_w(), LV_PCT(100));
    lv_obj_align(column, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_pad_hor(column, OVERLAY_SIDE_PAD, 0);
    lv_obj_set_style_pad_row(column, TEXT_GAP, 0);
    lv_obj_set_scrollable(column, false);
    lv_obj_set_clickable(column, false);
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(column, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    title = white_label(column, &satoshi_medium_32);
    lv_obj_set_width(title, LV_PCT(100));
    lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_WRAP);

    percentRow = lv_obj_create(column);
    lv_obj_remove_style_all(percentRow);
    lv_obj_set_size(percentRow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_scrollable(percentRow, false);
    lv_obj_set_clickable(percentRow, false);
    lv_obj_set_flex_flow(percentRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(percentRow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(percentRow, PERCENT_VALUE_GAP, 0);
    percentValue = white_label(percentRow, &satoshi_medium_140);
    lv_label_set_text(white_label(percentRow, &satoshi_medium_48), "%");

    detail = white_label(column, &satoshi_medium_24);
    lv_obj_set_width(detail, LV_PCT(100));
    lv_label_set_long_mode(detail, LV_LABEL_LONG_MODE_WRAP);

    hint = white_label(column, &satoshi_medium_20);
    lv_obj_set_style_text_opa(hint, HINT_OPA, 0);
    lv_label_set_text(hint, "Tap to dismiss");

    lv_obj_set_hidden(overlay, true);
}

void fw_update_overlay_update(const char *status, float progress)
{
    if (!overlay) return;

    /* the overlay is not rebuilt when the orientation changes: follow the content area's width */
    if (lv_obj_get_style_width(column, LV_PART_MAIN) != ui_content_w()) lv_obj_set_width(column, ui_content_w());

    const std::string key = status ? status : "None";
    const StepText text = step_for(key, progress);

    if (dismissedKey != dismiss_key(key)) dismissedKey.clear();   /* the step moved on: a later end state shows again */
    if (text.step == Step::Hidden || dismissedKey == dismiss_key(key)) {
        if (!shownKey.empty()) lv_obj_set_hidden(overlay, true);
        shownKey.clear();
        shownPercent = -1;
        return;
    }

    /* the download step is keyed on "Downloading" whether or not the status says so */
    const std::string shown = text.step == Step::Downloading ? "Downloading" : key;
    if (shown != shownKey) {
        shownKey = shown;
        lv_label_set_text(title, text.title);
        lv_obj_set_style_text_color(title, text.step == Step::Failed ? UI_COLOR_TEXT_ERROR
                                         : text.step == Step::Complete ? UI_COLOR_TEXT_SUCCESS
                                         : lv_color_white(), 0);
        lv_label_set_text(detail, text.detail ? text.detail : "");
        lv_obj_set_hidden(detail, text.detail == nullptr);
        lv_obj_set_hidden(percentRow, text.step != Step::Downloading);
        lv_obj_set_hidden(hint, !is_end_state(text.step));
        /* end states take the tap that dismisses them; otherwise touches reach the UI underneath */
        lv_obj_set_clickable(overlay, is_end_state(text.step));
        lv_obj_set_hidden(overlay, false);
        lv_obj_move_foreground(overlay);
        shownPercent = -1;
    }

    if (text.step == Step::Downloading) {
        const int percent = (int)std::lround(std::fmin(std::fmax(progress, 0.0f), 100.0f));
        if (percent != shownPercent) {
            shownPercent = percent;
            lv_label_set_text_fmt(percentValue, "%d", percent);
        }
    }
}
