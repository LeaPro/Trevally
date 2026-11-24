#pragma once
#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>

// The fixed name the CS Glass gives itself to external software (the Soundtrack player name, the Custom Channels
// listener name): LEA-CS-Glass-<last 4 hex digits of the eth0 MAC, lowercase>, the same as its Bluetooth name (see
// trevally-bt-agent.py in meta-lea-trevally). Not the user-editable /deviceInfo/deviceName, and not the hostname
// (lea-trevally-<last4mac>), which stays as it is.
inline std::string externalServiceName()
{
  const std::string prefix = "LEA-CS-Glass";
  std::ifstream file("/sys/class/net/eth0/address");
  std::string mac;
  std::getline(file, mac);
  mac.erase(std::remove(mac.begin(), mac.end(), ':'), mac.end());
  std::transform(mac.begin(), mac.end(), mac.begin(), [](unsigned char c) { return std::tolower(c); });
  if (mac.size() < 4 || mac == "000000000000")
    return prefix;
  return prefix + "-" + mac.substr(mac.size() - 4);
}
