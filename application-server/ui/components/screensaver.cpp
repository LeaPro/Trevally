/**
 * @file screensaver.cpp
 * @brief See screensaver.h.
 */

#include "screensaver.h"
#include "adaptive_text.h"
#include "album_art.h"
#include "audio_source.h"
#include "theme_images.h"
#include "../nav.h"
#include "tile.h"
#include "../ui_design_tokens.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>

#include <object.h>
#include <DateTime.h>
#include <Screensaver.h>
#include <Weather.h>

/* The device's root object (defined in main.cpp). */
extern std::shared_ptr<RootObject> root;

LV_IMAGE_DECLARE(screensaver_glow_portrait);
LV_IMAGE_DECLARE(screensaver_glow_landscape);


#define REFRESH_MS      500

/* Figma "Screensaver ..." frames. The content column is the 480x535 above the knob in portrait and the 535x480
 * left of it in landscape; the streaming block sits at its bottom, 24px up. */
#define PAD_PX           24
#define TOP_LABEL_H_PX   29    /* city / date, 24px text */
#define STREAM_H_PX      125   /* art, title and artist */
#define STREAM_GAP_PX    8
#define ART_PX           64
#define ART_RADIUS_PX    8
#define ICON_PX          48
#define ICON_GAP_PX      16
#define LOGO_MAX_H_PX    250   /* the logo is fitted into the column, 48px in from each side, and this tall */
#define LOGO_SIDE_PX     48
#define PORTRAIT_SHADE_H_PX   614   /* image + streaming: darkening toward the bottom */
#define LANDSCAPE_SHADE_H_PX  240

/* Everything is in the theme's colours (Figma text-primary / -secondary / -tertiary; tertiary is the secondary
 * text colour faded), except text on a background image, which is white and neutral-200. */
#define TEXT_TERTIARY_OPA  LV_OPA_70
#define COLOR_ON_IMAGE     0xFFFFFF
#define COLOR_ON_IMAGE_2   0xE0E2E5

/* The weather and date-time glow (ui/icons/screensaver_glow_*.png) is a mask of the Figma glow's brightness over
 * black, 0-80. Figma's glow is bg-secondary (#3A3C40, brightness ~60) light on the background, so the mask is
 * tinted with the background moved toward bg-secondary by 80/60: the dark theme comes out as in Figma, and every
 * other theme gets the same glow in its own colours. */
#define GLOW_MASK_FULL       80
#define GLOW_BG_SECONDARY    60

static lv_color_t glowTint()
{
    const lv_color_t bg = ui_get_background_color(), sec = ui_get_bg_secondary_color();
    auto channel = [](int b, int s) { return (uint8_t)std::clamp(b + (s - b) * GLOW_MASK_FULL / GLOW_BG_SECONDARY, 0, 255); };
    return lv_color_make(channel(bg.red, sec.red), channel(bg.green, sec.green), channel(bg.blue, sec.blue));
}

template <class T> static std::shared_ptr<T> leaf(const char *path)
{
    return std::dynamic_pointer_cast<T>(::root->leafFromPath(path));
}

static std::string jsonString(const json &j, const char *key)
{
    auto it = j.find(key);
    return (it != j.end() && it->is_string()) ? it->get<std::string>() : std::string();
}

/* ---- Settings ---- */

bool screensaver_enabled()
{
    auto s = leaf<Screensaver>("/screensaver");
    return s && s->enabledPtr->get();
}

uint32_t screensaver_timeout_ms()
{
    auto s = leaf<Screensaver>("/screensaver");
    return s ? s->timeoutSecondsPtr->get() * 1000 : 0;
}

static std::string screensaverType()
{
    auto s = leaf<Screensaver>("/screensaver");
    const std::string type = s ? s->typePtr->get() : std::string();
    return type == "weather" || type == "date-time" ? type : std::string("background-logo");
}

static bool showAnalogClock()
{
    auto s = leaf<Screensaver>("/screensaver");
    return s && s->showAnalogClockPtr->get();
}

static bool showStreamingInfo()
{
    auto s = leaf<Screensaver>("/screensaver");
    return s && s->showStreamingInfoPtr->get();
}

