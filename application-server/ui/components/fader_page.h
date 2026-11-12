/**
 * @file fader_page.h
 * @brief Pages for amp faders (see AmpFader.h), shared by the output-volume and input tiles.
 *
 * Levels are shown, and stepped, in the Appearance "volumeUnits" (dB or %, see fader_units.h).
 *
 * Gauge page: header (back + title), the level gauge for one fader element, and optionally an outlined
 * button (e.g. "Subgroups", "Inputs"). The encoder changes the element by one dB / % per tick; pressing it is the
 * button. The value reads "-" while the element doesn't exist (yet) or doesn't know its value. A read-only page
 * (view-only permission, see permissions.h) shows a lock and ignores the encoder.
 *
 * Fader list page: header (back + title) and a picker of rows, each with a title, an optional subtitle and
 * an optional live level ("-6 dB" / "93 %"); choosing a row calls back with it. The rows are re-read every tick, so
 * the page follows config changes (order, names, rows coming and going), keeping the selected row selected.
 */

#ifndef FADER_PAGE_H
#define FADER_PAGE_H

#include "fader_units.h"

#include <functional>
#include <string>
#include <vector>

/** Pushes a gauge page for @p leafPath / @p element. An empty @p buttonLabel means no button. With
 *  @p showButton, the button (and the encoder press) only works while it returns true; it's checked every tick,
 *  so the button comes and goes with e.g. the config. */
void fader_page_open(const std::string &title, const std::string &leafPath, const std::string &element,
                     const std::string &buttonLabel = std::string(), std::function<void()> onButton = nullptr,
                     std::function<bool()> showButton = nullptr, bool readOnly = false);

struct FaderListRow
{
    std::string key;        /* identifies the row across refreshes (keeps the selection on it) */
    std::string title;
    std::string subtitle;   /* "" for none */
    std::string leafPath;   /* level shown on the right; "" for none */
    std::string element;
    bool operator==(const FaderListRow &o) const
    {
        return key == o.key && title == o.title && subtitle == o.subtitle && leafPath == o.leafPath && element == o.element;
    }
};

/** Pushes a list page. @p rows is called now and every tick; @p onChoose gets the chosen row. Nothing is
 *  pushed if @p rows returns none at first. */
void fader_list_open(const std::string &title, std::function<std::vector<FaderListRow>()> rows,
                     std::function<void(const FaderListRow &)> onChoose);

#endif /* FADER_PAGE_H */
