#pragma once
#include <object.h>
#include <cstdint>
#include <map>

#ifndef SOUNDTRACK_ENABLED
#define SOUNDTRACK_ENABLED 0
#endif

#if SOUNDTRACK_ENABLED
extern "C" {
struct splayer_api;
struct splayer_controls_api;
struct splayer_auth_api;
struct splayer_metadata_api;
struct splayer_library_api;
struct splayer_troubles_api;
struct splayer_audio_api;
typedef struct splayer splayer_t;
}

class Soundtrack final : public LeafObject
{
public:
  Soundtrack(std::string path, Object *parent);
  ~Soundtrack() override;

  void initialize() override;
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;

private:
  void initializeSdk();
  void shutdownSdk();
  void applyRestoredPlayState();
  void syncSensorState();
  void syncPairingResult();
  void syncTroubles();
  void syncMetadata();
  void handleControls();
  const char *authStatusToString(int status) const;
  const char *sourceTypeToString(int type) const;
  int sourceTypeFromString(const std::string &type) const;

  std::shared_ptr<BoolSensor> sdkReadyPtr;
  std::shared_ptr<BoolSensor> pairedPtr;
  std::shared_ptr<StringSensor> authStatusPtr;
  std::shared_ptr<StringSensor> deviceIdPtr;
  std::shared_ptr<BoolSensor> isPlayingPtr;
  std::shared_ptr<BoolSensor> isPausedPtr;
  std::shared_ptr<BoolControl> enabledPtr;
  std::shared_ptr<UInt32Control> volumePtr;
  std::shared_ptr<StringControl> pairCodePtr;
  std::shared_ptr<BoolControl> pairPtr;
  std::shared_ptr<BoolControl> unpairPtr;
  std::shared_ptr<BoolControl> playPtr;
  std::shared_ptr<BoolControl> nextPtr;
  std::shared_ptr<StringControl> playFromSourceIdPtr;
  std::shared_ptr<EnumControl> playFromSourceTypePtr;
  std::shared_ptr<BoolControl> playFromNowPtr;
  std::shared_ptr<JsonSensor> soundzonePtr;
  std::shared_ptr<JsonSensor> currentTrackPtr;
  std::shared_ptr<JsonSensor> libraryPtr;
  std::shared_ptr<JsonSensor> troublesPtr;
  std::shared_ptr<StringSensor> lastErrorPtr;

  std::map<int, const char *> sourceTypeMap;

  const splayer_api *api = nullptr;
  const splayer_controls_api *controlsApi = nullptr;
  const splayer_auth_api *authApi = nullptr;
  const splayer_metadata_api *metadataApi = nullptr;
  const splayer_library_api *libraryApi = nullptr;
  const splayer_troubles_api *troublesApi = nullptr;
  splayer_audio_api *audioCallbacks = nullptr;
  splayer_t *splayer = nullptr;
  bool sdkInitialized = false;
  bool restorePlayStatePending = false;
  bool wasEnabled = false;
};
#else
class Soundtrack final : public LeafObject
{
public:
  Soundtrack(std::string path, Object *parent);
};
#endif // SOUNDTRACK_ENABLED