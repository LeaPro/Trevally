/**
 * @file audio_source.cpp
 * @brief See audio_source.h.
 */

#include "audio_source.h"
#include "tile.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <object.h>
#include <string>

/* The device's root object (defined in main.cpp). */
extern std::shared_ptr<RootObject> root;

/* The element's string, "" when it is empty or does not exist (tile_read_string returns "-" then). */
static std::string read_text(const char *leafPath, const char *elementName)
{
    auto element = std::dynamic_pointer_cast<StringElement>(root->elementFromPath(std::string(leafPath) + "/" + elementName));
    return element ? element->get() : std::string();
}

std::string audio_source_text(const AudioSource *source, const char *elementName)
{
    return source ? read_text(source->path, elementName) : std::string();
}

static bool has_text(const char *leafPath, const char *elementName)
{
    return !read_text(leafPath, elementName).empty();
}

/* Bluetooth: red when the bluetooth service is stopped, green when a device is connected, yellow when devices are paired. */
static AudioSourceState bluetooth_state(const char *path)
{
    if (!tile_read_bool(path, "serviceRunning")) return AUDIO_SOURCE_RED;
    if (has_text(path, "connectedDevice")) return AUDIO_SOURCE_GREEN;
    if (has_text(path, "pairedDevices"))   return AUDIO_SOURCE_YELLOW;
    return AUDIO_SOURCE_GRAY;
}

/* Soundtrack: green needs the SDK running (so enabled), paired and the website reachable. Yellow is the persisted
 * "lastPaired" flag, which is known while the SDK is not running too. */
static AudioSourceState soundtrack_state(const char *path)
{
    if (tile_read_bool(path, "enable") && tile_read_bool(path, "sdkReady") &&
        read_text(path, "authStatus") == "paired" && tile_read_bool(path, "websiteReachable"))
        return AUDIO_SOURCE_GREEN;
    if (tile_read_bool(path, "lastPaired")) return AUDIO_SOURCE_YELLOW;
    return AUDIO_SOURCE_GRAY;
}

/* Custom Channels: green needs it enabled, authenticated (the object refreshes the tokens itself and drops them
 * when the provider rejects them) and the website reachable. Yellow is having tokens. Mid-pairing, before any
 * tokens, is gray. */
static AudioSourceState custom_channels_state(const char *path)
{
    const bool haveAccess = has_text(path, "accessToken");
    if (tile_read_bool(path, "enable") && haveAccess && read_text(path, "status") == "authenticated" &&
        tile_read_bool(path, "websiteReachable"))
        return AUDIO_SOURCE_GREEN;
    if (haveAccess || has_text(path, "refreshToken")) return AUDIO_SOURCE_YELLOW;
    return AUDIO_SOURCE_GRAY;
}

/* In priority order when more than one source is enabled. A source whose leaf is not registered
 * (e.g. Soundtrack in builds without it) reads as disabled and gray. */
static const AudioSource SOURCES[] = {
    {"/bluetooth",      "Bluetooth",       false, bluetooth_state},
    {"/soundtrack",     "Soundtrack.io",   true,  soundtrack_state},
    {"/customChannels", "Custom Channels", true,  custom_channels_state},
};

const AudioSource *audio_source_active()
{
    for (const AudioSource &s : SOURCES)
        if (tile_read_bool(s.path, "enable")) return &s;
    return nullptr;
}

static const AudioSource *find_source(const std::string &path)
{
    for (const AudioSource &s : SOURCES)
        if (path == s.path) return &s;
    return nullptr;
}

AudioSourceState audio_source_state(const std::string &path)
{
    const AudioSource *source = find_source(path);
    return source ? source->state(source->path) : AUDIO_SOURCE_GRAY;
}

static std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

bool audio_source_binding_is_default(const std::string &binding)
{
    return lower(binding) == lower(AUDIO_SOURCE_DEFAULT_BINDING);
}

std::string audio_source_leaf_path(const std::string &binding)
{
    if (audio_source_binding_is_default(binding)) {
        for (const AudioSource &s : SOURCES)
            if (s.streaming && tile_read_bool(s.path, "enable")) return s.path;
        return std::string();
    }
    const std::string wanted = lower(binding);
    for (const AudioSource &s : SOURCES)
        if (lower(s.path + 1) == wanted) return s.path;
    return std::string();
}

bool audio_source_is_streaming_binding(const std::string &binding)
{
    if (audio_source_binding_is_default(binding)) return true;
    const AudioSource *source = find_source(audio_source_leaf_path(binding));
    return source && source->streaming;
}

std::vector<const AudioSource *> audio_source_streaming_sources()
{
    std::vector<const AudioSource *> sources;
    for (const AudioSource &s : SOURCES)
        if (s.streaming) sources.push_back(&s);
    return sources;
}

void audio_source_set_enabled(const std::string &path, bool enabled)
{
    if (!find_source(path)) return;
    if (enabled) {
        for (const AudioSource &s : SOURCES)
            if (path != s.path) tile_set_bool(s.path, "enable", false);
    }
    tile_set_bool(path, "enable", enabled);
}

void audio_source_toggle_play(const AudioSource *source)
{
    if (!source) return;
    tile_set_bool(source->path, "play", !tile_read_bool(source->path, "isPlaying"));
}

/* Every optional control, in the order the UI lists them. */
static const AudioControl CONTROLS[] = {
    {"previous", UI_ICON_SKIP_PREVIOUS, AUDIO_CONTROL_MOMENTARY, true,  AUDIO_CORNER_TOP_LEFT},
    {"next",     UI_ICON_SKIP_NEXT,     AUDIO_CONTROL_MOMENTARY, true,  AUDIO_CORNER_TOP_RIGHT},
    {"shuffle",  UI_ICON_SHUFFLE,       AUDIO_CONTROL_TOGGLE,    false, AUDIO_CORNER_BOTTOM_LEFT},
    {"repeat",   UI_ICON_REPEAT,        AUDIO_CONTROL_TOGGLE,    false, AUDIO_CORNER_BOTTOM_RIGHT},
};

static bool supports(const AudioSource *source, const AudioControl &control)
{
    return root->elementFromPath(std::string(source->path) + "/" + control.element) != nullptr;
}

std::vector<const AudioControl *> audio_source_controls(const AudioSource *source)
{
    std::vector<const AudioControl *> controls;
    if (!source) return controls;
    for (const AudioControl &c : CONTROLS)
        if (supports(source, c)) controls.push_back(&c);
    return controls;
}

void audio_source_press(const AudioSource *source, const AudioControl *control)
{
    if (!source || !control || !supports(source, *control)) return;
    const bool value = control->behavior == AUDIO_CONTROL_TOGGLE ? !tile_read_bool(source->path, control->element) : true;
    tile_set_bool(source->path, control->element, value);
}
