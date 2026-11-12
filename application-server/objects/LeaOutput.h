#pragma once
#include <object.h>

class LeaOutput final : public LeafObject
{
public:
  LeaOutput(std::string path, Object *parent);
  void initialize() override { LeafObject::initialize(); }
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;
  std::shared_ptr<JsonControl> configPtr;
private:
  json appliedConfig; // last config handed to /outputFaders
};
