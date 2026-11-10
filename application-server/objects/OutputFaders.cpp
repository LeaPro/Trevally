#include "OutputFaders.h"
#include <cmath>
#include <set>

using std::string;
using Clock = std::chrono::steady_clock;

// After a local set the amps echo each value back; ignore their reports this long so a quickly
// turned encoder doesn't jump back to older values.
static const auto echoHold = std::chrono::milliseconds(1000);

AmpFaderElement::AmpFaderElement(string path, LeafObject *parent)
  : FloatElement(path, parent, -80.0f/*min*/, 0.0f/*max*/, -80.0f/*default*/, 0.1f/*multipleOf*/, "dB"/*units*/)
{
  // a control, like FloatControl (whose set()/get() are final), but not persisted: the amps hold the real values
  control = true;
  persistent = false;
  accessControlled = false;
}

float AmpFaderElement::get()
{
  return known() ? FloatElement::get() : NAN;
}

float AmpFaderElement::set(float value)
{
  // until the amps report their level a relative change (the encoder) has nothing to start from
  if (!known() || std::isnan(value))
    return get();
  const float before = FloatElement::get();
  const float result = FloatElement::set(value);
  if (result != before)
  {
    sendPending = true;
    holdUntil = Clock::now() + echoHold;
  }
  return result;
}

void AmpFaderElement::mirror(float value)
{
  const bool first = !known();
  ampValue = value;
  if (first || (!sendPending && Clock::now() >= holdUntil))
    FloatElement::set(value);
  if (first)
    setModified(); // null -> value, even if it equals the stored default
}

void AmpFaderElement::tick()
{
  if (sendPending)
  {
    sendPending = false;
    holdUntil = Clock::now() + echoHold;
    const float value = FloatElement::get();
    for (const auto &target : targets)
      AmpLinks::instance().set(target.mac, ampLeafUrl(target), json{{ampElement, value}});
  }
  else if (ampValue && Clock::now() >= holdUntil && *ampValue != FloatElement::get())
  {
    FloatElement::set(*ampValue); // settle on what the amp reported last
  }
}

void AmpFaderElement::retarget(const string &ampElement, const std::vector<Target> &targets)
{
  if (ampElement == this->ampElement && targets == this->targets)
    return;
  handles.clear();
  this->ampElement = ampElement;
  this->targets = targets;
  sendPending = false;
  if (ampValue)
  {
    ampValue.reset();
    setModified();
  }
  if (targets.empty())
    return;

  const Target &shown = targets.front();
  handles.push_back(AmpLinks::instance().subscribe(shown.mac, ampLeafUrl(shown), [this](const json &elements) {
    auto it = elements.find(this->ampElement);
    if (it != elements.end() && it->is_number())
      mirror(it->get<float>());
  }));
  std::set<string> macs {AmpLinks::normalizeMac(shown.mac)};
  for (const auto &target : targets)
    if (macs.insert(AmpLinks::normalizeMac(target.mac)).second)
      handles.push_back(AmpLinks::instance().use(target.mac));
}

void AmpFaderElement::detach()
{
  retarget("", {});
}

OutputFaders::OutputFaders(string path, Object *parent) : LeafObject(path, parent)
{
}

void OutputFaders::update(bool sensorsOnly, bool refreshVolatileElements)
{
  for (auto &[name, element] : elements)
    if (auto level = std::dynamic_pointer_cast<AmpFaderElement>(element))
      level->tick();
}

/* {"mac": {"1": {}, "2": {}}, ...} -> targets, in config order */
static std::vector<AmpFaderElement::Target> targetsFrom(const json &channels)
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
        targets.push_back({amp.key(), channel});
    }
  }
  return targets;
}

void OutputFaders::apply(const json &outputConfig)
{
  struct Wanted { string ampElement; std::vector<AmpFaderElement::Target> targets; };
  std::vector<std::pair<string, Wanted>> wanted;
  if (outputConfig.is_object())
  {
    for (auto output = outputConfig.begin(); output != outputConfig.end(); ++output)
    {
      const json &entry = output.value();
      if (!entry.is_object())
        continue;
      wanted.push_back({output.key(), {"fader", targetsFrom(entry.value("amps", json::object()))}});
      const json subgroups = entry.value("subgroups", json::object());
      if (!subgroups.is_object())
        continue;
      for (auto sub = subgroups.begin(); sub != subgroups.end(); ++sub)
        if (sub.value().is_object())
          wanted.push_back({sub.key(), {"faderZoneAdjust", targetsFrom(sub.value().value("subchannels", json::object()))}});
    }
  }

  std::set<string> names;
  for (const auto &[name, w] : wanted)
  {
    if (name.empty() || name.find('/') != string::npos || !names.insert(name).second)
      continue;
    auto level = elements.count(name) ? std::dynamic_pointer_cast<AmpFaderElement>(elements[name]) : nullptr;
    if (!level)
    {
      level = std::make_shared<AmpFaderElement>(path + "/" + name, this);
      elements[name] = level;
      for (int client : subscribers)
        level->addSubscriber(client);
    }
    level->retarget(w.ampElement, w.targets);
  }

  // drop elements whose uuid left the config (the UI may still hold one; it just stops changing)
  std::vector<string> gone;
  for (auto &[name, element] : elements)
    if (!names.count(name))
      gone.push_back(name);
  for (const auto &name : gone)
  {
    if (auto level = std::dynamic_pointer_cast<AmpFaderElement>(elements[name]))
      level->detach();
    elements.erase(name);
  }
}
