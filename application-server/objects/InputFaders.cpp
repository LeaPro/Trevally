#include "InputFaders.h"
#include <algorithm>
#include <cctype>
#include <set>

using std::string;

namespace InputStates
{

bool State::hasVolume() const
{
  return std::any_of(channels.begin(), channels.end(), [](const Channel &c) { return c.volume; });
}

bool State::allMixer() const
{
  return !channels.empty() && std::all_of(channels.begin(), channels.end(), [](const Channel &c) { return c.mixer; });
}

std::vector<MixSource> State::mixSources() const
{
  std::vector<MixSource> result;
  for (const Channel &c : channels)
    for (const MixSource &s : c.mixSources)
      if (std::none_of(result.begin(), result.end(), [&](const MixSource &r) { return r.element == s.element; }))
        result.push_back(s);
  return result;
}

string mixFaderElement(const string &source)
{
  // "Analog 3" / "Dante 2": a single input (pairs like "Dante 1+2" can't be mixed)
  const auto space = source.find(' ');
  if (space == string::npos)
    return "";
  const string kind = source.substr(0, space), number = source.substr(space + 1);
  if (number.empty() || number.find_first_not_of("0123456789") != string::npos)
    return "";
  if (kind == "Analog")
    return "analog" + number + "MixFader";
  if (kind == "Dante")
    return "dante" + number + "MixFader";
  return "";
}

/* @p obj[@p key] if it is a string, else "" (the config is written by clients; a wrong type mustn't throw). */
static string stringAt(const json &obj, const char *key)
{
  auto it = obj.find(key);
  return (it != obj.end() && it->is_string()) ? it->get<string>() : string();
}

static Channel readChannel(const string &mac, const string &channel, const json &entry)
{
  Channel c;
  c.mac = mac;
  c.channel = channel;
  c.source = stringAt(entry, "source");
  c.mixer = c.source == "Mixer";
  const json volumeControl = entry.value("volumeControl", json::object());
  if (!volumeControl.is_object())
    return c;
  if (c.mixer)
  {
    for (auto it = volumeControl.begin(); it != volumeControl.end(); ++it)
    {
      const string element = mixFaderElement(it.key());
      if (it.value() == true && !element.empty())
        c.mixSources.push_back({it.key(), element});
    }
    c.volume = !c.mixSources.empty();
  }
  else
  {
    auto it = volumeControl.find(c.source);
    c.volume = !c.source.empty() && it != volumeControl.end() && *it == true;
  }
  return c;
}

std::vector<State> read(const json &config, const string &binding)
{
  std::vector<State> result;
  if (!config.is_object())
    return result;
  auto entry = config.find(binding);
  if (entry == config.end() || !entry->is_object())
    return result;
  auto states = entry->find("states");
  if (states == entry->end() || !states->is_object())
    return result;

  std::vector<string> ids;
  auto order = entry->find("stateOrder");
  if (order != entry->end() && order->is_array())
    for (const auto &id : *order)
      if (id.is_string() && states->contains(id.get<string>()) &&
          std::find(ids.begin(), ids.end(), id.get<string>()) == ids.end())
        ids.push_back(id.get<string>());
  for (auto it = states->begin(); it != states->end(); ++it)
    if (std::find(ids.begin(), ids.end(), it.key()) == ids.end())
      ids.push_back(it.key());

  for (const string &id : ids)
  {
    const json &s = (*states)[id];
    if (!s.is_object())
      continue;
    State state;
    state.id = id;
    state.name = stringAt(s, "name");
    if (state.name.empty())
      state.name = "State";
    const json selector = s.value("inputSelector", json::object());
    if (selector.is_object())
      for (auto amp = selector.begin(); amp != selector.end(); ++amp)
        if (amp.value().is_object())
          for (auto ch = amp.value().begin(); ch != amp.value().end(); ++ch)
            if (!ch.key().empty() && ch.key().find_first_not_of("0123456789") == string::npos && ch.value().is_object())
              state.channels.push_back(readChannel(amp.key(), ch.key(), ch.value()));
    result.push_back(std::move(state));
  }
  return result;
}

/* "Analog 1+2" -> {"analog", "A", {1, 2}}; false for anything that isn't analog / dante inputs */
struct Inputs
{
  string kind;   // "analog" | "dante" (the amp's /amp/inputs/<kind>/<n>)
  string abbrev; // "A" | "D"
  std::vector<int> numbers;
};

static bool parseInputs(const string &source, Inputs &out)
{
  const auto space = source.find(' ');
  if (space == string::npos)
    return false;
  const string kind = source.substr(0, space);
  if (kind == "Analog")     out = {"analog", "A", {}};
  else if (kind == "Dante") out = {"dante", "D", {}};
  else return false;
  string rest = source.substr(space + 1);
  size_t start = 0;
  while (start <= rest.size())
  {
    size_t end = rest.find('+', start);
    if (end == string::npos)
      end = rest.size();
    const string number = rest.substr(start, end - start);
    if (number.empty() || number.find_first_not_of("0123456789") != string::npos)
      return false;
    out.numbers.push_back(std::stoi(number));
    start = end + 1;
  }
  return !out.numbers.empty();
}

static const string deviceLeaf = "/amp/deviceInfo";
static string inputLeaf(const string &kind, int number) { return "/amp/inputs/" + kind + "/" + std::to_string(number); }

/* A name element of a subscribed amp leaf; "" while unknown or still at the amp's default. */
static string ampName(const string &mac, const string &leafUrl, const char *element, const char *unset)
{
  const json value = AmpLinks::instance().value(mac, leafUrl, element);
  return (value.is_string() && value.get<string>() != unset) ? value.get<string>() : string();
}

string sourceLabel(const string &mac, const string &source)
{
  Inputs inputs;
  if (!parseInputs(source, inputs))
    return source;
  string abbrevs, names;
  bool anyNamed = false;
  for (size_t i = 0; i < inputs.numbers.size(); i++)
  {
    const string abbrev = inputs.abbrev + std::to_string(inputs.numbers[i]);
    const string name = ampName(mac, inputLeaf(inputs.kind, inputs.numbers[i]), "name", "InputName");
    anyNamed = anyNamed || !name.empty();
    abbrevs += (i ? " + " : "") + abbrev;
    names += (i ? " + " : "") + (name.empty() ? abbrev : name);
  }
  return anyNamed ? abbrevs + " - " + names : source;
}

string channelLabel(const Channel &c)
{
  const string device = ampName(c.mac, deviceLeaf, "deviceName", "DeviceName");
  return (device.empty() ? c.mac : device) + " - Ch " + c.channel;
}

/* The amp leaves sourceLabel() / channelLabel() read for @p state's volume-controlled rows. */
static void nameLeaves(const State &state, std::set<std::pair<string, string>> &leaves)
{
  for (const Channel &c : state.channels)
  {
    if (!c.volume)
      continue;
    leaves.insert({AmpLinks::normalizeMac(c.mac), deviceLeaf});
    std::vector<string> sources;
    if (c.mixer)
      for (const MixSource &s : c.mixSources)
        sources.push_back(s.source);
    else
      sources.push_back(c.source);
    for (const string &source : sources)
    {
      Inputs inputs;
      if (parseInputs(source, inputs))
        for (int n : inputs.numbers)
          leaves.insert({AmpLinks::normalizeMac(c.mac), inputLeaf(inputs.kind, n)});
    }
  }
}

static string upper(string s)
{
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::toupper(c); });
  return s;
}

