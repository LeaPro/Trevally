#include <alsa/asoundlib.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

#include "splayerapi/splayer_audio_api-3.h"
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

/* ALSA context */
struct alsa_audio_impl {
  /* Context pointer */
  struct splayer_audio_api api;

  /* Master volume name */
  char volume_master_name[32];

  /* PCM volume name is not always present.
   * If present, both Master and PCM volume has to be set to 100 (or lower); otherwise audio won't be heard
   * */
  char volume_pcm_name[32];

  /* Alsa device name. eg. default or hw:0,0 */
  char alsa_device_name[32];

  /* Alsa mixer control name. eg. default or hw:0 */
  char alsa_ctl_name[32];

  /* Handle to ALSA device */
  snd_pcm_t* pcm_handle;

  /* Mixer handle */
  snd_mixer_t* mixer_handle;

  /* Volume PCM mixer handle if present */
  snd_mixer_elem_t* volume_pcm;

  /* Master volume mixer handle */
  snd_mixer_elem_t* volume_master;

  /* Initiated state */
  int inited;

  /* Paused state */
  int paused;

  /* snd_pcm_pause() function is not supported on all hardware */
  int pause_supported;

  /* Current volume setting */
  int volume;

  /* Channels and sample rate configured with */
  int sample_channels;
  int sample_rate;

  /* Alsa buffer size in frames */
  snd_pcm_uframes_t buffer_size;
};

// Forward declarations
static void alsa_mixer_open(struct alsa_audio_impl* this);
static void alsa_open(struct alsa_audio_impl* this);

static void alsa_close(struct alsa_audio_impl* this) {
  if (this->pcm_handle) {
    if (snd_pcm_close(this->pcm_handle) != 0) {
      printf("Couldn't close PCM handle\n");
    }
    this->pcm_handle = NULL;
  }
}

static void alsa_flush(struct splayer_audio_api* ctx) {
  struct alsa_audio_impl* this = (struct alsa_audio_impl*)ctx;
  int r;
  printf("alsa_audio_impl::audio_flush\n");
  // snd_pcm_drop() stops playing and clears all buffers in ALSA
  if (this->pcm_handle && (r = snd_pcm_drop(this->pcm_handle)) < 0) {
    printf("snd_pcm_drop failed: %d %s\n", r, snd_strerror(r));
  }
  // After calling drop, the PCM state will be set to SETUP and needs to be prepared again
  snd_pcm_prepare(this->pcm_handle);
}

static void alsa_init(struct splayer_audio_api* ctx, const int output_sample_channels, const int output_sample_rate) {
  struct alsa_audio_impl* this = (struct alsa_audio_impl*)ctx;
  if (this->inited) {
    printf("Audio already initialized.\n");
    return;
  }

  this->inited = 1;
  // Match the simple control name exposed by amixer on target.
  sprintf(this->volume_master_name, "NetAudio");
  this->volume_pcm_name[0] = '\0';

  // IMPORTANT: The device name dictates which card ALSA should use and the configuration as which alsa-lib will open
  // it with. If "default" is used as the ALSA device name, ALSA will first look for a default mapping in
  // /usr/share/cards/aliases.conf. If not found, it falls back to a generic default config, which may not match your
  // hardware and can cause issues with pause functionality or buffers not being flushed properly. For reliable mapping,
  // specify the hardware device directly (e.g., "hw:<card>,<device>"); available devices can be listed with "aplay -l".
  sprintf(this->alsa_device_name, "net_audio");

  // Use named control endpoint to avoid non-deterministic hw:<index> naming.
  sprintf(this->alsa_ctl_name, "net_audio");
  this->sample_channels = output_sample_channels;
  this->sample_rate = output_sample_rate;

  alsa_open(this);
  alsa_mixer_open(this);
}

