#include "Bluetooth.h"
#include <cctype>
#include <csignal>
#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

static constexpr std::chrono::seconds VOLUME_POLL_INTERVAL{1};       // BlueALSA PCM volume / connected device
static constexpr std::chrono::seconds PAIRED_POLL_INTERVAL{5};       // pairedDevices
static constexpr std::chrono::seconds DISCOVERED_POLL_INTERVAL{2};   // discoveredDevices while scanning
static constexpr std::chrono::milliseconds PAIRING_CODE_POLL_INTERVAL{250};
static const char *const PAIRING_CODE_FILE = "/run/trevally-bt-passkey"; // written by trevally-bt-agent during numeric comparison
static constexpr size_t MAC_LENGTH = 17;                             // length of "AA:BB:CC:DD:EE:FF"
static constexpr std::chrono::seconds PAIRING_WINDOW{60};            // how long the adapter stays pairable for a requestConnect that has to pair

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

using DeviceList = std::vector<std::pair<std::string, std::string>>; // (MAC, name)

// Runs a bluetoothctl listing command and returns the devices it prints
static DeviceList listDevices(const char *cmd)
{
  DeviceList devices;
  FILE *pipe = popen(cmd, "r");
  if (pipe == nullptr)
    return devices;
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
    devices.emplace_back(mac, s.size() > 25 ? s.substr(25) : mac);
  }
  pclose(pipe);
  return devices;
}

// "Name (MAC), Name (MAC), ..."
static std::string formatDevices(const DeviceList &devices)
{
  std::string result;
  for (const auto &d : devices)
  {
    if (!result.empty())
      result += ", ";
    result += Bluetooth::formatDevice(d.second, d.first);
  }
  return result;
}

static DeviceList pairedDeviceList()
{
#ifndef __ARM_ARCH
  return listDevices("bluetoothctl paired-devices 2>/dev/null");
#else
  return listDevices("bluetoothctl devices Paired 2>/dev/null");
#endif
}

static std::string readPairedDevices()
{
  return formatDevices(pairedDeviceList());
}

static bool isPaired(const DeviceList &paired, const std::string &mac)
{
  for (const auto &p : paired)
    if (p.first == mac)
      return true;
  return false;
}

// Known devices that are not paired, i.e. the ones found by scanning
static DeviceList unpairedDeviceList()
{
  const DeviceList paired = pairedDeviceList();
  DeviceList result;
  for (auto &d : listDevices("bluetoothctl devices 2>/dev/null"))
    if (!isPaired(paired, d.first))
      result.push_back(std::move(d));
  return result;
}

static std::string readDiscoveredDevices()
{
  return formatDevices(unpairedDeviceList());
}

// The code of the pairing in progress (digits only), or empty when there is none
static std::string readPairingCode()
{
  std::ifstream file(PAIRING_CODE_FILE);
  std::string code;
  if (!std::getline(file, code))
    return "";
  for (char c : code)
    if (!std::isdigit(static_cast<unsigned char>(c)))
      return "";
  return code;
}

// External devices can only pair while the adapter is pairable (the target stays discoverable either way)
static void setExternalPairing(bool enable)
{
  system(enable ? "bluetoothctl pairable on >/dev/null 2>&1" : "bluetoothctl pairable off >/dev/null 2>&1");
}

// True for "AA:BB:CC:DD:EE:FF"; required before a MAC is used in a shell command
bool Bluetooth::isValidMac(const std::string &mac)
{
  if (mac.size() != MAC_LENGTH)
    return false;
  for (size_t i = 0; i < mac.size(); i++)
    if (i % 3 == 2 ? mac[i] != ':' : !std::isxdigit(static_cast<unsigned char>(mac[i])))
      return false;
  return true;
}

std::string Bluetooth::formatDevice(const std::string &name, const std::string &mac)
{
  return name + " (" + mac + ")";
}

bool Bluetooth::parseDevice(const std::string &device, std::string &name, std::string &mac)
{
  // the MAC is always the last MAC_LENGTH characters, wrapped in " (" and ")"
  if (device.size() < MAC_LENGTH + 2 || device.back() != ')')
    return false;
  const size_t open = device.size() - MAC_LENGTH - 2;
  if (device[open] != '(')
    return false;
  const std::string candidate = device.substr(open + 1, MAC_LENGTH);
  if (!isValidMac(candidate))
    return false;
  mac = candidate;
  name = device.substr(0, open);
  if (!name.empty() && name.back() == ' ')
    name.pop_back();
  return true;
}

// Drops BlueZ's cached unpaired devices so a new scan only reports devices that are in range now
static void purgeUnpairedDevices()
{
  for (const auto &d : unpairedDeviceList())
    if (Bluetooth::isValidMac(d.first))
      system(("bluetoothctl remove " + d.first + " >/dev/null 2>&1").c_str());
}

