#include <AudioToolbox/AudioQueue.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "splayerapi/splayer_audio_api-3.h"

#define NUM_BUFFERS 3  
#define SAMPLE_TYPE int16_t // 16bit signed samples

struct darwin_audio_impl {
  /*Context pointer*/
  struct splayer_audio_api api;

  AudioStreamBasicDescription desc;
  AudioQueueRef queue;
  AudioQueueBufferRef buffers[NUM_BUFFERS];

  /* Bytes written into each buffer so far */
  size_t buffers_size[NUM_BUFFERS];

  /* Whether each buffer has been enqueued to AudioQueue (1 = enqueued, 0 = free) */
  int buffers_enqueued[NUM_BUFFERS];

  /* Total byte capacity per buffer: rate * channels * sizeof(sample) = 1 second of audio */
  size_t buffer_size;

  /* Flag to see if the playback device as been initialised */
  int inited;
  int started;
  int paused;

  int buffer_index;

  pthread_mutex_t mutex;
};

// Forward declarations
static void audio_callback_wrapper(void* ctx, AudioQueueRef aq, AudioQueueBufferRef aq_buf);

static void darwin_flush(struct splayer_audio_api* ctx) {
  struct darwin_audio_impl* this = (struct darwin_audio_impl*)ctx;
  // Lock the mutex to prevent audio_callback_wrapper from modifying the buffer state while we reset it
  pthread_mutex_lock(&this->mutex);
  for (int i = 0; i < NUM_BUFFERS; i++) {
    this->buffers_size[i] = 0;
    this->buffers_enqueued[i] = 0;
  }
  this->buffer_index = 0;
  pthread_mutex_unlock(&this->mutex);
  OSStatus res = AudioQueueReset(this->queue);
  if (res != noErr) {
    printf("Error: could not reset audio properly\n");
  } else {
    printf("Flushing all queue data\n");
  }
}

static void darwin_init(struct splayer_audio_api* ctx, const int output_sample_channels, const int output_sample_rate) {

  struct darwin_audio_impl* this = (struct darwin_audio_impl*)ctx;

  if (this->inited) {
    printf("Audio already initialised.\n");
    return;
  }

  pthread_mutex_init(&this->mutex, NULL);

  size_t i = 0;
  OSStatus res;

  this->desc.mSampleRate = output_sample_rate;
  this->desc.mFormatID = kAudioFormatLinearPCM;
  this->desc.mFormatFlags =
    kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked | kAudioFormatFlagsNativeEndian;
  this->desc.mBitsPerChannel = 8 * sizeof(SAMPLE_TYPE);
  this->desc.mChannelsPerFrame = 2;
  this->desc.mBytesPerFrame = sizeof(SAMPLE_TYPE) * output_sample_channels;
  this->desc.mFramesPerPacket = 1;
  this->desc.mBytesPerPacket = this->desc.mBytesPerFrame * this->desc.mFramesPerPacket;
  this->desc.mReserved = 0;

  this->inited = 0;
  this->started = 0;
  this->buffer_index = 0;

  this->buffer_size = output_sample_rate * this->desc.mBytesPerPacket;
  printf("Right before audi queue creation \n");
  res = AudioQueueNewOutput(&this->desc, audio_callback_wrapper, this, NULL, NULL, 0, &this->queue);
  if (res != noErr) {
    this->inited = 0;
    printf("Failed to initialise Darwin audio\n");
    return;
  } else {
    printf("Successful creation of Audio Queue output %d \n", res);
  }

  // split buffer_size into number of buffers. Buffer
  for (i = 0; i < NUM_BUFFERS; ++i) {
    res = AudioQueueAllocateBuffer(this->queue, (uint32_t)this->buffer_size, &this->buffers[i]);
    if (res != noErr) {
      this->inited = 0;
      printf("Failed Allocate buffer %zu \n", i);
      return;
    } else {
      printf("Successful creation of Audio Queue buffer %zu \n", i);
    }
  }

  this->inited = 1;
}

static void darwin_pause(struct splayer_audio_api* ctx, int pause) {
  struct darwin_audio_impl* this = (struct darwin_audio_impl*)ctx;
  if (!this->inited) {
    printf("Trying to pause without proper initialisation\n");
    return;
  }

  OSStatus res;

  if (pause) {
    res = AudioQueuePause(this->queue);
    if (res != noErr) {
      printf("Failed to pause playback\n");
    } else {
      printf("Pausing playback\n");
      this->paused = 1;
    }
  } else {
    res = AudioQueueStart(this->queue, NULL);
    if (res != noErr) {
      printf("Failed to resume playback\n");
    } else {
      this->paused = 0;
      printf("Resuming playback\n");
    }
  }
}


