#pragma once
#include <object.h>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// forward declarations so that users of this header don't need the GLib headers
struct _GDBusConnection;
struct _GMainContext;
struct _GMainLoop;
struct _GVariant;
struct _GError;

// Bluetooth audio sink: pairing, connecting, volume and playback of the connected phone.
//
// BlueZ (org.bluez) and BlueALSA (org.bluealsa) are accessed over D-Bus on the system bus. A worker thread runs a GLib main loop that keeps a
// cache of the adapter, devices, media players and A2DP PCMs up to date from the ObjectManager / PropertiesChanged signals. update() reads
// that cache; the commands are executed asynchronously on the worker thread (in the order they were issued) and failures are logged to stderr.
class Bluetooth final : public LeafObject
{
public:
  Bluetooth(std::string path, Object *parent);
  ~Bluetooth() override;
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;

  // Device strings have the form "Name (AA:BB:CC:DD:EE:FF)" (connectedDevice, and each entry of pairedDevices / discoveredDevices)
  static bool isValidMac(const std::string &mac);
  static std::string formatDevice(const std::string &name, const std::string &mac);
  static bool parseDevice(const std::string &device, std::string &name, std::string &mac); // false if not in the form above
private:
  struct Device
  {
    std::string mac;     // "AA:BB:CC:DD:EE:FF"
    std::string name;    // friendly name (alias), the MAC when the device has none
    bool paired = false;
  };
  // A device's AVRCP media player: playback state and current track
  struct Player
  {
    std::string mac;
    bool playing = false;
    std::string title, artist, album;
    std::string imgHandle; // handle of the track's cover art, valid while an OBEX BIP connection to obexPort exists; empty when there is none
    uint16_t obexPort = 0; // L2CAP PSM of the phone's OBEX (BIP) server, 0 when it has none
    uint32_t durationMs = 0;                       // length of the current track, 0 when the phone doesn't say
    uint32_t positionMs = 0;                       // playback position at positionAt
    std::chrono::steady_clock::time_point positionAt{}; // when positionMs was received / the player last started or stopped
    // The current playback position: positionMs, advanced by the time passed since positionAt while playing
    uint32_t position(std::chrono::steady_clock::time_point now) const
    {
      uint64_t p = positionMs;
      if (playing && positionAt != std::chrono::steady_clock::time_point{})
        p += std::chrono::duration_cast<std::chrono::milliseconds>(now - positionAt).count();
      if (durationMs != 0 && p > durationMs)
        p = durationMs;
      return static_cast<uint32_t>(p);
    }
  };
  // The cover art wanted by the main thread: the image of the connected phone's current track
  struct ArtRequest
  {
    std::string mac, handle;
    uint16_t port = 0;
    bool operator==(const ArtRequest &other) const { return mac == other.mac && handle == other.handle && port == other.port; }
  };
  struct PcmInfo
  {
    std::string mac;
    bool hasVolume = false;
    uint16_t volume = 0; // channel 1 in the high byte, channel 2 in the low byte; bit 7 of each is the mute flag
  };
  using Reply = std::function<void(_GVariant *result, const _GError *error)>;

  // ---- element handling (main thread) ----
  template <typename Predicate>
  static std::string formatDevices(const std::vector<Device> &devices, Predicate keep);
  void syncExternalPairing(std::chrono::steady_clock::time_point now);
  void handleScan(std::chrono::steady_clock::time_point now);
  void handleRemoveDevice();
  void handleRequestConnect(std::chrono::steady_clock::time_point now);
  void handleDisconnect();
  void handleReset();
  void updateDeviceLists();
  void updateConnectedDevice();
  void updateTrackMetadata();
  void updateCoverArt();
  void syncVolume();
  void syncPlayback(std::chrono::steady_clock::time_point now);
  void handleTrackButtons();