std::string screensaver_signature()
{
    const std::string type = screensaverType();
    std::string sig = type + (ui_is_landscape() ? "|landscape" : "|portrait");
    if (type == "date-time" && showAnalogClock()) sig += "|clock";
    for (lv_color_t c : {ui_get_background_color(), ui_get_bg_secondary_color(), ui_get_text_primary_color(),
                         ui_get_text_secondary_color(), ui_get_icon_primary_color(), ui_get_brand_color()}) {
        char hex[8];
        snprintf(hex, sizeof(hex), "|%06x", (unsigned)lv_color_to_u32(c) & 0xFFFFFF);
        sig += hex;
    }
    if (type == "background-logo")
        sig += "|" + theme_image_stamp(theme_image_file("screensaver.background")) + "|" +
               theme_image_stamp(theme_image_file("screensaver.logo"));
    return sig;
}

/* The screensaver's images: kept, so it comes up at once the next time. */
static ThemePicture backgroundPicture, logoPicture;

/* ---- Content ---- */

struct Layout {
    int w, h;        /* the content column */
    int infoY, infoH;
    int streamY;
    int logoAreaH;   /* the logo is centred in this, from the top */
};

static Layout layout()
{
    if (ui_is_landscape())
        return {535, 480, PAD_PX + TOP_LABEL_H_PX, 480 - 2 * PAD_PX - TOP_LABEL_H_PX - STREAM_H_PX,
                480 - PAD_PX - STREAM_H_PX, 480 - PAD_PX - STREAM_H_PX};
    return {480, 535, PAD_PX + TOP_LABEL_H_PX, 535 - 2 * PAD_PX - TOP_LABEL_H_PX - STREAM_H_PX,
            535 - PAD_PX - STREAM_H_PX, 535 - 2 * PAD_PX - STREAM_H_PX};
}

/* The date-time clock hands (Figma "Clock"): they turn about the knob's centre (the Figma "Knob": 282px,
 * centred horizontally, top at y=535) and run out from under it. In landscape the knob is on the right, and 12
 * o'clock stays up. Drawn in the theme's icon colour, the seconds hand in its brand colour. */
#define KNOB_CENTRE_ALONG_PX   676   /* from the panel's top in portrait, from the left in landscape */
#define KNOB_CENTRE_ACROSS_PX  240
#define CLOCK_TICK_MS          100
enum { HAND_SECONDS, HAND_HOUR, HAND_MINUTE, HAND_COUNT };   /* bottom to top, as in Figma */
static const int HAND_LENGTH[HAND_COUNT] = {557, 212, 318};
static const int HAND_WIDTH[HAND_COUNT]  = {3, 6, 6};

struct Hand {
    lv_obj_t *line = nullptr;
    lv_point_precise_t points[2];   /* lv_line keeps a pointer to them */
};

struct Saver {
    std::string type;
    lv_timer_t *timer = nullptr;
    lv_timer_t *clockTimer = nullptr;   /* date-time */
    Hand hands[HAND_COUNT];
    int shownSecond = -1;
    lv_obj_t *topLabel = nullptr;       /* weather / date-time */
    lv_obj_t *iconHolder = nullptr;     /* weather */
    lv_obj_t *condition = nullptr;
    lv_obj_t *tempHolder = nullptr;
    lv_obj_t *time = nullptr;           /* date-time */
    lv_obj_t *stream = nullptr, *title = nullptr, *artist = nullptr;
    lv_obj_t *wash = nullptr, *shade = nullptr;   /* background image, while streaming info shows */
    std::string shownTop, shownCondition, shownDegrees = "\x01", shownTime, shownTitle, shownArtist;
    int shownIcon = -1, shownStream = -1;
};

static lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, int w)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(l, "");
    if (w > 0) {
        lv_obj_set_width(l, w);
        lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    }
    return l;
}

/* Points @p hand at @p degrees (clockwise from 12 o'clock). The line object only covers the hand, so a tick
 * redraws just the area the hand leaves and enters. */
