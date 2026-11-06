/**
 * @file qr_code.h
 * @brief QR code on a white card. Always black on white, whatever the theme, so
 *        scanners can read it. Size, text and the card's placement are up to the caller.
 */

#ifndef QR_CODE_H
#define QR_CODE_H

#include "lvgl/lvgl.h"

#define QR_CODE_SIZE_PX     150   /* default side of the QR code itself */
#define QR_CODE_PADDING_PX  10    /* white border of the card around it (also its quiet zone) */

/**
 * @brief Create a QR code encoding @p text. The returned object is the card:
 *        (size_px + 2 * QR_CODE_PADDING_PX) square. Position it as any other object.
 * @param text     Encoded as is; NULL or "" leaves the code empty.
 * @param size_px  Side of the QR code, without the card's padding.
 */
lv_obj_t *qr_code_create(lv_obj_t *parent, const char *text, int size_px = QR_CODE_SIZE_PX);

/** Changes the encoded text. No-op if unchanged; empty text keeps the code already shown (hide the card instead). */
void qr_code_set_text(lv_obj_t *qr, const char *text);

#endif /* QR_CODE_H */
