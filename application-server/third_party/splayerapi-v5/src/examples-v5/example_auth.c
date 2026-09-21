#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "audio_output_callbacks.h"
#include "splayerapi/splayerapi-5.h"
// must come after splayerapi-5.h
#include "splayerapi/splayer_auth_api-1.h"
#include "splayerapi/splayer_controls_api-3.h"
#include "splayerapi/splayer_options_api-1.h"

#define Q(x) #x
#define QUOTE(x) Q(x)

static char token_storage_dir[1024];
static const char* token_filename = "example_auth_token.dat";

void load_token(char* buf, size_t buflen) {
  char path[2048];
  if (strlen(token_storage_dir) > 0) {
    sprintf(path, "%s/%s", token_storage_dir, token_filename);
  } else {
    strcpy(path, token_filename);
  }
  FILE* file = fopen(path, "r");
  if (!file) {
    printf("Token file could not be open\n");
    return;
  }
  if (!fread(buf, 1, buflen, file)) {
    printf("Token file is empty\n");
  }
  fclose(file);
}

void save_token(const char* token) {
  char path[2048];
  if (strlen(token_storage_dir) > 0) {
    sprintf(path, "%s/%s", token_storage_dir, token_filename);
  } else {
    strcpy(path, token_filename);
  }
  FILE* file = fopen(path, "w");
  if (!file) {
    printf("Token file could not be open\n");
    return;
  }
  if (!fwrite(token, 1, strlen(token) + 1, file)) {
    printf("Failed to write the token file\n");
  }
  fclose(file);
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

  // Override token load/save logic
  // use NULL to use defaults
  config->load_token_callback = load_token;
  config->save_token_callback = save_token;
}

void print_help() {
  printf("The list of current commands:\n");
  printf("\thelp - show this help message\n");
  printf("\texit - exit the application\n");
  printf("\tispaired - check if player is paired\n");
  printf("\tpair - initiate interactive player pairing\n");
  printf("\tpair {code} - pair player with an existing connect code\n");
  printf("\tunpair - unpair player\n");
  printf("\tpause - pause playback\n");
  printf("\tplay - resume playback\n");
}

size_t get_command(char** buf) {
  printf("Command: ");
  char* tmp = NULL;
  *buf = NULL;
  size_t sz;
  ssize_t ssz = getline(&tmp, &sz, stdin);
  // Some compilers require that the result of getline is used.
  if (ssz > 0) {
    // get rid of newlines
    for (size_t i = 0; i < strlen(tmp); i++) {
      if (tmp[i] == '\n' || tmp[i] == '\r') {
        tmp[i] = 0;
      }
    }
    *buf = strdup(tmp);
  }
  free(tmp);
  return *buf ? strlen(*buf) : 0;
}

// Tokenize a string
// output needs to be freed by the caller
void tokenize(char* input, char*** output, size_t* count) {
  if (!input || strlen(input) == 0) {
    return;
  }

  char* str = strdup(input);

  // get token count first
  *count = 1;
  char* token = strchr(str, ' ');
  while (token && ++(*count)) {
    token = strchr(++token, ' ');
  }

  *output = malloc(sizeof(char*) * *count);

  // put these tokens in a heap allocated char array
  token = strtok(str, " ");
  size_t token_count = *count;
  for (size_t i = 0; i < token_count; i++) {
    if (token && strlen(token) > 0) {
      (*output)[i] = strdup(token);
    } else {
      --(*count);
    }
    token = strtok(NULL, " ");
  }
  free(str);
}

#define CODE_LEN 6

char* get_code_interactively() {
  // This is an example of how to open a browser to select a sound zone to pair with.
  // For showcase purposes, we will start a local web server that will handle the callback and supply the code.
  // In a real-world scenario, you should NOT use this webserver.

  // clang-format off
  const char *node_command = ""
    "node -e '"
      "require(\"child_process\").exec(`"
          "${{"
              "darwin:\"open\","
              "win32:\"start\","
              "linux:\"xdg-open\""
          "}[process.platform]} "
          "\"https://business.soundtrackyourbrand.com/connect/generic?redirect=http://localhost:1234\"`);"
      "require(\"http\").createServer((req, res) => {"
          "if (req.url.includes(\"?\")) {"
              "res.end(\"success\"); "
              "console.log(new URLSearchParams(req.url.split(\"?\")[1]).get(\"code\")), "
              "process.exit(0)"
          "} else { "
              "res.end(\"error\")"
          "}"
      "}).listen(1234);'"
    "";
  // clang-format on

  FILE* fp = popen(node_command, "r");
  if (!fp) {
    printf("Failed to execute node command\n");
    return NULL;
  }
  char* code = malloc(CODE_LEN + 1);
  code[0] = '\0';
  if (!fgets(code, CODE_LEN + 1, fp)) {
    printf("Failed to read from node command\n");
  }
  pclose(fp);
  printf("Got code: %s\n", code);
  return code;
}