// Helper function to convert buffered samples in bytes to sample count (one sample = one int16_t value)
static uint32_t get_samples_buffered_unsafe(struct darwin_audio_impl* this) {
    int bytes_buffered = 0;
    for (int i = 0; i < NUM_BUFFERS; i++) {
        bytes_buffered += this->buffers_size[i];
    }

    return (uint32_t)(bytes_buffered / sizeof(SAMPLE_TYPE));  
}

static size_t darwin_play_audio(struct splayer_audio_api* ctx, const int16_t* samples, size_t sample_count,
                                uint32_t* samples_buffered) {
  struct darwin_audio_impl* this = (struct darwin_audio_impl*)ctx;

  if (this->inited == 0) {
    printf("Trying to play audio without proper initialisation\n");
    return 0;
  }

  pthread_mutex_lock(&this->mutex);

  if (sample_count == 0) {
    *samples_buffered = get_samples_buffered_unsafe(this);
    pthread_mutex_unlock(&this->mutex);
    return 0;
  }

  size_t bytes_to_write = sizeof(SAMPLE_TYPE) * sample_count;
  size_t buffer_available = this->buffer_size - this->buffers_size[this->buffer_index];

  // Limit the amount of bytes to the buffer size
  if (bytes_to_write > buffer_available) {
    bytes_to_write = buffer_available;
  }

  AudioQueueBufferRef dest_buffer = (this->buffers[this->buffer_index]);
  // Copy data into buffer
  if (bytes_to_write > 0) {
    //  printf("copy data: buffers_size: %zu bytes_to_write: %zu\n", this->buffers_size[this->buffer_index],
    //  bytes_to_write);
    memcpy(dest_buffer->mAudioData + this->buffers_size[this->buffer_index], samples, bytes_to_write);
    this->buffers_size[this->buffer_index] += bytes_to_write;
    dest_buffer->mAudioDataByteSize = (uint32_t)this->buffers_size[this->buffer_index];
  }

  // If the buffer is filled, enqueue it
  if (this->buffers_size[this->buffer_index] == this->buffer_size && this->buffers_enqueued[this->buffer_index] == 0 &&
      this->paused == 0) {
    // printf("Enqueue %zu bytes\n", this->buffers_size[this->buffer_index]);
    OSStatus res = AudioQueueEnqueueBuffer(this->queue, dest_buffer, 0, NULL);

    if (res != noErr) {
      printf("Failed to enqueue buffer: %d \n", res);
    } else {
      this->buffers_enqueued[this->buffer_index] = 1;
      this->buffer_index = (this->buffer_index + 1) % NUM_BUFFERS;
    }
  }

  // Sum up how many samples we have buffered
  *samples_buffered = get_samples_buffered_unsafe(this);
  pthread_mutex_unlock(&this->mutex);

  // Start the playback if it hasn't been started
  if (this->started == 0) {
    printf("Start audio queue\n");
    this->started = 1;
    OSStatus res = AudioQueueStart(this->queue, NULL);
    if (res != noErr) {
      printf("Something went wrong with Starting the AudioQueue, abort future enqueue \n");
      this->inited = 0;
      this->started = 0;
    }
  }

  return bytes_to_write / sizeof(SAMPLE_TYPE);
}

static void darwin_set_volume(struct splayer_audio_api* ctx, const int volume) {
  struct darwin_audio_impl* this = (struct darwin_audio_impl*)ctx;
  const float vol_norm = (float)volume * (1.0f / 100.0f);
  OSStatus res = AudioQueueSetParameter(this->queue, kAudioQueueParam_Volume, vol_norm);
  if (res != noErr) {
    printf("Error: Could not set volume\n");
  }
}

static void darwin_shutdown(struct splayer_audio_api* ctx) {
  struct darwin_audio_impl* this = (struct darwin_audio_impl*)ctx;
  AudioQueueStop(this->queue, false);
  AudioQueueDispose(this->queue, false);
  this->inited = 0;
  pthread_mutex_destroy(&this->mutex);
}

struct splayer_audio_api* audio_api_allocate() {
  struct darwin_audio_impl* api = malloc(sizeof(struct darwin_audio_impl));
  memset(api, 0, sizeof(struct darwin_audio_impl));
  splayer_audio_api_t e = {darwin_init,       darwin_shutdown, darwin_flush,
                           darwin_play_audio, darwin_pause,    darwin_set_volume};
  api->api = e;

  return &api->api;
}

static void audio_callback_wrapper(void* ctx, AudioQueueRef aq, AudioQueueBufferRef aq_buf) {
  struct darwin_audio_impl* this = (struct darwin_audio_impl*)ctx;
  for (int i = 0; i < NUM_BUFFERS; i++) {
    if (this->buffers[i] == aq_buf) {
      // printf("Buffer %d is now free\n", i);
      this->buffers_size[i] = 0;
      this->buffers_enqueued[i] = 0;
    }
  }
}
