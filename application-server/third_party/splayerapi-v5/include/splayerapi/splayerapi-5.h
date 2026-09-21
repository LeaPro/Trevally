/*************************************************************************
 *
 * SOUNDTRACK TECHNOLOGIES SWEDEN AB - CONFIDENTIAL
 * __________________
 *
 *  [2026] - SOUNDTRACK TECHNOLOGIES SWEDEN AB
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
 * https://www.soundtrack.io/legal/sdk-terms-of-use
 */

#ifndef _SPLAYER_H
#define _SPLAYER_H

#if __GNUC__ >= 4
#define SPLAYER_PUBLIC __attribute__((__visibility__("default")))
#elif defined(_MSC_VER)
#ifdef SPLAYER_EXPORTS
#define SPLAYER_PUBLIC __declspec(dllexport)
#else
#define SPLAYER_PUBLIC __declspec(dllimport)
#endif
#else
#define SPLAYER_PUBLIC
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Struct containing major and minor version number of current SDK
 */
typedef struct {
  int major;
  int minor;
} sdk_version_t;

/**
 * \brief SPLAYER SDK Version must match the version in library.
 */
static const sdk_version_t SPLAYER_SDK_VERSION = {5, 0};

/**
 * \brief This is the single entry point for SPLAYER API exposing the rest of the API calls
 * through a struct. This way new functions call be added in a backward compatible way
 * with minimal load time.
 */
#define SPLAYER_API SPLAYER_API_87GHV3
/**
 * \brief Symbol to do dlsym on if using shared library api.
 *        eg const struct splayer_api *api = dlsym(dl_handle, SPLAYER_API_SYMBOL_3_0);
 */
#define SPLAYER_API_SYMBOL "SPLAYER_API_87GHV3"

/**
 * These symbols are defined in the library, and shared among all the sdk:s
 */
SPLAYER_PUBLIC extern const char* SPLAYER_VERSION;
SPLAYER_PUBLIC extern const char* SPLAYER_BUILD;
SPLAYER_PUBLIC extern const char* SPLAYER_REV;
SPLAYER_PUBLIC extern const char* SPLAYER_PLATFORM_NAME;

/**
 * \brief Opaque type to SPLAYER API context.
 */
typedef struct splayer splayer_t;

/**
 * \brief 0 = SUCCESS, non-zero = FAILED
 */
typedef int err_t;

/*
 * \brief Opaque type to SPLAYER options context, used to set options before creating a player.
 */
typedef struct splayer_options splayer_options_t;

/**
 * \brief By default, the SDK logs messages directly to stdout.
 *        This callback allows you to intercept those messages and redirect them to
 *        your application's own logging.
 * \param message A null-terminated UTF-8 string
 * \param console_log_userdata An opaque pointer containing user specified data (e.g. file handles, structures, etc.).
 * \note Must point to a valid function for the entire lifetime of the splayer instance.
 * \note The formatting of the message might change at any time, it is not designed to be parsed, only
 *       outputted for debugging and logging purposes.
 * \ref console_log_callback
 */
typedef void (*console_log_callback)(const char* message, void* console_log_userdata);

/**
 * \brief Config used to create and initialize the player
 */
