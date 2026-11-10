#pragma once
#include <object.h>
#include "AmpLinks.h"
#include <chrono>
#include <optional>
#include <vector>

/*
 * Live fader values for the /leaOutput config entries: one element per output uuid (the amps' "fader")
 * and one per subgroup uuid (the subchannels' "faderZoneAdjust"), e.g.
 *   /outputFaders/output-uuid-carrot      -> fader on every channel in config[...].amps
 *   /outputFaders/output-sub-uuid-kale    -> faderZoneAdjust on every channel in that subgroup's subchannels
 * so an output-volume tile reads/sets /outputFaders/<its binding>, like the Bluetooth tile does
 * /bluetooth/volume. Elements come and go with the config (see LeaOutput::update()).
 *
 * Setting an element sends the value to all of its amp channels; the value shown is the first listed
 * channel's (the target is assumed to be the only thing adjusting them, so they all agree).
 * Not persisted: the amps hold the real values.
 */
class AmpFaderElement final : public FloatElement
{
public:
  struct Target
  {
    std::string mac;
    std::string channel; // "1", "2", ...
    bool operator==(const Target &other) const { return mac == other.mac && channel == other.channel; }
  };

  AmpFaderElement(std::string path, LeafObject *parent);
  float set(float value) override;
  float get() override; // NAN (null over the API, "-" in the UI) until the amp has reported a value
  void retarget(const std::string &ampElement, const std::vector<Target> &targets);
  void detach();
  void tick(); // from the leaf's update(): sends pending sets, settles echoes
  bool known() const { return ampValue.has_value(); }

private:
  void mirror(float value);
  static std::string ampLeafUrl(const Target &target) { return "/amp/channels/" + target.channel + "/output"; }

  std::string ampElement;
  std::vector<Target> targets;
  std::vector<AmpLinks::Handle> handles;
  std::optional<float> ampValue;      // last value reported by the first target
  bool sendPending = false;
  std::chrono::steady_clock::time_point holdUntil; // ignore amp echoes of our own recent sets until then
};

class OutputFaders final : public LeafObject
{
public:
  OutputFaders(std::string path, Object *parent);
  void initialize() override { LeafObject::initialize(); }
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;
  void apply(const json &outputConfig); // /leaOutput config
};
