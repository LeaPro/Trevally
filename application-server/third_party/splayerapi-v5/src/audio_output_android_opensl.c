/*************************************************************************
 *
 * SOUNDTRACK TECHNOLOGIES SWEDEN AB - CONFIDENTIAL
 * __________________
 *
 *  [2025] - SOUNDTRACK TECHNOLOGIES SWEDEN AB
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
 */

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "splayer_circ_buf.h"
#include "splayerapi/splayer_audio_api-3.h"

#define NUM_BUFFERS 7
#define BUFFER_SIZE 4096
#define SAMPLE_TYPE int16_t
#define RING_BUFFER_SIZE_SECONDS 1

static inline void MUTEX_INIT(pthread_mutex_t* mtx) {
  int r;
  if ((r = pthread_mutex_init(mtx, NULL)) != 0) {
    printf("Unable to init mutex '%s'\n", strerror(r));
    abort();
  }
}

static inline void MUTEX_DESTORY(pthread_mutex_t* mtx) {
  int r;
  if ((r = pthread_mutex_destroy(mtx)) != 0) {
    printf("Unable to destroy mutex '%s'\n", strerror(r));
    abort();
  }
}

static inline void MUTEX_LOCK(pthread_mutex_t* mtx) {
  int r;
  if ((r = pthread_mutex_lock(mtx)) != 0) {
    printf("Unable to lock mutex, return code %d '%s'\n", r, strerror(r));
    abort();
  }
}

static inline void MUTEX_UNLOCK(pthread_mutex_t* mtx) {
  int r;
  if ((r = pthread_mutex_unlock(mtx)) != 0) {
    printf("Unable to unlock mutex, return code %d '%s'\n", r, strerror(r));
    abort();
  }
}

struct android_audio_impl {
  struct splayer_audio_api api;

  // State
  int paused;
  float volume;
  SLmillibel sl_volume;
  SLmillibel sl_min_volume;
  SLmillibel sl_max_volume;

  // Buffer for incoming samples
  circ_buf_t ring_buffer;
  SAMPLE_TYPE* ring_buffer_data;
  pthread_mutex_t ring_mutex;

  // OpenSL state
  SLObjectItf engineObject;
  SLEngineItf engineEngine;
  SLObjectItf outputMixObject;

  // Buffer queue player interfaces
  SLObjectItf bqPlayerObject;
  SLPlayItf bqPlayerPlay;
  SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
  SLMuteSoloItf bqPlayerMuteSolo;
  SLVolumeItf bqPlayerVolume;

  // Buffers feed to OpenSL
  int current_buffer;
  SAMPLE_TYPE* buffer[NUM_BUFFERS];
};

static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void* ctx) {
  struct android_audio_impl* this = (struct android_audio_impl*)ctx;

  uint32_t bytes_to_enqueue = BUFFER_SIZE;

  MUTEX_LOCK(&this->ring_mutex);
  uint32_t bytes_available = (uint32_t)circ_used(&this->ring_buffer);
  if (bytes_available < bytes_to_enqueue) {
    printf("Not enough bytes in ring buffer (%d) vs wanted (%d), enquing silence\n", bytes_available, bytes_to_enqueue);
    memset(this->buffer[this->current_buffer], 0, bytes_to_enqueue);
  } else {
    uint32_t bytes_pulled =
      (uint32_t)circ_pull(&this->ring_buffer, (char*)this->buffer[this->current_buffer], bytes_to_enqueue);
    assert(bytes_pulled == bytes_to_enqueue);
    (void)bytes_pulled; // Assert isn't available in release builds, so adding this omits the unused warning
  }

  SLresult result;
  result = (*this->bqPlayerBufferQueue)
             ->Enqueue(this->bqPlayerBufferQueue, this->buffer[this->current_buffer], bytes_to_enqueue);
  if (result != SL_RESULT_SUCCESS) {
    printf("OpenSL ES: Failed to enqueue %d bytes, result=%d\n", bytes_to_enqueue, (int)result);
  }

  this->current_buffer++;
  if (this->current_buffer == NUM_BUFFERS) {
    this->current_buffer = 0;
  }

  MUTEX_UNLOCK(&this->ring_mutex);
}

