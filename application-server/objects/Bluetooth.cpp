#include "Bluetooth.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <future>
#include <gio/gio.h>
#include <sys/stat.h>

static constexpr std::chrono::seconds PAIRABLE_RETRY_INTERVAL{1};    // minimum time between attempts to change the adapter's pairable state
static constexpr std::chrono::seconds SCAN_START_GRACE{3};           // how long BlueZ gets to report that a requested scan runs
static constexpr std::chrono::seconds STATUS_HOLD{2};                // grace period after a play/pause command before mirroring the phone's state again
static constexpr std::chrono::seconds RESET_TIMEOUT{5};              // how long a reset waits for BlueZ to remove the paired devices
static constexpr std::chrono::seconds SERVICE_TIMEOUT{10};           // how long a start/stop of the bluetooth service gets before serviceRunning follows reality again
static constexpr std::chrono::milliseconds PAIRING_CODE_POLL_INTERVAL{250};
static const char *const PAIRING_CODE_FILE = "/run/trevally-bt-passkey"; // written by trevally-bt-agent during numeric comparison
static constexpr size_t MAC_LENGTH = 17;                             // length of "AA:BB:CC:DD:EE:FF"
static constexpr std::chrono::seconds PAIRING_WINDOW{60};            // how long the adapter stays pairable for a requestConnect that has to pair

static const char *const BLUEZ = "org.bluez";
static const char *const BLUEALSA = "org.bluealsa";
static const char *const IFACE_ADAPTER = "org.bluez.Adapter1";
static const char *const IFACE_DEVICE = "org.bluez.Device1";
static const char *const IFACE_PLAYER = "org.bluez.MediaPlayer1";
static const char *const IFACE_PCM = "org.bluealsa.PCM1";
static const char *const IFACE_PROPERTIES = "org.freedesktop.DBus.Properties";
static const char *const IFACE_OBJECT_MANAGER = "org.freedesktop.DBus.ObjectManager";

static constexpr int PAIR_TIMEOUT_MS = 60000;
static constexpr int CONNECT_TIMEOUT_MS = 30000;
static constexpr int DEFAULT_TIMEOUT_MS = 10000;

// ---------------------------------------------------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------------------------------------------------

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