static void aimHand(Hand &hand, int which, float degrees)
{
    const bool landscape = ui_is_landscape();
    const float px = landscape ? KNOB_CENTRE_ALONG_PX : KNOB_CENTRE_ACROSS_PX;
    const float py = landscape ? KNOB_CENTRE_ACROSS_PX : KNOB_CENTRE_ALONG_PX;
    const float rad = degrees * (float)M_PI / 180.0f;
    const float tx = px + HAND_LENGTH[which] * sinf(rad), ty = py - HAND_LENGTH[which] * cosf(rad);
    const int pad = HAND_WIDTH[which];   /* room for the round caps */
    const int x0 = (int)floorf(std::min(px, tx)) - pad, y0 = (int)floorf(std::min(py, ty)) - pad;
    const int x1 = (int)ceilf(std::max(px, tx)) + pad, y1 = (int)ceilf(std::max(py, ty)) + pad;
    hand.points[0] = {px - x0, py - y0};
    hand.points[1] = {tx - x0, ty - y0};
    lv_obj_set_pos(hand.line, x0, y0);
    lv_obj_set_size(hand.line, x1 - x0, y1 - y0);
    lv_line_set_points(hand.line, hand.points, 2);
}

static void tickClock(Saver *s)
{
    const time_t now = ::time(nullptr);
    struct tm local;
    localtime_r(&now, &local);
    if (local.tm_sec == s->shownSecond) return;
    s->shownSecond = local.tm_sec;
    aimHand(s->hands[HAND_SECONDS], HAND_SECONDS, local.tm_sec * 6.0f);
    aimHand(s->hands[HAND_MINUTE], HAND_MINUTE, (local.tm_min + local.tm_sec / 60.0f) * 6.0f);
    aimHand(s->hands[HAND_HOUR], HAND_HOUR, (local.tm_hour % 12 + local.tm_min / 60.0f) * 30.0f);
}

static void buildClock(lv_obj_t *parent, Saver *s)
{
    for (int i = 0; i < HAND_COUNT; i++) {
        lv_obj_t *line = lv_line_create(parent);
        lv_obj_set_clickable(line, false);
        lv_obj_set_style_line_width(line, HAND_WIDTH[i], 0);
        lv_obj_set_style_line_rounded(line, true, 0);
        lv_obj_set_style_line_color(line, i == HAND_SECONDS ? ui_get_brand_color() : ui_get_icon_primary_color(), 0);
        s->hands[i].line = line;
    }
    tickClock(s);
    s->clockTimer = lv_timer_create([](lv_timer_t *t) { tickClock((Saver *)lv_timer_get_user_data(t)); },
                                    CLOCK_TICK_MS, s);
}

static void refresh(Saver *s)
{
    if (s->topLabel) {
        std::string top;
        if (s->type == "weather") {
            if (auto weather = leaf<Weather>("/weather")) {
                const json config = weather->configPtr->get();
                auto it = config.find("default");
                if (it != config.end() && it->is_object()) {
                    top = jsonString(*it, "city");
                    const std::string condition = jsonString(*it, "condition");
                    if (condition != s->shownCondition) {
                        s->shownCondition = condition;
                        lv_label_set_text(s->condition, condition.c_str());
                    }
                }
            }
        } else {
            const time_t now = ::time(nullptr);
            struct tm local;
            localtime_r(&now, &local);
            char text[32];
            strftime(text, sizeof(text), "%a %b %-d", &local);
            top = text;
        }
        if (top != s->shownTop) {
            s->shownTop = top;
            lv_label_set_text(s->topLabel, top.c_str());
        }
    }

    if (s->iconHolder) {
        const int icon = (int)tile_weather_icon("");
        if (icon != s->shownIcon) {
            s->shownIcon = icon;
            lv_obj_clean(s->iconHolder);
            tile_weather_icon_create(s->iconHolder, (ui_icon_type_t)icon, ICON_PX);
        }
        const std::string degrees = tile_weather_degrees("");
        if (degrees != s->shownDegrees) {
            s->shownDegrees = degrees;
            lv_obj_clean(s->tempHolder);
            if (!degrees.empty())
                tile_weather_degrees_create(s->tempHolder, degrees, &satoshi_medium_140, ui_theme_get_style_text_primary());
        }
    }

    if (s->time) {
        auto dateTime = leaf<DateTime>("/dateTime");
        const std::string text = dateTime ? dateTime->displayTimePtr->get() : std::string();
        if (text != s->shownTime) {
            s->shownTime = text;
            lv_label_set_text(s->time, text.c_str());
        }
    }

    const AudioSource *source = showStreamingInfo() ? audio_source_active() : nullptr;
    const std::string title = source ? audio_source_text(source, "trackTitle") : std::string();
    const int stream = title.empty() ? 0 : 1;
    if (stream != s->shownStream) {
        s->shownStream = stream;
        for (lv_obj_t *obj : {s->stream, s->wash, s->shade}) {
            if (!obj) continue;
            lv_obj_set_hidden(obj, !stream);
        }
    }
    if (stream) {
        /* one line each: text that doesn't fit scrolls (adaptive_text.h), as on the now-playing ribbon */
        const std::string artist = audio_source_text(source, "trackArtist");
        lv_obj_update_layout(s->stream);   /* the labels' widths, which decide whether the text fits */
        if (title != s->shownTitle) {
            s->shownTitle = title;
            ui_adaptive_text_set(s->title, title.c_str(), 1);
        }
        if (artist != s->shownArtist) {
            s->shownArtist = artist;
            ui_adaptive_text_set(s->artist, artist.c_str(), 1);
        }
    }
}

