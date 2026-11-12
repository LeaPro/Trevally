#include "LeaInput.h"
#include "AmpLinks.h"
#include "InputFaders.h"

/*
"config" is a collection of input-proxy info blobs keyed by UUID (an input tile's binding), e.g.:
{
  "input-uuid-kansas": {
    "states": {
      "state-uuid-iowa": {
        "id": "state-uuid-iowa",
        "name": ...,
        "inputSelector": { "mac123": {"1": {"source": "Dante 1+2", "volumeControl": {"Dante 1+2": true}}, "2": {...}} }
      }
    },
    "stateOrder": ["state-uuid-utah", "state-uuid-iowa"]
  }
}
This is a generic JsonControl; whole-blob get/set and granular add/update/remove
(via url subpath) are handled entirely by JsonElement::processJson(). See InputFaders.h for how it is read.
The live fader values of these entries are elements of /inputFaders, kept in step with the config by update().

"active" is the state last applied to each entry, e.g. {"input-uuid-kansas": "state-uuid-utah"}. Setting it
(from the UI or the API) selects that state's sources on its amp channels:
/amp/channels/<ch>/inputSelector/primary = source.
*/
LeaInput::LeaInput(string path, Object *parent) : LeafObject(path, parent)
{
  methods.insert({"add", "update", "remove"});
  string name;
  name = "config"; elements[name] = configPtr = std::make_shared<JsonControl>(path + "/" + name, this, json::object());
  name = "active"; elements[name] = activePtr = std::make_shared<JsonControl>(path + "/" + name, this, json::object());
}

void LeaInput::initialize()
{
  // the restored active states were applied before; don't push them to the amps again at startup
  appliedActive = activePtr->get();
  LeafObject::initialize();
}

void LeaInput::update(bool sensorsOnly, bool refreshVolatileElements)
{
  if (sensorsOnly)
    return;
  auto faders = std::dynamic_pointer_cast<InputFaders>(getRoot()->leafFromPath("/inputFaders"));
  if (!faders)
    return;

  json currentConfig = configPtr->get();
  if (currentConfig != appliedConfig)
  {
    appliedConfig = currentConfig;
    faders->apply(currentConfig);
  }

  // states set over the API (the UI goes through applyState())
  json currentActive = activePtr->get();
  if (currentActive != appliedActive)
  {
    if (currentActive.is_object())
      for (auto it = currentActive.begin(); it != currentActive.end(); ++it)
        if (it.value().is_string() && (!appliedActive.is_object() || appliedActive.value(it.key(), json()) != it.value()))
          sendState(it.key(), it.value().get<string>());
    appliedActive = currentActive;
  }
}

std::string LeaInput::activeState(const std::string &binding)
{
  const json current = activePtr->get();
  if (!current.is_object())
    return "";
  auto it = current.find(binding);
  return (it != current.end() && it->is_string()) ? it->get<string>() : "";
}

void LeaInput::applyState(const std::string &binding, const std::string &stateId)
{
  json current = activePtr->get();
  if (!current.is_object())
    current = json::object();
  current[binding] = stateId;
  activePtr->set(current);
  appliedActive = current;
  sendState(binding, stateId);
}

void LeaInput::sendState(const std::string &binding, const std::string &stateId)
{
  for (const auto &state : InputStates::read(configPtr->get(), binding))
  {
    if (state.id != stateId)
      continue;
    for (const auto &channel : state.channels)
      if (!channel.source.empty())
        AmpLinks::instance().set(channel.mac, channel.leafUrl(), json{{"primary", channel.source}});
    return;
  }
}