// Extracts "AA:BB:CC:DD:EE:FF" from an object path containing "/dev_AA_BB_CC_DD_EE_FF"; empty if there is none
static std::string macFromPath(const std::string &path)
{
  const size_t pos = path.find("/dev_");
  if (pos == std::string::npos || path.size() < pos + 5 + MAC_LENGTH)
    return "";
  std::string mac = path.substr(pos + 5, MAC_LENGTH);
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

// BlueALSA's A2DP sink streams, e.g. /org/bluealsa/hci0/dev_AA_BB_CC_DD_EE_FF/a2dpsnk/source
static bool isStreamPcmPath(const std::string &path)
{
  return path.find("/a2dpsnk/") != std::string::npos && !macFromPath(path).empty();
}

static bool isType(GVariant *v, const GVariantType *type)
{
  return v != nullptr && g_variant_is_of_type(v, type);
}

static std::string stringOf(GVariant *v)
{
  return isType(v, G_VARIANT_TYPE_STRING) ? g_variant_get_string(v, nullptr) : "";
}

static void logError(const char *what, const GError *error)
{
  fprintf(stderr, "[bluez] %s failed: %s\n", what, error != nullptr ? error->message : "unknown error");
}

// "Name (MAC), Name (MAC), ..." of the devices for which keep() is true
template <typename Predicate>
std::string Bluetooth::formatDevices(const std::vector<Device> &devices, Predicate keep)
{
  std::string result;
  for (const auto &d : devices)
  {
    if (!keep(d))
      continue;
    if (!result.empty())
      result += ", ";
    result += formatDevice(d.name, d.mac);
  }
  return result;
}

// True for "AA:BB:CC:DD:EE:FF"
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

// ---------------------------------------------------------------------------------------------------------------------
// elements
// ---------------------------------------------------------------------------------------------------------------------

Bluetooth::Bluetooth(std::string path, Object *parent) : LeafObject(path, parent)
{
  string name;
  name = "reset"; elements[name] = resetPtr = std::make_shared<BoolControl>(path + "/" + name, this,  false/*default*/);
  // false: stop the bluetooth service (and the pairing agent), e.g. to recover from a bad state; true: start it again. Follows the actual state of the
  // service, so it reads false while the service is stopped and true while it runs, however that came about. Pairings are kept. Not persisted.
  name = "serviceRunning"; elements[name] = serviceRunningPtr = std::make_shared<BoolControl>(path + "/" + name, this, true/*default*/, false/*persist*/);
  // true: scan for nearby devices until set to false (discoveredDevices follows while it is on); false: stop scanning.
  // Reads false again if the scan ends by itself (e.g. bluetooth restarted). Not persisted: scanning is always off at startup.
  name = "scan"; elements[name] = scanPtr = std::make_shared<BoolControl>(path + "/" + name, this, false/*default*/, false/*persist*/);
  // true: external devices can pair with the target; false: they can't (the target stays discoverable, and already paired devices still connect).
  // Not persisted, so the target always starts with external pairing disabled.
  name = "enableExternalPairing"; elements[name] = enableExternalPairingPtr = std::make_shared<BoolControl>(path + "/" + name, this, false/*default*/, false/*persist*/);
  // MAC address (AA:BB:CC:DD:EE:FF) of a paired or discovered device to remove; cleared once handled
  name = "removeDevice"; elements[name] = removeDevicePtr = std::make_shared<StringControl>(path + "/" + name, this, ""/*default*/, 32/*maxLength*/, false/*persist*/);
  // MAC address (AA:BB:CC:DD:EE:FF) of a paired or discovered device to connect to; cleared once handled.
  // The connectedDevice is disconnected first; an unpaired device is paired and trusted before connecting.
  name = "requestConnect"; elements[name] = requestConnectPtr = std::make_shared<StringControl>(path + "/" + name, this, ""/*default*/, 32/*maxLength*/, false/*persist*/);
  // true: disconnect the connectedDevice, if any (it stays paired); resets to false right away
  name = "disconnectDevice"; elements[name] = disconnectDevicePtr = std::make_shared<BoolControl>(path + "/" + name, this, false/*default*/, false/*persist*/);
  // Volume of the connected phone's A2DP stream (BlueALSA PCM volume, which BlueALSA also syncs with the phone via AVRCP).
  // Changes made here are applied to the stream and vice versa.
  name = "volume"; elements[name] = volumePtr = std::make_shared<UInt32Control>(path + "/" + name, this, 0, 100, 0, "percent", false/*persist*/);
  name = "pairedDevices"; elements[name] = pairedDevicesPtr = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 1024/*maxLength*/);
  // Devices found by the last scan, "Name (MAC), ..."; refreshed while scan is on
  name = "discoveredDevices"; elements[name] = discoveredDevicesPtr = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 1024/*maxLength*/);
  // Device whose audio stream is connected, "Name (MAC)" (empty when none)
  name = "connectedDevice"; elements[name] = connectedDevicePtr = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 160/*maxLength*/);
  // Numeric-comparison code (digits) of the pairing in progress that the other device shows too; empty when there is none
  name = "pairingCode"; elements[name] = pairingCodePtr = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 16/*maxLength*/);
  // Metadata of the track the connected phone is playing (AVRCP); empty when nothing is connected or the phone sends none
  name = "trackTitle"; elements[name] = trackTitlePtr = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 128/*maxLength*/);
  name = "trackArtist"; elements[name] = trackArtistPtr = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 128/*maxLength*/);
  name = "trackAlbum"; elements[name] = trackAlbumPtr = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 128/*maxLength*/);
  // trackAlbumArt: path of the album art file (a PNG of at most 500 px) of the current track, looked up on MusicBrainz / Cover Art
  // Archive; empty when nothing is connected or the album art is unknown or hasn't arrived yet. The file name changes with every new image.
  name = "trackAlbumArt"; elements[name] = trackAlbumArtPtr = std::make_shared<StringSensor>(path + "/" + name, this, ""/*default*/, 64/*maxLength*/);
  // Length of the current track and how far the phone is into it, in whole seconds; 0 when nothing is playing or the phone does not say
  name = "trackDuration"; elements[name] = trackDurationPtr = std::make_shared<UInt32Sensor>(path + "/" + name, this, 0, __UINT32_MAX__, 0/*default*/, "s");
  name = "trackPosition"; elements[name] = trackPositionPtr = std::make_shared<UInt32Sensor>(path + "/" + name, this, 0, __UINT32_MAX__, 0/*default*/, "s");
  // Audio source selection, like soundtrack/enable and customChannels/enable: while false the phone is kept paused, and the
  // track* elements, isPlaying and next/previous are inactive. The adapter and the connection stay up.
  name = "enable"; elements[name] = enablePtr = std::make_shared<BoolControl>(path + "/" + name, this, true/*default*/);
  // isPlaying: what the connected phone is doing (true while playing; false when paused/stopped/not connected), whoever caused it.
  // play: the request. Writing it plays/pauses the phone; it follows the phone when the phone changes state, so it is never stale.
  name = "isPlaying"; elements[name] = isPlayingPtr = std::make_shared<BoolSensor>(path + "/" + name, this, false/*default*/);
  name = "play"; elements[name] = playPtr = std::make_shared<BoolControl>(path + "/" + name, this, false/*default*/, false/*persist*/);
  // true: skip to the next / previous track on the phone; resets to false right away
  name = "next"; elements[name] = nextPtr = std::make_shared<BoolControl>(path + "/" + name, this, false/*default*/, false/*persist*/);
  name = "previous"; elements[name] = previousPtr = std::make_shared<BoolControl>(path + "/" + name, this, false/*default*/, false/*persist*/);
  // shuffle / repeat: true turns the phone's shuffle (all tracks) / repeat (all tracks) on, false turns it off. They follow the phone when it
  // changes them, like play; they read false while nothing is connected or the phone does not report the setting.
  name = "shuffle"; elements[name] = shufflePtr = std::make_shared<BoolControl>(path + "/" + name, this, false/*default*/, false/*persist*/);
  name = "repeat"; elements[name] = repeatPtr = std::make_shared<BoolControl>(path + "/" + name, this, false/*default*/, false/*persist*/);

  context_ = g_main_context_new();
  loop_ = g_main_loop_new(context_, FALSE);
  thread_ = std::thread(&Bluetooth::busMain, this);
}

Bluetooth::~Bluetooth()
{
  // posted functions only run inside the loop, so this is safe even if the worker hasn't started running it yet
  postToBus([this]() { g_main_loop_quit(loop_); });
  if (thread_.joinable())
    thread_.join();
  g_main_loop_unref(loop_);
  g_main_context_unref(context_);
}

// External devices can only pair while the adapter is pairable (the target stays discoverable either way). The pairing agent turns
// pairing back on whenever it restarts (e.g. after reset), so the adapter's actual state is checked on every update, not just on change.
// While not pairable the adapter is not bondable either, so any pairing (also one started by the target) would only get a temporary key
// that is dropped on disconnect; hence a requestConnect that has to pair keeps the adapter pairable for PAIRING_WINDOW.
void Bluetooth::syncExternalPairing(std::chrono::steady_clock::time_point now)
{
  enableExternalPairingPtr->isModified(); // the value is read below; clear the flag
  const bool wanted = enableExternalPairingPtr->get() || now < pairingUntil;
  if (pairable() != wanted && now - lastPairableSet >= PAIRABLE_RETRY_INTERVAL)
  {
    lastPairableSet = now;
    setPairable(wanted);
  }
}