typedef struct {
  /**
   * \brief SDK version must match the version in library.
   * \note This field is required to be set.
   */
  sdk_version_t sdk_version;

  /**
   * \brief Pointer to users implementation of the callbacks.
   * \note Needs to point to valid memory during the entire lifetime of the application.
   */
  struct splayer_audio_api* audio_api_callbacks;

  /**
   * \brief DEPRECATED: This field is no longer required.
   */
  int hardware_bandwidth_limitation_kbps;

  /**
   * \brief Path where to store your cache directory. Default is workdir/cache.
   * \note You can destroy the buffer holding the path after calling create.
   */
  const char* diskcache_dir;

  /**
   * \brief Maximum size of disk cache.
   * A value > 0 will fix the max size of the disk cache. This will disable the possibility to change the size from
   * Soundtrack. A value 0 will result in a default value to be set (currently 5 GB, but subject to change). This will
   * enable the possibility to change the max size from Soundtrack. \note You can destroy the buffer holding the path
   * after calling create.
   */
  int diskcache_max_mb;

  /**
   * \brief Minimum free storage on the disk holding the disc cache.
   */
  int diskcache_remain_mb;

  /**
   * \brief This is the number of channels of your soundcard. E.g., 1 (mono) or 2 (stereo)
   * Audio is output as either mono or stereo. If the source is mono but `output_sample_channels` is set to stereo, the
   * audio will be converted to stereo samples.
   *
   * \note To avoid unnecessary re-sampling from mono to stereo or vice versa, choose the same number of channels
   * for both output_sample_channels rate AND your soundcard.
   */
  int output_sample_channels;

  /**
   * \brief This is the sample rate of your soundcard. E.g., 44100 Hz or 48000Hz.
   * Audio is output either at 44.1 kHz or 48 kHz. If your soundcard is configured with a different sample rate than
   * the audio output, the audio will be resampled.
   *
   * \note To avoid unnecessary re-sampling, choose the same sample rate for both `output_sample` rate AND your
   * soundcard.
   */
  int output_sample_rate;

  /**
   * \brief DEPRECATED: This field is no longer required.
   */
  const char* vendor_hardware_id;

  /**
   * \brief Name of the device running splayer.
   * \note This field is required to be set.
   */
  const char* vendor_device_name;

  /**
   * \brief DEPRECATED: This field is no longer required.
   */
  const char* vendor_secret;

  /**
   * \brief App version
   * \note Your current Application version, this is used for logging and troubleshooting.
   *       format should be [1-9][0-9]*\.[0-9]+ (eg. 1.0, 2.3, 10.20 etc)
   * \note This field is required to be set.
   */
  const char* app_version;

  /**
   * \brief This setting allows you to disable core dumps if set to true.
   */
  bool disable_core_dumps;

  /**
   * \brief DEPRECATED: This field is no longer required.
   */
  bool use_interactive_pairing;

  /**
   * \brief This setting allows you to disable the on-disk caching of played audio files.
   * \note Enabling this setting increases network requests and data downloads, but is only
   *       recommended for platforms with limited storage.
   *       Note that it disables offline audio playback and renders diskcache_max_mb and
   *       diskcache_remain_mb settings irrelevant.
   */
  bool disable_audio_caching;

  /**
   * \brief This callback function allows you to add custom logic when loading the token.
   * \note A custom callback function which overrides default token loading logic.
   *       If an invalid token is returned, pairing will fail.
   *       Must point to a valid function for the entire lifetime of the splayer instance.
   */
  void (*load_token_callback)(char* buf, size_t buflen);

  /**
   * \brief This callback function allows you to add custom logic when saving the token.
   * \note A custom callback function which overrides default token saving logic.
   *       Store this token to load it using load_token_callback.
   *       Must point to a valid function for the entire lifetime of the splayer instance.
   */
  void (*save_token_callback)(const char*);

} splayer_config_t;

/**
 * \brief Sets the quit flag in the session.
 */
typedef enum {
  /**
   * \brief Wait for all tasks and threads to finish and quit.
   */
  SPLAYER_EXIT_NORMAL = 0,

  /**
   * \brief Restart after finishing playing the current song.
   */
  SPLAYER_EXIT_RESTART = 1,

  /**
   * \brief Quit after finishing playing the current song.
   */
  SPLAYER_EXIT_END_OF_SONG = 2,

  /**
   * \brief Restart and upgrade after finishing playing the current song.
   */
  SPLAYER_EXIT_UPGRADE = 3,

  /**
   * \brief Quit now.
   */
  SPLAYER_EXIT_HARD = 4
} splayer_exit_t;

/**
 * \brief This is the struct declaring all API calls for SPLAYER API. The simplest use case
 * is to first create a `splayer_t` context. Don't forget to create a `splayer_audio_api` struct, with
 * the callback code and set `config.audio_api_callbacks` variable. `splayer_audio_api` struct needs
 * to be valid memory during the entire lifetime of the application.
 * To determine when the player intends to shut down, your application should regularly check the return value of
 * `should_exit()`. Once it returns `TRUE`, you can let the process exit. You can also request shutdown by
 * calling `request_exit()` with the desired exit method. Once you do, you have to wait for `should_exit()`to return
 * `TRUE` before exiting the process.
 * After the player has shut down, you must free the `splayer_t` context using `free()`.
 */
struct splayer_api {
  /**
   * \brief SPLAYER SDK version must match the version in library.
   */
  sdk_version_t splayer_sdk_version;

  /**
   * \brief Creates a SPLAYER API context, that you need to store somewhere (eg. on the stack). All values are copied
   * from config, so config parameter could be stored on the stack or freed after calling create.
   * Once the context is created, the player is considered active and internal threads are spawned by the SDK during
   * this process. These threads remain active until the player is shut down.
   * \note config.audio_api_callbacks needs to be valid memory during the lifetime of the application
   * \note See \ref create_with_log_handler for alternative that allows console logging override.
   * \note See \ref create_with_options for alternative that allows passing additional configurations before starting
   * the player.
   * \param[in] config Configuration for the SPLAYER API.
   * \param[out] SPLAYER API context, that you will need to call the rest of the API functions and current and future
   * extensions. SPLAYER_CONTROLS_API is such an extension
   * \return Returns 0 if success otherwise non-zero.
   * \ref create
   */
  err_t (*create)(const splayer_config_t config, splayer_t** splayer);