static void alsa_mixer_open(struct alsa_audio_impl* this) {
  if (snd_mixer_open(&this->mixer_handle, 0) != 0) {
    printf("Couldn't open ALSA mixer.\n");
    return;
  }
  // Adds an ALSA mixer to the ALSA device, so that we can change volume
  if (snd_mixer_attach(this->mixer_handle, this->alsa_ctl_name) != 0) {
    printf("Couldn't attach to ALSA mixer.\n");
    return;
  }
  // Registers the mixers handle to the simple element (selem) class handlers
  if (snd_mixer_selem_register(this->mixer_handle, NULL, NULL) != 0) {
    printf("Couldn't register ALSA mixer.\n");
    return;
  }
  if (snd_mixer_load(this->mixer_handle) != 0) {
    printf("Couldn't load ALSA mixer.\n");
    return;
  }

  {
    snd_mixer_selem_id_t* selem_handle;

    // Allocates memory, assigns setting values and retrieves the pointer for this->volume_master
    snd_mixer_selem_id_alloca(&selem_handle);
    snd_mixer_selem_id_set_index(selem_handle, 0);
    snd_mixer_selem_id_set_name(selem_handle, this->volume_master_name);
    this->volume_master = snd_mixer_find_selem(this->mixer_handle, selem_handle);
  }

  if (this->volume_pcm_name[0] != '\0') {
    // Same as above for PCM if present
    snd_mixer_selem_id_t* selem_handle;
    snd_mixer_selem_id_alloca(&selem_handle);
    snd_mixer_selem_id_set_index(selem_handle, 0);
    snd_mixer_selem_id_set_name(selem_handle, this->volume_pcm_name);
    this->volume_pcm = snd_mixer_find_selem(this->mixer_handle, selem_handle);
  }

  if (this->volume_master == NULL) {
    printf("Couldn't find Master '%s' volume controls.\n", this->volume_master_name);
  }
  if (this->volume_pcm_name[0] != '\0' && this->volume_pcm == NULL) {
    printf("Couldn't find PCM '%s' volume control.\n", this->volume_pcm_name);
  }
}