string stateFader(const State &state) { return state.id; }
string channelFader(const State &state, const Channel &c) { return state.id + "|" + upper(c.mac) + "|" + c.channel; }
string mixFader(const State &state, const Channel &c, const MixSource &s) { return channelFader(state, c) + "|" + s.element; }
string sharedMixFader(const State &state, const MixSource &s) { return state.id + "|*|" + s.element; }

} // namespace InputStates

using namespace InputStates;

void InputFaders::apply(const json &inputConfig)
{
  Wanted wanted;
  std::set<string> macs;
  std::set<std::pair<string, string>> nameLeafs; // (mac, leaf url)
  if (inputConfig.is_object())
  {
    for (auto entry = inputConfig.begin(); entry != inputConfig.end(); ++entry)
    {
      for (const State &state : read(inputConfig, entry.key()))
      {
        nameLeaves(state, nameLeafs);
        std::vector<AmpFaderElement::Target> all;
        for (const Channel &c : state.channels)
        {
          macs.insert(AmpLinks::normalizeMac(c.mac));
          if (!c.volume)
            continue;
          if (!c.mixer)
          {
            AmpFaderElement::Target t {c.mac, c.leafUrl(), "primaryFader"};
            all.push_back(t);
            wanted.push_back({channelFader(state, c), {t}});
            continue;
          }
          for (const MixSource &s : c.mixSources)
          {
            AmpFaderElement::Target t {c.mac, c.leafUrl(), s.element};
            all.push_back(t);
            wanted.push_back({mixFader(state, c, s), {t}});
          }
        }
        for (const MixSource &s : state.mixSources())
        {
          std::vector<AmpFaderElement::Target> shared;
          for (const Channel &c : state.channels)
            for (const MixSource &cs : c.mixSources)
              if (cs.element == s.element)
                shared.push_back({c.mac, c.leafUrl(), s.element});
          wanted.push_back({sharedMixFader(state, s), shared});
        }
        if (!all.empty())
          wanted.push_back({stateFader(state), all});
      }
    }
  }
  sync(wanted);

  // new handles before dropping the old ones, so amps / leaves still in use stay connected / subscribed
  std::vector<AmpLinks::Handle> handles;
  for (const string &mac : macs)
    handles.push_back(AmpLinks::instance().use(mac));
  connections = std::move(handles);
  std::vector<AmpLinks::Handle> names;
  for (const auto &[mac, leafUrl] : nameLeafs)
    names.push_back(AmpLinks::instance().subscribe(mac, leafUrl)); // the values are read from AmpLinks' cache
  nameSubscriptions = std::move(names);
}
