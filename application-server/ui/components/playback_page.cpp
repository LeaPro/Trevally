/**
 * @file playback_page.cpp
 * @brief See playback_page.h.
 */

#include "playback_page.h"
#include "audio_source.h"
#include "album_art.h"
#include "play_button.h"
#include "tile.h"
#include "../nav.h"
#include "../ui_design_tokens.h"

#include <algorithm>
#include <cstdio>
#include <string>

/* Geometry from the Figma "Streaming Controls" frames (portrait 5455:9472, landscape 5552:27240). The text row and the
 * progress block form one column; in portrait it starts below the art, in landscape the row is at the top of the right-hand
 * panel and the progress block at its bottom. */
#define PORTRAIT_ART_H        410
#define PORTRAIT_INFO_Y       420    /* top of the text row */
#define PORTRAIT_LOGO_BOTTOM  6
#define LANDSCAPE_ART_W       498
#define LANDSCAPE_PROGRESS_BOTTOM 21 /* from the bottom edge */

#define INFO_X                16     /* from the left edge of the column */
#define INFO_H                72
#define INFO_PLAY_RIGHT       8      /* the play button is this far in from the right edge */
#define INFO_PLAY_GAP         8      /* between the text and the play button */
#define PROGRESS_GAP_ABOVE    12     /* between the text row and the progress bar (portrait) */
#define PROGRESS_SIDE_PAD     16
#define PROGRESS_BAR_H        6
#define PROGRESS_KNOB_PX      14
#define PROGRESS_KNOB_OVERHANG ((PROGRESS_KNOB_PX - PROGRESS_BAR_H) / 2)   /* knob above / below the bar */
#define PROGRESS_TIMES_GAP    8      /* between the bar and the times */
#define PROGRESS_TRACK_OPA    ((lv_opa_t)31)   /* 12% */

#define LOGO_W                78
#define LOGO_H                24

#define BACK_SLOT_PX          84     /* touch target */
#define BACK_CIRCLE_PX        64     /* what is drawn, centred in the slot */

#define POLL_MS               200

struct PlaybackPage {
    lv_obj_t   *info;          /* column of album / title / artist labels, rebuilt when the track changes */
    lv_obj_t   *progress;      /* bar and times; hidden when the source has no track length */
    lv_obj_t   *fill;
    lv_obj_t   *knob;
    lv_obj_t   *position;      /* elapsed time label */
    lv_obj_t   *remaining;     /* time left label */
    lv_timer_t *timer;
    int         infoMaxW;
    int         barW;
    std::string album, title, artist;
    int         shownPosition = -1;   /* seconds; -1: nothing shown yet */
    int         shownDuration = -1;
};

static void add_label(PlaybackPage *p, const std::string &text, const lv_font_t *font, lv_style_t *style)
{
    if (text.empty()) return;
    lv_obj_t *label = tile_adaptive_label(p->info, text, font, style, p->infoMaxW);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
}

/* m:ss, or h:mm:ss from an hour on */
static std::string format_time(int seconds)
{
    char text[16];
    if (seconds >= 3600)
        snprintf(text, sizeof text, "%d:%02d:%02d", seconds / 3600, seconds / 60 % 60, seconds % 60);
    else
        snprintf(text, sizeof text, "%d:%02d", seconds / 60, seconds % 60);
    return text;
}

static void show_progress(PlaybackPage *p, int position, int duration)
{
    if (position == p->shownPosition && duration == p->shownDuration) return;
    p->shownPosition = position;
    p->shownDuration = duration;

    if (duration <= 0) {
        lv_obj_set_hidden(p->progress, true);
        return;
    }
    lv_obj_set_hidden(p->progress, false);

    const int shown = std::min(position, duration);
    const int fillW = (int)((int64_t)p->barW * shown / duration);
    lv_obj_set_width(p->fill, fillW);
    lv_obj_set_x(p->knob, std::clamp(fillW - PROGRESS_KNOB_PX / 2, 0, p->barW - PROGRESS_KNOB_PX));
    lv_label_set_text(p->position, format_time(shown).c_str());
    lv_label_set_text(p->remaining, ("-" + format_time(duration - shown)).c_str());
}