static void update_sl_volume_no_lock(struct android_audio_impl* this) {
  // Convert to OpenSL millibel volume
  int minVolume = this->sl_min_volume;
  int maxVolume = this->sl_max_volume;

  if (minVolume < -8000)
    minVolume = -8000;

  // Nicer volume curve
  float vol = powf(this->volume, 0.5f);

  this->sl_volume = minVolume + vol * (maxVolume - minVolume);
  printf("Setting OpenSL volume to %d. Max: %d, Min: %d. in: %f\n", this->sl_volume, this->sl_max_volume,
         this->sl_min_volume, this->volume);
  if (this->bqPlayerVolume && (*this->bqPlayerVolume)) {
    (*this->bqPlayerVolume)->SetVolumeLevel(this->bqPlayerVolume, this->sl_volume);
  }
}

static void opensl_init(struct splayer_audio_api* ctx, const int output_sample_channels, const int output_sample_rate) {
  struct android_audio_impl* this = (struct android_audio_impl*)ctx;

  printf("Init opensl. output_sample_channels: %d, output_sample_rate: %d\n", output_sample_channels,
         output_sample_rate);
  if (this->engineObject) {
    printf("Audio already initialized.\n");
    return;
  }

  this->paused = 0;

  MUTEX_INIT(&this->ring_mutex);

  // Buffer one second ahead in the incomming buffer;
  size_t ring_buffer_size =
    RING_BUFFER_SIZE_SECONDS * output_sample_channels * output_sample_rate * sizeof(SAMPLE_TYPE);
  assert(ring_buffer_size < UINT32_MAX);
  this->ring_buffer_data = (SAMPLE_TYPE*)malloc(ring_buffer_size);
  circ_init(&this->ring_buffer, (char*)this->ring_buffer_data, ring_buffer_size);

  for (int i = 0; i < NUM_BUFFERS; i++) {
    this->buffer[i] = (SAMPLE_TYPE*)malloc(BUFFER_SIZE);
    memset(this->buffer[i], 0, BUFFER_SIZE);
  }

  SLresult result;
  // Create engine
  result = slCreateEngine(&this->engineObject, 0, NULL, 0, NULL, NULL);
  assert(SL_RESULT_SUCCESS == result);
  result = (*this->engineObject)->Realize(this->engineObject, SL_BOOLEAN_FALSE);
  assert(SL_RESULT_SUCCESS == result);
  result = (*this->engineObject)->GetInterface(this->engineObject, SL_IID_ENGINE, &this->engineEngine);
  assert(SL_RESULT_SUCCESS == result);
  result = (*this->engineEngine)->CreateOutputMix(this->engineEngine, &this->outputMixObject, 0, 0, 0);
  assert(SL_RESULT_SUCCESS == result);
  result = (*this->outputMixObject)->Realize(this->outputMixObject, SL_BOOLEAN_FALSE);
  assert(SL_RESULT_SUCCESS == result);

  SLuint32 sr;
  if (output_sample_rate == 44100) {
    sr = SL_SAMPLINGRATE_44_1;
  } else if (output_sample_rate == 48000) {
    sr = SL_SAMPLINGRATE_48;
  } else if (output_sample_rate == 22050) {
    sr = SL_SAMPLINGRATE_22_05;
  } else {
    printf("Sample rate not supported: %d\n", output_sample_rate);
    return;
  }

  SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
  SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM,
                                 (SLuint32)output_sample_channels,
                                 sr,
                                 SL_PCMSAMPLEFORMAT_FIXED_16,
                                 SL_PCMSAMPLEFORMAT_FIXED_16,
                                 SL_SPEAKER_FRONT_LEFT | (output_sample_channels == 2 ? SL_SPEAKER_FRONT_RIGHT : 0),
                                 SL_BYTEORDER_LITTLEENDIAN};

  // Configure audio source
  SLDataSource audioSrc = {&loc_bufq, &format_pcm};

  // Configure audio sink
  SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, this->outputMixObject};
  SLDataSink audioSnk = {&loc_outmix, NULL};

  // Create audio player
  const SLInterfaceID ids[2] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
  const SLboolean req[2] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

  result = (*this->engineEngine)
             ->CreateAudioPlayer(this->engineEngine, &this->bqPlayerObject, &audioSrc, &audioSnk, 2, ids, req);
  assert(SL_RESULT_SUCCESS == result);

  result = (*this->bqPlayerObject)->Realize(this->bqPlayerObject, SL_BOOLEAN_FALSE);
  assert(SL_RESULT_SUCCESS == result);
  result = (*this->bqPlayerObject)->GetInterface(this->bqPlayerObject, SL_IID_PLAY, &this->bqPlayerPlay);
  assert(SL_RESULT_SUCCESS == result);
  result = (*this->bqPlayerObject)->GetInterface(this->bqPlayerObject, SL_IID_BUFFERQUEUE, &this->bqPlayerBufferQueue);
  assert(SL_RESULT_SUCCESS == result);
  result = (*this->bqPlayerBufferQueue)->RegisterCallback(this->bqPlayerBufferQueue, bqPlayerCallback, (void*)this);
  assert(SL_RESULT_SUCCESS == result);
  result = (*this->bqPlayerObject)->GetInterface(this->bqPlayerObject, SL_IID_VOLUME, &this->bqPlayerVolume);
  assert(SL_RESULT_SUCCESS == result);
  result =
    (*this->bqPlayerPlay)->SetPlayState(this->bqPlayerPlay, this->paused ? SL_PLAYSTATE_PAUSED : SL_PLAYSTATE_PLAYING);
  assert(SL_RESULT_SUCCESS == result);

  (*this->bqPlayerVolume)->GetMaxVolumeLevel(this->bqPlayerVolume, &this->sl_max_volume);
  this->sl_min_volume = SL_MILLIBEL_MIN;

  this->current_buffer = 0;

  result =
    (*this->bqPlayerBufferQueue)->Enqueue(this->bqPlayerBufferQueue, this->buffer[this->current_buffer], BUFFER_SIZE);
  if (SL_RESULT_SUCCESS != result) {
    printf("Error enqueueing audio: %d\n", (int)result);
    return;
  }

  this->current_buffer++;
  if (this->current_buffer == NUM_BUFFERS)
    this->current_buffer = 0;

  printf("Init successful\n");

  update_sl_volume_no_lock(this);
}

