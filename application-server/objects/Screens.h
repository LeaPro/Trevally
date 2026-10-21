#pragma once
#include <object.h>

class Screens final : public LeafObject
{
public:
  Screens(std::string path, Object *parent);
  void initialize() override { LeafObject::initialize(); }
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override {}
};