// Starts "bluetoothctl scan on" with a day-long timeout. BlueZ scans as long as this client lives and stops when it exits
// (also if we crash: the child gets SIGTERM when its parent dies). Returns the pid, or -1 on failure.
static pid_t startScanProcess()
{
  const pid_t pid = fork();
  if (pid == 0)
  {
    prctl(PR_SET_PDEATHSIG, SIGTERM);
    const int devnull = open("/dev/null", O_RDWR);
    if (devnull >= 0)
    {
      dup2(devnull, 0);
      dup2(devnull, 1);
      dup2(devnull, 2);
    }
    // plain "scan on" returns immediately in non-interactive mode and discovery stops with it; --timeout keeps the client alive
    execlp("bluetoothctl", "bluetoothctl", "--timeout", "86400", "scan", "on", static_cast<char *>(nullptr));
    _exit(127);
  }
  return pid;
}

Bluetooth::Bluetooth(std::string path, Object *parent) : LeafObject(path, parent)
{
  string name;
  name = "reset"; elements[name] = std::make_shared<BoolControl>(path + "/" + name, this,  false/*default*/);
  // true: scan for nearby devices until set to false (discoveredDevices follows while it is on); false: stop scanning.
  // Reads false again if the scan ends by itself (e.g. bluetooth restarted). Not persisted: scanning is always off at startup.
  name = "scan"; elements[name] = scanPtr = std::make_shared<BoolControl>(path + "/" + name, this, false/*default*/, false/*persist*/);
  // true: external devices can pair with the target; false: they can't (the target stays discoverable, and already paired devices still connect).
  // Not persisted, so the target always starts with external pairing disabled.
  name = "enableExternalPairing"; elements[name] = enableExternalPairingPtr = std::make_shared<BoolControl>(path + "/" + name, this, false/*default*/, false/*persist*/);
  // MAC address (AA:BB:CC:DD:EE:FF) of a paired or discovered device to remove; cleared once handled
  name = "removeDevice"; elements[name] = std::make_shared<StringControl>(path + "/" + name, this, ""/*default*/, 32/*maxLength*/, false/*persist*/);
  // MAC address (AA:BB:CC:DD:EE:FF) of a paired or discovered device to connect to; cleared once handled.
  // The connectedDevice is disconnected first; an unpaired device is paired and trusted before connecting.
  name = "requestConnect"; elements[name] = std::make_shared<StringControl>(path + "/" + name, this, ""/*default*/, 32/*maxLength*/, false/*persist*/);
  // true: disconnect the connectedDevice, if any (it stays paired); resets to false right away
  name = "disconnectDevice"; elements[name] = std::make_shared<BoolControl>(path + "/" + name, this, false/*default*/, false/*persist*/);
  // Volume of the connected phone's A2DP stream (BlueALSA PCM volume, which BlueALSA also syncs with the phone via AVRCP).
  // Changes made here are applied to the stream and vice versa.
  name = "volume"; elements[name] = volumePtr = std::make_shared<UInt32Control>(path + "/" + name, this, 0, 100, 0, "percent", false/*persist*/);
  name = "pairedDevices"; elements[name] = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 1024/*maxLength*/);
  // Devices found by the last scan, "Name (MAC), ..."; refreshed while scan is on
  name = "discoveredDevices"; elements[name] = discoveredDevicesPtr = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 1024/*maxLength*/);
  // Device whose audio stream is connected, "Name (MAC)" (empty when none)
  name = "connectedDevice"; elements[name] = connectedDevicePtr = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 160/*maxLength*/);
  // Numeric-comparison code (digits) of the pairing in progress that the other device shows too; empty when there is none
  name = "pairingCode"; elements[name] = pairingCodePtr = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 16/*maxLength*/);
}

Bluetooth::~Bluetooth()
{
  stopScan();
}

void Bluetooth::stopScan()
{
  if (scanPid > 0)
  {
    kill(scanPid, SIGTERM);
    waitpid(scanPid, nullptr, 0);
  }
  scanPid = -1;
  scanning = false;
}

// Mirrors the connected phone (taken from the BlueALSA PCM path) into the connectedDevice element
void Bluetooth::updateConnectedDevice()
{
  const std::string mac = macFromPcmPath(transportPath);
  if (mac == connectedMac)
    return;
  connectedMac = mac;
  connectedDevicePtr->set(mac.empty() ? "" : formatDevice(readDeviceName(mac), mac));
}

