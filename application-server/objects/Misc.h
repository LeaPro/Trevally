#pragma once
#include <object.h>
#include <boost/asio.hpp>
#ifdef __ARMEL__
#include "gpio.h"
#endif

class Misc final : public LeafObject
{
public:
  Misc(std::string path, Object *parent);
  void initialize() override;
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;

private:
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
