#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "splayerapi/splayer_audio_api-3.h"
struct dummy_audio_impl {
  struct splayer_audio_api api;
};
static void dummy_flush(struct splayer_audio_api* ctx) {}

static void dummy_init(struct splayer_audio_api* ctx, const int output_sample_channels, const int output_sample_rate) {}

static void dummy_pause(struct splayer_audio_api* ctx, int pause) {}

static size_t dummy_play_audio(struct splayer_audio_api* ctx, const int16_t* samples, size_t sample_count,
                               uint32_t* samples_buffered) {
  return 0; // sample_count * sizeof(int16_t);
}

static void dummy_set_volume(struct splayer_audio_api* ctx, const int volume) {}

static void dummy_shutdown(struct splayer_audio_api* ctx) {}

struct splayer_audio_api* audio_api_allocate() {
  struct dummy_audio_impl* api = malloc(sizeof(struct dummy_audio_impl));
  memset(api, 0, sizeof(struct dummy_audio_impl));
  splayer_audio_api_t e = {dummy_init, dummy_shutdown, dummy_flush, dummy_play_audio, dummy_pause, dummy_set_volume};
  api->api = e;

  return &api->api;
}
