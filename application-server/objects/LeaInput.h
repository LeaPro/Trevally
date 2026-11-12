#pragma once
#include <object.h>

class LeaInput final : public LeafObject
{
public:
  LeaInput(std::string path, Object *parent);
  void initialize() override;
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;
  /* Selects @p stateId's sources on its amp channels and records it as @p binding's active state
   * (even if it already was: re-applies it). */
  void applyState(const std::string &binding, const std::string &stateId);
  std::string activeState(const std::string &binding);
  std::shared_ptr<JsonControl> configPtr;
  std::shared_ptr<JsonControl> activePtr;
private:
  void sendState(const std::string &binding, const std::string &stateId);
  json appliedConfig; // last config handed to /inputFaders
  json appliedActive; // last "active" whose states were sent to the amps
};
