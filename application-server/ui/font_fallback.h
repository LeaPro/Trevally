/**
 * @file font_fallback.h
 * @brief Characters the Satoshi fonts don't have (curly quotes, accents beyond Latin-1, Greek, Cyrillic, Arabic,
 *        Hebrew, Chinese, ...), drawn from TTF/OTF fonts at runtime with FreeType.
 *
 * Satoshi (ui/fonts/satoshi_medium_*.cpp) is a set of bitmap fonts with every character the Satoshi font has
 * (Latin-1, Latin Extended-A, curly quotes, dashes, ...), except the 140 px size: it only shows numbers and times,
 * so it has just the digits, space, % - . : and the degree sign. Each size's fallback is the
 * matching ui_font_fallback_<size> here, and from there the chain is:
 *   Montserrat   LVGL's symbol icons (LV_SYMBOL_*); checked before the TTFs, which have other characters there
 *   DejaVu Sans  punctuation, Latin, Greek, Cyrillic, Hebrew, Arabic (with the forms LVGL's Arabic shaping uses)
 *   Source Han Sans CN Medium  Chinese (Simplified), and Japanese kana / Korean Hangul
 * The TTFs come from FONT_DIRS (the firmware image's trevally-fonts recipe installs them; so does the host build's
 * docker image). A font that isn't found is left out of the chain; with neither, the chain is just Montserrat,
 * as it was before.
 *
 * To regenerate a Satoshi font, keep its fallback (lv_font_conv ... --lv-fallback ui_font_fallback_<size>) and the
 * hand-edited PUBLIC FONT block at the end of the file (C++ field order, and the line_height / base_line it had
 * with Latin-1 only: the full character set makes the lines taller, which would move every layout).
 */

#ifndef FONT_FALLBACK_H
#define FONT_FALLBACK_H

#include "lvgl/lvgl.h"

extern lv_font_t ui_font_fallback_14;
extern lv_font_t ui_font_fallback_20;
extern lv_font_t ui_font_fallback_24;
extern lv_font_t ui_font_fallback_32;
extern lv_font_t ui_font_fallback_48;
extern lv_font_t ui_font_fallback_140;

/** Loads the TTF fonts into the chains. Call once, after lv_init() and before any text is drawn. */
void ui_font_fallback_init(void);

#endif /* FONT_FALLBACK_H */
