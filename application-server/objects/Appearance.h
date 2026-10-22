#pragma once
#include <object.h>
#include <string>

// Device-wide display appearance settings. All elements are plain
// StringControls, since the webUI sends/receives strings for these
// settings (e.g. theme references a key in the "theme" leaf's config,
// which is a dynamically extensible UUID/name-keyed map).
class Appearance final : public LeafObject
{
public:
  Appearance(std::string path, Object *parent);

  std::shared_ptr<StringControl> themePtr;
  std::shared_ptr<StringControl> orientationPtr;
  std::shared_ptr<StringControl> temperatureUnitsPtr;
  std::shared_ptr<StringControl> volumeUnitsPtr;
};
