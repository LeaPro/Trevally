#pragma once
#include "AmpFader.h"
#include <string>
#include <vector>

/*
 * The /leaInput config, read into what the input tile and its pages need, plus the live fader values for it.
 *
 * A config entry (an input tile's binding) holds states; a state sets the primary source of some amp channels
 * and says which sources get volume control:
 *   "states": { "<state uuid>": { "name": ..., "inputSelector": { "<mac>": { "<ch>": {
 *       "source": "Dante 1+2" | "Mixer" | ...,
 *       "volumeControl": { "<source>": true|false, ... } } } } } },
 *   "stateOrder": [ "<state uuid>", ... ]
 * A channel whose source isn't "Mixer" is volume controlled if volumeControl[source] is true; its level is the
 * channel's primaryFader. A "Mixer" channel is volume controlled if any of its mixer sources is; their levels
 * are the channel's <source>MixFader elements (analog1MixFader, dante3MixFader, ...).
 *
 * /inputFaders elements (all dB; a group shows its loudest member and keeps the members' offsets, see AmpFaderElement):
 *   <state>                         every volume-controlled fader of the state (the state's volume page)
 *   <state>|<mac>|<ch>              that channel's primaryFader (a row of the Inputs page)
 *   <state>|<mac>|<ch>|<mixFader>   that mixer source on that channel (a row of a channel's Input Mixer page)
 *   <state>|*|<mixFader>            that mixer source on every channel of the state that has it enabled
 *                                   (a row of the Input Mixer page of an all-Mixer state)
 * Elements come and go with the config (see LeaInput::update()).
 */
namespace InputStates
{
  struct MixSource
  {
    std::string source;   // "Analog 1"
    std::string element;  // "analog1MixFader"
  };

  struct Channel
  {
    std::string mac;
    std::string channel;  // "1", "2", ...
    std::string source;   // primary source to select, e.g. "Dante 1+2" or "Mixer"
    bool mixer = false;
    bool volume = false;  // non-mixer: volumeControl[source]; mixer: any mix source enabled
    std::vector<MixSource> mixSources; // mixer only: the volume-controlled ones, in config order
    std::string leafUrl() const { return "/amp/channels/" + channel + "/inputSelector"; }
  };

  struct State
  {
    std::string id;
    std::string name;
    std::vector<Channel> channels; // config order
    bool hasVolume() const;
    bool allMixer() const;         // every channel is Mixer (and there is at least one)
    std::vector<MixSource> mixSources() const; // volume-controlled mix sources across the channels, first occurrence order
  };

  // The states of /leaInput config[@p binding], in stateOrder (then any it leaves out, in config order).
  std::vector<State> read(const json &config, const std::string &binding);

  // "Analog 1" -> "analog1MixFader"; "" if it isn't a single mixable source.
  std::string mixFaderElement(const std::string &source);

  // Labels, with the names kept on the amps (/amp/deviceInfo deviceName, /amp/inputs/<analog|dante>/<n> name; set
  // from the amp's web UI and followed live; names still at the amp's defaults "DeviceName" / "InputName" count as unset):
  //   "Analog 3" -> "A3 - Bluray" ("Analog 3" while the input has no name);
  //   "Analog 1+2" -> "A1 + A2 - TV + Bluray" (an unnamed half reads as its "A2"; "Analog 1+2" if neither is named);
  //   anything else ("Mixer", "None") as is.
  std::string sourceLabel(const std::string &mac, const std::string &source);
  // "OS Simulator 01 - Ch 3"; the MAC while the amp has no name
  std::string channelLabel(const Channel &channel);

  // /inputFaders element names (see above)
  std::string stateFader(const State &state);
  std::string channelFader(const State &state, const Channel &channel);
  std::string mixFader(const State &state, const Channel &channel, const MixSource &source);
  std::string sharedMixFader(const State &state, const MixSource &source);
}

class InputFaders final : public AmpFaderLeaf
{
public:
  InputFaders(std::string path, Object *parent) : AmpFaderLeaf(path, parent) {}
  void apply(const json &inputConfig); // /leaInput config
private:
  std::vector<AmpLinks::Handle> connections; // every amp in the config, so states can be applied at any time
  std::vector<AmpLinks::Handle> nameSubscriptions; // the amp leaves the row labels read (see sourceLabel())
};
