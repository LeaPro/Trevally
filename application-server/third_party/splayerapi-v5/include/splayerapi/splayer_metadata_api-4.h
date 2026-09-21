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

#ifndef _SPLAYER_METADATA_H
#define _SPLAYER_METADATA_H

typedef struct splayer splayer_t;
typedef int err_t;

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Config used to initialize the Metadata API
 */
typedef struct splayer_metadata_config {
  /**
   * \brief Boolean to enable or disable downloading of album art
   * \note Set this to TRUE if you want to be able to get album art for tracks, otherwise set it to FALSE to save disk
   * space.
   */
  bool download_album_art;
} splayer_metadata_config_t;

/**
 * \brief Struct containing information about album cover data
 * \note The image data is in jpeg/jfif format
 */
typedef struct {

  /**
   * \brief Pointer to byte data containing the image data
   */
  char* data;

  /**
   * \brief Size of data in bytes
   */
  unsigned int data_size;

  /**
   * \brief Width of the image
   */
  int width;

  /**
   * \brief Height of the image
   */
  int height;
} splayer_album_image_t;

/**
 * \brief Struct containing all available metadata for a track
 */
typedef struct {

  /**
   * \brief Title of the track
   * \note The char buf is free:ed by a call to free_track_metadata().
   */
  const char* track_title;

  /**
   * \brief Name of the Album where the track is from
   * \note The char buf is free:ed by a call to free_track_metadata().
   */
  const char* album_name;

  /**
   * \brief Array of artist names for the track
   * \note The array is free:ed by a call to free_track_metadata().
   */
  const char** artists;

  /**
   * \brief ISRC (International Standard Recording Code) for the track
   * \note The char buf is free:ed by a call to free_track_metadata().
   */
  const char* isrc;

  /**
   * \brief Number of artists in the artists array
   */
  int num_artists;

  /**
   * \brief Duration of the track in milliseconds
   */
  int duration_ms;

  /**
   * \brief Current number of seconds played of the track.
   * \note This is set when fetching the metadata. You need to fetch the metadata again to get an up to date track
   * position.
   */
  int current_track_position_s;

  /**
   * \brief URI to album art image.
   * \note The structure is free:ed by a call to free_track_metadata().
   */
  const char* album_image_uri;

  /**
   * \brief flag that signals whether or not a image is already cached.
   */
  bool is_album_image_cached;
} splayer_track_metadata_t;

/**
 * \brief This is the struct declaring all API calls for SPLAYER METADATA API. Before you can
 * use any of these calls you need to create a SPLAYER API context using create(splayer_config_t config).
 * See SPLAYER API section for more info.
 */
struct splayer_metadata_api {
  /**
   * \brief Version of the API
   */
  int api_version;

  /**
   * \brief Call this to get update the settings config for the Metadata API in runtime
   * \param[in] splayer SPLAYER API context.
   * \param[in] metadata config
   * \return Returns 0 if success otherwise non-zero.
   * \note Use this to enable/disable prefetching (caching) of album cover data
   */
  err_t (*update_config)(splayer_t* splayer, splayer_metadata_config_t config);

  /**
   * \brief Call this to get a splayer_track_metadata_t pointer with all the metadata for the current playing track
   * \param[in] splayer SPLAYER API context.
   * \returns Returns NULL when it could not fetch the metadata for the current track. Otherwise it will contain the
   * metadata for the current track \note You MUST call free_track_metadata() once you are done with the object.
   */
  splayer_track_metadata_t* (*get_current_track_metadata)(splayer_t* splayer);

  /**
   * \brief Call this to get a current track position in seconds
   * \param[in] splayer SPLAYER API context.
   * \returns Returns current number of seconds played of the current track.
   * \note Prefer calling this when you need to know the position of the current playing track instead of fetching the
   * entire metadata struct as that would copy unnecessary data.
   */
  int (*get_current_track_position)(splayer_t* splayer);

  /**
   * \brief Call this to retrieve the image data for the current track.
   * \param[in] splayer SPLAYER API context.
   * \returns Returns NULL if when we could not fetch image data otherwise a * splayer_album_image_t pointer.
   * \note The resource returned should be freed by free_splayer_image_data.
   */
  splayer_album_image_t* (*get_current_track_album_image)(splayer_t* splayer);

  /**
   * \brief Free the memory of splayer_track_metadata_t, all it's arrays, structures and char buffers.
   * \param[in] splayer_track_metadata_t a pointer to the metadata object.
   */
  void (*free_track_metadata)(splayer_track_metadata_t* track_metadata);

  /**
   * \brief Free the memory of splayer_album_image_t resource/
   * \param[in] splayer_album_image_t a pointer to the image object pointer.
   */
  void (*free_album_image)(splayer_album_image_t** album_image);
};

#ifdef __cplusplus
} // extern C
#endif

#endif // _SPLAYER_METADATA_H/
