#pragma once
#include <object.h>
#include <boost/asio.hpp>
#include "gpio.h"

class Misc final : public LeafObject
{
public:
  Misc(std::string path, Object *parent);
  void initialize() override;
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;
  void factoryReset();
  void reboot();

  std::shared_ptr<BoolControl> rebootPtr;
  std::shared_ptr<BoolControl> factoryResetPtr;
  std::shared_ptr<StringSensor> fwUpdateStatusPtr;
  std::shared_ptr<BoolControl> ledTestPtr;
  std::shared_ptr<BoolControl> setupModePtr;

private:
  // PILOTFISH-QUAD-v128.syscfg: GPIO1_3_FACTORY_RESET -> RGMII2_RD0 (main_gpio1 pin 3)
  // active-low push button (reads 0 while held down); adjust if board wiring differs
  Gpio factoryResetButton{am62x_gpio_num("gpio1", 3)};

  // PILOTFISH-QUAD-v128.syscfg: GPIO0_3_LED_CTRL_nRESET -> OSPI0_D0 (main_gpio0 pin 3)
  // active-low reset for the IS31FL3235A LED driver; output, default high (not in reset)
  Gpio ledDriverReset{am62x_gpio_num("gpio0", 3), 1};
};

extern bool firCoeffsStringFormatIsValid(string s);
extern std::valarray<float> stringToFirCoeffs(std::string s, uint32_t len);
extern std::string firCoeffsToString(std::valarray<float> coeffs);

class FirCoeffsStringControl final : public StringControl
{
public:
  FirCoeffsStringControl(std::string path, LeafObject *parent, std::string value, int maxLength = 128, bool persistFlag = true, bool accessControlFlag = true) :
    StringControl(path, parent, value, maxLength, persistFlag, accessControlFlag)
  {
  }
  json processJson(std::string method, json j, int client)
  {
    if (method == "set" && client != Object::InternalClient)
    {
      string value = locked ? unobfuscate(j.get<string>()) : j.get<string>();
      if (!firCoeffsStringFormatIsValid(value))
        throw std::runtime_error("bogus FIR coeff format");
    }
    return StringControl::processJson(method, j, client); 
  }
  std::string set(std::string value)
  {
    if (!firCoeffsStringFormatIsValid(value))
    {
      printf("%s:%d(%s) bogus FIR coeff format:  path = '%s', coeffs = '%s'\n", __FILE__, __LINE__, __func__, path.c_str(), value.c_str());
      return StringControl::get();
    }
    return StringControl::set(value);
  }
};