void Bluetooth::handleScan(std::chrono::steady_clock::time_point now)
{
  if (scanPtr->isModified())
  {
    // elements start out flagged as modified: starting/stopping only happens on a real change
    const bool requested = scanPtr->get();
    if (requested && !scanning)
    {
      // drop BlueZ's cached unpaired devices so the scan only reports devices that are in range now
      removeUnpairedDevices();
      discoveredDevicesPtr->set("");
      startDiscovery();
      scanning = true;
      discoveryConfirmed = false;
      scanRequestedAt = now;
    }
    else if (!requested && scanning)
    {
      stopDiscovery();
      scanning = false;
    }
  }
  if (scanning)
  {
    if (discovering())
      discoveryConfirmed = true;
    else if (discoveryConfirmed || now - scanRequestedAt >= SCAN_START_GRACE)
    {
      // the scan ended by itself (or never started, e.g. bluetoothd restarted): reflect that in the element
      scanning = false;
      scanPtr->set(false);
      scanPtr->isModified(); // don't treat our own write as a request
    }
  }
}

void Bluetooth::handleRemoveDevice()
{
  if (!removeDevicePtr->isModified())
    return;
  const std::string mac = removeDevicePtr->get();
  removeDevicePtr->set("");
  if (isValidMac(mac))
    removeDevice(mac);
}

void Bluetooth::handleRequestConnect(std::chrono::steady_clock::time_point now)
{
  if (!requestConnectPtr->isModified())
    return;
  const std::string mac = requestConnectPtr->get();
  requestConnectPtr->set("");
  if (!isValidMac(mac))
    return;
  if (!isPaired(mac))
  {
    pairingUntil = now + PAIRING_WINDOW;
    lastPairableSet = now;
    setPairable(true);
  }
  // the connected device is untrusted and disconnected first, then the new one is paired (if needed), trusted and connected
  connectDevice(mac, connectedMac);
}

void Bluetooth::handleDisconnect()
{
  if (!disconnectDevicePtr->isModified())
    return;
  const bool requested = disconnectDevicePtr->get();
  disconnectDevicePtr->set(false);
  if (requested && !connectedMac.empty())
    disconnectDevice(connectedMac); // the device stays paired and is trusted again when it next connects or is requested
}

void Bluetooth::handleReset()
{
  if (!resetPtr->isModified())
    return;
  // elements start out flagged as modified, so only act on an explicit request
  const bool requested = resetPtr->get();
  resetPtr->set(false);
  if (!requested)
    return;
#ifdef __ARM_ARCH
  system("systemctl stop trevally-bt-agent");
#endif
  // forget the paired devices, and wait until BlueZ has done so before it is restarted
  auto removed = std::make_shared<std::promise<void>>();
  std::future<void> removedFuture = removed->get_future();
  removeAllPairedDevices([removed]() { removed->set_value(); });
  removedFuture.wait_for(RESET_TIMEOUT);
#ifndef __ARM_ARCH
  // host build: use the host's bluetooth service (systemctl will prompt for authentication)
  system("systemctl restart bluetooth");
#else
  system("systemctl restart bluetooth trevally-bt-agent");
#endif
}

// serviceRunning is the request to start/stop the bluetooth service and follows whether org.bluez is on the bus. The commands don't wait for
// systemd (--no-block); the service's appearing on / vanishing from the bus (see busMain) reports the outcome.
void Bluetooth::syncService(std::chrono::steady_clock::time_point now)
{
  const int state = bluezRunning_;
  if (state < 0)
    return; // the bus watch has not reported yet
  const bool running = state == 1;
  if (!serviceInitialised)
  {
    // elements start out flagged as modified: take over the actual state instead of acting on the default
    serviceInitialised = true;
    serviceRunningPtr->isModified();
    serviceRunningPtr->set(running);
    serviceRunningPtr->isModified();
    return;
  }
  if (serviceRunningPtr->isModified()) // clears the flag
  {
    const bool requested = serviceRunningPtr->get();
    if (requested != running)
    {
#ifdef __ARM_ARCH
      system(requested ? "systemctl --no-block start bluetooth trevally-bt-agent" : "systemctl --no-block stop trevally-bt-agent bluetooth");
#else
      system(requested ? "systemctl --no-block start bluetooth" : "systemctl --no-block stop bluetooth");
#endif
      servicePending = true;
      servicePendingValue = requested;
      serviceHoldUntil = now + SERVICE_TIMEOUT;
    }
  }
  if (servicePending)
  {
    if (running != servicePendingValue && now < serviceHoldUntil)
      return; // the service has not reached the requested state yet
#ifdef __ARM_ARCH
    // BlueALSA registered its A2DP endpoints with the BlueZ instance that was stopped; restart it so it registers with the new one.
    // bluealsa-aplay (the playback to the audio output) is PartOf=bluetooth.service, so stopping BlueZ stopped it and starting BlueZ does not bring it back.
    if (running && servicePendingValue)
      system("systemctl --no-block restart bluealsa bluealsa-aplay");
#endif
    servicePending = false;
  }
  if (serviceRunningPtr->get() != running)
  {
    // the service started/stopped by itself, or the request failed: follow it without echoing it back
    serviceRunningPtr->set(running);
    serviceRunningPtr->isModified();
  }
}

void Bluetooth::updateDeviceLists()
{
  const std::vector<Device> list = devices();
  pairedDevicesPtr->set(formatDevices(list, [](const Device &d) { return d.paired; }));
  if (scanning)
    discoveredDevicesPtr->set(formatDevices(list, [](const Device &d) { return !d.paired; }));
}

// Mirrors the connected phone (the one with a BlueALSA A2DP stream) into the connectedDevice element
void Bluetooth::updateConnectedDevice()
{
  connectedMac = streamMac();
  connectedDevicePtr->set(connectedMac.empty() ? "" : formatDevice(deviceName(connectedMac), connectedMac));
}

