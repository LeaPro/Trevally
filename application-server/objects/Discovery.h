#pragma once
#include <object.h>

class Discovery final : public DiscoBase
{
public:
  Discovery(std::string path, Object *parent);
  ~Discovery();
  void initialize() override { LeafObject::initialize(); }
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;
};
