/**
 * @file audio_source.cpp
 * @brief See audio_source.h.
 */

#include "audio_source.h"
#include "tile.h"

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

/* In priority order when more than one source is playing. A source whose leaf is not registered
 * (e.g. Soundtrack in builds without it) reads as disabled and is skipped. */
static const AudioSource SOURCES[] = {
    {"/bluetooth"},
    {"/soundtrack"},
    {"/customChannels"},
};

const AudioSource *audio_source_active()
{
    const AudioSource *paused = nullptr;
    for (const AudioSource &s : SOURCES) {
        if (!tile_read_bool(s.path, "enable")) continue;
        if (tile_read_bool(s.path, "isPlaying")) return &s;
        if (!paused && !read_text(s.path, "trackTitle").empty()) paused = &s;
    }
    return paused;
}

void audio_source_toggle_play(const AudioSource *source)
{
    if (!source) return;
    tile_set_bool(source->path, "play", !tile_read_bool(source->path, "isPlaying"));
}