// Mirrors the connected phone's current track (BlueZ MediaPlayer1) into the track* elements
void Bluetooth::updateTrackMetadata()
{
  const std::optional<Player> phone = enablePtr->get() ? player(connectedMac) : std::nullopt;
  trackTitlePtr->set(phone ? phone->title : "");
  trackArtistPtr->set(phone ? phone->artist : "");
  trackAlbumPtr->set(phone ? phone->album : "");
  trackDurationPtr->set(phone ? phone->durationMs / 1000 : 0);
  trackPositionPtr->set(phone ? phone->position(std::chrono::steady_clock::now()) / 1000 : 0);
}

// Mirrors the album art of the connected phone's current track into trackAlbumArt. The phone only tells the track,
// so the album art is looked up by artist / album / title (MusicBrainz + Cover Art Archive) on a worker thread of its own.
void Bluetooth::updateAlbumArt()
{
  const std::optional<Player> phone = enablePtr->get() ? player(connectedMac) : std::nullopt;
  if (phone)
    albumArt_.request(phone->artist, phone->album, phone->title);
  else
    albumArt_.request("", "", "");
  trackAlbumArtPtr->set(albumArt_.file());
}

// isPlaying reports the phone's player status; play is the request to it and follows the phone's state (like syncVolume())
void Bluetooth::syncPlayback(std::chrono::steady_clock::time_point now)
{
  const std::optional<Player> phone = player(connectedMac);
  if (!enablePtr->get())
  {
    // not the selected audio source: keep the phone paused and refuse play requests
    playPtr->isModified();
    if (phone && phone->playing && now >= statusHoldUntil)
    {
      playerCommand(connectedMac, "Pause");
      statusHoldUntil = now + STATUS_HOLD;
    }
    playing = false;
    playPending = false;
    isPlayingPtr->set(false);
    if (playPtr->get())
    {
      playPtr->set(false);
      playPtr->isModified();
    }
    return;
  }

  if (playPtr->isModified()) // clears the flag
  {
    // elements start out flagged as modified: only act when the request differs from the last state seen
    const bool requested = playPtr->get();
    if (requested != playing && phone)
    {
      playerCommand(connectedMac, requested ? "Play" : "Pause");
      playPending = true;
      playPendingValue = requested;
      statusHoldUntil = now + STATUS_HOLD; // keep the request until the phone has applied it
    }
  }

  const bool phonePlaying = phone && phone->playing;
  playing = phonePlaying;
  isPlayingPtr->set(phonePlaying);

  if (playPending)
  {
    if (phonePlaying != playPendingValue && now < statusHoldUntil)
      return; // the phone has not applied the request yet
    playPending = false;
  }
  if (playPtr->get() != phonePlaying)
  {
    // changed on the phone, or the request could not be applied: follow the phone, and clear the resulting
    // modified flag so it is not echoed back
    playPtr->set(phonePlaying);
    playPtr->isModified();
  }
}

// shuffle / repeat: the element is the request and follows the phone's setting. The phone only reports whether the mode is "off",
// so turning one on asks for all tracks.
void Bluetooth::syncPlayerSetting(const std::shared_ptr<BoolControl> &ptr, SettingSync &sync, const char *property, const char *onValue,
                                  std::optional<bool> phoneValue, std::chrono::steady_clock::time_point now)
{
  if (!enablePtr->get() || !phoneValue)
  {
    // not the selected audio source, or no phone setting to follow: refuse requests and read false
    ptr->isModified();
    sync.pending = false;
    if (ptr->get())
    {
      ptr->set(false);
      ptr->isModified();
    }
    return;
  }

  if (ptr->isModified()) // clears the flag; only a real write gets here, the flag was cleared while there was no phone
  {
    const bool requested = ptr->get();
    if (requested != *phoneValue)
    {
      setPlayerProperty(connectedMac, property, requested ? onValue : "off");
      sync.pending = true;
      sync.pendingValue = requested;
      sync.holdUntil = now + STATUS_HOLD; // keep the request until the phone has applied it
    }
  }

  if (sync.pending)
  {
    if (*phoneValue != sync.pendingValue && now < sync.holdUntil)
      return; // the phone has not applied the request yet
    sync.pending = false;
  }
  if (ptr->get() != *phoneValue)
  {
    // changed on the phone, or the request could not be applied: follow the phone without echoing it back
    ptr->set(*phoneValue);
    ptr->isModified();
  }
}

// next / previous: momentary controls that skip the phone's track
void Bluetooth::handleTrackButtons()
{
  const std::pair<std::shared_ptr<BoolControl> *, const char *> buttons[] = {{&nextPtr, "Next"}, {&previousPtr, "Previous"}};
  for (const auto &b : buttons)
  {
    const std::shared_ptr<BoolControl> &ptr = *b.first;
    if (!ptr->isModified())
      continue;
    const bool requested = ptr->get();
    ptr->set(false);
    if (requested && enablePtr->get() && player(connectedMac))
      playerCommand(connectedMac, b.second);
  }
}

// Two-way sync between the volume element (0-100 %) and the BlueALSA PCM Volume (0-127)
void Bluetooth::syncVolume()
{
  const bool userChanged = volumePtr->isModified(); // clears the flag
  const int v = streamVolume();
  if (v < 0)
  {
    lastStreamVolume = -1;
    return;
  }
  if (userChanged)
  {
    const int requested = static_cast<int>((volumePtr->get() * 127 + 50) / 100);
    setStreamVolume(requested);
    lastStreamVolume = requested;
    return;
  }
  if (v != lastStreamVolume)
  {
    // volume changed on the phone: mirror it, and clear the resulting modified flag so it is not echoed back
    lastStreamVolume = v;
    volumePtr->set(static_cast<uint32_t>((v * 100 + 63) / 127));
    volumePtr->isModified();
  }
}

