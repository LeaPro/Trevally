/**
 * @file qr_code.cpp
 * @brief QR code on a white card (see qr_code.h).
 */

#include "qr_code.h"

#include <cstring>
#include <string>

/* The card's children: the lv_qrcode. */
#define QR_CHILD 0

/* The text currently encoded, kept in the card's user data. */
static void card_deleted_cb(lv_event_t *e)
{
    lv_obj_t *card = (lv_obj_t *)lv_event_get_target(e);
    delete (std::string *)lv_obj_get_user_data(card);
    lv_obj_set_user_data(card, nullptr);
}

lv_obj_t *qr_code_create(lv_obj_t *parent, const char *text, int size_px)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, size_px + 2 * QR_CODE_PADDING_PX, size_px + 2 * QR_CODE_PADDING_PX);
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(card, QR_CODE_PADDING_PX, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *qr = lv_qrcode_create(card);
    lv_qrcode_set_size(qr, size_px);
    lv_qrcode_set_dark_color(qr, lv_color_black());
    lv_qrcode_set_light_color(qr, lv_color_white());

    lv_obj_set_user_data(card, new std::string());
    lv_obj_add_event_cb(card, card_deleted_cb, LV_EVENT_DELETE, nullptr);
    qr_code_set_text(card, text);
    return card;
}

void qr_code_set_text(lv_obj_t *card, const char *text)
{
    std::string *shown = (std::string *)lv_obj_get_user_data(card);
    if (!shown) return;
    if (!text) text = "";
    if (*shown == text) return;
    *shown = text;

    lv_obj_t *qr = lv_obj_get_child(card, QR_CHILD);
    if (*shown != "")
        lv_qrcode_update(qr, text, (uint32_t)std::strlen(text));
}
