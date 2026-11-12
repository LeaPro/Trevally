#pragma once
#include <object.h>

class LeaSensor final : public LeafObject
{
public:
  LeaSensor(std::string path, Object *parent);
  void initialize() override { LeafObject::initialize(); }
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override {}
};