void Bluetooth::update(bool sensorsOnly, bool refreshVolatileElements)
{
  const auto now = std::chrono::steady_clock::now();
  if (now - lastPairingCodePoll >= PAIRING_CODE_POLL_INTERVAL)
  {
    lastPairingCodePoll = now;
    pairingCodePtr->set(readPairingCode());
  }
  updateConnectedDevice();
  syncVolume();
  handleTrackButtons();
  syncPlayback(now);
  {
    const std::optional<Player> phone = player(connectedMac);
    syncPlayerSetting(shufflePtr, shuffleSync, "Shuffle", "alltracks", phone ? phone->shuffle : std::nullopt, now);
    syncPlayerSetting(repeatPtr, repeatSync, "Repeat", "alltracks", phone ? phone->repeat : std::nullopt, now);
  }
  updateTrackMetadata();
  updateAlbumArt();
  syncExternalPairing(now);
  handleScan(now);
  handleRemoveDevice();
  handleRequestConnect(now);
  handleDisconnect();
  handleReset();
  syncService(now);
  updateDeviceLists();
}

// ---------------------------------------------------------------------------------------------------------------------
// D-Bus worker thread
// ---------------------------------------------------------------------------------------------------------------------

void Bluetooth::busMain()
{
  g_main_context_push_thread_default(context_);

  GError *error = nullptr;
  gchar *address = g_dbus_address_get_for_bus_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
  if (address != nullptr)
  {
    conn_ = g_dbus_connection_new_for_address_sync(
        address, static_cast<GDBusConnectionFlags>(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION),
        nullptr, nullptr, &error);
    g_free(address);
  }
  if (conn_ == nullptr)
  {
    logError("connecting to the system bus", error);
    if (error != nullptr)
      g_error_free(error);
    g_main_context_pop_thread_default(context_);
    return;
  }

  // BlueZ and BlueALSA announce their objects with InterfacesAdded / InterfacesRemoved, and changes to them with PropertiesChanged
  auto onObjectManagerSignal = [](GDBusConnection *, const gchar *, const gchar *, const gchar *, const gchar *signal, GVariant *parameters, gpointer self)
  {
    Bluetooth *bt = static_cast<Bluetooth *>(self);
    const gchar *path = nullptr;
    GVariant *second = nullptr;
    g_variant_get(parameters, "(&o@*)", &path, &second);
    {
      std::lock_guard<std::mutex> lock(bt->mutex_);
      if (g_strcmp0(signal, "InterfacesAdded") == 0)
        bt->addObject(path, second);
      else if (g_strcmp0(signal, "InterfacesRemoved") == 0)
        bt->removeInterfaces(path, second);
    }
    g_variant_unref(second);
  };
  auto onPropertiesChanged = [](GDBusConnection *, const gchar *, const gchar *path, const gchar *, const gchar *, GVariant *parameters, gpointer self)
  {
    Bluetooth *bt = static_cast<Bluetooth *>(self);
    GVariant *iface = g_variant_get_child_value(parameters, 0);
    GVariant *changed = g_variant_get_child_value(parameters, 1);
    {
      std::lock_guard<std::mutex> lock(bt->mutex_);
      bt->applyProperties(path, stringOf(iface), changed, false/*create*/);
    }
    g_variant_unref(iface);
    g_variant_unref(changed);
  };
  for (const char *bus : {BLUEZ, BLUEALSA})
  {
    g_dbus_connection_signal_subscribe(conn_, bus, IFACE_OBJECT_MANAGER, nullptr, nullptr, nullptr, G_DBUS_SIGNAL_FLAGS_NONE, onObjectManagerSignal, this,
                                       nullptr);
    g_dbus_connection_signal_subscribe(conn_, bus, IFACE_PROPERTIES, "PropertiesChanged", nullptr, nullptr, G_DBUS_SIGNAL_FLAGS_NONE, onPropertiesChanged,
                                       this, nullptr);
  }

  // (re)load the objects whenever the services appear (also covers them starting after us or being restarted)
  const guint bluezWatch = g_bus_watch_name_on_connection(
      conn_, BLUEZ, G_BUS_NAME_WATCHER_FLAGS_NONE,
      [](GDBusConnection *, const gchar *, const gchar *, gpointer self)
      {
        static_cast<Bluetooth *>(self)->bluezRunning_ = 1;
        static_cast<Bluetooth *>(self)->loadManagedObjects(BLUEZ, "/");
      },
      [](GDBusConnection *, const gchar *, gpointer self)
      {
        static_cast<Bluetooth *>(self)->bluezRunning_ = 0;
        static_cast<Bluetooth *>(self)->clearBluez();
      },
      this, nullptr);
  const guint blueAlsaWatch = g_bus_watch_name_on_connection(
      conn_, BLUEALSA, G_BUS_NAME_WATCHER_FLAGS_NONE,
      [](GDBusConnection *, const gchar *, const gchar *, gpointer self) { static_cast<Bluetooth *>(self)->loadManagedObjects(BLUEALSA, "/org/bluealsa"); },
      [](GDBusConnection *, const gchar *, gpointer self) { static_cast<Bluetooth *>(self)->clearBlueAlsa(); }, this, nullptr);

  g_main_loop_run(loop_);

  g_bus_unwatch_name(bluezWatch);
  g_bus_unwatch_name(blueAlsaWatch);
  // closing the connection also ends our discovery session
  g_dbus_connection_close_sync(conn_, nullptr, nullptr);
  g_object_unref(conn_);
  conn_ = nullptr;
  g_main_context_pop_thread_default(context_);
}

void Bluetooth::postToBus(std::function<void()> fn)
{
  auto *f = new std::function<void()>(std::move(fn));
  g_main_context_invoke_full(
      context_, G_PRIORITY_DEFAULT,
      [](gpointer data) -> gboolean
      {
        (*static_cast<std::function<void()> *>(data))();
        return G_SOURCE_REMOVE;
      },
      f, [](gpointer data) { delete static_cast<std::function<void()> *>(data); });
}

