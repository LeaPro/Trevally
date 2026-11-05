#pragma once
#include <object.h>

class Roles final : public LeafObject
{
public:
  Roles(std::string path, Object *parent);
  void initialize() override { LeafObject::initialize(); }
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override {}

  std::shared_ptr<JsonControl> configPtr;
};
