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

#ifndef _SPLAYER_AUDIO_API_H
#define _SPLAYER_AUDIO_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/**
 * \brief Callback Audio API where your (the users) platform/hardware specific code exists. Write your callback
 * functions and make a splayer_audio_api struct out of them. The struct needs to be valid memory during
 * the entire runtime of the application. Assign it to config.audio_api_callbacks, before you call loop_iteration.
 */
typedef struct splayer_audio_api {

  /**
   * \brief Initialization callback.
   * This function is called when the audio pipeline is setup.
   * An init and shutdown might be run in series if splayer detects that
   * audio data is not delivered in a 60 second window, to attempt
   * to restore the operation.
   */
  void (*audio_init)(struct splayer_audio_api* ctx, const int output_sample_channels, const int output_sample_rate);

  /**
   * \brief Shutdown callback.
   * Will be called when the audio pipeline is shutting down.
   */
  void (*audio_shutdown)(struct splayer_audio_api* ctx);

  /**
   * \brief Flush the buffer.
   * Will be called when a sudden track change is required and all
   * the buffers should be emptied (including harware buffers).
   */
  void (*audio_flush)(struct splayer_audio_api* ctx);

  /**
   * @brief Callback function responsible for playing a buffer of audio data.
   *
   * The player calls this function to deliver a chunk of 16-bit signed PCM audio that should be played.
   * The implementation is responsible for delivering the samples to the destination of choice, e.g. ALSA, PulseAudio
   * or a custom audio backend.
   *
   * @param[in]  ctx A pointer to the audio API context for the implementation.
   * @param[in]  samples A pointer to a read-only buffer containing the PCM audio samples to play.
   * @param[in]  sample_count The total number of samples available in the `samples` buffer.
   * @param[out] samples_buffered A pointer to a variable that the implementation must fill with
   * the total number of unplayed samples currently held in the hardware/driver buffers.
   *
   * @return The total number of samples actually consumed from the `samples` buffer.
   *
   * @note This function cannot block, as it shares a thread with the playback content provider. If the underlying
   * audio API can block, a separate writer thread with a ring buffer should be used.
   */
  size_t (*audio_data)(struct splayer_audio_api* ctx, const int16_t* samples, size_t sample_count,
                       uint32_t* samples_buffered);

  /**
   * \brief Pause output.
   * This will be called if user ask to pause audio output.
   * You should not flush buffers on unpause, just continue delivering the audio.
   */
  void (*audio_pause)(struct splayer_audio_api* ctx, int pause);

  /**
   * \brief Adjust volume.
   * \param vol is an integer between 0 and 100, inclusive. The value is linear, i.e., it is not logarithmic and is not
   * processed for any loudness curves or perceptual adjustments.
   */
  void (*audio_volume)(struct splayer_audio_api* ctx, int vol);
} splayer_audio_api_t;

#ifdef __cplusplus
} // extern C
#endif

#endif // _SPLAYER_AUDIO_API_H/
