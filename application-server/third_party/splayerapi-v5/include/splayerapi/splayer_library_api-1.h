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

#ifndef SPLAYER_LIBRARY_API_H
#define SPLAYER_LIBRARY_API_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct splayer splayer_t;
typedef int err_t;

/**
 * \brief Async operation status for `fetch_library()` requests.
 */
typedef enum {
  /** \brief No operation in progress. */
  SPLAYER_ASYNC_IDLE = 0,
  /** \brief An async operation has been submitted and is in progress. */
  SPLAYER_ASYNC_PENDING = 1,
  /** \brief The operation completed successfully; result is ready to be consumed. */
  SPLAYER_ASYNC_READY = 2,
  /** \brief The async operation failed. */
  SPLAYER_ASYNC_ERROR = 3,
} splayer_async_status_t;

/**
 * \brief Opaque result object returned by `consume_library_result()`.
 *
 * Owns all memory for the fetched sources.
 * Must be freed with `free_library_result()` when no longer needed.
 */
typedef struct splayer_library_result splayer_library_result_t;

/**
 * \brief Represents a music source (playlist or schedule) in the user's library.
 */
typedef struct splayer_music_source splayer_music_source_t;

/**
 * \brief Enumeration of music source types in the user's library.
 * A playlist is a static collection of tracks.
 * A schedule is a time-based configuration that automatically selects
 * which playlist to play depending on the time of day or week.
 */
typedef enum {
  SPLAYER_SOURCE_PLAYLIST = 0,
  SPLAYER_SOURCE_SCHEDULE = 1,
} splayer_source_type_t;

struct splayer_library_api {
  /**
   * \brief Version of the API
   */
  int api_version;

  /* ------ Synchronous library helper function (blocking) ------ */

  /**
   * \brief Fetch a batch of library sources synchronously (blocking).
   *
   * This is a convenience wrapper that calls `fetch_library()`, polls
   * `get_library_status()` in a loop, and returns the result. Intended
   * for simple integrations that don't need non-blocking behavior.
   *
   * Call `reset_library()` to reset pagination to the beginning, if `fetch_library_sync()` fails `reset_library()` will
   * be called internally.
   * The caller must free the result with `free_library_result()` when done.
   *
   * \param[in]  splayer  Player context.
   * \param[in]  limit    Maximum number of sources to fetch.
   * \param[out] result   Pointer to the result object (NULL on failure).
   * \return 0 on success, non-zero on failure.
   */
  err_t (*fetch_library_sync)(splayer_t* splayer, int limit, splayer_library_result_t** result);

  /* ------ Async library fetching (submit / poll / consume) ------ */

  /**
   * \brief Submit an async request to fetch a batch of music sources.
   *
   * Transitions `splayer_async_status_t` from IDLE to PENDING. Poll with `get_library_status()`
   * until READY or ERROR, then call `consume_library_result()` to consume the result.
   *
   * If called while PENDING, the previous request is cancelled and a new one
   * is submitted (state goes back to PENDING with the new request).
   *
   * If called while READY or ERROR, returns an error — consume the pending
   * result with `consume_library_result()` first.
   *
   * Call `reset_library()` to cancel any in-flight request or reset pagination to the beginning.
   *
   * \param[in] splayer  Player context.
   * \param[in] limit    Maximum number of sources to fetch.
   * \return 0 on success (request submitted), non-zero on failure.
   */
  err_t (*fetch_library)(splayer_t* splayer, int limit);

  /**
   * \brief Poll the status of the most recent `fetch_library()` request.
   *
   * Returns IDLE if no request has been made, PENDING while in-flight,
   * READY when a result is available, or ERROR on failure.
   *
   * \param[in] splayer  Player context.
   * \return The current async status.
   */
  splayer_async_status_t (*get_library_status)(splayer_t* splayer);

