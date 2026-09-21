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

#ifndef _SPLAYER_TROUBLES_API_H
#define _SPLAYER_TROUBLES_API_H

#include <stddef.h>

typedef struct splayer splayer_t;
typedef int err_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Give a hint if the user must or should take action or just general information.
 */
typedef enum {
  /**
   * \brief Information for the user that might be useful in troubleshooting
   */
  SPLAYER_TROUBLE_INFO = 0,

  /**
   * \brief Warning, the player will continue to play music, but the user should take action to improve
   * the user experience.
   */
  SPLAYER_TROUBLE_WARNING = 1,

  /**
   * \brief Critical error, that requires user action. The player will not play music.
   */
  SPLAYER_TROUBLE_CRITICAL = 2,

} splayer_trouble_severity_t;

/**
 * \brief Troubleshoting information, status and severity. Lives until you call free_troubles().
 */
typedef struct {

  /**
   * \brief The name of the trouble shooting information. Don't make any assumptions, these names can change.
   * \note The char buf is free:ed by a call to free_troubles().
   */
  const char* trouble_name;

  /**
   * \brief The result or comment for the troubleshooting information. Don't make any assumptions, these values will
   * change. \note The char buf is free:ed by a call to free_troubles().
   */
  const char* trouble_comment;

  /**
   * \brief Indicates if the test has been performed.
   * \note If this value is 0 you can disregard any other information.
   */
  int trouble_test_performed;

  /**
   * \brief Indicates if the test has been passed.
   * \note If this value is 0 there is something wrong.
   */
  int trouble_test_value;

  /**
   * \brief The severity of the trouble. This defaults to critical in this version of the API.
   */
  splayer_trouble_severity_t trouble_severity;
} splayer_trouble_t;

/**
 * \brief A wrapper for splayer_trouble_t
 */
typedef struct {
  /**
   * \brief Pointer to an array of splayer_trouble_t
   */
  splayer_trouble_t* splayer_trouble_array;

  /**
   * \brief Number of elements, so you know how many you have to iterate through.
   */
  size_t size;
} splayer_troubles_array_t;

/**
 * \brief This is the struct declaring all API calls for SPLAYER TROUBLES API. Before you can
 * use any of these calls you need to create a SPLAYER API context using create(splayer_config_t config).
 * See SPLAYER API section for more info.
 */

struct splayer_troubles_api {

  /**
   * \brief Version of the API
   */
  int api_version;

  /**
   * \brief Call this to retrieve an array splayer_trouble_t wrapped in splayer_troubles_array_t.
   * \param[in] splayer SPLAYER API context.
   * \param[out] splayer_troubles Returns an array of splayer_trouble_t data.
   * \returns Returns 0 for SUCCESS, otherwise FAILED.
   * \note You MUST call free_troubles() once you are done with the array.
   */
  err_t (*get_troubles)(splayer_t* splayer, splayer_troubles_array_t** splayer_troubles);

  /**
   * \brief Free the memory of splayer_troubles_array_t, all it's arrays and char buffers.
   * \param[in] splayer_troubles An array of splayer_trouble_t data.
   */
  void (*free_troubles)(splayer_troubles_array_t* splayer_troubles);
};

#ifdef __cplusplus
} // extern C
#endif

#endif // _SPLAYER_TROUBLES_API_H/