void opensl_shutdown(struct splayer_audio_api* ctx) {
  struct android_audio_impl* this = (struct android_audio_impl*)ctx;

  SLresult result;

  if (!this->engineObject) {
    printf("Already shut down\n");
    return;
  }

  if (this->bqPlayerBufferQueue) {
    result = (*this->bqPlayerBufferQueue)->Clear(this->bqPlayerBufferQueue);
    if (SL_RESULT_SUCCESS != result) {
      printf("Clearing the buffer queue failed\n");
    }
  }

  if (this->bqPlayerPlay != NULL) {
    result = (*this->bqPlayerPlay)->SetPlayState(this->bqPlayerPlay, SL_PLAYSTATE_STOPPED);
    if (SL_RESULT_SUCCESS != result) {
      printf("SetPlayState failed\n");
    }
  }

  if (this->bqPlayerObject != NULL) {
    (*this->bqPlayerObject)->Destroy(this->bqPlayerObject);
    this->bqPlayerObject = NULL;
    this->bqPlayerPlay = NULL;
    this->bqPlayerBufferQueue = NULL;
    this->bqPlayerMuteSolo = NULL;
    this->bqPlayerVolume = NULL;
  }

  if (this->outputMixObject != NULL) {
    (*this->outputMixObject)->Destroy(this->outputMixObject);
    this->outputMixObject = NULL;
  }

  if (this->engineObject != NULL) {
    (*this->engineObject)->Destroy(this->engineObject);
    this->engineObject = NULL;
    this->engineEngine = NULL;
  }

  for (int i = 0; i < NUM_BUFFERS; i++) {
    free(this->buffer[i]);
    this->buffer[i] = NULL;
  }

  MUTEX_DESTORY(&this->ring_mutex);
  free(this->ring_buffer_data);
}

