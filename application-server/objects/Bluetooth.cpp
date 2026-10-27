#include "Bluetooth.h"
#include <cctype>
#include <cstdio>
#include <sstream>

static std::string runCapture(const std::string &cmd)
{
  FILE *pipe = popen(cmd.c_str(), "r");
  if (pipe == nullptr)
    return "";
  std::string out;
  char buf[256];
  while (fgets(buf, sizeof(buf), pipe) != nullptr)
    out += buf;
  pclose(pipe);
  return out;
}

// D-Bus object paths only contain [A-Za-z0-9_/]; reject anything else before it is used in a shell command
static bool isSafeDbusPath(const std::string &p)
{
  if (p.empty() || p[0] != '/')
    return false;
  for (char c : p)
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '/')
      return false;
  return true;
}

// Finds the BlueALSA A2DP sink PCM of the connected phone, e.g. /org/bluealsa/hci0/dev_AA_BB_CC_DD_EE_FF/a2dpsnk/source
static std::string findBlueAlsaPcm()
{
  std::istringstream lines(runCapture("busctl --system tree org.bluealsa --list 2>/dev/null"));
  std::string line;
  while (std::getline(lines, line))
  {
    while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back())))
      line.pop_back();
    if (line.find("/dev_") != std::string::npos && line.find("/a2dpsnk/") != std::string::npos && isSafeDbusPath(line))
      return line;
  }
  return "";
}

// Returns the PCM volume (0-127, left channel, mute bit ignored) or -1 if unavailable
static int readPcmVolume(const std::string &path)
{
  std::string out = runCapture("busctl --system get-property org.bluealsa " + path + " org.bluealsa.PCM1 Volume 2>/dev/null");
  char type = 0;
  int v = -1;
  if (sscanf(out.c_str(), " %c %d", &type, &v) != 2 || type != 'q' || v < 0)
    return -1;
  return (v >> 8) & 0x7F; // high byte = channel 1, bit 7 = mute
}

// Sets both channels to v (0-127, unmuted). BlueALSA applies it as soft volume and forwards it to the phone when supported.
static bool writePcmVolume(const std::string &path, int v)
{
  const unsigned packed = (static_cast<unsigned>(v) << 8) | static_cast<unsigned>(v);
  return system(("busctl --system set-property org.bluealsa " + path + " org.bluealsa.PCM1 Volume q " + std::to_string(packed) + " >/dev/null 2>&1").c_str()) == 0;
}

// Extracts "AA:BB:CC:DD:EE:FF" from a PCM path containing "/dev_AA_BB_CC_DD_EE_FF/"; empty if not found
static std::string macFromPcmPath(const std::string &path)
{
  const size_t pos = path.find("/dev_");
  if (pos == std::string::npos || path.size() < pos + 5 + 17)
    return "";
  std::string mac = path.substr(pos + 5, 17);
  for (size_t i = 0; i < mac.size(); i++)
  {
    if (i % 3 == 2)
    {
      if (mac[i] != '_')
        return "";
      mac[i] = ':';
    }
    else if (!std::isxdigit(static_cast<unsigned char>(mac[i])))
      return "";
  }
  return mac;
}

// Looks up the friendly name of a device via bluetoothctl; falls back to the MAC address
static std::string readDeviceName(const std::string &mac)
{
  std::istringstream lines(runCapture("bluetoothctl info " + mac + " 2>/dev/null")); // mac is validated hex/colons
  std::string line;
  while (std::getline(lines, line))
  {
    const size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos || line.compare(start, 6, "Name: ") != 0)
      continue;
    std::string name = line.substr(start + 6);
    while (!name.empty() && (name.back() == '\r' || name.back() == '\n'))
      name.pop_back();
    if (!name.empty())
      return name;
  }
  return mac;
}

// Returns "Name (MAC), Name (MAC), ..." for all paired devices
static std::string readPairedDevices()
{
#ifndef __ARM_ARCH
  FILE *pipe = popen("bluetoothctl paired-devices 2>/dev/null", "r");
#else
  FILE *pipe = popen("bluetoothctl devices Paired 2>/dev/null", "r");
#endif
  if (pipe == nullptr)
    return "";
  std::string result;
  char line[256];
  while (fgets(line, sizeof(line), pipe) != nullptr)
  {
    // format: "Device AA:BB:CC:DD:EE:FF Name"
    std::string s(line);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
      s.pop_back();
    if (s.compare(0, 7, "Device ") != 0 || s.size() < 24)
      continue;
    std::string mac = s.substr(7, 17);
    std::string devName = s.size() > 25 ? s.substr(25) : mac;
    if (!result.empty())
      result += ", ";
    result += devName + " (" + mac + ")";
  }
  pclose(pipe);
  return result;
}

