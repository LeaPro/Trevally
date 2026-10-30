#pragma once
#include <object.h>

class Ethernet final : public LeafObject
{
public:
  Ethernet(std::string path, Object *parent);
  void initialize() override { LeafObject::initialize(); }
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;

  std::shared_ptr<BoolControl>   dhcpPtr;
  std::shared_ptr<StringControl> ipPtr;
  std::shared_ptr<StringControl> gatewayPtr;
  std::shared_ptr<StringControl> dns1Ptr;
  std::shared_ptr<StringControl> dns2Ptr;
  std::shared_ptr<StringSensor>  macPtr;
  std::shared_ptr<EnumControl>   dot1xModePtr;
  std::shared_ptr<StringControl> dot1xIdentityPtr;
  std::shared_ptr<StringControl> dot1xPasswordPtr;
  std::shared_ptr<StringControl> dot1xAnonymousIdentityPtr;
  std::shared_ptr<StringControl> dot1xDomainPtr;
  std::shared_ptr<StringSensor>  dot1xCACertValidPtr;
  std::shared_ptr<StringSensor>  dot1xClientCertValidPtr;
  std::shared_ptr<StringSensor>  dot1xClientKeyCheckPtr;
  std::shared_ptr<StringSensor>  dot1xUploadedCACertValidPtr;
  std::shared_ptr<StringSensor>  dot1xUploadedClientCertValidPtr;
  std::shared_ptr<StringSensor>  dot1xUploadedClientKeyCheckPtr;
  std::shared_ptr<BoolControl>   dot1xSaveUploadedCertsPtr;
  std::shared_ptr<UInt32Sensor>  phyErrorCountPtr;

  enum class Dot1xMode {Disabled, PEAP, TLS, TTLS, Max=TTLS};
  std::map<int, const char *> Dot1xModeMap {
    {(int)Dot1xMode::Disabled, "Disabled"},
    {(int)Dot1xMode::PEAP, "PEAP"},
    {(int)Dot1xMode::TLS, "TLS"},
    {(int)Dot1xMode::TTLS, "TTLS"}
  };
  #ifdef __ARM_ARCH
  void doit();
  #endif
};
