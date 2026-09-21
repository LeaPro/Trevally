#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "audio_output_callbacks.h"
#include "splayerapi/splayerapi-5.h"
// Must come after splayerapi-5.h
#include <splayerapi/splayer_auth_api-1.h>

#include "splayerapi/splayer_controls_api-3.h"
#include "splayerapi/splayer_metadata_api-4.h"

#define Q(x) #x
#define QUOTE(x) Q(x)

volatile sig_atomic_t exit_requested = 0;

static void handle_signal(int sig) {
  printf("Requesting exit from player\n");
  exit_requested = 1;
}

static void log_handler(const char* msg, void* userdata) {
  int* counter = (int*)userdata;
  (*counter)++;
  printf("Internal SDK message (%i): %s\n", *counter, msg);
}

static void print_metadata(splayer_track_metadata_t* track_metadata) {
  // Print album name
  printf("Track Title: %s \n", track_metadata->track_title);
  printf("Album name: %s \n", track_metadata->album_name);
  printf("ISRC: %s \n", track_metadata->isrc);
  // print artists
  if (track_metadata->num_artists > 0) {
    printf("Artists: ");
    int i;
    for (i = 0; i < track_metadata->num_artists; i++) {
      printf("%s, ", track_metadata->artists[i]);
    }
    printf("\n");
  }
  // Track positon / track duration
  printf("Time played: %ds / %ds \n", track_metadata->current_track_position_s,
         (int)(track_metadata->duration_ms * (1.0 / 1000)));

  if (track_metadata->album_image_uri) {
    printf("Album image uri: %s\n", track_metadata->album_image_uri);
  }
  printf("\n");
  printf("\n");
}

static void write_album_cover_to_file(const char* file_name, splayer_album_image_t* album_image) {
  FILE* file_ptr = fopen(file_name, "wb");
  fwrite(album_image->data, album_image->data_size, 1, file_ptr);
  fclose(file_ptr);
}

static void configure_splayer_api(splayer_config_t* config) {
  config->diskcache_dir = "cache";
  config->diskcache_max_mb = 5120;
  config->diskcache_remain_mb = 320;

  config->output_sample_rate = 44100; // Or 48000 depending on your ALSA implementation.
  config->output_sample_channels = 2;

  config->vendor_device_name = "Splayer-1"; // Please change this to the name of the device running splayer.
  config->app_version = "1.0";

  // Create the audio api callbacks. The specific audio implementation is decided in the CMakeLists.txt
  config->audio_api_callbacks = audio_api_allocate();
}

int main(int argc, const char** argv) {
  signal(SIGINT, handle_signal);
  if (argc > 2 || (argc == 2 && !strcmp(argv[1], "-h"))) {
    printf("Usage: ./example_metadata [CODE]\n");
  }

  const char* code = argc == 2 ? argv[1] : NULL;

  splayer_config_t config = {SPLAYER_SDK_VERSION};

  // Fill config struct
  configure_splayer_api(&config);

  // To link statically, just grab the address of the global struct SPLAYER_API
  const struct splayer_api* api = &SPLAYER_API;

  {
    char version[1024];
    api->get_current_version(NULL, version, sizeof(version));
    printf("Running sdk version '%s'\n", version);
  }

  // Get other useful APIs
  const struct splayer_controls_api* controls_api = api->get_controls_api();
  assert(controls_api);
  const struct splayer_auth_api* auth_api = api->get_auth_api();
  assert(auth_api);
  const struct splayer_metadata_api* metadata_api = api->get_metadata_api();
  assert(metadata_api);

  splayer_t* splayer = NULL;
  int message_counter = 0;
  int retval = api->create_with_log_handler(config, &splayer, log_handler, (void*)&message_counter);

  if (!splayer) {
    printf("Unable to allocate splayer. Error = %d\n", retval);
    return 1;
  }

  if (auth_api->get_auth_status(splayer) != SPLAYER_AUTH_STATUS_PAIRED) {
    if (!code || !strlen(code)) {
      printf("Device is not paired and no code provided.\n");
      return 1;
    }
    splayer_pair_result_t* pair_result = auth_api->pair_with_code_sync(splayer, code);
    if (pair_result) {
      if (!pair_result->success) {
        printf("Pairing failed: %s\n", pair_result->message);
        auth_api->free_pairing_result(pair_result);
        return 1;
      }
      printf("Paired successfully! Device ID: %s\n", pair_result->device_id);
      auth_api->free_pairing_result(pair_result);
    }
  }

  if (!controls_api->is_playing(splayer)) {
    printf("is_playing() = false\n\n\n");
  }

  splayer_metadata_config_t metadata_config = {false}; // Disable album art prefetching

  metadata_api->update_config(splayer, metadata_config);
  splayer_track_metadata_t* track_metadata = metadata_api->get_current_track_metadata(splayer);
  int last_track_pos = metadata_api->get_current_track_position(splayer);

  bool should_print_metadata = true;
  bool should_save_album_art = true;
  // The splayer main loop;
  while (!api->should_exit(splayer)) {

    if (track_metadata && should_print_metadata) {
      print_metadata(track_metadata);
      if (should_save_album_art) {
        splayer_album_image_t* image = metadata_api->get_current_track_album_image(splayer);
        if (image) {
          char buf[128];
          snprintf(buf, 128, "track_album_cover.jfif");
          write_album_cover_to_file(buf, image);
          metadata_api->free_album_image(&image);
        }
      }
    }

    const int current_track_position = metadata_api->get_current_track_position(splayer);
    if (current_track_position >= last_track_pos) {
      last_track_pos = current_track_position;
      should_print_metadata = false;
    } else {
      last_track_pos = current_track_position;
      metadata_api->free_track_metadata(track_metadata);
      track_metadata = NULL;

      track_metadata = metadata_api->get_current_track_metadata(splayer);
      should_print_metadata = true;
    }
    if (exit_requested) {
      api->request_exit(splayer, SPLAYER_EXIT_NORMAL);
    }

    usleep(500000);
  }

  if (track_metadata) {
    metadata_api->free_track_metadata(track_metadata);
  }

  api->free(splayer);
  free(config.audio_api_callbacks);

  return 0;
}