static void refresh(lv_timer_t *timer)
{
    PlaybackPage *p = (PlaybackPage *)lv_timer_get_user_data(timer);

    const AudioSource *source = audio_source_active();
    const std::string album  = ui_is_landscape() ? std::string() : audio_source_text(source, "trackAlbum");
    const std::string title  = audio_source_text(source, "trackTitle");
    const std::string artist = audio_source_text(source, "trackArtist");
    if (album != p->album || title != p->title || artist != p->artist) {
        p->album = album;
        p->title = title;
        p->artist = artist;
        lv_obj_clean(p->info);
        add_label(p, album, &satoshi_medium_14, ui_theme_get_style_text_secondary());
        add_label(p, title, &satoshi_medium_24, ui_theme_get_style_text_primary());
        add_label(p, artist, &satoshi_medium_20, ui_theme_get_style_text_secondary());
    }

    const int duration = source ? (int)tile_read_number(source->path, "trackDuration").value : 0;
    const int position = source ? (int)tile_read_number(source->path, "trackPosition").value : 0;
    show_progress(p, position, duration);
}

/* The bar, the knob at the end of its fill, and the elapsed / remaining times below it. (x, y) is the top left of the bar;
 * the container reaches PROGRESS_KNOB_OVERHANG above it so the knob is not clipped. */