  // ---- BlueZ / BlueALSA state: snapshots of the cache, callable from any thread ----
  std::vector<Device> devices() const;             // all devices BlueZ knows about, ordered by object path
  std::string deviceName(const std::string &mac) const; // friendly name, or mac when unknown
  bool isPaired(const std::string &mac) const;
  bool discovering() const;                        // adapter is scanning
  bool pairable() const;                           // adapter accepts pairing requests
  std::string streamMac() const;                   // phone that has an A2DP sink stream (BlueALSA PCM), empty when none
  int streamVolume() const;                        // volume of that stream, 0-127 (left channel); -1 when unavailable
  std::optional<Player> player(const std::string &mac) const; // media player of the device, if it has one
  std::string coverArtFile(const ArtRequest &request, int kind) const; // image file of that kind fetched for the request, empty when it isn't available (yet)

  // ---- BlueZ / BlueALSA commands (asynchronous) ----
  void setPairable(bool enable);
  void startDiscovery();
  void stopDiscovery();
  void removeDevice(const std::string &mac);
  void removeUnpairedDevices();                    // drops the cached devices found by scanning
  void removeAllPairedDevices(std::function<void()> done = nullptr); // done runs on the worker thread once BlueZ has answered every removal
  // Connects to the device, pairing it first when it isn't paired. When previousMac is not empty (and differs) that device is untrusted and
  // disconnected first: the target's pairing agent reconnects trusted devices as soon as they drop.
  void connectDevice(const std::string &mac, const std::string &previousMac);
  void disconnectDevice(const std::string &mac);   // untrusts and disconnects; the device stays paired
  void playerCommand(const std::string &mac, const char *method); // "Play", "Pause", "Next" or "Previous"
  void setStreamVolume(int volume);                // 0-127, both channels, unmuted

  // ---- D-Bus plumbing (worker thread unless noted) ----
  void busMain();
  void postToBus(std::function<void()> fn);        // any thread
  void busCall(const char *bus, const std::string &path, const char *iface, const char *method, _GVariant *params, int timeoutMs, Reply done = nullptr);
  void busSetProperty(const char *bus, const std::string &path, const char *iface, const char *name, _GVariant *value);
  void busCallDevice(const std::string &path, const char *method, int timeoutMs, Reply done = nullptr);

  // ---- cover art over OBEX (worker thread) ----
  // obexd (org.bluez.obex) runs on a private session bus. For the wanted track it keeps one BIP session to the phone and fetches the images of
  // the current ImgHandle into files, the thumbnail first and then the native image; every step re-enters artPump(), which decides what to do
  // next from the state below.
  enum ArtKind { ART_THUMBNAIL = 0, ART_NATIVE = 1, ART_KINDS = 2 }; // thumbnail: always a 200x200 JPEG; native: whatever the phone has
  bool obexConnect();                              // connects to the obexd bus if not done yet; false when unavailable
  void artPump();
  void artBeginSession();
  void artEndSession();                            // removes the session and forgets everything fetched through it
  void artStartNext();                             // starts the transfer of the next image kind of the handle being fetched, if any is left
  void artDropTransfer();                          // cancels the transfer in flight and deletes its file
  void artTransferChanged(const std::string &transferPath, const std::string &status);
  void artFailed(const std::string &handle);

  std::string devicePath(const std::string &mac) const;
  std::string adapterPath() const;

  void loadManagedObjects(const char *bus, const char *rootPath);
  void addObject(const std::string &path, _GVariant *interfaces);
  void removeInterfaces(const std::string &path, _GVariant *interfaceNames);
  void applyProperties(const std::string &path, const std::string &iface, _GVariant *props, bool create);
  void applyProperty(const std::string &path, const std::string &iface, const std::string &key, _GVariant *value, bool create);
  void clearBluez();
  void clearBlueAlsa();

  _GMainContext *context_ = nullptr;
  _GMainLoop *loop_ = nullptr;
  _GDBusConnection *conn_ = nullptr;
  _GDBusConnection *obexConn_ = nullptr;           // private session bus of obexd, connected on demand
  bool obexUnavailableLogged_ = false;
  std::thread thread_;

