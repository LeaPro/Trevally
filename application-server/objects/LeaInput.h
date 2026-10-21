#pragma once
#include <object.h>

class LeaInput final : public LeafObject
{
public:
  LeaInput(std::string path, Object *parent);
  void initialize() override { LeafObject::initialize(); }
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override {}
};
