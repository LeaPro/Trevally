/**
 * @file fader_units.h
 * @brief How amp fader levels (dB elements, see AmpFader.h) are shown and stepped, per the Appearance
 *        "volumeUnits" element: "db" (the default) or "percent".
 *
 * Percent is linear over the fader range, like the amps' own faderPct / faderZoneAdjustPct:
 * -80 dB = 0 %, 0 dB = 100 %, so 1 % = 0.8 dB (levels below -80, an output's footroom, read 0 %).
 * The elements, the amps and the API stay in dB; only the display and the encoder step change.
 */

#ifndef FADER_UNITS_H
#define FADER_UNITS_H

#include <string>

/** volumeUnits is "percent". */
bool fader_units_percent();

struct FaderDisplay
{
    std::string number;   /* "-20", "-8.1" (dB: one decimal when not whole), "75" (%: whole); "-" when unknown */
    std::string units;    /* "dB" | "%" */
    std::string min;      /* "-80" | "0" */
    std::string max;      /* "0" | "100" */
    bool operator==(const FaderDisplay &o) const
    {
        return number == o.number && units == o.units && min == o.min && max == o.max;
    }
    bool operator!=(const FaderDisplay &o) const { return !(*this == o); }
};

/** @p dB (NAN: unknown) in the current units. */
FaderDisplay fader_display(float dB);

/** The level of @p leafPath / @p element in dB; NAN while it doesn't exist or doesn't know its value. */
float fader_read_db(const std::string &leafPath, const std::string &element);

/** "-20 dB" / "75 %", or "-" while @p leafPath / @p element doesn't exist or doesn't know its value. */
std::string fader_level_text(const std::string &leafPath, const std::string &element);

/** Encoder step: rounds the level to a whole unit of the current units (dB or %), then moves it @p steps units. */
void fader_step(const std::string &leafPath, const std::string &element, int steps);

#endif /* FADER_UNITS_H */
