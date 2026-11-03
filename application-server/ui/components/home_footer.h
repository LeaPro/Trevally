/**
 * @file home_footer.h
 * @brief The bottom strip of the home page (Figma "Streaming Controls", bottom): date and time at the left,
 *        the LEA logo centred, the sleep icon at the right.
 *
 * It is the bottom of the streaming controls panel: in portrait it is as wide as the display, in landscape it is
 * the bottom of the 356px panel on the right. The date and time follow /dateTime (displayTime, displayDate) by a
 * timer of the strip's own.
 */

#ifndef HOME_FOOTER_H
#define HOME_FOOTER_H

#include "lvgl/lvgl.h"

#define HOME_FOOTER_H_PX 98

/** Creates the footer, @p w pixels wide and HOME_FOOTER_H_PX high, in @p parent. The caller positions it.
 *  @p logo_bottom is the gap between the logo and the bottom edge. */
lv_obj_t *home_footer_create(lv_obj_t *parent, int w, int logo_bottom);

#endif /* HOME_FOOTER_H */
