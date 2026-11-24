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
  /* Asks the devices on the network to announce themselves now (they otherwise do so every 64 s once they've
   * been up a while). Unlike the "start" method this keeps the device list. */
  void requestAnnouncements();
private:
  std::string lastDevices; // "devices" as last handed to AmpLinks
};