  /**
   * \brief Consume the result of a completed `fetch_library()` request.
   *
   * May only be called when `get_library_status()` returns READY or ERROR.
   * On success (READY), *result is set to the fetched data. On failure
   * (ERROR), *result is set to NULL. In both cases the state transitions
   * back to IDLE, allowing a new `fetch_library()` call.
   *
   * Calling this while IDLE or PENDING is a no-op that returns an error.
   *
   * \param[in]  splayer  Player context.
   * \param[out] result   Receives the result pointer (NULL on error).
   * \return 0 on success, non-zero on failure.
   */
  err_t (*consume_library_result)(splayer_t* splayer, splayer_library_result_t** result);

  /**
   * \brief Free out the result returned by `fetch_library()`.
   *
   * After this call, all pointers into the result (including string fields
   * on `splayer_music_source_t`) are invalid.
   *
   * \param[in] result  Result to free. No-op if NULL.
   */
  void (*free_library_result)(splayer_library_result_t* result);

  /**
   * \brief Resets the internal cursors, cancels any in-flight requests and enables fetching from the beginning again.
   *
   * The next `fetch_library()` call will start from the beginning of the library.
   * Cancels any in-flight request and resets the state to IDLE. If a request is currently in-flight, the result of that
   * request will be discarded.
   */
  void (*reset_library)(splayer_t* splayer);

  /* ------ Result accessors ------ */

  /**
   * \brief Get a single library item by index.
   * \return Pointer to the item `splayer_music_source_t` at the given index, or nullptr if index is out of bounds.
   * The returned pointer can be used by the Music source getters to access the fields of the item.
   */
  const splayer_music_source_t* (*get_library_result_item)(splayer_library_result_t* result, int index);

  /**
   * \brief Get the total number of items in the result.
   */
  int (*get_result_count)(splayer_library_result_t* result);

  /**
   * \brief Check if there are more items to fetch after the current batch.
   * \return 1 if there are more items to fetch, 0 otherwise.
   */
  int (*result_has_more)(splayer_library_result_t* result);

  /* ------ Music source getters -------*/

  /**
   * \brief Get the unique identifier of the music source.
   * The `id` field is a unique identifier for the source, which can be used to play it.
   * The returned pointer remains valid as long as the source object is valid.
   */
  const char* (*get_source_id)(const splayer_music_source_t* source);

  /**
   * \brief Get the display name of the music source.
   * The `name` field is a human-readable name for display purposes.
   * The returned pointer remains valid as long as the source object is valid.
   */
  const char* (*get_source_name)(const splayer_music_source_t* source);

  /**
   * \brief Get the type of the music source (playlist or schedule).
   * The `type` field indicates whether the source is a playlist or a schedule.
   * The returned pointer remains valid as long as the source object is valid.
   */
  splayer_source_type_t (*get_source_type)(const splayer_music_source_t* source);

  /**
   * \brief Get a display image uri of the music source (playlist or schedule) for display purposes.
   * The returned pointer remains valid as long as the source object is valid.
   */
  const char* (*get_source_image_uri)(const splayer_music_source_t* source);

  /**
   * \brief Change the playback music source of the sound zone.
   *
   * Given a source_id of a music source which is returned using \ref `get_source_id`, the player will change the
   * music source to the one specified by `source_id`.
   *
   * \param[in] splayer SPLAYER API context.
   * \param[in] source_id The unique identifier of the music source to play.
   * \param[in] source_type The type of the music source, either Playlist or Schedule.
   * \param[in] play_now If non-zero, the player will start playing the new source immediately. If zero, the new source
   * will be scheduled to play after the current track finishes.
   * \return Returns 0 for SUCCESS, otherwise FAILED.
   */
  err_t (*set_play_from)(splayer_t* splayer, const char* source_id, splayer_source_type_t source_type, int play_now);
};

#ifdef __cplusplus
} // extern C
#endif

#endif // SPLAYER_LIBRARY_API_H