// Worker thread only. params is a floating GVariant tuple (or nullptr); done is called with the reply (or the error)
void Bluetooth::busCall(const char *bus, const std::string &path, const char *iface, const char *method, GVariant *params, int timeoutMs, Reply done)
{
  GDBusConnection *conn = conn_;
  if (conn == nullptr)
  {
    if (params != nullptr)
      g_variant_unref(g_variant_ref_sink(params));
    return;
  }
  const std::string what = std::string(iface) + "." + method;
  auto *reply = new Reply([what, done](GVariant *result, const GError *error)
                          {
                            if (error != nullptr)
                              logError(what.c_str(), error);
                            if (done)
                              done(result, error);
                          });
  g_dbus_connection_call(
      conn, bus, path.c_str(), iface, method, params, nullptr, G_DBUS_CALL_FLAGS_NONE, timeoutMs, nullptr,
      [](GObject *source, GAsyncResult *res, gpointer data)
      {
        std::unique_ptr<Reply> callback(static_cast<Reply *>(data));
        GError *error = nullptr;
        GVariant *result = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), res, &error);
        (*callback)(result, error);
        if (result != nullptr)
          g_variant_unref(result);
        if (error != nullptr)
          g_error_free(error);
      },
      reply);
}

// Worker thread only. value is a floating GVariant of the property's type
void Bluetooth::busSetProperty(const char *bus, const std::string &path, const char *iface, const char *name, GVariant *value)
{
  busCall(bus, path, IFACE_PROPERTIES, "Set", g_variant_new("(ssv)", iface, name, value), DEFAULT_TIMEOUT_MS);
}

void Bluetooth::busCallDevice(const std::string &path, const char *method, int timeoutMs, Reply done)
{
  busCall(BLUEZ, path, IFACE_DEVICE, method, nullptr, timeoutMs, std::move(done));
}

std::string Bluetooth::devicePath(const std::string &mac) const
{
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto &d : devices_)
    if (d.second.mac == mac)
      return d.first;
  return "";
}

std::string Bluetooth::adapterPath() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return adapterPath_;
}

// ---------------------------------------------------------------------------------------------------------------------
// cache maintenance (worker thread; mutex_ held except in loadManagedObjects, which takes it itself)
// ---------------------------------------------------------------------------------------------------------------------

void Bluetooth::loadManagedObjects(const char *bus, const char *rootPath)
{
  busCall(bus, rootPath, IFACE_OBJECT_MANAGER, "GetManagedObjects", nullptr, DEFAULT_TIMEOUT_MS,
          [this](GVariant *result, const GError *error)
          {
            if (error != nullptr || result == nullptr)
              return;
            GVariant *objects = g_variant_get_child_value(result, 0); // a{oa{sa{sv}}}
            GVariantIter it;
            const gchar *path = nullptr;
            GVariant *interfaces = nullptr;
            g_variant_iter_init(&it, objects);
            {
              std::lock_guard<std::mutex> lock(mutex_);
              while (g_variant_iter_next(&it, "{&o@a{sa{sv}}}", &path, &interfaces))
              {
                addObject(path, interfaces);
                g_variant_unref(interfaces);
              }
            }
            g_variant_unref(objects);
          });
}

// interfaces is a{sa{sv}}: interface name -> properties
void Bluetooth::addObject(const std::string &path, GVariant *interfaces)
{
  GVariantIter it;
  const gchar *iface = nullptr;
  GVariant *props = nullptr;
  g_variant_iter_init(&it, interfaces);
  while (g_variant_iter_next(&it, "{&s@a{sv}}", &iface, &props))
  {
    applyProperties(path, iface, props, true/*create*/);
    g_variant_unref(props);
  }
}

// interfaceNames is as
void Bluetooth::removeInterfaces(const std::string &path, GVariant *interfaceNames)
{
  GVariantIter it;
  const gchar *iface = nullptr;
  g_variant_iter_init(&it, interfaceNames);
  while (g_variant_iter_next(&it, "&s", &iface))
  {
    if (g_strcmp0(iface, IFACE_DEVICE) == 0)
      devices_.erase(path);
    else if (g_strcmp0(iface, IFACE_PLAYER) == 0)
      players_.erase(path);
    else if (g_strcmp0(iface, IFACE_PCM) == 0)
      pcms_.erase(path);
    else if (g_strcmp0(iface, IFACE_ADAPTER) == 0 && path == adapterPath_)
    {
      adapterPath_.clear();
      discovering_ = false;
      pairable_ = false;
    }
  }
}

// props is a{sv}. create is false for PropertiesChanged signals, which must not resurrect objects that were removed already
void Bluetooth::applyProperties(const std::string &path, const std::string &iface, GVariant *props, bool create)
{
  GVariantIter it;
  const gchar *key = nullptr;
  GVariant *value = nullptr;
  g_variant_iter_init(&it, props);
  while (g_variant_iter_next(&it, "{&sv}", &key, &value))
  {
    applyProperty(path, iface, key, value, create);
    g_variant_unref(value);
  }
}

