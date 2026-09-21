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

#define STR_HELPER(s) #s
#define STR(s) STR_HELPER(s)
#define PI 3.14159265359
#define PLAYBACK_MOCK_BLOCK_SAMPLE_SIZE 2048
#define MEGABYTE (1024 * 1024)
#define ALLOCATE_MEMORY (MEGABYTE * 100) // 100mb ram should be enough :)

#define TEST(x)                                                                                                        \
  if (!(x)) {                                                                                                          \
    fprintf(stderr, "Failed test at: %s:%d (" #x ") != %d errno: %s\n", __FILE__, __LINE__, x, strerror(errno));       \
    exit(1);                                                                                                           \
  }

#ifdef DEF_SPLAYER_VERSION
SPLAYER_PUBLIC const char* SPLAYER_VERSION = STR(DEF_SPLAYER_VERSION);
SPLAYER_PUBLIC const char* SPLAYER_BUILD = "";
SPLAYER_PUBLIC const char* SPLAYER_REV = "";
SPLAYER_PUBLIC const char* SPLAYER_PLATFORM_NAME = "";
#endif

struct splayer {
  splayer_config_t config;
  int quit;
  pthread_t audio_thread;
  pthread_t disk_thread;
  int audio_thread_running;
  int disk_thread_running;

  // Note
  int track;

  // Playpos
  int offset;

  // Phase
  int track_pos;
  double phase;
  int16_t samples[PLAYBACK_MOCK_BLOCK_SAMPLE_SIZE];
  int samples_avail;
  int samples_pos;

  char* memory;
};

static err_t validate_config(const splayer_config_t* config) {

  if (!config->vendor_device_name || strlen(config->vendor_device_name) < 1) {
    printf("No vendor product name.\n");
    return 4;
  }
  if (!config->app_version || strlen(config->app_version) < 1) {
    printf("No platform version.\n");
    return 6;
  }
  regex_t regex;
  if (regcomp(&regex, "[1-9][0-9]*\\.[0-9]+", REG_EXTENDED)) {
    printf("Could not compile regex\n");
    return 7;
  }
  if (regexec(&regex, config->app_version, 0, NULL, 0) == REG_NOMATCH) {
    regfree(&regex);
    printf("Incorrect format of app version \"%s\" It should be number.number, see header file for more info.\n",
           config->app_version);
    return 7;
  }
  regfree(&regex);

  if (!config->diskcache_dir || strlen(config->diskcache_dir) < 1) {
    printf("No diskcache directory. Default ./cache will be used.\n");
  }
  if (config->diskcache_max_mb <= 0) {
    printf("No diskcache max size. Default 5 120 MB will be used.\n");
  }
  if (config->diskcache_remain_mb <= 0) {
    printf("No diskcache max size. Default 300 MB will be used.\n");
  }

  return 0;
}

static void play_beep(struct splayer* sp) {
  const int track_len = 1;
  const int channels = 2;
  const int sample_rate = 44100;
  /* .channels: Number of channels (1 = mono, 2 = stereo) */
  /* .sample_rate: Sample rate in Hz (such as 22050, 44100 or 48000) */
  const int track_len_samples = (int)(sample_rate * channels * track_len);

  // When buffer is empty, generate some new sound.
  if (sp->samples_avail == 0) {
    for (int i = 0; i < PLAYBACK_MOCK_BLOCK_SAMPLE_SIZE && sp->track_pos < track_len_samples; i += channels) {
      int note = sp->track % 24;
      double freq = (440.0 / 1) * pow(1.059463, note) / sample_rate;
      double tone = sin(sp->phase);
      sp->phase += freq * (2 * PI);
      while (sp->phase >= (2 * PI))
        sp->phase -= (2 * PI);
      for (int j = 0; j < channels; j++) {
        sp->samples[i + j] = (int16_t)(((1 << 15) - 1) * tone);
        sp->samples_avail++;
        sp->track_pos++;
      }
    }
    sp->samples_pos = 0;
  }

  // Play back whats avail.
  uint32_t samples_buffered;
  int played = (int)sp->config.audio_api_callbacks->audio_data(
    sp->config.audio_api_callbacks, sp->samples + sp->samples_pos, sp->samples_avail, &samples_buffered);
  TEST((int)played <= (int)sp->samples_avail);
  sp->offset += played;
  sp->samples_avail -= played;
  sp->samples_pos += played;
  if (played)
    printf("Track: %d Offset: %d Len: %d Played: %d samples_avail: %d -> %d\n", sp->track, sp->offset,
           track_len_samples, played, sp->samples_avail + played, sp->samples_avail);

  if (sp->offset == track_len_samples) {
    sp->track++;
    sp->offset = 0;
    sp->track_pos = 0;

    // Every 10 tracks, reload audio pipeline.
    if (sp->track % 10 == 0) {
      sp->config.audio_api_callbacks->audio_shutdown(sp->config.audio_api_callbacks);
      sp->config.audio_api_callbacks->audio_init(sp->config.audio_api_callbacks, 2, 44600);
    }
  }
}

static double get_time() {
  struct timespec t;
  int rc = clock_gettime(CLOCK_MONOTONIC, &t);
  if (rc != 0) {
    return 0;
  }
  return (double)t.tv_sec + (double)t.tv_nsec / (double)1000000000;
}

static void* DiskThreadFun(void* ctx) {
  struct splayer* sp = (struct splayer*)ctx;
  double before, after, mid;

  char filename[1024];
  snprintf(filename, 1024, "%s/xxx.tmp", sp->config.diskcache_dir);
  unlink(filename); // If left from old test..
  int test_run = 0;
  while (sp->quit == 0) {
    printf("Disk test run: %d\n", test_run);
    switch (test_run) {
    case 0: {
      printf("Starting write test of file %s\n", filename);
      int outputfile = open(filename, O_CREAT | O_WRONLY, 0777);
      int i;
      TEST(outputfile >= 0);

      before = get_time();

      for (i = 0; i < 10; i++)
        if (write(outputfile, sp->memory, ALLOCATE_MEMORY) != ALLOCATE_MEMORY)
          printf("Fail to write\n");

      mid = get_time();
      fsync(outputfile);
      after = get_time();

      double timetowrite = after - before;

      if (timetowrite > 0)
        printf("%f megabytes written in %f seconds  = %f megabytes per seconds (%f sync)\n",
               (double)ALLOCATE_MEMORY * 10 / MEGABYTE, timetowrite,
               (double)ALLOCATE_MEMORY * 10 / MEGABYTE / timetowrite, after - mid);

      close(outputfile);
      test_run++;
      break;
    }
    case 1: {
      printf("Starting read test of file %s\n", filename);
      int inputfile = open(filename, O_RDONLY);
      int i;
      TEST(inputfile >= 0);

      before = get_time();

      for (i = 0; i < 10; i++) {
        if (read(inputfile, sp->memory, ALLOCATE_MEMORY) != ALLOCATE_MEMORY)
          printf("Fail to read\n");
      }

      after = get_time();

      double timetoread = after - before;

      if (timetoread > 0)
        printf("%f megabytes read in %f seconds  = %f megabytes per seconds\n", (double)ALLOCATE_MEMORY * 10 / MEGABYTE,
               timetoread, (double)ALLOCATE_MEMORY * 10 / MEGABYTE / timetoread);

      close(inputfile);
      test_run++;
      break;
    }
    case 3:
      TEST(unlink(filename) == 0);
      test_run++;
      break;
    case 10:
      test_run = 0;
      break;
    default:
      usleep(1000000);
      test_run++;
      break;
    }
  }
  sp->disk_thread_running = 0;
  return NULL;
}

static void* AudioThreadFun(void* ctx) {
  printf("AudioThreadFun Enter\n");
  struct splayer* sp = (struct splayer*)ctx;
  sp->config.audio_api_callbacks->audio_init(sp->config.audio_api_callbacks, 2, 44600);
  printf("AudioThreadFun Enter Loop\n");
  while (sp->quit == 0) {
    play_beep(sp);
    usleep(1000);
  }
  printf("AudioThreadFun Leave Loop\n");
  sp->config.audio_api_callbacks->audio_shutdown(sp->config.audio_api_callbacks);
  printf("AudioThreadFun Leave\n");
  sp->audio_thread_running = 0;
  return NULL;
}

static err_t splayer_create(const splayer_config_t config, splayer_t** splayer_ret) {
  printf("splayer_create\n");
  err_t err = validate_config(&config);
  if (err != 0) {
    printf("Config is invalid. Can't create splayer context.\n");
    return err;
  }

  int res = mkdir(config.diskcache_dir, 0777);
  printf("res: %d\n", res);
  TEST(res == 0 || errno == EEXIST);

  struct splayer* sp = (struct splayer*)malloc(sizeof(struct splayer));
  memset(sp, 0, sizeof(struct splayer));
  sp->config = config;
  sp->memory = (char*)malloc(ALLOCATE_MEMORY);
  for (int i = 0; i < ALLOCATE_MEMORY; i++)
    sp->memory[i] = (char)i;
  sp->audio_thread_running = 1;
  sp->disk_thread_running = 1;
  pthread_create(&sp->audio_thread, NULL, AudioThreadFun, (void*)sp);
  pthread_create(&sp->disk_thread, NULL, DiskThreadFun, (void*)sp);
  *splayer_ret = sp;
  return 0;
}

static void splayer_free(splayer_t* splayer) {
  printf("splayer_free\n");
  free(splayer->memory);
  pthread_join(splayer->audio_thread, NULL);
  pthread_join(splayer->disk_thread, NULL);
  free(splayer);
}

static void splayer_get_current_version(splayer_t* splayer, char* version, int version_size) {
  strcpy(version, SPLAYER_VERSION);
}

static int splayer_loop_iteration(splayer_t* splayer) {
  // printf("LOOP quit: %d\n", splayer->quit);
  usleep(10000);
  return splayer->audio_thread_running || splayer->disk_thread_running;
}

static int mock_is_playing(splayer_t* splayer) {
  return 1;
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

static struct splayer_controls_api mock_control_api = {
  3, &mock_is_playing, &mock_pause, &mock_play, &mock_skip_tracks, &mock_get_volume, &mock_set_volume, &mock_is_paused};

static err_t mock_update_config(splayer_t* splayer, splayer_metadata_config_t config) {
  return 0;
}

static const char* mock_artists[] = {"THE BEEPER", "THE NOICE", NULL};
static splayer_track_metadata_t mock_current_track = {"BEEEEP", "BEEPING SOUNDS", mock_artists, "1234567", 2, 1000,
                                                      500,      "SOME URI"};
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

static void mock_free_album_image(splayer_album_image_t** image) {}

static void mock_free_track_metadata(splayer_track_metadata_t* track_metadata) {}

static struct splayer_metadata_api mock_metadata_api = {1,
                                                        &mock_update_config,
                                                        &mock_get_current_track_metadata,
                                                        &mock_get_current_track_position,
                                                        &mock_get_current_track_album_image,
                                                        &mock_free_track_metadata,
                                                        &mock_free_album_image};

static splayer_trouble_t mock_troubles[1] = {
  {"ERROR_TROUBLE_IS_MOCK", "This player is using a mock", 1, 0, SPLAYER_TROUBLE_WARNING}};
static splayer_troubles_array_t mock_troubles_array = {mock_troubles, 1};
static err_t mock_get_troubles(splayer_t* splayer, splayer_troubles_array_t** splayer_troubles) {
  *splayer_troubles = &mock_troubles_array;
  return 0;
}

static void mock_free_troubles(splayer_troubles_array_t* splayer_troubles) {}

static struct splayer_troubles_api mock_troubles_api = {1, &mock_get_troubles, &mock_free_troubles};

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