Bluetooth::Bluetooth(std::string path, Object *parent) : LeafObject(path, parent)
{
  string name;
  name = "reset"; elements[name] = std::make_shared<BoolControl>(path + "/" + name, this,  false/*default*/);
  // Volume of the connected phone's A2DP stream (BlueALSA PCM volume, which BlueALSA also syncs with the phone via AVRCP).
  // Changes made here are applied to the stream and vice versa.
  name = "volume"; elements[name] = volumePtr = std::make_shared<UInt32Control>(path + "/" + name, this, 0, 100, 0, "percent", false/*persist*/);
  name = "pairedDevices"; elements[name] = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 1024/*maxLength*/);
  // Name of the device whose audio stream is connected (empty when none)
  name = "connectedDevice"; elements[name] = connectedDevicePtr = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 128/*maxLength*/);
}

// Mirrors the connected phone (taken from the BlueALSA PCM path) into the connectedDevice element
void Bluetooth::updateConnectedDevice()
{
  const std::string mac = macFromPcmPath(transportPath);
  if (mac == connectedMac)
    return;
  connectedMac = mac;
  connectedDevicePtr->set(mac.empty() ? "" : readDeviceName(mac));
}

// Two-way sync between the volume element (0-100 %) and the BlueALSA PCM Volume (0-127)
void Bluetooth::syncVolume()
{
  const auto now = std::chrono::steady_clock::now();
  const bool userChanged = volumePtr->isModified(); // clears the flag
  if (!userChanged && now - lastVolumePoll < std::chrono::seconds(1))
    return;
  lastVolumePoll = now;

  if (transportPath.empty())
    transportPath = findBlueAlsaPcm();
  if (transportPath.empty())
  {
    lastTransportVolume = -1;
    return;
  }

  if (userChanged)
  {
    int v = static_cast<int>((volumePtr->get() * 127 + 50) / 100);
    if (writePcmVolume(transportPath, v))
      lastTransportVolume = v;
    else
      transportPath.clear(); // PCM gone (phone disconnected); rediscover next time
    return;
  }

  int v = readPcmVolume(transportPath);
  if (v < 0)
  {
    transportPath.clear();
    lastTransportVolume = -1;
    return;
  }
  if (v != lastTransportVolume)
  {
    // volume changed on the phone: mirror it, and clear the resulting modified flag so it is not echoed back
    lastTransportVolume = v;
    volumePtr->set(static_cast<uint32_t>((v * 100 + 63) / 127));
    volumePtr->isModified();
  }
}

void Bluetooth::update(bool sensorsOnly, bool refreshVolatileElements)
{
  syncVolume();
  updateConnectedDevice();
  // refreshVolatileElements is never set by the framework, so also poll the paired devices periodically
  const auto now = std::chrono::steady_clock::now();
  bool refreshPaired = refreshVolatileElements || lastPairedPoll == std::chrono::steady_clock::time_point{} ||
                       now - lastPairedPoll >= std::chrono::seconds(5);
  if (refreshPaired)
    lastPairedPoll = now;
  if (elements["reset"]->isModified())
  {
    std::shared_ptr<BoolControl> resetPtr = std::dynamic_pointer_cast<BoolControl>(elements["reset"]);
    if (resetPtr != nullptr)
    {
      // elements start out flagged as modified, so only act on an explicit request
      bool requested = resetPtr->get();
      resetPtr->set(false);
      if (!requested)
      {
        if (refreshPaired)
          std::dynamic_pointer_cast<StringSensor>(elements["pairedDevices"])->set(readPairedDevices());
        return;
      }
      refreshPaired = true;
      // remove paired BT devices
#ifndef __ARM_ARCH
      // host build: use the host's bluetooth service (systemctl will prompt for authentication)
      system(R"(bluetoothctl paired-devices | awk '{print $2}' | xargs -r -n1 bluetoothctl remove)");
      system(R"(systemctl restart bluetooth)");
#else
      system(R"(systemctl stop trevally-bt-agent)");
      system(R"(bluetoothctl devices Paired | awk '{print $2}' | xargs -r -n1 bluetoothctl remove)");
      system(R"(systemctl restart bluetooth trevally-bt-agent)");
#endif
    }
  }
  if (refreshPaired)
    std::dynamic_pointer_cast<StringSensor>(elements["pairedDevices"])->set(readPairedDevices());
}