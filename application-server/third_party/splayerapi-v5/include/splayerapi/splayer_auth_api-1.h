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

#ifndef _SPLAYER_AUTH_H
#define _SPLAYER_AUTH_H

typedef struct splayer splayer_t;
typedef int err_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Struct containing results from pairing.
 */
typedef struct {
  /**
   * \brief 0 if the pairing has failed, 1 if it has succeeded.
   */
  int success;

  /**
   * \brief Error message in the case of failed pairing.
   */
  const char* message;

  /**
   * \brief Generated id of the device for use in provisioning.
   */
  const char* device_id;
} splayer_pair_result_t;

/**
 * \brief Struct containing soundzone information.
 */
typedef struct {
  /**
   * \brief The id of the soundzone.
   */
  const char* id;

  /**
   * \brief The name of the soundzone.
   */
  const char* name;
} splayer_soundzone_t;

/**
 * \brief The authentication status of the player.
 */
typedef enum {
  /** \brief Status is not yet known or is being determined. */
  SPLAYER_AUTH_STATUS_UNKNOWN = 0,
  /** \brief Paired and connected to the backend. Ready for playback. */
  SPLAYER_AUTH_STATUS_PAIRED = 1,
  /** \brief Not paired; no credentials are stored, pairing is required. */
  SPLAYER_AUTH_STATUS_NOT_PAIRED = 2,
  /** \brief Old version of Tyson, an update is required to connect. */
  SPLAYER_AUTH_STATUS_UPDATE_REQUIRED = 3
} splayer_auth_status_t;

/**
 * \brief This is the struct declaring all  API calls for SPLAYER CONTROLS API.
 * Before you can use any of these calls you need to create a SPLAYER API
 * context using create(splayer_config_t config). See SPLAYER API section for
 * more info.
 */
struct splayer_auth_api {

  /**
   * \deprecated This API is deprecated and will be removed in a future version.
   * Use get_auth_status() for detailed auth state information.
   *
   * \brief DEPRECATED: Check if player is paired.
   * \param[in] splayer SPLAYER API context.
   * \return Returns 0 for player is not paired, otherwise 1.
   */
  int (*is_paired)(splayer_t* splayer);

  /**
   * \brief Initiate player pairing using a pairing code. Use get_pairing_result to check for pairing status.
   * \param[in] splayer SPLAYER API context.
   * \param[in] code pairing code e.g. ABCDEF
   * \return Returns the status for pairing initialization, 0 for SUCCESS, otherwise FAILED.
   */
  err_t (*initiate_pair_with_code)(splayer_t* splayer, const char* code);

  /**
   * \brief Returns the latest result of pairing with code.
   * \param[in] splayer SPLAYER API context.
   * \return Returns NULL if pairing is in progress, otherwise a splayer_pair_result_t pointer.
   * \note Pairing is done asynchronous so this function should be used to poll for the result of a pairing attempt
   * initiated by 'initiate_pair_with_code'. The function can also be used to get the device id of an already paired
   * device by calling it without initiating pairing first. For all ongoing status checks of an already-paired device,
   * please use 'get_auth_status'. If pairing fails, it will repeatedly be retried with the same code until a new code
   * is registered. Pairing attempts will time out after 35 seconds.
   * \note You MUST call free_pairing_result once you are done with the object.
   */
  splayer_pair_result_t* (*get_pairing_result)(splayer_t* splayer);

  /**
   * \brief Free the memory of splayer_pair_result_t.
   * \param[in] pairing_result The object to free.
   */
  void (*free_pairing_result)(splayer_pair_result_t* pairing_result);

  /**
   * \brief Pair player using a pairing code and await the result.
   * This is a convenience function that combines initiate_pair_with_code and get_pairing_result until a result is
   * provided.
   * See initiate_pair_with_code and get_pairing_result for more information.
   * \param[in] splayer SPLAYER API context.
   * \param[in] code pairing code e.g. ABCDEF
   * \return Returns a splayer_pair_result_t pointer containing the result. \note You
   * MUST call free_pairing_result once you are done with the object.
   */
  splayer_pair_result_t* (*pair_with_code_sync)(splayer_t* splayer, const char* code);

  /**
   * \brief Unpair player from the currently paired soundzone.
   * This will make an attempt to unpair the soundzone, if the network requests fails the player will still forget
   * its pairing information and be available for pairing again but the soundzone will appear as still paired.
   */
  void (*unpair)(splayer_t* splayer);

  /**
   * \brief Get the soundzone of the player.
   * \param[in] splayer SPLAYER API context.
   * \return Returns a splayer_soundzone_t pointer containing the soundzone. If the device isn't paired, NULL is
   *         returned. \note You MUST call free_soundzone once you are done with the object.
   */
  splayer_soundzone_t* (*get_soundzone)(splayer_t* splayer);

  /**
   * \brief Free the memory of splayer_soundzone_t.
   * \param[in] soundzone The object to free.
   */
  void (*free_soundzone)(splayer_soundzone_t* soundzone);

  /**
   * \brief Gets the last known authentication status of the player.
   * \param[in] splayer SPLAYER API context.
   * \return A status code from the splayer_auth_status_t enumeration.
   * \note This function provides the persistent authentication state of the device. To check the result of a one-time
   * pairing action, please use 'get_pairing_result'.
   */
  splayer_auth_status_t (*get_auth_status)(splayer_t* splayer);
};

#ifdef __cplusplus
} // extern C
#endif

#endif // _SPLAYER_AUTH_H/