/**
 * @file album_art.cpp
 * @brief See album_art.h.
 */

#include "album_art.h"
#include "audio_source.h"
#include "tile.h"
#include "../ui_design_tokens.h"

#include <cstdio>
#include <memory>
#include <object.h>
#include <string>

/* The device's root object (defined in main.cpp). */
extern std::shared_ptr<RootObject> root;

#define ALBUM_ART_POLL_MS 200

/* The album art file path as published by the active audio source, or "" when there is none. */
static std::string album_art_path()
{
    return audio_source_text(audio_source_active(), "trackAlbumArt");
}

struct AlbumArt {
    lv_obj_t   *image;
    lv_timer_t *timer;
    std::string shown;   /* path currently set on the image */
};

/* Files get a new name with every image, so a path change is the only trigger needed. */
static void refresh(lv_timer_t *timer)
{
    AlbumArt *art = (AlbumArt *)lv_timer_get_user_data(timer);
    const std::string path = album_art_path();
    if (path == art->shown) return;
    art->shown = path;   /* the old image ages out of LVGL's image cache */

    if (path.empty()) {
        fprintf(stderr, "album_art: no image\n");
        lv_obj_add_flag(art->image, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    const std::string src = "A:" + path;
    lv_image_header_t header;
    const lv_result_t res = lv_image_decoder_get_info(src.c_str(), &header);
    fprintf(stderr, "album_art: %s -> %s (%dx%d)\n", path.c_str(),
            res == LV_RESULT_OK ? "decodable" : "NO DECODER / unreadable",
            res == LV_RESULT_OK ? (int)header.w : 0, res == LV_RESULT_OK ? (int)header.h : 0);
    lv_image_set_src(art->image, src.c_str());
    lv_obj_clear_flag(art->image, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *album_art_create(lv_obj_t *parent, int w, int h, int radius)
{
    lv_obj_t *box = tile_container(parent);
    lv_obj_set_size(box, w, h);
    lv_obj_add_style(box, ui_theme_get_style_bg_tertiary(), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(box, radius, 0);
    lv_obj_set_style_clip_corner(box, true, 0);

    lv_obj_t *image = lv_image_create(box);
    lv_obj_set_size(image, w, h);
    lv_obj_align(image, LV_ALIGN_CENTER, 0, 0);
    lv_image_set_inner_align(image, LV_IMAGE_ALIGN_COVER);
    lv_obj_add_flag(image, LV_OBJ_FLAG_HIDDEN);

    AlbumArt *art = new AlbumArt{image, nullptr, std::string()};
    art->timer = lv_timer_create(refresh, ALBUM_ART_POLL_MS, art);
    lv_obj_add_event_cb(box, [](lv_event_t *e) {
        AlbumArt *art = (AlbumArt *)lv_event_get_user_data(e);
        lv_timer_delete(art->timer);
        delete art;
    }, LV_EVENT_DELETE, art);

    lv_timer_ready(art->timer);   /* show what is there right away */
    return box;
}
