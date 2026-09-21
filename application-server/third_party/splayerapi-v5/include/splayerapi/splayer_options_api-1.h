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

#ifndef SPLAYER_OPTIONS_H
#define SPLAYER_OPTIONS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Opaque type to represent a options context.
 */
typedef struct splayer_options splayer_options_t;

/**
 * \brief By default, the SDK logs messages directly to stdout.
 *        This callback allows you to intercept those messages and redirect them to
 *        your application's own logging.
 * \param message A null-terminated UTF-8 string
 * \param console_log_userdata An opaque pointer containing user specified data (e.g. file handles, structures, etc.).
 * \note Must point to a valid function for the entire lifetime of the splayer instance.
 * \note Message format may change between SDK versions. Do not parse it, only log it.
 * \ref console_log_callback
 */
typedef void (*console_log_callback)(const char* message, void* console_log_userdata);

typedef int err_t;

/**
 * \brief API for configuring player options before creating an splayer context.
 * This API allows you to set options that affect the behavior of the player.
 * It is optional and intended to replace `splayer_config_t` in the future.
 * \note The `splayer_options_t` context must be freed using the `free` function
 *       in this struct after the player has been created.
 */
struct splayer_options_api {
  /**
   * \brief Version of the API
   */
  int api_version;

  /**
   * \brief Creates a new options context. The returned pointer should be freed with the free function in this struct.
   */
  splayer_options_t* (*create)();

  /**
   * \brief Frees the options API context.
   * \param[in] splayer_options_t options API context.
   */
  void (*free)(splayer_options_t* options);

  /**
   * \brief Controls whether the player starts in a paused state.
   *
   * By default, the player will start playing as soon as a new device is
   * paired to a sound zone. Setting paused to true will change that behavior,
   * and the player will wait in a paused state until play is called.
   *
   * \param[in] options Options instance.
   * \param[in] paused true to start paused, false to start playing (default).
   * \return 0 on success, non-zero on failure.
   */
  err_t (*set_initially_paused)(splayer_options_t* options, bool paused);

  /**
   * \brief Redirects SDK log messages to your own logging system.
   *
   * By default, the SDK writes log messages directly to stdout.
   * Set this to intercept those messages and forward them to your own logger instead.
   * \param[in] options       Options instance created via \ref create.
   * \param[in] log_callback  Function the SDK will call for each log message. See \ref console_log_callback.
   * \param[in] log_userdata  Passed through as-is to \c log_callback on every call. Can be NULL.
   * \return 0 on success, non-zero on failure.
   */
  err_t (*set_log_handler)(splayer_options_t* options, console_log_callback log_callback, void* log_userdata);
};
#ifdef __cplusplus
} // extern C
#endif

#endif // SPLAYER_OPTIONS_H