  /**
   * \brief Frees the SPLAYER API context.
   * \param[in] splayer SPLAYER API context.
   */
  void (*free)(splayer_t* splayer);

  /**
   * \brief Call this to retrieve the librarys current player version.
   * \param[in] splayer SPLAYER API context.
   * \param[out] version A buffer where the version string will be placed. Recommended size is at least 128 bytes.
   * \param[in] version_size The allocated buffer's size. Recommended size is at least 128 bytes.
   */
  void (*get_current_version)(splayer_t* splayer, char* version, int version_size);

  /**
   * DEPRECATED: This API is deprecated and will be removed in a future version.
   * Use should_exit() instead.
   */
  int (*loop_iteration)(splayer_t* splayer);

  /**
   * \brief Call this if you want the player to quit gracefully, e.g on the next song.
   * \param[in] splayer SPLAYER API context.
   * \param[in] exit_method For more info checkout splayer_exit_t.
   */
  void (*request_exit)(splayer_t* splayer, splayer_exit_t exit_method);

  /**
   * \brief Call this to get a pointer to handle to the Troubles API
   * \return Returns a pointer to the Troubles API
   */
  const struct splayer_troubles_api* (*get_troubles_api)();
  /**
   * \brief Call this to get a pointer to handle to the Controls API
   * \return Returns a pointer to the Controls API
   */
  const struct splayer_controls_api* (*get_controls_api)();
  /**
   * \brief Call this to get a pointer to handle to the Metadata API
   * \return Returns a pointer to the Metadata API
   */
  const struct splayer_metadata_api* (*get_metadata_api)();
  /**
   * \brief Call this to get a pointer to handle to the Auth API
   * \return Returns a pointer to the Auth API
   */
  const struct splayer_auth_api* (*get_auth_api)();

  /**
   * \brief Check if the application is to be shut down.
   * This function should be called regularly to check if the player intends to shut down.
   * \param[in] splayer SPLAYER API context.
   * \return Returns 1 if the application is to be shut down, otherwise 0.
   */
  int (*should_exit)(splayer_t*);

  /**
   * \brief Creates SPLAYER API context the same way as \see create but allows adding
   *        a console log message handler with optional user data.
   * \param[in] config Configuration for the SPLAYER API.
   * \param[out] SPLAYER API context, that you will need to call the rest of the API functions and current and future
   * extensions. SPLAYER_CONTROLS_API is such an extension
   * \param[in] Callback that will receive the log messages. \see console_log_callback
   * \param[in] User data for the callback. \see console_log_callback
   * \return Returns 0 if success otherwise non-zero.
   * \ref create_with_log_handler
   */
  err_t (*create_with_log_handler)(const splayer_config_t config, splayer_t** splayer,
                                   console_log_callback log_callback, void* log_userdata);
  /**
   * \brief Call this to get a pointer to handle to the Options API
   * \return Returns a pointer to the options API
   */
  const struct splayer_options_api* (*get_options_api)();

  /**
   * \brief Creates SPLAYER API context the same way as \ref create but with additional options.
   *
   * This function extends the player creation by accepting an `splayer_options_t` context,
   * allowing you to configure behavior that is not available through `splayer_config_t` to keep SDK backwards
   * compatible.
   *
   * \param[in] config Configuration for the SPLAYER API.
   * \param[out] splayer SPLAYER API context.
   * \param[in] options Options instance created via `splayer_options_api`. See \ref get_options_api.
   * \return Returns 0 if success otherwise non-zero.
   * \note The `splayer_options_t` instance must be freed after this call using `splayer_options_api::free()`.
   */
  err_t (*create_with_options)(const splayer_config_t config, splayer_t** splayer, const splayer_options_t* options);

  /**
   * \brief Call this to get a pointer to handle to the library API
   * \return Returns a pointer to the library API
   */
  const struct splayer_library_api* (*get_library_api)();
};

/**
 * \brief This is the structure holding function pointers to the SPLAYER API
 * \note You should use the SPLAYER_API define to get this struct in a backwards compatible way
 */
SPLAYER_PUBLIC extern const struct splayer_api SPLAYER_API_87GHV3;

#ifdef __cplusplus
} // extern C
#endif

#endif // _SPLAYER_H/