static void alsa_open(struct alsa_audio_impl* this) {
  // PCM period size in frames
  snd_pcm_uframes_t period_size;
  printf("alsa_open samp: %d channels: %d\n", this->sample_rate, this->sample_channels);

  int r = 0;

  // Open ALSA device for playback in a non-blocking mode.
  // This means that snd_pcm_write() will return immediately, before playing any sound.
  // In blocking mode (0) snd_pcm_write() only returns after the PCM data has been played.
  if ((r = snd_pcm_open(&this->pcm_handle, this->alsa_device_name, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) < 0) {
    printf("snd_pcm_open failed: %d\n", r);
    alsa_close(this);
    return;
  }

  // Configure HW/SW params explicitly; this is more reliable than snd_pcm_set_params
  // on plugin chains such as softvol/plug/rate.
  snd_pcm_hw_params_t* hw = NULL;
  snd_pcm_hw_params_alloca(&hw);

  if ((r = snd_pcm_hw_params_any(this->pcm_handle, hw)) < 0) {
    printf("snd_pcm_hw_params_any failed: %d %s\n", r, snd_strerror(r));
    alsa_close(this);
    return;
  }

  if ((r = snd_pcm_hw_params_set_access(this->pcm_handle, hw, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    printf("snd_pcm_hw_params_set_access failed: %d %s\n", r, snd_strerror(r));
    alsa_close(this);
    return;
  }

  if ((r = snd_pcm_hw_params_set_format(this->pcm_handle, hw, SND_PCM_FORMAT_S16)) < 0) {
    printf("snd_pcm_hw_params_set_format failed: %d %s\n", r, snd_strerror(r));
    alsa_close(this);
    return;
  }

  unsigned int rate = (unsigned int)this->sample_rate;
  if ((r = snd_pcm_hw_params_set_rate_near(this->pcm_handle, hw, &rate, 0)) < 0) {
    printf("snd_pcm_hw_params_set_rate_near failed: %d %s\n", r, snd_strerror(r));
    alsa_close(this);
    return;
  }

  unsigned int channels = (unsigned int)this->sample_channels;
  if ((r = snd_pcm_hw_params_set_channels_near(this->pcm_handle, hw, &channels)) < 0) {
    printf("snd_pcm_hw_params_set_channels_near failed: %d %s\n", r, snd_strerror(r));
    alsa_close(this);
    return;
  }

  snd_pcm_uframes_t desired_period = 1024;
  if ((r = snd_pcm_hw_params_set_period_size_near(this->pcm_handle, hw, &desired_period, 0)) < 0) {
    printf("snd_pcm_hw_params_set_period_size_near failed: %d %s\n", r, snd_strerror(r));
    alsa_close(this);
    return;
  }

  snd_pcm_uframes_t desired_buffer = desired_period * 4;
  if ((r = snd_pcm_hw_params_set_buffer_size_near(this->pcm_handle, hw, &desired_buffer)) < 0) {
    printf("snd_pcm_hw_params_set_buffer_size_near failed: %d %s\n", r, snd_strerror(r));
    alsa_close(this);
    return;
  }

  if ((r = snd_pcm_hw_params(this->pcm_handle, hw)) < 0) {
    printf("snd_pcm_hw_params apply failed: %d %s\n", r, snd_strerror(r));
    alsa_close(this);
    return;
  }

  snd_pcm_sw_params_t* sw = NULL;
  snd_pcm_sw_params_alloca(&sw);
  if ((r = snd_pcm_sw_params_current(this->pcm_handle, sw)) < 0) {
    printf("snd_pcm_sw_params_current failed: %d %s\n", r, snd_strerror(r));
    alsa_close(this);
    return;
  }

  if ((r = snd_pcm_sw_params_set_start_threshold(this->pcm_handle, sw, desired_period)) < 0) {
    printf("snd_pcm_sw_params_set_start_threshold failed: %d %s\n", r, snd_strerror(r));
    alsa_close(this);
    return;
  }

  if ((r = snd_pcm_sw_params_set_avail_min(this->pcm_handle, sw, desired_period)) < 0) {
    printf("snd_pcm_sw_params_set_avail_min failed: %d %s\n", r, snd_strerror(r));
    alsa_close(this);
    return;
  }

  if ((r = snd_pcm_sw_params(this->pcm_handle, sw)) < 0) {
    printf("snd_pcm_sw_params apply failed: %d %s\n", r, snd_strerror(r));
    alsa_close(this);
    return;
  }

  printf("alsa_open configured: rate=%u channels=%u period=%lu buffer=%lu\n",
         rate, channels, desired_period, desired_buffer);

  // Get the size of the ALSA buffer
  if (snd_pcm_get_params(this->pcm_handle, &this->buffer_size, &period_size) == 0) {
    printf("Alsa initialized, buffer_size: %lu period_size: %lu\n", this->buffer_size, period_size);
  }

  // Check if the hardware supports pausing the stream with snd_pcm_pause(), if not we have to drop the buffers when
  // pausing Some plugins in ALSA do not support pausing, even if the hardware does.
  snd_pcm_hw_params_t* hw_params = NULL;
  snd_pcm_hw_params_alloca(&hw_params);
  snd_pcm_hw_params_current(this->pcm_handle, hw_params);

  this->pause_supported = snd_pcm_hw_params_can_pause(hw_params) == 1;

  // Prepare works like an init and has to be called before snd_pcm_writei()
  if ((r = snd_pcm_prepare(this->pcm_handle)) < 0) {
    printf("snd_pcm_prepare() failed: %d\n", r);
  }
}

static void alsa_pause(struct splayer_audio_api* ctx, int pause) {
  struct alsa_audio_impl* this = (struct alsa_audio_impl*)ctx;
  printf("alsa_audio_impl::audio_pause: %d\n", pause);
  if (this->paused == pause) {
    return;
  }

  if (!this->pcm_handle) {
    printf("No device opened, can not pause/unpause\n");
    return;
  }

  int err;
  if (this->pause_supported) {
    // Audio buffers will be kept and ALSA is paused. When unpaused, data in the buffer continues to play.
    // NOTE: snd_pcm_pause() is not supported on all hardware but is the preferred way of pausing if possible.
    if ((err = snd_pcm_pause(this->pcm_handle, pause)) == 0) {
      this->paused = pause;
      return;
    }
    printf("snd_pcm_pause failed: %d %s\n", err, snd_strerror(err));
  }

  // Fallback when snd_pcm_pause() isn't supported or failed:
  if (pause) {
    // Dropping the buffers and stopping the stream immediately, this method is preferred to minimize latency when
    // pausing. NOTE: this can cause a slight jump in audio depending on how many samples that are buffered
    printf("Dropping buffered samples to pause\n");
    if ((err = snd_pcm_drop(this->pcm_handle)) < 0) {
      // If dropping fails, try draining the buffers instead this can cause a noticeable delay before the pause
      // completes.
      printf("snd_pcm_drop failed: %d %s\n, draining buffers to pause", err, snd_strerror(err));
      err = snd_pcm_drain(this->pcm_handle);
      if (err == -EAGAIN) {
        // In non-blocking mode, we initiate buffer draining but don't wait for completion.
        // alsa_audio_data() will skip writing new samples during this state.
        this->paused = 1;
        return;
      }
      if (err < 0) {
        printf("snd_pcm_drain failed: %d %s\n, playback continues", err, snd_strerror(err));
        this->paused = 0;
        return;
      }
    }
    this->paused = 1;
  } else {
    // We are in a paused state and want to resume
    printf("Resuming from a paused state, snd_pcm_prepare\n");
    if ((err = snd_pcm_prepare(this->pcm_handle)) < 0) {
      printf("snd_pcm_prepare() failed: %d %s\n", err, snd_strerror(err));
      return;
    }
    this->paused = 0;
  }
}

// Helper function to return the total number of unplayed samples currently in the ALSA buffer.
static uint32_t get_samples_buffered(const struct splayer_audio_api* ctx) {
  const struct alsa_audio_impl* this = (struct alsa_audio_impl*)ctx;
  // If we are paused using the drop or drain method, the state will be set to SETUP. Calling snd_pcm_avail() in this
  // state is invalid and returns an error. Therefore, we report 0 samples buffered as buffer is empty.
  if (this->paused && snd_pcm_state(this->pcm_handle) == SND_PCM_STATE_SETUP) {
    return 0;
  }

  // Get available frames via a direct (and potentially expensive) hardware query.
  // For a lighter-weight check, use snd_pcm_avail_update().
  // NOTE: snd_pcm_delay() measures latency, not available buffer space
  const snd_pcm_sframes_t available_frames = snd_pcm_avail(this->pcm_handle);
  if (available_frames < 0) {
    printf("ERROR, available frames negative error code: %s\n", snd_strerror((int)available_frames));
    return 0;
  }
  return (uint32_t)((this->buffer_size - available_frames) * this->sample_channels);
}

static size_t alsa_audio_data(struct splayer_audio_api* ctx, const int16_t* samples, size_t sample_count,
                              uint32_t* samples_buffered) {
  struct alsa_audio_impl* this = (struct alsa_audio_impl*)ctx;

  if (!this->pcm_handle) {
    alsa_open(this);
    if (!this->pcm_handle) {
      printf("ERROR: Failed to open ALSA device '%s'\n", this->alsa_device_name);
      *samples_buffered = 0;
      return 0;
    }
  }

  // For a GUI application to display a correct progressbar and crossfade to work, we need to report how much data we
  // have sent to ALSA and how much data still resides in unplayed buffers. Since we might return early if error
  // occurs, we need to report how much data is buffered in ALSA before and after we write frames to ALSA.
  *samples_buffered = get_samples_buffered(ctx);

  // If paused or actively draining the buffers, we do not write any samples to ALSA
  if (this->paused || snd_pcm_state(this->pcm_handle) == SND_PCM_STATE_DRAINING) {
    return 0;
  }

  if (sample_count == 0) {
    // Nothing to write to ALSA
    return 0;
  }

  if (this->sample_channels == 0) {
    printf("ERROR: sample_channels is configured to be 0\n");
    return 0;
  }
  // Convert sample_count to number of frames
  // A frame consists of one sample per channel (e.g., for stereo, 2 samples per frame)
  const snd_pcm_uframes_t num_frames = sample_count / this->sample_channels;

  // Returns the number of frames written to ALSA, or a negative error code
  snd_pcm_sframes_t frames_written = snd_pcm_writei(this->pcm_handle, samples, num_frames);

  // If an error occurred during writing, try to handle it and recover if possible
  if (frames_written < 0) {
    snd_pcm_sframes_t err = frames_written;

    switch (err) {
    // In non-blocking mode, this means that the device is not ready for data and we should try again later
    case -EAGAIN:
      return 0;

    // Error occurs when the application does not feed new data in time to alsa-lib.
    case -EPIPE:
      printf("ERROR: EPIPE, an underrun occurred, trying to recover.\n");
      err = snd_pcm_recover(this->pcm_handle, (int)err, 1);
      if (err < 0) {
        // If it cannot handle the error, it will return the original error code
        printf("Failed to recover from underrun: %ld (%s)\n", err, snd_strerror((int)err));
        alsa_close(this);
        return 0;
      }
      break;

    // System has suspended drivers and application needs to wait in loop
    case -ESTRPIPE:
      printf("Error: %ld (%s), System has suspended drivers.\n", err, snd_strerror((int)err));
      err = snd_pcm_resume(this->pcm_handle);
      if (err == -EAGAIN) {
        // If resume returns -EAGAIN, we report 0 frames written and try again
        return 0;
      }
      // If resume fails for other reasons, prepare the PCM handle again
      if (err < 0) {
        if ((err = snd_pcm_prepare(this->pcm_handle)) < 0) {
          printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror((int)err));
          alsa_close(this);
          return 0;
        }
      }
      break;

    // The PCM device is in a invalid state and needs to be prepared
    case -EBADFD:
      printf("Error: %ld (%s), PCM are in bad state, trying to prepare.\n", err, snd_strerror((int)err));
      if (snd_pcm_state(this->pcm_handle) < SND_PCM_STATE_PREPARED) {
        if ((err = snd_pcm_prepare(this->pcm_handle)) < 0) {
          printf("Can't recover from a bad state, prepare failed: %s\n", snd_strerror((int)err));
          alsa_close(this);
          return 0;
        }
      }
      break;

    default:
      printf("Unhandled error: %ld (%s), closing ALSA.\n", err, snd_strerror((int)err));
      alsa_close(this);
      return 0;
    }

    // After recovering from an error, try to write the frames again
    frames_written = snd_pcm_writei(this->pcm_handle, samples, num_frames);

    if (frames_written < 0) {
      printf("ERROR: Failed to write to ALSA device '%s': %ld (%s)\n", this->alsa_device_name, frames_written,
             snd_strerror((int)frames_written));
      alsa_close(this);
      return 0;
    }
  }

  // Update the number of samples currently buffered in ALSA after writing new frames.
  *samples_buffered = get_samples_buffered(ctx);

  // Returns the number of samples written to ALSA.
  return frames_written * this->sample_channels;
}

static void alsa_set_volume(struct splayer_audio_api* ctx, const int volume) {
  struct alsa_audio_impl* this = (struct alsa_audio_impl*)ctx;
  // Volume is given as a linear value, but no perceptual scaling is applied. As a result, the perceived loudness
  // between volume steps may not be uniform.
  if (this->volume == volume) {
    return;
  }

  size_t i;
  // Array of mixer elements to apply volume changes to.
  // PCM volume control may not be available on all systems. In some cases, dividing the volume between PCM and Master
  // can produce better results (e.g., if the dynamic range of one control is too small), but it's up to the integrator
  // to decide how volume control is handled (e.g., via alsamixer).
  snd_mixer_elem_t* mixers[] = {
    this->volume_pcm,
    this->volume_master,
  };
  // Iterates over the two defined mixers
  for (i = 0; i < ARRAY_SIZE(mixers); i++) {
    if (mixers[i] != NULL) {
      int r;
      long min = 0, max = 0;
      // Get the range of min and max volumes. Usually from 0 to 100, but also from -100 to +100
      if ((r = snd_mixer_selem_get_playback_volume_range(mixers[i], &min, &max)) < 0) {
        printf("snd_mixer_selem_get_playback_volume_range failed: %d\n", r);
      } else {
        // Calculates the correct volume based on min and max
        long new_vol = volume * (max - min) / 100 + min;
        int ch;
        for (ch = 0; ch <= SND_MIXER_SCHN_LAST; ++ch) {
          snd_mixer_selem_channel_id_t channel = (snd_mixer_selem_channel_id_t)ch;
          if (!snd_mixer_selem_has_playback_channel(mixers[i], channel)) {
            continue;
          }

          r = snd_mixer_selem_set_playback_volume(mixers[i], channel, new_vol);
          if (r < 0) {
            printf("snd_mixer_selem_set_playback_volume ch=%d %ld failed: %d (%s)\n",
                   ch, new_vol, r, snd_strerror(r));
          }
        }
      }
    }
  }
  this->volume = volume;
}

static void alsa_shutdown(struct splayer_audio_api* ctx) {
  struct alsa_audio_impl* this = (struct alsa_audio_impl*)ctx;
  // Cleanup mixer resources
  if (this->mixer_handle) {
    const int err = snd_mixer_detach(this->mixer_handle, this->alsa_ctl_name);
    if (err < 0) {
      printf("Failed to detach mixer from ALSA device: %s\n", snd_strerror(err));
    }

    snd_mixer_free(this->mixer_handle);
    if (snd_mixer_close(this->mixer_handle) != 0) {
      printf("Couldn't close ALSA mixer\n");
    }

    this->volume_pcm = NULL;
    this->volume_master = NULL;
    this->mixer_handle = NULL;
  }

  alsa_close(this);
  this->inited = 0;
}

struct splayer_audio_api* audio_api_allocate() {
  /* Creates and sets the callback api. audio_api_allocate() is assigned to config.audio_api_callbacks.
   * Here we use a Object Oriented C trick, where the first object in alsa_audio_impl is splayer_audio_api.
   * This way the same object can be cast to splayer_audio_api() which in this case would be the base class
   * or an inherited pure virtual interface in C++. alsa_audio_impl is then the derived and extended class.
   * This is exactly the way vtable are built in C++, but here we do it manually instead of the C++ compiler
   * doing it for us.
   * */
  struct alsa_audio_impl* api = malloc(sizeof(struct alsa_audio_impl));
  memset(api, 0, sizeof(struct alsa_audio_impl));
  splayer_audio_api_t e = {alsa_init, alsa_shutdown, alsa_flush, alsa_audio_data, alsa_pause, alsa_set_volume};
  api->api = e;

  return &api->api;
}
