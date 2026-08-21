#pragma once
#include <object.h>
#include <Aes256Gcm.h>
#include <HmacSha256.h>

class Test final : public LeafObject
{
public:
  Test(std::string path, Object *parent);
  void initialize() override { LeafObject::initialize(); }
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;
  void update(const std::string &elementName, const json &patchOps, int client) override;
private:
  std::shared_ptr<JsonControl> testPtr;
  std::shared_ptr<JsonControl> testHookPtr;
};