void opensl_flush(struct splayer_audio_api* ctx) {
  struct android_audio_impl* this = (struct android_audio_impl*)ctx;

  printf("opensl_flush\n");

  MUTEX_LOCK(&this->ring_mutex);
  circ_clear(&this->ring_buffer);

  if (this->bqPlayerBufferQueue) {
    (*this->bqPlayerBufferQueue)->Clear(this->bqPlayerBufferQueue);

    // Okay, nothing in the queue. Enqueue some silence to get things going again.
    this->current_buffer = 0;
    memset(this->buffer[0], 0, BUFFER_SIZE);
    SLresult result =
      (*this->bqPlayerBufferQueue)->Enqueue(this->bqPlayerBufferQueue, this->buffer[this->current_buffer], BUFFER_SIZE);
    if (SL_RESULT_SUCCESS != result) {
      printf("Error enquing after flush\n");
    }
    this->current_buffer++;
  }

  MUTEX_UNLOCK(&this->ring_mutex);
}

size_t opensl_play_audio(struct splayer_audio_api* ctx, const SAMPLE_TYPE* samples, size_t sample_count,
                         uint32_t* samples_buffered) {
  struct android_audio_impl* this = (struct android_audio_impl*)ctx;

  MUTEX_LOCK(&this->ring_mutex);
  uint32_t pushed_bytes = (uint32_t)circ_push(&this->ring_buffer, (char*)samples, sample_count * sizeof(SAMPLE_TYPE));
  *samples_buffered = (uint32_t)circ_used(&this->ring_buffer);
  MUTEX_UNLOCK(&this->ring_mutex);

  return pushed_bytes / sizeof(SAMPLE_TYPE);
}

void opensl_pause(struct splayer_audio_api* ctx, int pause) {
  struct android_audio_impl* this = (struct android_audio_impl*)ctx;

  printf("Toggling paused state from '%s' to '%s'\n", this->paused ? "paused" : "playing",
         pause ? "paused" : "playing");

  if (this->bqPlayerPlay) {
    (*this->bqPlayerPlay)->SetPlayState(this->bqPlayerPlay, pause ? SL_PLAYSTATE_PAUSED : SL_PLAYSTATE_PLAYING);
  }
  this->paused = pause;
}

void opensl_set_volume(struct splayer_audio_api* ctx, int vol) {
  struct android_audio_impl* this = (struct android_audio_impl*)ctx;

  float new_volume = (float)vol * (1.0f / 100.0f);
  if (new_volume > 1.0f) {
    new_volume = 1.0f;
  }
  if (new_volume < 0.0f) {
    new_volume = 0.0f;
  }
  if (this->volume != new_volume) {
    printf("Setting volume from %f to %f\n", this->volume, new_volume);
    this->volume = new_volume;
  }

  MUTEX_LOCK(&this->ring_mutex);
  update_sl_volume_no_lock(this);
  MUTEX_UNLOCK(&this->ring_mutex);
}

struct splayer_audio_api* audio_api_allocate() {
  struct android_audio_impl* api = malloc(sizeof(struct android_audio_impl));
  memset(api, 0, sizeof(struct android_audio_impl));

  splayer_audio_api_t e = {opensl_init,       opensl_shutdown, opensl_flush,
                           opensl_play_audio, opensl_pause,    opensl_set_volume};
  api->api = e;

  return &api->api;
}
