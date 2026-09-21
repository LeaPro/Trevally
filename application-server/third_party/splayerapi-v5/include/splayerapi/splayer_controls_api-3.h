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

#ifndef _SPLAYER_CONTROL_H
#define _SPLAYER_CONTROL_H

typedef struct splayer splayer_t;
typedef int err_t;

/*
 * Api Changes:
 *
 * api_version: 3
 *  Add is_paused()
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief This is the struct declaring all  API calls for SPLAYER CONTROLS API. Before you can
 * use any of these calls you need to create a SPLAYER API context using create(splayer_config_t config).
 * See SPLAYER API section for more info.
 */
struct splayer_controls_api {
  /**
   * \brief Version of the API
   */
  int api_version;
  /**
   * \brief Check if the player is playing music. This is not the same as if its paused or not. Until loop_iteration()
   * is called it will not be playing. \param[in] splayer SPLAYER API context. \return Returns 0 when not playing,
   * otherwise non-zero and one on most platforms.
   */
  int (*is_playing)(splayer_t* splayer);

  /**
   * \brief Pause the player. No effect unless loop_iteration() is called.
   * \param[in] splayer SPLAYER API context.
   * \return Returns 0 for SUCCESS, otherwise FAILED.
   */
  err_t (*pause)(splayer_t* splayer);

  /**
   * \brief Start the player if it's paused. No effect unless loop_iteration() is called.
   * \param[in] splayer SPLAYER API context.
   * \return Returns 0 for SUCCESS, otherwise FAILED.
   */
  err_t (*play)(splayer_t* splayer);

  /**
   * \brief Skip a number of tracks. No effect unless loop_iteration() is called.
   * \param[in] splayer SPLAYER API context.
   * \param[in] count Number of tracks to skip.
   * \return Returns 0 for SUCCESS, otherwise FAILED.
   */
  err_t (*skip_tracks)(splayer_t* splayer, int count);

  /**
   * \brief Get player target volume from 0 - 100%. No effect unless loop_iteration() is called.
   *        Note, this volume is not intended for controlling the mixer, use audio api for that.
   * \param[in] splayer SPLAYER API context.
   * \return Returns volume 0 - 100
   */
  int (*get_volume)(splayer_t* splayer);

  /**
   * \brief Set wanted player volume from 0 - 100%. No effect unless loop_iteration() is called.
   * \param[in] splayer SPLAYER API context.
   * \param[in] volume from 0 - 100
   * \return Returns 0 for SUCCESS, otherwise FAILED.
   */
  err_t (*set_volume)(splayer_t* splayer, int volume);

  /**
   * \brief Check if the player is paused. An unpaused player could still not play music if nothing is scheduled. Until
   * loop_iteration() is called it will not be playing. \param[in] splayer SPLAYER API context. \return Returns 0 when
   * not paused, otherwise non-zero and one on most platforms.
   */
  int (*is_paused)(splayer_t* splayer);
};

#ifdef __cplusplus
} // extern C
#endif

#endif // _SPLAYER_CONTROL_H/
