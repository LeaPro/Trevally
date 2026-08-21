#pragma once
#include <object.h>
#include <boost/asio.hpp>
#ifdef __ARMEL__
#include "gpio.h"
#endif

class Files final : public LeafObject
{
public:
  Files(std::string path, Object *parent);
  void initialize() override;
  json processJson(const std::string &method, json &j, int client = -1) override;
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;

private:
  bool deleteWatchedFile(const std::string &partialPath, std::string &error);
  void updateDirectory(bool bootstrap = false);

  std::shared_ptr<JsonSensor>   directoryPtr;
};