static void progress_create(PlaybackPage *p, lv_obj_t *screen, int x, int y, int w)
{
    const int timesH = lv_font_get_line_height(&satoshi_medium_14);
    p->barW = w;

    p->progress = tile_container(screen);
    lv_obj_set_size(p->progress, w, PROGRESS_KNOB_OVERHANG + PROGRESS_BAR_H + PROGRESS_TIMES_GAP + timesH);
    lv_obj_set_pos(p->progress, x, y - PROGRESS_KNOB_OVERHANG);
    lv_obj_set_hidden(p->progress, true);

    lv_obj_t *track = tile_container(p->progress);
    lv_obj_set_size(track, w, PROGRESS_BAR_H);
    lv_obj_set_pos(track, 0, PROGRESS_KNOB_OVERHANG);
    lv_obj_add_style(track, ui_theme_get_style_bg_icon_primary(), 0);
    lv_obj_set_style_bg_opa(track, PROGRESS_TRACK_OPA, 0);
    lv_obj_set_style_radius(track, PROGRESS_BAR_H / 2 + 1, 0);

    p->fill = tile_container(p->progress);
    lv_obj_set_size(p->fill, 0, PROGRESS_BAR_H);
    lv_obj_set_pos(p->fill, 0, PROGRESS_KNOB_OVERHANG);
    lv_obj_add_style(p->fill, ui_theme_get_style_bg_icon_primary(), 0);
    lv_obj_set_style_bg_opa(p->fill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(p->fill, PROGRESS_BAR_H / 2 + 1, 0);

    p->knob = tile_container(p->progress);
    lv_obj_set_size(p->knob, PROGRESS_KNOB_PX, PROGRESS_KNOB_PX);
    lv_obj_set_pos(p->knob, 0, 0);
    lv_obj_add_style(p->knob, ui_theme_get_style_bg_icon_primary(), 0);
    lv_obj_set_style_bg_opa(p->knob, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(p->knob, LV_RADIUS_CIRCLE, 0);

    p->position = lv_label_create(p->progress);
    lv_obj_add_style(p->position, ui_theme_get_style_text_secondary(), 0);
    lv_obj_set_style_text_font(p->position, &satoshi_medium_14, 0);
    lv_obj_align(p->position, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    p->remaining = lv_label_create(p->progress);
    lv_obj_add_style(p->remaining, ui_theme_get_style_text_secondary(), 0);
    lv_obj_set_style_text_font(p->remaining, &satoshi_medium_14, 0);
    lv_obj_align(p->remaining, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

/* The back button: a half-transparent black disc with the arrow, over the art. */
static void back_button_create(lv_obj_t *parent)
{
    lv_obj_t *slot = tile_container(parent);
    lv_obj_set_size(slot, BACK_SLOT_PX, BACK_SLOT_PX);
    lv_obj_align(slot, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_flag(slot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(slot, nav_back_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *disc = tile_container(slot);
    lv_obj_set_size(disc, BACK_CIRCLE_PX, BACK_CIRCLE_PX);
    lv_obj_center(disc);
    lv_obj_set_style_bg_color(disc, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(disc, LV_OPA_50, 0);
    lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);

    lv_obj_t *icon = UI_ICON_CREATE_DEFAULT(disc, UI_ICON_ARROW_BACK);
    if (icon) lv_obj_center(icon);
}

void playback_page_create(lv_obj_t *screen)
{
    const bool landscape = ui_is_landscape();
    const int displayW = lv_display_get_horizontal_resolution(nullptr);
    const int displayH = lv_display_get_vertical_resolution(nullptr);

    PlaybackPage *p = new PlaybackPage{};

    lv_obj_t *art = album_art_create(screen, landscape ? LANDSCAPE_ART_W : displayW,
                                     landscape ? displayH : PORTRAIT_ART_H, 0);
    lv_obj_align(art, LV_ALIGN_TOP_LEFT, 0, 0);
    back_button_create(screen);

    /* the column the text row and the progress block share */
    const int columnX = landscape ? LANDSCAPE_ART_W : 0;
    const int columnW = displayW - columnX;
    const int infoY = landscape ? 0 : PORTRAIT_INFO_Y;

    p->infoMaxW = columnW - INFO_X - INFO_PLAY_GAP - INFO_PLAY_RIGHT - PLAY_BUTTON_SIZE_PX;
    p->info = tile_container(screen);
    lv_obj_set_size(p->info, p->infoMaxW, INFO_H);
    lv_obj_set_pos(p->info, columnX + INFO_X, infoY);
    lv_obj_set_flex_flow(p->info, LV_FLEX_FLOW_COLUMN);
    /* portrait: the three lines fill the row to its bottom edge; landscape: the two lines are centred in it */
    lv_obj_set_flex_align(p->info, landscape ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *play = play_button_create(screen);
    lv_obj_set_pos(play, displayW - INFO_PLAY_RIGHT - PLAY_BUTTON_SIZE_PX, infoY + INFO_H - PLAY_BUTTON_SIZE_PX);

    const int progressH = PROGRESS_BAR_H + PROGRESS_TIMES_GAP + lv_font_get_line_height(&satoshi_medium_14);
    const int progressY = landscape ? displayH - LANDSCAPE_PROGRESS_BOTTOM - progressH
                                    : infoY + INFO_H + PROGRESS_GAP_ABOVE;
    progress_create(p, screen, columnX + PROGRESS_SIDE_PAD, progressY, columnW - 2 * PROGRESS_SIDE_PAD);

    if (!landscape) {
        lv_obj_t *logo = UI_ICON_CREATE_DEFAULT(screen, UI_ICON_LOGO);
        if (logo) lv_obj_set_pos(logo, (displayW - LOGO_W) / 2, displayH - PORTRAIT_LOGO_BOTTOM - LOGO_H);
    }

    p->timer = lv_timer_create(refresh, POLL_MS, p);
    lv_obj_add_event_cb(screen, [](lv_event_t *e) {
        PlaybackPage *p = (PlaybackPage *)lv_event_get_user_data(e);
        lv_timer_delete(p->timer);
        delete p;
    }, LV_EVENT_DELETE, p);

    lv_timer_ready(p->timer);
}
