#pragma once
#include <object.h>
#include <chrono>
#include <memory>
#include <string>

class Bluetooth final : public LeafObject
{
public:
  Bluetooth(std::string path, Object *parent);
  void initialize() override { LeafObject::initialize(); }
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;
private:
  void syncVolume();
  void updateConnectedDevice();
  std::shared_ptr<UInt32Control> volumePtr;
  std::shared_ptr<StringSensor> connectedDevicePtr;
  std::string connectedMac;                  // MAC of the device connectedDevice currently describes (empty when none)
  std::string transportPath;                 // cached BlueALSA PCM object path (empty when no phone is connected)
  int lastTransportVolume = -1;              // last PCM volume (0-127) seen on / sent to BlueALSA
  std::chrono::steady_clock::time_point lastVolumePoll{};
  std::chrono::steady_clock::time_point lastPairedPoll{};
};