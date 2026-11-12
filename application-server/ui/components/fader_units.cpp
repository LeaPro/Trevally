/**
 * @file fader_units.cpp
 * @brief See fader_units.h.
 */

#include "fader_units.h"
#include "tile.h"

#include <cmath>
#include <memory>

#include <object.h>
/* The device's root object (defined in main.cpp). */
extern std::shared_ptr<RootObject> root;

static const float minDb = -80.0f, maxDb = 0.0f;

bool fader_units_percent()
{
    auto element = std::dynamic_pointer_cast<StringControl>(root->elementFromPath("/appearance/volumeUnits"));
    return element && element->get() == "percent";
}

static float to_percent(float dB)
{
    return std::fmin(std::fmax((dB - minDb) * 100.0f / (maxDb - minDb), 0.0f), 100.0f);
}

static float from_percent(float percent)
{
    return minDb + percent * (maxDb - minDb) / 100.0f;
}

FaderDisplay fader_display(float dB)
{
    FaderDisplay d;
    if (fader_units_percent())
    {
        d.number = std::isnan(dB) ? "-" : std::to_string(std::lround(to_percent(dB)));
        d.units  = "%";
        d.min    = "0";
        d.max    = "100";
    }
    else
    {
        d.number = std::isnan(dB) ? "-" : tile_format_number(dB);
        d.units  = "dB";
        d.min    = tile_format_number(minDb);
        d.max    = tile_format_number(maxDb);
    }
    return d;
}

static std::shared_ptr<FloatElement> fader_element(const std::string &leafPath, const std::string &element)
{
    if (leafPath.empty() || element.empty()) return nullptr;
    return std::dynamic_pointer_cast<FloatElement>(root->elementFromPath(leafPath + "/" + element));
}

float fader_read_db(const std::string &leafPath, const std::string &element)
{
    auto f = fader_element(leafPath, element);
    return f ? f->get() : NAN;
}

std::string fader_level_text(const std::string &leafPath, const std::string &element)
{
    const FaderDisplay d = fader_display(fader_read_db(leafPath, element));
    return d.number == "-" ? d.number : d.number + " " + d.units;
}

void fader_step(const std::string &leafPath, const std::string &element, int steps)
{
    auto f = fader_element(leafPath, element);
    if (!f || steps == 0) return;
    const float dB = f->get();
    if (std::isnan(dB)) return;   /* nothing to step from until the amp reports */
    float target;
    if (fader_units_percent())
    {
        const float percent = std::fmin(std::fmax(std::round(to_percent(dB)) + (float)steps, 0.0f), 100.0f);
        target = from_percent(percent);
    }
    else
    {
        target = std::fmin(std::fmax(std::round(dB) + (float)steps, minDb), maxDb);
    }
    f->set(target);
}
