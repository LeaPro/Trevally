#pragma once
#include <object.h>

class Tech final : public LeafObject
{
public:
  Tech(std::string path, Object *parent);
  void initialize() override { LeafObject::initialize(); }
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;
  const std::string serialNumKey = "paranoid635!";
  const std::string upTimeKey = "66";
  std::shared_ptr<StringControl>  techTrPtr;
  std::shared_ptr<StringControl>  techSnPtr;
  std::shared_ptr<StringControl>  techMdPtr;
  std::shared_ptr<StringControl>  techSmPtr;
  std::shared_ptr<StringControl>  techMrPtr;
  std::shared_ptr<StringControl>  techNrPtr;
  std::shared_ptr<StringControl>  techBrPtr;
private:
};
