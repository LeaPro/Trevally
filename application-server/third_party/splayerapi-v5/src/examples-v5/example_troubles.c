#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "audio_output_callbacks.h"
#include "splayerapi/splayerapi-5.h"
// Must come after splayerapi-5.h
#include "splayerapi/splayer_controls_api-3.h"
#include "splayerapi/splayer_troubles_api-2.h"

#define Q(x) #x
#define QUOTE(x) Q(x)

volatile sig_atomic_t exit_requested = 0;

static void handle_signal(int sig) {
  printf("Requesting exit from player\n");
  exit_requested = 1;
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

static void pretty_print_troubles(splayer_trouble_t* troubles_it, size_t troubles_count) {
  printf("%-40s\t\t\t%s\t%s\t%-10s\t%s\t\n", "NAME", "PERFORMED", "PASSED", "SEVERITY", "COMMENT");
  for (size_t i = 0; i < troubles_count; ++i) {

    printf("%-40s\t\t\t%s\t\t%s\t", troubles_it->trouble_name,
           troubles_it->trouble_test_performed == 0 ? "false" : "true",
           troubles_it->trouble_test_value == 0 ? "false" : "true");
    switch (troubles_it->trouble_severity) {
    case SPLAYER_TROUBLE_INFO:
      printf("%-10s\t", "Info");
      break;
    case SPLAYER_TROUBLE_WARNING:
      printf("%-10s\t", "Warning");
      break;
    case SPLAYER_TROUBLE_CRITICAL:
      printf("%-10s\t", "Critical");
      break;
    }
    char* buf = strdup(troubles_it->trouble_comment);
    char* pch = strtok(buf, "\n");
    if (pch != NULL)
      printf("%s\n", pch);

    while (pch != NULL) {
      pch = strtok(NULL, "\n");
      if (pch != NULL)
        printf("%-80s\t%s\n", "", pch);
    };
    ++troubles_it;
    free(buf);
  }
  printf("\n");
}

int main() {
  signal(SIGINT, handle_signal);
  splayer_config_t config = {SPLAYER_SDK_VERSION};
  configure_splayer_api(&config);

  // To link statically, grab the address of the global struct SPLAYER_API
  const struct splayer_api* api = &SPLAYER_API;

  {
    char version[1024];
    api->get_current_version(NULL, version, sizeof(version));
    printf("Running sdk version '%s'\n", version);
  }

  // Get handles to other useful APIs
  const struct splayer_controls_api* controls_api = api->get_controls_api();
  assert(controls_api);
  const struct splayer_troubles_api* troubles_api = api->get_troubles_api();
  assert(troubles_api);

  splayer_t* splayer = NULL;
  int retval = api->create(config, &splayer);

  if (!splayer) {
    printf("Unable to allocate splayer. Error = %d\n", retval);
    return 1;
  }

  if (!controls_api->is_playing(splayer)) {
    printf("is_playing() = false\n\n\n");
  }

  // Get troubles array and print its contents
  splayer_troubles_array_t* troubles = NULL;
  if (troubles_api->get_troubles(splayer, &troubles) == 0) {
    pretty_print_troubles(troubles->splayer_trouble_array, troubles->size);
    troubles_api->free_troubles(troubles);
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

  return 0;
}
