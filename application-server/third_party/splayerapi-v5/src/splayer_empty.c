/*************************************************************************
 *
 * SOUNDTRACK TECHNOLOGIES SWEDEN AB - CONFIDENTIAL
 * __________________
 *
 *  [2025] - SOUNDTRACK TECHNOLOGIES SWEDEN AB
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of SOUNDTRACK TECHNOLOGIES SWEDEN AB and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to SOUNDTRACK TECHNOLOGIES SWEDEN AB
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret and copyright law.
 * Dissemination of this information or reproduction, sub-licensing or
 * modification of this material is strictly forbidden unless prior written
 * permission is obtained from SOUNDTRACK TECHNOLOGIES SWEDEN AB.
 * Violations of these rights will result in legal actions.
 *
 * https://www.soundtrackyourbrand.com/legal/sdk-terms-of-use
 */

// (C) Soundtrack Technologies 2025

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define SPLAYER_EXPORTS
#include "splayerapi/splayerapi-5.h"
// must come after splayerapi-5.h
#include "splayerapi/splayer_audio_api-3.h"
#include "splayerapi/splayer_controls_api-3.h"
#include "splayerapi/splayer_metadata_api-4.h"
#include "splayerapi/splayer_troubles_api-2.h"

SPLAYER_PUBLIC const char* SPLAYER_VERSION = "empty";
SPLAYER_PUBLIC const char* SPLAYER_BUILD = "";
SPLAYER_PUBLIC const char* SPLAYER_REV = "";
SPLAYER_PUBLIC const char* SPLAYER_PLATFORM_NAME = "";

struct splayer {
  int quit;
};

static err_t splayer_create(const splayer_config_t config, splayer_t** splayer_ret) {
  struct splayer* sp = (struct splayer*)malloc(sizeof(struct splayer));
  memset(sp, 0, sizeof(struct splayer));
  *splayer_ret = sp;
  return 0;
}

static void splayer_free(splayer_t* splayer) {
  free(splayer);
}

static void splayer_get_current_version(splayer_t* splayer, char* version, int version_size) {
  strcpy(version, SPLAYER_VERSION);
}

static int splayer_loop_iteration(splayer_t* splayer) {
  usleep(10000);
  return !splayer->quit;
}

static int mock_is_playing(splayer_t* splayer) {
  return 0;
}

static err_t mock_pause(splayer_t* splayer) {
  return 0;
}

static err_t mock_play(splayer_t* splayer) {
  return 0;
}

static err_t mock_skip_tracks(splayer_t* splayer, int count) {
  return 0;
}

static err_t mock_get_volume(splayer_t* splayer) {
  return 100;
}

static err_t mock_set_volume(splayer_t* splayer, int volume) {
  return 0;
}

static int mock_is_paused(splayer_t* splayer) {
  return 0;
}

struct splayer_controls_api mock_control_api = {
  3, &mock_is_playing, &mock_pause, &mock_play, &mock_skip_tracks, &mock_get_volume, &mock_set_volume, &mock_is_paused};

static err_t mock_update_config(splayer_t* splayer, splayer_metadata_config_t config) {
  return 0;
}

const char* mock_artists[] = {"EMPTY"};
splayer_track_metadata_t mock_current_track = {"EMPTY", "EMPTY", mock_artists, "1234567", 1, 1000, 500, "SOME URI"};
static splayer_track_metadata_t* mock_get_current_track_metadata(splayer_t* splayer) {
  return &mock_current_track;
}

static splayer_album_image_t mock_album = {NULL, 0, 0, 0};
static splayer_album_image_t* mock_get_current_track_album_image(splayer_t* splayer) {
  return &mock_album;
}

static int mock_get_current_track_position(splayer_t* splayer) {
  return 500;
}

static void mock_free_track_metadata(splayer_track_metadata_t* track_metadata) {}
static void mock_free_album_image(splayer_album_image_t** album_image) {}

struct splayer_metadata_api mock_metadata_api = {1,
                                                 &mock_update_config,
                                                 &mock_get_current_track_metadata,
                                                 &mock_get_current_track_position,
                                                 &mock_get_current_track_album_image,
                                                 &mock_free_track_metadata,
                                                 &mock_free_album_image};

splayer_trouble_t mock_troubles[1] = {{"EMPTY", "NO LIBRARY LOADED", 1, 0, SPLAYER_TROUBLE_WARNING}};
splayer_troubles_array_t mock_troubles_array = {mock_troubles, 1};
static err_t mock_get_troubles(splayer_t* splayer, splayer_troubles_array_t** splayer_troubles) {
  *splayer_troubles = &mock_troubles_array;
  return 0;
}

static void mock_free_troubles(splayer_troubles_array_t* splayer_troubles) {}

struct splayer_troubles_api mock_troubles_api = {1, &mock_get_troubles, &mock_free_troubles};

static void splayer_request_exit(splayer_t* splayer, splayer_exit_t exit_method) {
  splayer->quit = 1;
}

static const struct splayer_troubles_api* splayer_get_troubles_api() {
  return &mock_troubles_api;
}

static const struct splayer_controls_api* splayer_get_controls_api() {
  return &mock_control_api;
}

static const struct splayer_metadata_api* splayer_get_metadata_api() {
  return &mock_metadata_api;
}

SPLAYER_PUBLIC const struct splayer_api SPLAYER_API_87GHV3 = {{5, 0},
                                                              splayer_create,
                                                              splayer_free,
                                                              splayer_get_current_version,
                                                              splayer_loop_iteration,
                                                              splayer_request_exit,
                                                              splayer_get_troubles_api,
                                                              splayer_get_controls_api,
                                                              splayer_get_metadata_api};
