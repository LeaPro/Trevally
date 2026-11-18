#pragma once
#include "AmpFader.h"

/*
 * Live fader values for the /leaOutput config entries: one element per output uuid (the amps' "fader")
 * and one per subgroup uuid (the subchannels' "faderZoneAdjust"), e.g.
 *   /outputFaders/output-uuid-carrot      -> fader on every channel in config[...].amps
 *   /outputFaders/output-sub-uuid-kale    -> faderZoneAdjust on every channel in that subgroup's subchannels
 *                                            (the zone level: bumping it moves those channels' faders with it)
 * so an output-volume tile reads/sets /outputFaders/<its binding>, like the Bluetooth tile does
 * /bluetooth/volume. Elements come and go with the config (see LeaOutput::update()).
 * Each is a group control: it shows its loudest channel and keeps the channels' offsets (see AmpFaderElement).
 */
class OutputFaders final : public AmpFaderLeaf
{
public:
  OutputFaders(std::string path, Object *parent) : AmpFaderLeaf(path, parent) {}
  void apply(const json &outputConfig); // /leaOutput config
};
