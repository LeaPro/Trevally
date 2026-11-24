/**
 * @file fw_update_overlay.h
 * @brief Full-screen overlay shown while a firmware update runs (/misc/fwUpdateStatus, /misc/fwDownloadProgress).
 *
 * The screen is dimmed and the current step is shown over it as a line of text. While the file is uploading the
 * progress is shown under it as a big percentage, like the playback page's volume overlay. The upload itself does
 * not change fwUpdateStatus (it stays "None"), so a non-zero fwDownloadProgress counts as downloading.
 *   Active steps (downloading, decrypting, installing, restarting): touches and the encoder still reach the UI
 *   underneath.
 *   End states (update complete after the reboot, or a failure): the overlay takes touches, and a tap dismisses it
 *   until the step changes.
 * "None", "FactoryReset" and "SoftwareReboot" are not firmware update steps: the overlay is hidden for them.
 *
 * It lives on lv_layer_top(), above every page, and follows the display size, so it survives navigation and
 * orientation changes without being rebuilt. The dimming covers the whole display; the text is centred in the content
 * area (ui_content_w()), which in landscape keeps it clear of the knob on the right.
 */

#ifndef FW_UPDATE_OVERLAY_H
#define FW_UPDATE_OVERLAY_H

#include "lvgl/lvgl.h"

/** Builds the overlay (hidden) on lv_layer_top(). Call once, after the theme is initialised. */
void fw_update_overlay_create(void);

/**
 * Shows the step for @p status (a /misc/fwUpdateStatus enum name, e.g. "Decrypting") and @p progress
 * (fwDownloadProgress, 0 to 100), or hides the overlay. Cheap when nothing changed; meant to be called every tick.
 */
void fw_update_overlay_update(const char *status, float progress);

#endif /* FW_UPDATE_OVERLAY_H */
