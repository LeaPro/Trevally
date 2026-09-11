#pragma once
#include <object.h>

class Bluetooth final : public LeafObject
{
public:
  Bluetooth(std::string path, Object *parent);
  void initialize() override { LeafObject::initialize(); }
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;
};