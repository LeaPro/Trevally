#pragma once
#include <object.h>
#include <chrono>
#include <memory>
#include <string>
#include <sys/types.h>

class Bluetooth final : public LeafObject
{
public:
  Bluetooth(std::string path, Object *parent);
  ~Bluetooth() override;
  void initialize() override { LeafObject::initialize(); }
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;

  // Device strings have the form "Name (AA:BB:CC:DD:EE:FF)" (connectedDevice, and each entry of pairedDevices / discoveredDevices)
  static bool isValidMac(const std::string &mac);
  static std::string formatDevice(const std::string &name, const std::string &mac);
  static bool parseDevice(const std::string &device, std::string &name, std::string &mac); // false if not in the form above
private:
  void syncVolume();
  void updateConnectedDevice();
  std::shared_ptr<UInt32Control> volumePtr;
  std::shared_ptr<BoolControl> enableExternalPairingPtr;
  std::shared_ptr<StringSensor> connectedDevicePtr;
  std::shared_ptr<StringSensor> discoveredDevicesPtr;
  std::shared_ptr<StringSensor> pairingCodePtr;
  std::chrono::steady_clock::time_point lastPairingCodePoll{};
  void stopScan();
  std::shared_ptr<BoolControl> scanPtr;
  bool scanning = false;                     // true while scanPid runs
  pid_t scanPid = -1;                        // long-lived "bluetoothctl scan on"; BlueZ scans for as long as it lives
  std::chrono::steady_clock::time_point pairingUntil{}; // end of the window in which a requestConnect may pair (adapter must be bondable)
  bool pairingWindowActive = false;
  std::chrono::steady_clock::time_point lastDiscoveredPoll{};
  std::string connectedMac;                  // MAC of the device connectedDevice currently describes (empty when none)
  std::string transportPath;                 // cached BlueALSA PCM object path (empty when no phone is connected)
  int lastTransportVolume = -1;              // last PCM volume (0-127) seen on / sent to BlueALSA
  std::chrono::steady_clock::time_point lastVolumePoll{};
  std::chrono::steady_clock::time_point lastPairedPoll{};
};