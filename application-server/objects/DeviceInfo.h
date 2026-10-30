#pragma once
#include <object.h>

const std::string mfgInfoPath = "/mnt/data/mfg";
const std::string mfgInfoFileName = "/mnt/data/mfg/mfgInfo.txt";
const std::string ampDataFilePath = "/mnt/data/amp";
const std::string upTimeFileName = ampDataFilePath + "/upTime.txt";

class DeviceInfo final : public LeafObject
{
public:
  DeviceInfo(std::string path, Object *parent);
  void initialize() override { LeafObject::initialize(); }
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;
  void fileCleanUp();
  int  readMfgDataFromFile();
  std::shared_ptr<StringControl> deviceNamePtr;
  std::shared_ptr<StringControl> venueNamePtr;
  std::shared_ptr<StringControl> locationPtr;
  std::shared_ptr<StringControl> defaultPostalCodePtr;
  std::shared_ptr<StringControl> installerNamePtr;
  std::shared_ptr<StringControl> installerContactInfoPtr;
  std::shared_ptr<FloatSensor> numInputsPtr;
  std::shared_ptr<FloatSensor> numOutputsPtr;
  std::shared_ptr<StringSensor> modelIdPtr;
  std::shared_ptr<StringSensor> familyPtr;
  std::shared_ptr<StringSensor> serialNumPtr;
  std::shared_ptr<StringSensor> dateOfMfgPtr;
  std::shared_ptr<StringControl> dateOfInstallationPtr;
  std::shared_ptr<StringSensor> hardwareIDPtr;
  std::shared_ptr<StringSensor> hardwareRevisionPtr;
  std::shared_ptr<StringSensor> ampHardwareRevisionPtr;
  std::shared_ptr<BoolSensor> wifiHwSupportPtr;
  std::shared_ptr<BoolControl> mfgTestModeControlPtr;
  std::shared_ptr<BoolSensor> mfgTestModePtr;
  std::shared_ptr<StringControl> assetTagNumberPtr;
  std::shared_ptr<StringSensor> arcFwVerPtr;
  std::shared_ptr<StringSensor> arcFwVerRcPtr;
  std::shared_ptr<StringSensor> upTimePtr;
  std::shared_ptr<StringSensor> mfgTestPtr;
  std::shared_ptr<StringSensor> mftResultPtr;
  std::shared_ptr<StringSensor> netResultPtr;
  std::shared_ptr<StringSensor> burnInResultPtr;

private:
  time_t lastUpTimeUpdate = 0;
};
