/**
 * @file theme_images.h
 * @brief The active theme's images (see Themes.cpp): the operating-mode "background" and the screensaver's
 *        "background" / "logo", which name files in /mnt/data/image-files (uploaded through the Files API).
 *
 * Uploads can be JPEG or PNG of any size, so they are decoded here and scaled to the size they are shown at. Each
 * is kept until its file, the theme or the size changes.
 */

#ifndef THEME_IMAGES_H
#define THEME_IMAGES_H

#include "lvgl/lvgl.h"
#include <cstdint>
#include <string>
#include <vector>

/** The file of the active theme's image @p key ("background", "screensaver.background" or "screensaver.logo");
 *  "" when the theme has none. "scheduled" is whichever of the dark and light presets started last by its
 *  startTime. */
std::string theme_image_file(const std::string &key);

/** Identifies a file's contents: changes when it is replaced. "" when it doesn't exist. */
std::string theme_image_stamp(const std::string &path);

/** A decoded image. Its descriptor stays valid (and keeps its address) for the life of the ThemePicture; when
 *  the image changes, LVGL's cache of it is dropped. */
struct ThemePicture {
    std::string key;
    std::vector<uint8_t> pixels;   /* ARGB8888 */
    lv_image_dsc_t dsc = {};
};

/** The image in @p path in @p pic, scaled to cover a @p boxW x @p boxH box (centre-cropped to it) or, without
 *  @p cover, to fit inside it. nullptr when there is no file or it isn't an image. */
const lv_image_dsc_t *theme_picture(ThemePicture &pic, const std::string &path, int boxW, int boxH, bool cover);

/** The style every screen has (nav_screen_create(), the home screen): the active theme's "background" image,
 *  covering the display, over the theme's background colour; no image when the theme has none. */
lv_style_t *theme_background_style();

/** Whether the screens currently show a background image (widgets that paint the background colour to blend in,
 *  like the picker wheel's fades, do something else then). */
bool theme_background_has_image();

/** Every UI tick: follows the theme, its file and the display size. */
void theme_background_update();

#endif /* THEME_IMAGES_H */
