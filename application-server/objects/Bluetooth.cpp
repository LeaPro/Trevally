#include "Bluetooth.h"

Bluetooth::Bluetooth(std::string path, Object *parent) : LeafObject(path, parent)
{
  string name;
  name = "reset"; elements[name] = std::make_shared<BoolControl>(path + "/" + name, this,  false/*default*/);
}

void Bluetooth::update(bool sensorsOnly, bool refreshVolatileElements)
{
  if (elements["reset"]->isModified())
  {
    std::shared_ptr<BoolControl> resetPtr = std::dynamic_pointer_cast<BoolControl>(elements["reset"]);
    if (resetPtr != nullptr)
    {
      resetPtr->set(false);
      // remove paired BT devices 
      system(R"(systemctl stop trevally-bt-agent)");
      system(R"(bluetoothctl devices Paired | awk '{print $2}' | xargs -r -n1 bluetoothctl remove)");
      system(R"(systemctl restart bluetooth trevally-bt-agent)");
    }
  }
}