void Bluetooth::applyProperty(const std::string &path, const std::string &iface, const std::string &key, GVariant *value, bool create)
{
  if (iface == IFACE_ADAPTER)
  {
    if (!adapterPath_.empty() && adapterPath_ != path)
      return; // only the first adapter is used
    adapterPath_ = path;
    if (isType(value, G_VARIANT_TYPE_BOOLEAN))
    {
      if (key == "Pairable")
        pairable_ = g_variant_get_boolean(value);
      else if (key == "Discovering")
        discovering_ = g_variant_get_boolean(value);
    }
  }
  else if (iface == IFACE_DEVICE)
  {
    auto it = devices_.find(path);
    if (it == devices_.end())
    {
      if (!create)
        return;
      it = devices_.emplace(path, Device()).first;
      it->second.mac = macFromPath(path);
      it->second.name = it->second.mac;
    }
    Device &d = it->second;
    if (key == "Address")
      d.mac = stringOf(value);
    else if (key == "Alias" || (key == "Name" && d.name == d.mac))
    {
      const std::string name = stringOf(value);
      if (!name.empty())
        d.name = name;
    }
    else if (isType(value, G_VARIANT_TYPE_BOOLEAN))
    {
      if (key == "Paired")
        d.paired = g_variant_get_boolean(value);
    }
  }
  else if (iface == IFACE_PLAYER)
  {
    auto it = players_.find(path);
    if (it == players_.end())
    {
      if (!create)
        return;
      it = players_.emplace(path, Player()).first;
      it->second.mac = macFromPath(path);
    }
    Player &p = it->second;
    if (key == "Status")
    {
      const bool playing = stringOf(value) == "playing";
      if (playing != p.playing)
      {
        // the position moves only while playing: settle it at the moment of the change
        const auto now = std::chrono::steady_clock::now();
        p.positionMs = p.position(now);
        p.positionAt = now;
      }
      p.playing = playing;
    }
    else if (key == "Position" && isType(value, G_VARIANT_TYPE_UINT32))
    {
      p.positionMs = g_variant_get_uint32(value);
      p.positionAt = std::chrono::steady_clock::now();
    }
    else if (key == "Shuffle" && isType(value, G_VARIANT_TYPE_STRING))
      p.shuffle = stringOf(value) != "off";
    else if (key == "Repeat" && isType(value, G_VARIANT_TYPE_STRING))
      p.repeat = stringOf(value) != "off";
    else if (key == "Track" && isType(value, G_VARIANT_TYPE("a{sv}")))
    {
      // the property always carries the complete track: whatever is missing is empty
      p.title.clear();
      p.artist.clear();
      p.album.clear();
      p.durationMs = 0;
      GVariantIter track;
      const gchar *field = nullptr;
      GVariant *fieldValue = nullptr;
      g_variant_iter_init(&track, value);
      while (g_variant_iter_next(&track, "{&sv}", &field, &fieldValue))
      {
        if (g_strcmp0(field, "Title") == 0)
          p.title = stringOf(fieldValue);
        else if (g_strcmp0(field, "Artist") == 0)
          p.artist = stringOf(fieldValue);
        else if (g_strcmp0(field, "Album") == 0)
          p.album = stringOf(fieldValue);
        else if (g_strcmp0(field, "Duration") == 0 && isType(fieldValue, G_VARIANT_TYPE_UINT32))
          p.durationMs = g_variant_get_uint32(fieldValue);
        g_variant_unref(fieldValue);
      }
    }
  }
  else if (iface == IFACE_PCM)
  {
    if (!isStreamPcmPath(path))
      return;
    auto it = pcms_.find(path);
    if (it == pcms_.end())
    {
      if (!create)
        return;
      it = pcms_.emplace(path, PcmInfo()).first;
      it->second.mac = macFromPath(path);
    }
    if (key == "Volume" && isType(value, G_VARIANT_TYPE_UINT16))
    {
      it->second.volume = g_variant_get_uint16(value);
      it->second.hasVolume = true;
    }
  }
}

void Bluetooth::clearBluez()
{
  std::lock_guard<std::mutex> lock(mutex_);
  adapterPath_.clear();
  discovering_ = false;
  pairable_ = false;
  devices_.clear();
  players_.clear();
}

void Bluetooth::clearBlueAlsa()
{
  std::lock_guard<std::mutex> lock(mutex_);
  pcms_.clear();
}

// ---------------------------------------------------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------------------------------------------------

std::vector<Bluetooth::Device> Bluetooth::devices() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<Device> result;
  for (const auto &d : devices_)
    if (!d.second.mac.empty())
      result.push_back(d.second);
  return result;
}

std::string Bluetooth::deviceName(const std::string &mac) const
{
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto &d : devices_)
    if (d.second.mac == mac)
      return d.second.name.empty() ? mac : d.second.name;
  return mac;
}

bool Bluetooth::isPaired(const std::string &mac) const
{
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto &d : devices_)
    if (d.second.mac == mac)
      return d.second.paired;
  return false;
}

bool Bluetooth::discovering() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return discovering_;
}

bool Bluetooth::pairable() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return pairable_;
}

std::string Bluetooth::streamMac() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return pcms_.empty() ? "" : pcms_.begin()->second.mac;
}

int Bluetooth::streamVolume() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (pcms_.empty() || !pcms_.begin()->second.hasVolume)
    return -1;
  return (pcms_.begin()->second.volume >> 8) & 0x7F;
}

std::optional<Bluetooth::Player> Bluetooth::player(const std::string &mac) const
{
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto &p : players_)
    if (p.second.mac == mac)
      return p.second;
  return std::nullopt;
}

// ---------------------------------------------------------------------------------------------------------------------
// commands
// ---------------------------------------------------------------------------------------------------------------------

void Bluetooth::setPairable(bool enable)
{
  postToBus([this, enable]()
            {
              const std::string adapter = adapterPath();
              if (!adapter.empty())
                busSetProperty(BLUEZ, adapter, IFACE_ADAPTER, "Pairable", g_variant_new_boolean(enable));
            });
}

void Bluetooth::startDiscovery()
{
  postToBus([this]()
            {
              const std::string adapter = adapterPath();
              if (!adapter.empty())
                busCall(BLUEZ, adapter, IFACE_ADAPTER, "StartDiscovery", nullptr, DEFAULT_TIMEOUT_MS);
            });
}

