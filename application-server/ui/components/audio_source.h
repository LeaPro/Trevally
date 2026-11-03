/**
 * @file audio_source.h
 * @brief The audio sources the UI can show as "now playing", which one is active, and what state each is in.
 *
 * Every source is a leaf with the same element schema:
 *   enable             BoolControl   the source is selected (the UI keeps one enabled at a time, see
 *                                    audio_source_set_enabled(); a websocket client may enable several)
 *   isPlaying          BoolSensor    what the source is actually doing
 *   play               BoolControl   the request: true starts, false pauses (it may follow the source's state)
 *   trackTitle/trackArtist/trackAlbum   StringSensors, empty when there is no track
 *   trackDuration/trackPosition         UInt32Sensors, whole seconds; 0 when there is no track or the source does not say
 *   trackAlbumArt                      StringSensor, path of the album art file or empty
 *
 * Optional controls: a source supports a control (see the table in audio_source.cpp) when its leaf has the
 * control's element. A momentary control (next, previous) is set true and the source resets it; a toggle control
 * is one BoolControl the source keeps in sync with its actual state.
 *
 * To add a streaming provider, give its leaf that schema and add it, with a state function, to the table in
 * audio_source.cpp.
 */

#ifndef AUDIO_SOURCE_H
#define AUDIO_SOURCE_H

#include "../ui_design_tokens.h"

#include <string>
#include <vector>

/** How far a source is from playing: GRAY nothing paired or configured, YELLOW paired or configured but not
 *  connected, GREEN connected and live. */
enum AudioSourceState {
    AUDIO_SOURCE_GRAY,
    AUDIO_SOURCE_YELLOW,
    AUDIO_SOURCE_GREEN,
};

struct AudioSource {
    const char *path;                                /* leaf path, e.g. "/bluetooth" */
    const char *name;                                /* shown to the user, e.g. "Custom Channels" */
    bool        streaming;                           /* a streaming service a streaming tile can be bound to */
    AudioSourceState (*state)(const char *path);     /* reads the leaf's elements */
};

/** The source the UI should present: the first enabled source in table order. nullptr when none is enabled. */
const AudioSource *audio_source_active();

/** Reads an element of the active source's art/track schema; "" when there is no active source or it is empty. */
std::string audio_source_text(const AudioSource *source, const char *elementName);

/** Toggles play/pause of @p source. */
void audio_source_toggle_play(const AudioSource *source);

enum AudioControlBehavior {
    AUDIO_CONTROL_MOMENTARY,   /* pressing sets the element true; the source resets it */
    AUDIO_CONTROL_TOGGLE,      /* pressing flips the element, which holds the state */
};

/** Where the playback page puts a control. */
enum AudioControlCorner {
    AUDIO_CORNER_TOP_LEFT,
    AUDIO_CORNER_TOP_RIGHT,
    AUDIO_CORNER_BOTTOM_LEFT,
    AUDIO_CORNER_BOTTOM_RIGHT,
};

/** An optional control a source may have (play/pause is not one: every source has it). */
struct AudioControl {
    const char      *element;   /* BoolControl on the source's leaf, e.g. "next" */
    ui_icon_type_t   icon;
    AudioControlBehavior behavior;
    bool             onHome;    /* shown on the home panel too; every control is on the playback page */
    AudioControlCorner corner;  /* its place on the playback page */
};

/** The controls @p source supports, in a fixed order. The pointers stay valid. Empty for nullptr. */
std::vector<const AudioControl *> audio_source_controls(const AudioSource *source);

/** Presses @p control of @p source. No-op for nullptr or a control the source does not support. */
void audio_source_press(const AudioSource *source, const AudioControl *control);

/** The state of the source at @p path; GRAY for a path that is not a source or whose leaf is missing. */
AudioSourceState audio_source_state(const std::string &path);

/** The leaf path of the source a tile binding names, matched ignoring case ("Soundtrack" -> "/soundtrack",
 *  "CustomChannels" -> "/customChannels"); "" when the binding names no source. */
std::string audio_source_leaf_path(const std::string &binding);

/** The streaming services (not Bluetooth), in table order. The pointers stay valid. */
std::vector<const AudioSource *> audio_source_streaming_sources();

/** The UI's enable switch: turning a source on turns every other source off; turning it off only affects it.
 *  (Elements written over the websocket are not subject to this.) No-op for a path that is not a source. */
void audio_source_set_enabled(const std::string &path, bool enabled);

#endif /* AUDIO_SOURCE_H */
