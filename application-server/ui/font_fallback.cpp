/**
 * @file font_fallback.cpp
 * @brief See font_fallback.h.
 */

#include "font_fallback.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>

extern const lv_font_t lv_font_montserrat_14;
extern const lv_font_t lv_font_montserrat_20;
extern const lv_font_t lv_font_montserrat_24;
extern const lv_font_t lv_font_montserrat_32;

/* Searched in order; an AppImage carries its own copies under $APPDIR. */
static const char *const FONT_DIRS[] = {"$APPDIR/usr/share/fonts/trevally", "/usr/share/fonts/trevally"};
static const char *const TTF_FONTS[] = {"DejaVuSans.ttf", "SourceHanSansCN-Medium.otf"};   /* chain order */

/* Until ui_font_fallback_init() runs, each fallback finds nothing itself and passes on to Montserrat, the
 * Satoshi fonts' fallback before. */
static bool find_nothing(const lv_font_t *, lv_font_glyph_dsc_t *, uint32_t, uint32_t)
{
    return false;
}

static lv_font_t passing_to(const lv_font_t *next)
{
    lv_font_t font{};
    font.get_glyph_dsc = find_nothing;
    font.fallback = next;
    return font;
}

lv_font_t ui_font_fallback_14  = passing_to(&lv_font_montserrat_14);
lv_font_t ui_font_fallback_20  = passing_to(&lv_font_montserrat_20);
lv_font_t ui_font_fallback_24  = passing_to(&lv_font_montserrat_24);
lv_font_t ui_font_fallback_32  = passing_to(&lv_font_montserrat_32);
lv_font_t ui_font_fallback_48  = passing_to(&lv_font_montserrat_32);
lv_font_t ui_font_fallback_140 = passing_to(&lv_font_montserrat_32);

static std::string find_font(const char *file)
{
    for (const char *dir : FONT_DIRS) {
        std::string path = dir;
        if (path.rfind("$APPDIR", 0) == 0) {
            const char *appdir = getenv("APPDIR");
            if (!appdir) continue;
            path = appdir + path.substr(7);
        }
        path += std::string("/") + file;
        struct stat st;
        if (stat(path.c_str(), &st) == 0) return path;
    }
    return "";
}

#if LV_USE_FREETYPE
/* Builds @p fallback into: a copy of @p montserrat (so its symbols come first), then the TTFs at @p size px. */
static void build_chain(lv_font_t &fallback, const lv_font_t &montserrat, uint32_t size,
                        const std::string (&paths)[sizeof(TTF_FONTS) / sizeof(TTF_FONTS[0])])
{
    const lv_font_t *next = nullptr;
    for (int i = (int)(sizeof(TTF_FONTS) / sizeof(TTF_FONTS[0])) - 1; i >= 0; i--) {
        if (paths[i].empty()) continue;
        lv_font_t *ttf = lv_freetype_font_create(paths[i].c_str(), LV_FREETYPE_FONT_RENDER_MODE_BITMAP, size,
                                                 LV_FREETYPE_FONT_STYLE_NORMAL);
        if (!ttf) {
            fprintf(stderr, "font_fallback: %s at %u px could not be loaded\n", paths[i].c_str(), (unsigned)size);
            continue;
        }
        ttf->fallback = next;
        next = ttf;
    }
    if (!next) return;   /* no TTFs: keep passing to Montserrat */
    fallback = montserrat;   /* its glyph data is shared, not copied */
    fallback.fallback = next;
}
#endif

void ui_font_fallback_init(void)
{
#if LV_USE_FREETYPE
    std::string paths[sizeof(TTF_FONTS) / sizeof(TTF_FONTS[0])];
    for (size_t i = 0; i < sizeof(TTF_FONTS) / sizeof(TTF_FONTS[0]); i++) {
        paths[i] = find_font(TTF_FONTS[i]);
        fprintf(stderr, "font_fallback: %s: %s\n", TTF_FONTS[i], paths[i].empty() ? "not found" : paths[i].c_str());
    }
    build_chain(ui_font_fallback_14, lv_font_montserrat_14, 14, paths);
    build_chain(ui_font_fallback_20, lv_font_montserrat_20, 20, paths);
    build_chain(ui_font_fallback_24, lv_font_montserrat_24, 24, paths);
    build_chain(ui_font_fallback_32, lv_font_montserrat_32, 32, paths);
    build_chain(ui_font_fallback_48, lv_font_montserrat_32, 48, paths);
    build_chain(ui_font_fallback_140, lv_font_montserrat_32, 140, paths);
#else
    fprintf(stderr, "font_fallback: LVGL is built without FreeType; Satoshi falls back to Montserrat only\n");
#endif
}
