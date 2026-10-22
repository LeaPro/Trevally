#pragma once
#include <object.h>

class Tiles final : public LeafObject
{
public:
  Tiles(std::string path, Object *parent);
  void initialize() override { LeafObject::initialize(); }
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override {}

  std::shared_ptr<JsonControl> configPtr;
};