void Bluetooth::stopDiscovery()
{
  postToBus([this]()
            {
              const std::string adapter = adapterPath();
              if (!adapter.empty())
                busCall(BLUEZ, adapter, IFACE_ADAPTER, "StopDiscovery", nullptr, DEFAULT_TIMEOUT_MS);
            });
}

void Bluetooth::removeDevice(const std::string &mac)
{
  postToBus([this, mac]()
            {
              const std::string adapter = adapterPath();
              const std::string device = devicePath(mac);
              if (!adapter.empty() && !device.empty())
                busCall(BLUEZ, adapter, IFACE_ADAPTER, "RemoveDevice", g_variant_new("(o)", device.c_str()), DEFAULT_TIMEOUT_MS);
            });
}

void Bluetooth::removeUnpairedDevices()
{
  postToBus([this]()
            {
              const std::string adapter = adapterPath();
              std::vector<std::string> unpaired;
              {
                std::lock_guard<std::mutex> lock(mutex_);
                for (const auto &d : devices_)
                  if (!d.second.paired)
                    unpaired.push_back(d.first);
              }
              if (adapter.empty())
                return;
              for (const std::string &device : unpaired)
                busCall(BLUEZ, adapter, IFACE_ADAPTER, "RemoveDevice", g_variant_new("(o)", device.c_str()), DEFAULT_TIMEOUT_MS);
            });
}

void Bluetooth::removeAllPairedDevices(std::function<void()> done)
{
  postToBus([this, done]()
            {
              const std::string adapter = adapterPath();
              std::vector<std::string> paired;
              {
                std::lock_guard<std::mutex> lock(mutex_);
                for (const auto &d : devices_)
                  if (d.second.paired)
                    paired.push_back(d.first);
              }
              if (adapter.empty() || paired.empty())
              {
                if (done)
                  done();
                return;
              }
              auto remaining = std::make_shared<size_t>(paired.size());
              for (const std::string &device : paired)
                busCall(BLUEZ, adapter, IFACE_ADAPTER, "RemoveDevice", g_variant_new("(o)", device.c_str()), DEFAULT_TIMEOUT_MS,
                        [remaining, done](GVariant *, const GError *)
                        {
                          if (--*remaining == 0 && done)
                            done();
                        });
            });
}

void Bluetooth::connectDevice(const std::string &mac, const std::string &previousMac)
{
  postToBus([this, mac, previousMac]()
            {
              if (!previousMac.empty() && previousMac != mac)
              {
                const std::string previous = devicePath(previousMac);
                if (!previous.empty())
                {
                  busSetProperty(BLUEZ, previous, IFACE_DEVICE, "Trusted", g_variant_new_boolean(false));
                  busCallDevice(previous, "Disconnect", DEFAULT_TIMEOUT_MS);
                }
              }
              const std::string device = devicePath(mac);
              if (device.empty())
              {
                fprintf(stderr, "[bluez] connect: unknown device %s\n", mac.c_str());
                return;
              }
              auto trustAndConnect = [this, device]()
              {
                busSetProperty(BLUEZ, device, IFACE_DEVICE, "Trusted", g_variant_new_boolean(true));
                busCallDevice(device, "Connect", CONNECT_TIMEOUT_MS);
              };
              if (isPaired(mac))
                trustAndConnect();
              else
                busCallDevice(device, "Pair", PAIR_TIMEOUT_MS, [trustAndConnect](GVariant *, const GError *error)
                              {
                                if (error == nullptr)
                                  trustAndConnect();
                              });
            });
}

void Bluetooth::disconnectDevice(const std::string &mac)
{
  postToBus([this, mac]()
            {
              const std::string device = devicePath(mac);
              if (device.empty())
                return;
              busSetProperty(BLUEZ, device, IFACE_DEVICE, "Trusted", g_variant_new_boolean(false));
              busCallDevice(device, "Disconnect", DEFAULT_TIMEOUT_MS);
            });
}

void Bluetooth::playerCommand(const std::string &mac, const char *method)
{
  const std::string name = method; // method is a literal, but don't rely on it outliving the posted function
  postToBus([this, mac, name]()
            {
              std::string playerPath;
              {
                std::lock_guard<std::mutex> lock(mutex_);
                for (const auto &p : players_)
                  if (p.second.mac == mac)
                  {
                    playerPath = p.first;
                    break;
                  }
              }
              if (!playerPath.empty())
                busCall(BLUEZ, playerPath, IFACE_PLAYER, name.c_str(), nullptr, DEFAULT_TIMEOUT_MS);
            });
}

void Bluetooth::setPlayerProperty(const std::string &mac, const char *name, const char *value)
{
  const std::string property = name, newValue = value; // the literals may not outlive the posted function
  postToBus([this, mac, property, newValue]()
            {
              std::string playerPath;
              {
                std::lock_guard<std::mutex> lock(mutex_);
                for (const auto &p : players_)
                  if (p.second.mac == mac)
                  {
                    playerPath = p.first;
                    break;
                  }
              }
              if (!playerPath.empty())
                busSetProperty(BLUEZ, playerPath, IFACE_PLAYER, property.c_str(), g_variant_new_string(newValue.c_str()));
            });
}

void Bluetooth::setStreamVolume(int volume)
{
  const uint16_t clamped = static_cast<uint16_t>(volume < 0 ? 0 : volume > 127 ? 127 : volume);
  const uint16_t packed = static_cast<uint16_t>((clamped << 8) | clamped);
  std::string pcm;
  {
    // update the cache right away so that a read before BlueALSA's PropertiesChanged doesn't see the old value
    std::lock_guard<std::mutex> lock(mutex_);
    if (pcms_.empty())
      return;
    pcm = pcms_.begin()->first;
    pcms_.begin()->second.volume = packed;
    pcms_.begin()->second.hasVolume = true;
  }
  postToBus([this, pcm, packed]() { busSetProperty(BLUEALSA, pcm, IFACE_PCM, "Volume", g_variant_new_uint16(packed)); });
}
