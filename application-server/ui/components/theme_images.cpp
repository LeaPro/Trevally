/**
 * @file theme_images.cpp
 * @brief See theme_images.h.
 */

#include "theme_images.h"
#include "../ui_design_tokens.h"

#include "stb/stb_image.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <memory>
#include <sys/stat.h>

#include <object.h>
#include <Appearance.h>
#include <Themes.h>

/* The device's root object (defined in main.cpp). */
extern std::shared_ptr<RootObject> root;

/* LVGL's (misc/cache/instance/lv_image_cache.h), which the installed headers don't include */
extern "C" void lv_image_cache_drop(const void *src);

#define IMAGE_FILES_DIR   "/mnt/data/image-files/"
#define UPDATE_CHECK_MS   1000

static std::string jsonString(const json &j, const char *key)
{
    auto it = j.find(key);
    return (it != j.end() && it->is_string()) ? it->get<std::string>() : std::string();
}

/* "HH:MM" as minutes into the day, -1 if it isn't one. */
static int minutesOf(const std::string &hhmm)
{
    int h = 0, m = 0;
    if (sscanf(hhmm.c_str(), "%d:%d", &h, &m) != 2 || h < 0 || h > 23 || m < 0 || m > 59) return -1;
    return h * 60 + m;
}

/* The active /themes config entry. */
static json activeTheme()
{
    auto appearance = std::dynamic_pointer_cast<Appearance>(::root->leafFromPath("/appearance"));
    auto themes = std::dynamic_pointer_cast<Themes>(::root->leafFromPath("/themes"));
    if (!appearance || !themes) return json::object();
    const json config = themes->configPtr->get();
    if (!config.is_object()) return json::object();

    std::string key = appearance->themePtr->get();
    if (key == "scheduled") {
        const time_t now = time(nullptr);
        struct tm local;
        localtime_r(&now, &local);
        const int nowMin = local.tm_hour * 60 + local.tm_min;
        int best = 24 * 60;
        key = "dark";
        for (const char *preset : {"dark", "light"}) {
            auto it = config.find(preset);
            if (it == config.end() || !it->is_object()) continue;
            const int start = minutesOf(jsonString(*it, "startTime"));
            if (start < 0) continue;
            const int since = (nowMin - start + 24 * 60) % (24 * 60);
            if (since < best) {
                best = since;
                key = preset;
            }
        }
    }
    auto it = config.find(key);
    return (it != config.end() && it->is_object()) ? *it : json::object();
}

std::string theme_image_file(const std::string &key)
{
    json node = activeTheme();
    std::string field = key;
    const size_t dot = key.find('.');
    if (dot != std::string::npos) {   /* "screensaver.logo" */
        auto it = node.find(key.substr(0, dot));
        if (it == node.end() || !it->is_object()) return "";
        node = *it;
        field = key.substr(dot + 1);
    }
    const std::string name = jsonString(node, field.c_str());
    if (name.empty() || name == "." || name == ".." || name.find('/') != std::string::npos) return "";
    return IMAGE_FILES_DIR + name;
}

std::string theme_image_stamp(const std::string &path)
{
    struct stat st;
    if (path.empty() || stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) return "";
    return path + "@" + std::to_string((long long)st.st_mtime) + "/" + std::to_string((long long)st.st_size);
}

/* Scales @p src (RGBA, sw x sh) by @p scale into @p dst (ARGB8888, dw x dh), output pixel (x, y) taking the source
 * under (x + ox, y + oy) / scale. Shrinking averages the source pixels each output pixel covers; enlarging is
 * bilinear. Colours are weighted by alpha so transparent pixels don't darken a logo's edges. */
