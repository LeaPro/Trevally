#include "objects.h"

using std::string;

Appearance::Appearance(string path, Object *parent) :
  LeafObject(path, parent)
{
  string name;
  name = "theme"; elements[name] = themePtr = std::make_shared<StringControl>(path + "/" + name, this, "dark"/*default*/, 64);
  name = "orientation"; elements[name] = orientationPtr = std::make_shared<StringControl>(path + "/" + name, this, "portrait"/*default*/, 16);
  name = "temperatureUnits"; elements[name] = temperatureUnitsPtr = std::make_shared<StringControl>(path + "/" + name, this, "f"/*default*/, 16);
  name = "volumeUnits"; elements[name] = volumeUnitsPtr = std::make_shared<StringControl>(path + "/" + name, this, "db"/*default*/, 16);
  name = "levelGaugeStyle"; elements[name] = levelGaugeStylePtr = std::make_shared<StringControl>(path + "/" + name, this, "ellipse"/*default*/, 16);
}
