#pragma once
#include <object.h>

#include <chrono>
#include <set>

#include "WeatherLookup.h"

class Weather final : public LeafObject
{
public:
  Weather(std::string path, Object *parent);
  void initialize() override { LeafObject::initialize(); }
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;

  std::shared_ptr<JsonControl> configPtr;

private:
  std::set<std::string> weatherTileBindings();

  WeatherLookup lookup_; // finds the conditions of the postal codes of the weather tiles
  std::chrono::steady_clock::time_point lastSync_{};
};
