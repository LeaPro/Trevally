
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "audio_output_callbacks.h"
#include "splayerapi/splayerapi-5.h"
// must come after splayerapi-5.h
#include <splayerapi/splayer_auth_api-1.h>

#include "splayerapi/splayer_controls_api-3.h"

#define Q(x) #x
#define QUOTE(x) Q(x)

#ifdef linux
#include <dlfcn.h>
#include <string.h>

const char* LIBNAME = "libsplayer-1.so";
#elif WINDOWS
const char* LIBNAME = "libsplayer-1.dll";
#else
#include <dlfcn.h>
const char* LIBNAME = "libsplayer-1.dylib";
#endif

volatile sig_atomic_t exit_requested = 0;

static void handle_signal(int sig) {
  printf("Requesting exit from player\n");
  exit_requested = 1;
}

static void* open_shared_lib() {
  void* dl_handle = dlopen(LIBNAME, RTLD_NOW);
  if (!dl_handle) {
    fprintf(stderr, "WHAT %s\n", dlerror());
    exit(1);
  }
  dlerror(); /* Clear any existing error */
  return dl_handle;
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
    printf("Usage: ./example_dlopen [CODE]\n");
  }

  const char* code = argc == 2 ? argv[1] : NULL;

  splayer_config_t config = {SPLAYER_SDK_VERSION};
  configure_splayer_api(&config);

  void* dl_handle = open_shared_lib();

  // Wire up API handles
  const struct splayer_api* api = dlsym(dl_handle, SPLAYER_API_SYMBOL);
  const struct splayer_controls_api* controls_api = api->get_controls_api();
  assert(controls_api);
  const struct splayer_auth_api* auth_api = api->get_auth_api();
  assert(auth_api);

  {
    char version[1024];
    api->get_current_version(NULL, version, sizeof(version));
    printf("Running sdk version '%s'\n", version);
  }

  splayer_t* splayer = NULL;
  int retval = api->create(config, &splayer);

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

  // The splayer main loop;
  while (!api->should_exit(splayer)) {
    if (exit_requested) {
      api->request_exit(splayer, SPLAYER_EXIT_NORMAL);
    }

    usleep(500000);
  }

  api->free(splayer);
  free(config.audio_api_callbacks);
  dlclose(dl_handle);

  return 0;
}