/* The title and artist are white on a background image, else in the theme's text colours. */
static lv_obj_t *streamBlock(lv_obj_t *column, Saver *s, const Layout &L, bool onImage)
{
    lv_obj_t *block = tile_container(column);
    lv_obj_set_size(block, L.w, LV_SIZE_CONTENT);   /* STREAM_H_PX in Figma; the fonts' line heights decide */
    lv_obj_set_pos(block, 0, L.streamY);
    lv_obj_set_layout(block, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(block, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(block, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(block, STREAM_GAP_PX, 0);
    album_art_create(block, ART_PX, ART_PX, ART_RADIUS_PX);

    lv_obj_t *music = tile_container(block);
    lv_obj_set_size(music, L.w, LV_SIZE_CONTENT);
    lv_obj_set_layout(music, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(music, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(music, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    const lv_color_t primary = onImage ? lv_color_hex(COLOR_ON_IMAGE) : ui_get_text_primary_color();
    const lv_color_t secondary = onImage ? lv_color_hex(COLOR_ON_IMAGE_2) : ui_get_text_secondary_color();
    s->title = label(music, &satoshi_medium_24, primary, L.w - 2 * PAD_PX);
    s->artist = label(music, &satoshi_medium_20, secondary, L.w - 2 * PAD_PX);
    lv_obj_set_hidden(block, true);
    return block;
}

/* Weather and date-time: city / date at the top, conditions and the big number centred under it. */
static void buildInfo(lv_obj_t *column, Saver *s, const Layout &L)
{
    s->topLabel = label(column, &satoshi_medium_24, ui_get_text_secondary_color(), L.w - 2 * PAD_PX);
    lv_obj_set_style_text_opa(s->topLabel, TEXT_TERTIARY_OPA, 0);
    lv_obj_set_pos(s->topLabel, PAD_PX, PAD_PX);

    lv_obj_t *info = tile_container(column);
    lv_obj_set_size(info, L.w, L.infoH);
    lv_obj_set_pos(info, 0, L.infoY);
    lv_obj_set_layout(info, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* the conditions row: on date-time it's there but invisible, so the time sits where the temperature does */
    lv_obj_t *row = tile_container(info);
    lv_obj_set_size(row, LV_SIZE_CONTENT, ICON_PX);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, ICON_GAP_PX, 0);

    if (s->type == "weather") {
        s->iconHolder = tile_container(row);
        lv_obj_set_size(s->iconHolder, ICON_PX, ICON_PX);
        s->condition = label(row, &satoshi_medium_24, ui_get_text_secondary_color(), 0);
        s->tempHolder = tile_container(info);
        lv_obj_set_size(s->tempHolder, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    } else {
        lv_obj_set_style_opa(row, LV_OPA_TRANSP, 0);
        s->time = label(info, &satoshi_medium_140, ui_get_text_primary_color(), 0);
    }
}

void screensaver_build(lv_obj_t *parent)
{
    Saver *s = new Saver{};
    s->type = screensaverType();
    const Layout L = layout();
    const int dispW = lv_display_get_horizontal_resolution(nullptr);
    const int dispH = lv_display_get_vertical_resolution(nullptr);

    lv_obj_t *rootObj = tile_container(parent);
    lv_obj_set_size(rootObj, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(rootObj, ui_get_background_color(), 0);
    lv_obj_set_style_bg_opa(rootObj, LV_OPA_COVER, 0);

    bool onImage = false;
    const lv_image_dsc_t *logo = nullptr;
    if (s->type == "background-logo") {
        if (const lv_image_dsc_t *bg = theme_picture(backgroundPicture, theme_image_file("screensaver.background"), dispW, dispH, true)) {
            onImage = true;
            lv_image_set_src(lv_image_create(rootObj), bg);
            if (!ui_is_landscape()) {
                s->wash = tile_container(rootObj);
                lv_obj_set_size(s->wash, LV_PCT(100), LV_PCT(100));
                lv_obj_set_style_bg_color(s->wash, lv_color_white(), 0);
                lv_obj_set_style_bg_opa(s->wash, LV_OPA_20, 0);
                lv_obj_set_hidden(s->wash, true);
            }
            s->shade = tile_container(rootObj);
            lv_obj_set_size(s->shade, LV_PCT(100), ui_is_landscape() ? LANDSCAPE_SHADE_H_PX : PORTRAIT_SHADE_H_PX);
            lv_obj_align(s->shade, LV_ALIGN_BOTTOM_MID, 0, 0);
            lv_obj_set_style_bg_opa(s->shade, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(s->shade, lv_color_black(), 0);
            lv_obj_set_style_bg_grad_color(s->shade, lv_color_black(), 0);
            lv_obj_set_style_bg_grad_dir(s->shade, LV_GRAD_DIR_VER, 0);
            lv_obj_set_style_bg_main_opa(s->shade, LV_OPA_TRANSP, 0);
            lv_obj_set_style_bg_grad_opa(s->shade, ui_is_landscape() ? LV_OPA_COVER : LV_OPA_90, 0);
            lv_obj_set_hidden(s->shade, true);
        }
        logo = theme_picture(logoPicture, theme_image_file("screensaver.logo"), L.w - 2 * LOGO_SIDE_PX, LOGO_MAX_H_PX, false);
    } else {
        lv_obj_t *glow = lv_image_create(rootObj);
        lv_image_set_src(glow, ui_is_landscape() ? &screensaver_glow_landscape : &screensaver_glow_portrait);
        lv_obj_set_style_image_recolor(glow, glowTint(), 0);
        lv_obj_set_style_image_recolor_opa(glow, LV_OPA_COVER, 0);
        if (s->type == "date-time" && showAnalogClock()) buildClock(rootObj, s);   /* over the glow, under the text */
    }

    lv_obj_t *column = tile_container(rootObj);
    lv_obj_set_size(column, L.w, L.h);
    lv_obj_set_pos(column, 0, 0);

    if (s->type == "background-logo") {
        if (logo) {
            lv_obj_t *image = lv_image_create(column);
            lv_image_set_src(image, logo);
            lv_obj_set_pos(image, (L.w - (int)logo->header.w) / 2, (L.logoAreaH - (int)logo->header.h) / 2);
        }
    } else {
        buildInfo(column, s, L);
    }
    s->stream = streamBlock(column, s, L, onImage);

    /* The LEA logo beneath the knob, where every other screen has it (nav_logo_area()); white over a background
     * image, like the text there */
    if (lv_obj_t *logo = UI_ICON_CREATE_DEFAULT(rootObj, UI_ICON_LOGO)) {
        const lv_area_t area = nav_logo_area();
        lv_obj_set_pos(logo, area.x1, area.y1);
        if (onImage) lv_obj_set_style_image_recolor(logo, lv_color_hex(COLOR_ON_IMAGE), 0);
    }

    s->timer = lv_timer_create([](lv_timer_t *t) { refresh((Saver *)lv_timer_get_user_data(t)); }, REFRESH_MS, s);
    lv_obj_add_event_cb(rootObj, [](lv_event_t *e) {
        Saver *s = (Saver *)lv_event_get_user_data(e);
        lv_timer_delete(s->timer);
        if (s->clockTimer) lv_timer_delete(s->clockTimer);
        delete s;
    }, LV_EVENT_DELETE, s);
    refresh(s);
}
