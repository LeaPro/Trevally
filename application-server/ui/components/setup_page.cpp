/**
 * @file setup_page.cpp
 * @brief First-run setup page (see setup_page.h).
 */

#include "setup_page.h"
#include "qr_code.h"
#include "../ui_design_tokens.h"

#include <string>

/* Figma: LEA CS Touch Wall Panel UX/UI, "Setup - Network Connected" / "Setup - No Network". */
#define PORTRAIT_H_PX    532
#define LANDSCAPE_H_PX   480
#define PAD_X_PX         48
#define SECTION_GAP_PX   48   /* between the text and the QR code */
#define TEXT_GAP_PX      8    /* between the title and the description */

#define TITLE_FONT satoshi_medium_32
#define BODY_FONT  satoshi_medium_24

/* The screen's children: the text block (title, description) and the QR code. */
#define TEXT_CHILD  0
#define QR_CHILD    1
#define TITLE_CHILD 0
#define BODY_CHILD  1

/* Kept in the page's user data. */
struct SetupData {
    bool        valid = false;   /* false until the first setup_page_set_ip() */
    bool        connected = false;
    std::string ip;
};

static void page_deleted_cb(lv_event_t *e)
{
    lv_obj_t *screen = (lv_obj_t *)lv_event_get_target(e);
    delete (SetupData *)lv_obj_get_user_data(screen);
    lv_obj_set_user_data(screen, nullptr);
}

lv_obj_t *setup_page_create(lv_obj_t *parent)
{
    lv_obj_t *screen = lv_obj_create(parent);
    lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen, ui_content_w(), ui_is_landscape() ? LANDSCAPE_H_PX : PORTRAIT_H_PX);
    lv_obj_align(screen, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(screen, PAD_X_PX, 0);
    lv_obj_set_style_pad_row(screen, SECTION_GAP_PX, 0);

    lv_obj_t *text = lv_obj_create(screen);
    lv_obj_remove_style_all(text);
    lv_obj_set_size(text, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_clear_flag(text, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(text, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(text, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(text, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(text, TEXT_GAP_PX, 0);

    lv_obj_t *title = lv_label_create(text);
    lv_obj_set_style_text_font(title, &TITLE_FONT, 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *body = lv_label_create(text);
    lv_obj_set_width(body, LV_PCT(100));
    lv_label_set_long_mode(body, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_add_style(body, ui_theme_get_style_text_primary(), 0);
    lv_obj_set_style_text_font(body, &BODY_FONT, 0);
    lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);

    qr_code_create(screen, "");

    lv_obj_set_user_data(screen, new SetupData());
    lv_obj_add_event_cb(screen, page_deleted_cb, LV_EVENT_DELETE, nullptr);
    setup_page_set_ip(screen, nullptr);
    return screen;
}

void setup_page_set_ip(lv_obj_t *screen, const char *ip)
{
    SetupData *data = (SetupData *)lv_obj_get_user_data(screen);
    if (!data) return;

    std::string address = ip ? ip : "";
    address = address.substr(0, address.find('/'));
    const bool connected = !address.empty() && address != "-";
    if (!connected) address.clear();
    if (data->valid && data->connected == connected && data->ip == address) return;
    data->valid = true;
    data->connected = connected;
    data->ip = address;

    lv_obj_t *text  = lv_obj_get_child(screen, TEXT_CHILD);
    lv_obj_t *title = lv_obj_get_child(text, TITLE_CHILD);
    lv_obj_t *body  = lv_obj_get_child(text, BODY_CHILD);
    lv_obj_t *qr    = lv_obj_get_child(screen, QR_CHILD);

    lv_label_set_text(title, connected ? "Network Detected" : "No Network Detected");
    lv_obj_set_style_text_color(title, connected ? UI_COLOR_TEXT_SUCCESS : UI_COLOR_TEXT_ERROR, 0);
    lv_label_set_text(body, connected ? ("Scan the QR code to configure your CSTouch, or visit " + address).c_str()
                                      : "Please check your ethernet connection");
    if (connected) {
        qr_code_set_text(qr, ("http://" + address).c_str());
        lv_obj_remove_flag(qr, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(qr, LV_OBJ_FLAG_HIDDEN);
    }
}