  mutable std::mutex mutex_; // guards the cache below
  std::string adapterPath_;
  bool discovering_ = false;
  bool pairable_ = false;
  std::map<std::string, Device> devices_;      // by object path
  std::map<std::string, Player> players_;      // by object path
  std::map<std::string, PcmInfo> pcms_;        // by object path; only A2DP sink PCMs
  struct ArtImage { std::string mac, handle, file; };
  ArtImage art_[ART_KINDS];                    // the fetched cover art: the image file for that handle of that phone

  // cover art fetching state (worker thread only)
  ArtRequest artWanted_;
  std::string artSessionMac_;                  // phone of the OBEX session being created or established, empty when there is none
  std::string artSessionPath_;                 // object path of the established session
  unsigned artSessionSerial_ = 0;              // identifies the session being created, so that replies for abandoned attempts are recognised
  std::string artFetchHandle_;                 // handle whose images are being / have been fetched through the session
  int artNextKind_ = ART_KINDS;                // next image kind to fetch for artFetchHandle_ (ART_KINDS: all started)
  int artTransferKind_ = ART_KINDS;            // kind of the transfer in flight
  int artFetched_ = 0;                         // images of artFetchHandle_ downloaded so far
  std::string artFailedHandle_;                // handle that failed to download; not retried
  std::string artTransferPath_;                // object path of the transfer in flight (empty until obexd has answered)
  std::string artTransferFile_;                // file the transfer in flight writes to; empty when no transfer is in flight
  unsigned artCounter_ = 0;                    // makes the image file names unique, so a changed file name tells the UI that the image changed

  // ---- elements ----
  std::shared_ptr<BoolControl> scanPtr;
  std::shared_ptr<BoolControl> enableExternalPairingPtr;
  std::shared_ptr<BoolControl> resetPtr;
  std::shared_ptr<BoolControl> disconnectDevicePtr;
  std::shared_ptr<StringControl> removeDevicePtr;
  std::shared_ptr<StringControl> requestConnectPtr;
  std::shared_ptr<UInt32Control> volumePtr;
  std::shared_ptr<StringSensor> pairedDevicesPtr;
  std::shared_ptr<StringSensor> discoveredDevicesPtr;
  std::shared_ptr<StringSensor> connectedDevicePtr;
  std::shared_ptr<StringSensor> pairingCodePtr;
  std::shared_ptr<StringSensor> trackTitlePtr;
  std::shared_ptr<StringSensor> trackArtistPtr;
  std::shared_ptr<StringSensor> trackAlbumPtr;
  std::shared_ptr<StringSensor> trackArtPtr;
  std::shared_ptr<StringSensor> trackArtThumbnailPtr;
  std::shared_ptr<UInt32Sensor> trackDurationPtr;
  std::shared_ptr<UInt32Sensor> trackPositionPtr;
  std::shared_ptr<BoolControl> enablePtr;
  std::shared_ptr<BoolSensor> isPlayingPtr;
  std::shared_ptr<BoolControl> playPtr;
  std::shared_ptr<BoolControl> nextPtr;
  std::shared_ptr<BoolControl> previousPtr;

  std::string connectedMac;                  // MAC of the device connectedDevice currently describes (empty when none)
  ArtRequest lastArtRequest;                 // cover art last asked of the worker thread
  int lastStreamVolume = -1;                 // last stream volume (0-127) seen on / sent to BlueALSA
  bool playing = false;                      // phone's playback state at the last update
  bool playPending = false;                  // a play/pause command was sent and the phone has not applied it yet
  bool playPendingValue = false;             // what that command asked for
  std::chrono::steady_clock::time_point statusHoldUntil{}; // after a play/pause command, ignore the phone's state until it had time to apply it
  bool scanning = false;                     // scan requested and not ended yet
  bool discoveryConfirmed = false;           // BlueZ reported that discovery is running since the scan was requested
  std::chrono::steady_clock::time_point scanRequestedAt{};
  std::chrono::steady_clock::time_point pairingUntil{};   // end of the window in which a requestConnect may pair (adapter must be bondable)
  std::chrono::steady_clock::time_point lastPairableSet{};
  std::chrono::steady_clock::time_point lastPairingCodePoll{};
};
