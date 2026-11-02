/**
 * @file audio_source.h
 * @brief The audio sources the UI can show as "now playing", and which one is active.
 *
 * Every source is a leaf with the same element schema:
 *   enable             BoolControl   the source is selected (several may be enabled; only one should play)
 *   isPlaying          BoolSensor    what the source is actually doing
 *   play               BoolControl   the request: true starts, false pauses (it may follow the source's state)
 *   trackTitle/trackArtist/trackAlbum   StringSensors, empty when there is no track
 *   trackDuration/trackPosition         UInt32Sensors, whole seconds; 0 when there is no track or the source does not say
 *   trackArtThumbnail/trackArt          StringSensors, path of an image file or empty
 *
 * To add a streaming provider, give its leaf that schema and add it to the table in audio_source.cpp.
 */

#ifndef AUDIO_SOURCE_H
#define AUDIO_SOURCE_H

#include <string>

struct AudioSource {
    const char *path;   /* leaf path, e.g. "/bluetooth" */
};

/**
 * The source the UI should present: the first enabled source that is playing, otherwise the first
 * enabled source that has a track (paused). nullptr when there is none.
 */
const AudioSource *audio_source_active();

/** Reads an element of the active source's art/track schema; "" when there is no active source or it is empty. */
std::string audio_source_text(const AudioSource *source, const char *elementName);

/** Toggles play/pause of @p source. */
void audio_source_toggle_play(const AudioSource *source);

#endif /* AUDIO_SOURCE_H */
