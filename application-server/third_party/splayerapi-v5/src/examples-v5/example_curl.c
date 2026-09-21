
#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "audio_output_callbacks.h"
#include "curl_update.h"
#include "splayerapi/splayerapi-5.h"
// must come after splayerapi-5.h
#include <splayerapi/splayer_auth_api-1.h>

#define Q(x) #x
#define QUOTE(x) Q(x)
#define LOG_PRINTF(f_, ...)                                                                                            \
  {                                                                                                                    \
    printf(log_decorator, getpid());                                                                                   \
    printf((f_), ##__VA_ARGS__);                                                                                       \
  }

#ifdef linux
#include <dlfcn.h>
const char* LIBNAME = "libsplayer-1.so";
const char* LIBNAME_LATEST = "libsplayer-1-latest.so";
#elif WINDOWS
const char* LIBNAME = "libsplayer-1.dll";
const char* LIBNAME_LATEST = "libsplayer-1-latest.dll";
#else
#include <dlfcn.h>
const char* LIBNAME = "libsplayer-1.dylib";
const char* LIBNAME_LATEST = "libsplayer-1-latest.dylib";
#endif

enum splayer_update_state {
  SPLAYER_UPDATE_START,   // Will start player from standard library path
  SPLAYER_UPDATE_UPGRADE, // Will start player form upgrade library path
  SPLAYER_UPDATE_PENDING  // Waiting for player to quit.
};

// These must be global, so the signal handler could be used.
struct splayer_api* api = NULL;
splayer_t* splayer = NULL;
int force_check_now = 0;
const char* log_decorator = "UPDATER (%d)\t";

struct version_payload {
  char version[1024];
};

// Signal handler for player process
static void splayer_termination_handler(int signum) {
  static int second_quit = 0;

  switch (signum) {
  case SIGINT:
    if (second_quit) {
      LOG_PRINTF("Second Ctrl + C detected, force quit.\n");
      exit(1);
    } else {
      LOG_PRINTF("Ctrl + C detected, quitting nicely.\n");
      api->request_exit(splayer, SPLAYER_EXIT_NORMAL);
      second_quit = 1;
    }
    break;
  case SIGTERM:
    LOG_PRINTF("Got SIGTERM signal, will quit at end of current song.\n");
    api->request_exit(splayer, SPLAYER_EXIT_END_OF_SONG);
    break;
  default:
    LOG_PRINTF("Unknown signal %d\n", signum);
    break;
  }
  if (signal(signum, &splayer_termination_handler) == SIG_IGN) {
    signal(signum, SIG_IGN);
  }
}

// Signal handler for updater process
static void updater_termination_handler(int signum) {
  switch (signum) {
  case SIGHUP:
    LOG_PRINTF("Got SIGHUP, will force a update check.\n");
    force_check_now = 1;
    break;
  default:
    LOG_PRINTF("Unknown signal %d\n", signum);
    break;
  }
  if (signal(signum, &updater_termination_handler) == SIG_IGN) {
    signal(signum, SIG_IGN);
  }
}

static void* open_shared_lib(enum splayer_update_state state) {
  void* dl_handle = NULL;
  switch (state) {
  case SPLAYER_UPDATE_START:
    LOG_PRINTF("Player in state 'SPLAYER_UPDATE_START' will load library: '%s'\n", LIBNAME);
    dl_handle = dlopen(LIBNAME, RTLD_NOW);
    break;
  case SPLAYER_UPDATE_UPGRADE:
    LOG_PRINTF("Player in state 'SPLAYER_UPDATE_UPGRADE' will load library: '%s'\n", LIBNAME_LATEST);
    dl_handle = dlopen(LIBNAME_LATEST, RTLD_NOW);
    break;
  default:
    LOG_PRINTF("Invalid state %d", state);
    exit(3);
  }

  if (!dl_handle) {
    LOG_PRINTF("WHAT %s\n", dlerror());
    exit(1);
  }
  dlerror(); /* Clear any existing error */
  return dl_handle;
}

static void send_all(int fd, void* buffer, size_t total_bytes) {
  size_t bytes_written = 0;
  while (bytes_written < total_bytes) {
    ssize_t written = write(fd, ((uint8_t*)buffer) + bytes_written, total_bytes - bytes_written);
    if (written < 0)
      break;
    bytes_written += written;
  }
  if (bytes_written != total_bytes) {
    LOG_PRINTF("current_version was not written to the update process!\n");
    exit(1);
  }
}

static void read_all(int fd, void* buffer, size_t total_bytes) {
  size_t bytes_read = 0;
  while (bytes_read < total_bytes) {
    ssize_t read_size = read(fd, ((uint8_t*)buffer) + bytes_read, total_bytes - bytes_read);
    if (read_size < 0)
      break;
    bytes_read += read_size;
  }

  if (bytes_read != total_bytes) {
    LOG_PRINTF("Unable to read version payload\n");
    exit(1);
  }
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

/*
 * There are two ways to debug child processes.
 *
 * 1. In GDB type "set follow-fork-mode child" before you fork()
 *
 * 2. Attach to process in your IDE.
 *
 * If you get "ptrace: Operation not permitted." you need to permit it:
 *
 * 2a) One time in the same terminal as your IDE
 * echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope
 *
 * 2a) Permanent
 * Modify /etc/sysctl.d/10-ptrace.conf and change to kernel.yama.ptrace_scope = 0
 * Restart the service:
 * sudo service procps restart
 *
 * */
static void splayer_process(int other_pid, int pipe, int state, const char* code) {

  splayer_config_t config = {SPLAYER_SDK_VERSION};
  configure_splayer_api(&config);

  void* dl_handle = open_shared_lib(state);

  // Wire up splayer handles..
  api = (struct splayer_api*)dlsym(dl_handle, SPLAYER_API_SYMBOL);

  {
    char version[1024] = {0};
    api->get_current_version(NULL, version, sizeof(version));
    LOG_PRINTF("Running sdk version '%s'\n", version);
  }

  const struct splayer_auth_api* auth_api = api->get_auth_api();
  assert(auth_api);

  // Create an soundtrack api
  int retval = api->create(config, &splayer);

  if (!splayer) {
    LOG_PRINTF("Unable to allocate splayer. Error = %d\n", retval);
    exit(1);
  }

  // register sighandlers
  if (signal(SIGINT, &splayer_termination_handler) == SIG_IGN) {
    signal(SIGINT, SIG_IGN);
  }
  if (signal(SIGTERM, &splayer_termination_handler) == SIG_IGN) {
    signal(SIGTERM, SIG_IGN);
  }

  // Let the parent process know which version we are running
  // by sending the payload struct oer a pipe.
  struct version_payload payload = {};
  api->get_current_version(splayer, payload.version, sizeof(payload.version));
  send_all(pipe, &payload, sizeof(payload));
  close(pipe); // close the write-end of the pipe, thus sending EOF to the reader

  if (!auth_api->is_paired(splayer)) {
    if (!code || !strlen(code)) {
      printf("Device is not paired and no code provided.\n");
      exit(1);
    }
    splayer_pair_result_t* pair_result = auth_api->pair_with_code_sync(splayer, code);
    if (pair_result) {
      if (!pair_result->success) {
        printf("Pairing failed: %s\n", pair_result->message);
        auth_api->free_pairing_result(pair_result);
        exit(1);
      }
      printf("Paired successfully! Device ID: %s\n", pair_result->device_id);
      auth_api->free_pairing_result(pair_result);
    }
  }

  // The splayer main loop;
  for (;;) {
    if (!api->loop_iteration(splayer)) {
      break;
    }
  }

  api->free(splayer);
  free(config.audio_api_callbacks);
  dlclose(dl_handle);
  api = NULL;
  splayer = NULL;
  LOG_PRINTF("splayer_process exit state (%d) \n\n\n", state);
  exit(0);
}

static const int INITIAL_CHECK = 60;
static const int CHECK_INTERVAL = 900;              // 900s=15min
static const int CHECK_WORKING_LIB_SECS = 4 * 3600; // 14400s=4h
static const int EXIT_TIMEOUT = 3600 / 2;

int main(int argc, const char** argv) {
  if (argc > 2 || (argc == 2 && !strcmp(argv[1], "-h"))) {
    printf("Usage: ./example_curl [CODE]\n");
  }
  const char* code = argc == 2 ? argv[1] : NULL;
  const char* JSON_PATH = "http://127.0.0.1:8000/latest.json";
  enum splayer_update_state local_state = SPLAYER_UPDATE_START;
  time_t local_state_time = time(0);
  time_t local_check_time = 0;
  struct version_payload payload = {0};
  pid_t player_pid = 0;
  int pipefd[2];
  LOG_PRINTF("local_state = SPLAYER_UPDATE_START\n");

  if (signal(SIGHUP, &updater_termination_handler) == SIG_IGN) {
    signal(SIGHUP, SIG_IGN);
  }

  while (1) {

    switch (local_state) {
    case SPLAYER_UPDATE_UPGRADE:
    case SPLAYER_UPDATE_START: {

      if (player_pid == 0) {
        // We need to retrieve version and status data from our player fork,
        // so set up a pipe.
        // create the pipe
        if (pipe(pipefd) != 0) {
          LOG_PRINTF("Failed to create pipe.\n");
        };

        // Fork of the player process, and keep this process to be the updater...
        player_pid = fork();

        // If id is 0, we are the fork
        if (player_pid == 0) {
          log_decorator = "PLAYER  (%d)\t";
          close(pipefd[0]); // close the read-end of the pipe, I'm not going to use it
          splayer_process(player_pid, pipefd[1], local_state, code); // child process
          exit(1);
        }

        close(pipefd[1]); // close the write-end of the pipe, I'm not going to use it
        read_all(pipefd[0], &payload, sizeof(payload));
        close(pipefd[0]); // close the read-end of the pipe
        LOG_PRINTF("Current version is: '%s'\n", payload.version);
      }

      // Trigger update check on timeout
      if ((time(0) > (local_state_time + INITIAL_CHECK) && time(0) > (local_check_time + CHECK_INTERVAL)) ||
          force_check_now) {
        force_check_now = 0;
        LOG_PRINTF("Check for update! \n\n");

        res_t upgrade = upgrade_player(JSON_PATH, payload.version);
        LOG_PRINTF("upgrade_player returned: %d (%s)\n", upgrade, res_string[upgrade]);
        if (upgrade == OK) {
          local_state = SPLAYER_UPDATE_PENDING;
          local_state_time = time(0);
          LOG_PRINTF("local_state = SPLAYER_UPDATE_PENDING\n");
          LOG_PRINTF("Requesting gracefull termination (%d, SIGTERM)...\n", player_pid);
          kill(player_pid, SIGTERM);
          break;
        }
        // Restart timer..
        local_check_time = time(0);
      }

      // After some time, load the testing library over as the permanent option.
      if (local_state == SPLAYER_UPDATE_UPGRADE && time(0) > local_state_time + CHECK_WORKING_LIB_SECS) {
        local_state = SPLAYER_UPDATE_START;
        local_state_time = time(0);
        LOG_PRINTF("local_state = SPLAYER_UPDATE_START\n");
        LOG_PRINTF("New library loaded successfully, will replace default library.\n\n");
        if (rename(LIBNAME_LATEST, LIBNAME) != 0) {
          printf("Unable to move '%s' to '%s'\n", LIBNAME_LATEST, LIBNAME);
        }
      }

      // Check if player have died..
      int child_status = 0;
      int wait_pid = waitpid(player_pid, &child_status, WNOHANG);
      if (wait_pid == player_pid && (WIFEXITED(child_status) || WIFSIGNALED(child_status))) {
        LOG_PRINTF("Unexpected exit(%d)  wait_pid=%d   status=%d \n\n\n", local_state, wait_pid, child_status);

        // Revert to old library.
        if (local_state == SPLAYER_UPDATE_UPGRADE) {
          local_state = SPLAYER_UPDATE_START;
          LOG_PRINTF("local_state = SPLAYER_UPDATE_START\n");
          local_state_time = time(0);
        }
        player_pid = 0;
      }
      break;
    }
    case SPLAYER_UPDATE_PENDING: {
      int child_status = 0;
      int wait_pid = waitpid(player_pid, &child_status, WNOHANG);
      if (wait_pid == player_pid && (WIFEXITED(child_status) || WIFSIGNALED(child_status))) {
        LOG_PRINTF("Pending exit(%d)  wait_pid=%d   status=%d \n\n\n", local_state, wait_pid, child_status);
        local_state = SPLAYER_UPDATE_UPGRADE; // Start new lib next time..
        local_state_time = time(0);
        LOG_PRINTF("local_state = SPLAYER_UPDATE_UPGRADE\n");
        player_pid = 0;
      }

      // After been quitting for a while, start hard kill.
      if (time(0) > local_state_time + EXIT_TIMEOUT) {
        LOG_PRINTF("Timed out, will hard kill player kill(%d, SIGKILL)...\n", player_pid);
        kill(player_pid, SIGKILL);
      }
      break;
    }
    } /* switch */

    sleep(1);
  } /* while */
  return 0;
}