static void resample(const uint8_t *src, int sw, int sh, uint8_t *dst, int dw, int dh, float scale, float ox, float oy)
{
    auto px = [&](int x, int y) { return src + ((size_t)std::clamp(y, 0, sh - 1) * sw + std::clamp(x, 0, sw - 1)) * 4; };
    for (int y = 0; y < dh; y++) {
        for (int x = 0; x < dw; x++) {
            float r = 0, g = 0, b = 0, a = 0, n = 0;
            auto add = [&](const uint8_t *p, float w) {
                const float pa = p[3] * w;
                r += p[0] * pa; g += p[1] * pa; b += p[2] * pa; a += pa; n += w;
            };
            if (scale >= 1.0f) {
                const float fx = (x + 0.5f + ox) / scale - 0.5f, fy = (y + 0.5f + oy) / scale - 0.5f;
                const int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
                const float tx = fx - x0, ty = fy - y0;
                add(px(x0, y0), (1 - tx) * (1 - ty));
                add(px(x0 + 1, y0), tx * (1 - ty));
                add(px(x0, y0 + 1), (1 - tx) * ty);
                add(px(x0 + 1, y0 + 1), tx * ty);
            } else {
                const int sx0 = (int)((x + ox) / scale), sx1 = std::max(sx0 + 1, (int)((x + 1 + ox) / scale));
                const int sy0 = (int)((y + oy) / scale), sy1 = std::max(sy0 + 1, (int)((y + 1 + oy) / scale));
                for (int sy = sy0; sy < sy1; sy++)
                    for (int sx = sx0; sx < sx1; sx++)
                        add(px(sx, sy), 1.0f);
            }
            uint8_t *d = dst + ((size_t)y * dw + x) * 4;
            d[3] = (uint8_t)std::lround(a / n);
            d[2] = a > 0 ? (uint8_t)std::lround(r / a) : 0;
            d[1] = a > 0 ? (uint8_t)std::lround(g / a) : 0;
            d[0] = a > 0 ? (uint8_t)std::lround(b / a) : 0;
        }
    }
}

const lv_image_dsc_t *theme_picture(ThemePicture &pic, const std::string &path, int boxW, int boxH, bool cover)
{
    const std::string stamp = theme_image_stamp(path);
    if (stamp.empty()) return nullptr;
    const std::string key = stamp + (cover ? "|cover|" : "|fit|") + std::to_string(boxW) + "x" + std::to_string(boxH);
    if (key == pic.key) return pic.pixels.empty() ? nullptr : &pic.dsc;

    if (pic.dsc.header.magic == LV_IMAGE_HEADER_MAGIC)
        lv_image_cache_drop(&pic.dsc);   /* LVGL caches by descriptor; its pixels are about to change */
    pic.key = key;
    pic.pixels.clear();
    pic.dsc = {};

    int sw = 0, sh = 0, channels = 0;
    uint8_t *src = stbi_load(path.c_str(), &sw, &sh, &channels, 4);
    if (!src) {
        fprintf(stderr, "theme_images: %s is not a readable image\n", path.c_str());
        return nullptr;
    }
    const float sx = (float)boxW / sw, sy = (float)boxH / sh;
    const float scale = cover ? std::max(sx, sy) : std::min(sx, sy);
    const int dw = cover ? boxW : std::max(1, (int)std::lround(sw * scale));
    const int dh = cover ? boxH : std::max(1, (int)std::lround(sh * scale));
    const float ox = cover ? (sw * scale - dw) / 2 : 0, oy = cover ? (sh * scale - dh) / 2 : 0;
    pic.pixels.resize((size_t)dw * dh * 4);
    resample(src, sw, sh, pic.pixels.data(), dw, dh, scale, ox, oy);
    stbi_image_free(src);

    pic.dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    pic.dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
    pic.dsc.header.w = dw;
    pic.dsc.header.h = dh;
    pic.dsc.header.stride = dw * 4;
    pic.dsc.data_size = pic.pixels.size();
    pic.dsc.data = pic.pixels.data();
    return &pic.dsc;
}

/* ---- Operating-mode background ---- */

static lv_style_t backgroundStyle;
static bool backgroundStyleReady = false;
static ThemePicture backgroundPicture;
static std::string backgroundShown;   /* the picture key on the style; "" without an image */
static uint32_t backgroundCheckedAt = 0;

lv_style_t *theme_background_style()
{
    if (!backgroundStyleReady) {
        lv_style_init(&backgroundStyle);
        backgroundStyleReady = true;
    }
    return &backgroundStyle;
}

bool theme_background_has_image()
{
    return !backgroundShown.empty();
}

void theme_background_update()
{
    if (backgroundStyleReady && lv_tick_elaps(backgroundCheckedAt) < UPDATE_CHECK_MS) return;
    backgroundCheckedAt = lv_tick_get();
    lv_style_t *style = theme_background_style();

    const int w = lv_display_get_horizontal_resolution(nullptr), h = lv_display_get_vertical_resolution(nullptr);
    const lv_image_dsc_t *image = theme_picture(backgroundPicture, theme_image_file("background"), w, h, true);
    const std::string shown = image ? backgroundPicture.key : std::string();
    if (shown == backgroundShown) return;
    backgroundShown = shown;
    if (image) lv_style_set_bg_image_src(style, image);
    else lv_style_remove_prop(style, LV_STYLE_BG_IMAGE_SRC);
    lv_obj_report_style_change(style);
    /* widgets that blend into the background colour follow its style's change event (see picker_wheel.cpp) */
    lv_obj_report_style_change(ui_theme_get_style_bg());
}