// Two-way sync between the volume element (0-100 %) and the BlueALSA PCM Volume (0-127)
void Bluetooth::syncVolume()
{
  const auto now = std::chrono::steady_clock::now();
  const bool userChanged = volumePtr->isModified(); // clears the flag
  if (!userChanged && now - lastVolumePoll < VOLUME_POLL_INTERVAL)
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
  if (now - lastPairingCodePoll >= PAIRING_CODE_POLL_INTERVAL)
  {
    lastPairingCodePoll = now;
    pairingCodePtr->set(readPairingCode());
  }
  bool refreshPaired = refreshVolatileElements || lastPairedPoll == std::chrono::steady_clock::time_point{} ||
                       now - lastPairedPoll >= PAIRED_POLL_INTERVAL;
  if (refreshPaired)
    lastPairedPoll = now;
  // apply on change (elements start flagged modified, so also at startup) and re-apply periodically, since the pairing agent
  // turns pairing back on whenever it restarts (e.g. after reset). While not pairable the adapter is not bondable either, so any
  // pairing (also one started by the target) would only get a temporary key that is dropped on disconnect; hence a requestConnect
  // that has to pair keeps the adapter pairable for PAIRING_WINDOW.
  const bool pairingWindow = now < pairingUntil;
  if (enableExternalPairingPtr->isModified() || refreshPaired || pairingWindow != pairingWindowActive)
    setExternalPairing(enableExternalPairingPtr->get() || pairingWindow);
  pairingWindowActive = pairingWindow;
  if (scanPtr->isModified())
  {
    // elements start out flagged as modified; starting/stopping only happens on a real change
    const bool requested = scanPtr->get();
    if (requested && !scanning)
    {
      purgeUnpairedDevices();
      discoveredDevicesPtr->set("");
      scanPid = startScanProcess();
      scanning = scanPid > 0;
      lastDiscoveredPoll = {};
      if (!scanning)
      {
        scanPid = -1;
        scanPtr->set(false);
        scanPtr->isModified();   // don't treat our own write as a request
      }
    }
    else if (!requested && scanning)
    {
      stopScan();
    }
  }
  if (scanning && waitpid(scanPid, nullptr, WNOHANG) == scanPid)
  {
    // the scan ended by itself (e.g. bluetoothd restarted): reflect that in the element
    scanPid = -1;
    scanning = false;
    scanPtr->set(false);
    scanPtr->isModified();
  }
  if (scanning && now - lastDiscoveredPoll >= DISCOVERED_POLL_INTERVAL)
  {
    lastDiscoveredPoll = now;
    discoveredDevicesPtr->set(readDiscoveredDevices());
  }
  if (elements["removeDevice"]->isModified())
  {
    std::shared_ptr<StringControl> removePtr = std::dynamic_pointer_cast<StringControl>(elements["removeDevice"]);
    if (removePtr != nullptr)
    {
      const std::string mac = removePtr->get();
      removePtr->set("");
      if (isValidMac(mac))
      {
        system(("bluetoothctl remove " + mac + " >/dev/null 2>&1").c_str());
        refreshPaired = true;
        discoveredDevicesPtr->set(readDiscoveredDevices());
      }
    }
  }
  if (elements["requestConnect"]->isModified())
  {
    std::shared_ptr<StringControl> connectPtr = std::dynamic_pointer_cast<StringControl>(elements["requestConnect"]);
    if (connectPtr != nullptr)
    {
      const std::string mac = connectPtr->get();
      connectPtr->set("");
      if (isValidMac(mac))
      {
        // disconnect the current device first, then pair (if needed) and connect. This can take several seconds,
        // so it runs in the background like the scan; connectedMac only ever holds a validated MAC
        std::string cmd;
        // (untrust first: the target's pairing agent reconnects trusted devices as soon as they drop)
        if (!connectedMac.empty() && connectedMac != mac)
          cmd += "bluetoothctl untrust " + connectedMac + "; bluetoothctl disconnect " + connectedMac + "; ";
        if (isPaired(pairedDeviceList(), mac))
          cmd += "bluetoothctl trust " + mac + "; bluetoothctl connect " + mac;
        else
        {
          cmd += "bluetoothctl pair " + mac + " && bluetoothctl trust " + mac + " && bluetoothctl connect " + mac;
          pairingUntil = now + PAIRING_WINDOW;
          pairingWindowActive = true;
          setExternalPairing(true);
        }
        system(("(" + cmd + ") >/dev/null 2>&1 &").c_str());
      }
    }
  }
  if (elements["disconnectDevice"]->isModified())
  {
    std::shared_ptr<BoolControl> disconnectPtr = std::dynamic_pointer_cast<BoolControl>(elements["disconnectDevice"]);
    if (disconnectPtr != nullptr)
    {
      const bool requested = disconnectPtr->get();
      disconnectPtr->set(false);
      // connectedMac only ever holds a validated MAC. Untrust first: the target's pairing agent reconnects trusted
      // devices as soon as they drop (the device stays paired and is trusted again when it next connects or is requested)
      if (requested && !connectedMac.empty())
        system(("(bluetoothctl untrust " + connectedMac + "; bluetoothctl disconnect " + connectedMac + ") >/dev/null 2>&1 &").c_str());
    }
  }
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