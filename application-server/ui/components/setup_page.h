/**
 * @file setup_page.h
 * @brief First-run setup page: tells the user whether the device is on a network and
 *        shows a QR code leading to the device's web UI, where it is configured.
 *
 *        Network detected: green title, a QR code encoding http://<ip>, and the address
 *        in words. No network: red title asking to check the ethernet connection, and no
 *        QR code (there is no address to send the user to).
 */

#ifndef SETUP_PAGE_H
#define SETUP_PAGE_H

#include "lvgl/lvgl.h"

/** Creates the page's content at the top left of @p parent: ui_content_w() wide. Starts as "no network". */
lv_obj_t *setup_page_create(lv_obj_t *parent);

/**
 * Sets the device's address. @p ip may be in CIDR notation ("192.168.0.100/24", the prefix is
 * dropped). NULL, "" and "-" mean there is no network. No-op if unchanged.
 */
void setup_page_set_ip(lv_obj_t *page, const char *ip);

#endif /* SETUP_PAGE_H */
