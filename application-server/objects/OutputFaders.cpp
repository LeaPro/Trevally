#include "OutputFaders.h"

using std::string;

/* {"mac": {"1": {}, "2": {}}, ...} -> @p element on each channel's output leaf, in config order */
static std::vector<AmpFaderElement::Target> targetsFrom(const json &channels, const string &element)
{
  std::vector<AmpFaderElement::Target> targets;
  if (!channels.is_object())
    return targets;
  for (auto amp = channels.begin(); amp != channels.end(); ++amp)
  {
    if (!amp.value().is_object())
      continue;
    for (auto ch = amp.value().begin(); ch != amp.value().end(); ++ch)
    {
      const string &channel = ch.key();
      if (!channel.empty() && channel.find_first_not_of("0123456789") == string::npos)
      {
        // changed relatively like the wall controllers do; a fader's range follows its zone adjust (see AmpFader.h)
        AmpFaderElement::Target t {amp.key(), "/amp/channels/" + channel + "/output", element, true /*bump*/};
        if (element == "fader")
          t.topElement = "faderZoneAdjust";
        targets.push_back(t);
      }
    }
  }
  return targets;
}

void OutputFaders::apply(const json &outputConfig)
{
  Wanted wanted;
  if (outputConfig.is_object())
  {
    for (auto output = outputConfig.begin(); output != outputConfig.end(); ++output)
    {
      const json &entry = output.value();
      if (!entry.is_object())
        continue;
      wanted.push_back({output.key(), targetsFrom(entry.value("amps", json::object()), "fader")});
      const json subgroups = entry.value("subgroups", json::object());
      if (!subgroups.is_object())
        continue;
      for (auto sub = subgroups.begin(); sub != subgroups.end(); ++sub)
        if (sub.value().is_object())
          wanted.push_back({sub.key(), targetsFrom(sub.value().value("subchannels", json::object()), "faderZoneAdjust")});
    }
  }
  sync(wanted);
}