int main() {
  splayer_config_t config = {SPLAYER_SDK_VERSION};
  configure_splayer_api(&config);
  strcpy(token_storage_dir, config.diskcache_dir);

  // To link statically, grab the address of the global struct SPLAYER_API
  const struct splayer_api* api = &SPLAYER_API;

  {
    char version[1024];
    api->get_current_version(NULL, version, sizeof(version));
    printf("Running sdk version '%s'\n", version);
  }

  // Get handles to other useful APIs
  const struct splayer_controls_api* controls_api = api->get_controls_api();
  const struct splayer_auth_api* auth_api = api->get_auth_api();
  const struct splayer_options_api* options_api = api->get_options_api();
  splayer_options_t* options = options_api->create();
  options_api->set_initially_paused(options, false);

  splayer_t* splayer = NULL;
  int retval = api->create_with_options(config, &splayer, options);
  options_api->free(options);

  if (!splayer) {
    printf("Unable to allocate splayer. Error = %d\n", retval);
    return 1;
  }

  if (controls_api && !controls_api->is_playing(splayer)) {
    printf("is_playing() = false\n\n\n");
  }

  // The splayer main loop;
  bool exit_requested = false;
  while (!api->should_exit(splayer)) {
    char* buf;
    if (exit_requested) {
      usleep(100);
      continue;
    }

    size_t sz = get_command(&buf);
    if (sz == 0) {
      continue;
    }
    size_t tokens;
    char** output;
    tokenize(buf, &output, &tokens);
    free(buf);

    if (tokens == 0) {
      continue;
    }

    // execute the command
    if (strcmp(output[0], "help") == 0) {
      print_help();
    } else if (strcmp(output[0], "exit") == 0) {
      api->request_exit(splayer, SPLAYER_EXIT_NORMAL);
      exit_requested = true;
    } else if (strcmp(output[0], "ispaired") == 0) {
      if (auth_api->get_auth_status(splayer) == SPLAYER_AUTH_STATUS_PAIRED) {
        printf("Device is paired\n");
      } else {
        printf("Device is not paired\n");
      }
    } else if (strcmp(output[0], "pair") == 0 && (tokens == 1 || tokens == 2)) {
      char* code = tokens == 1 ? get_code_interactively() : strdup(output[1]);
      if (!code) {
        printf("Failed to get connect code\n");
        continue;
      }

      splayer_pair_result_t* pair_result = auth_api->pair_with_code_sync(splayer, code);
      printf("Last pairing result:\nSuccess: %i\nMessage: %s\nDevice id: %s\n", pair_result->success,
             pair_result->message, pair_result->device_id);

      splayer_soundzone_t* soundzone = auth_api->get_soundzone(splayer);
      if (soundzone) {
        printf("Paired to sound zone: %s (%s)\n", soundzone->name, soundzone->id);
        auth_api->free_soundzone(soundzone);
      }

      auth_api->free_pairing_result(pair_result);
      free(code);
    } else if (strcmp(output[0], "unpair") == 0) {
      printf("Unpairing device\n");
      auth_api->unpair(splayer);
    } else if (strcmp(output[0], "pause") == 0) {
      printf("Pausing playback\n");
      controls_api->pause(splayer);
    } else if (strcmp(output[0], "play") == 0) {
      printf("Resume playback\n");
      controls_api->play(splayer);
    } else {
      printf("Invalid command entered, type 'help' for the list of commands'\n");
    }
    while (tokens--) {
      free(output[tokens]);
    }
    free(output);
  }

  api->free(splayer);
  free(config.audio_api_callbacks);

  return 0;
}
