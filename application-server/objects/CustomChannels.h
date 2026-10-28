#pragma once

#include "CustomChannelsClient.h"

#include <object.h>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

class CustomChannelsClient;
class UrlPlaybackProcess;

class CustomChannels final : public LeafObject
{
public:
  CustomChannels(std::string path, Object *parent);
  ~CustomChannels() override;

  void initialize() override;
  using LeafObject::update;
  void update(bool sensorsOnly = false, bool refreshVolatileElements = false) override;

private:
  enum class WorkerRequestKind : uint8_t {
    RegisterListener = 0,
    ExchangeAuthCode,
    RefreshToken,
    FetchChannels,
    PlayNow,
    Count
  };

  struct WorkerRequest {
    WorkerRequestKind kind = WorkerRequestKind::RegisterListener;
    uint64_t epoch = 0;
    bool fromAutomation = false;
    bool forceNext = false;
    std::string redirectUrl;
    std::string successUrl;
    std::string authCode;
    std::string refreshToken;
    std::string accessToken;
    std::string format;
    std::string zone;
    uint32_t channelId = 0;
  };

  struct WorkerResponse {
    WorkerRequestKind kind = WorkerRequestKind::RegisterListener;
    uint64_t epoch = 0;
    bool fromAutomation = false;
    bool forceNext = false;
    CustomChannelsClient::Response apiResponse;
    std::string localError;
    uint32_t channelId = 0;
  };

  void loadConfiguration();
  void startWorker();
  void stopWorker();
  void workerLoop();
  void drainWorkerResponses();
  void cancelPendingWork();
  bool enqueueWorkerRequest(const WorkerRequest &request);
  bool hasPendingOrInFlight(WorkerRequestKind kind) const;
  static size_t requestKindIndex(WorkerRequestKind kind);

  bool enqueueRegisterListener(bool fromAutomation);
  bool enqueueExchangeAuthCode(bool fromAutomation);
  bool enqueueRefreshToken(bool fromAutomation);
  bool enqueueFetchChannels(bool fromAutomation);
  bool enqueuePlayNow(bool forceNext, bool fromAutomation);

  void applyRegisterListenerResponse(const WorkerResponse &response);
  void applyExchangeAuthCodeResponse(const WorkerResponse &response);
  void applyRefreshTokenResponse(const WorkerResponse &response);
  void applyFetchChannelsResponse(const WorkerResponse &response);
  void applyPlayNowResponse(const WorkerResponse &response);

  void runAutomation();

  bool isAuthenticated() const;
  bool setMixerVolume(uint32_t percent);
  void setError(const std::string &msg);

  std::shared_ptr<StringSensor> statusPtr;
  std::shared_ptr<StringSensor> lastErrorPtr;
  std::shared_ptr<StringSensor> authUrlPtr;
  std::shared_ptr<JsonSensor> channelsPtr;
  std::shared_ptr<JsonSensor> nowPlayingPtr;
  std::shared_ptr<BoolSensor> isPlayingPtr;
  std::shared_ptr<BoolSensor> websiteReachablePtr;
  std::shared_ptr<BoolControl> enabledPtr;

  std::shared_ptr<StringControl> redirectUrlPtr;
  std::shared_ptr<StringControl> successUrlPtr;
  std::shared_ptr<BoolControl> registerListenerPtr;
  std::shared_ptr<StringControl> authCodePtr;
  std::shared_ptr<BoolControl> exchangeAuthCodePtr;
  std::shared_ptr<BoolControl> refreshTokenPtr;
  std::shared_ptr<BoolControl> fetchChannelsPtr;
  std::shared_ptr<UInt32Control> channelIdPtr;
  std::shared_ptr<StringControl> zonePtr;
  std::shared_ptr<StringControl> formatPtr;
  std::shared_ptr<StringControl> accessTokenPtr;
  std::shared_ptr<StringControl> refreshTokenValuePtr;
  std::shared_ptr<StringControl> accessExpiresAtPtr;
  std::shared_ptr<StringControl> refreshExpiresAtPtr;
  std::shared_ptr<UInt32Control> volumePtr;
  std::shared_ptr<BoolControl> mutePtr;
  std::shared_ptr<BoolControl> playPtr;
  std::shared_ptr<BoolControl> nextPtr;

  std::unique_ptr<CustomChannelsClient> client;
  std::unique_ptr<UrlPlaybackProcess> playback;

  std::string apiUrl;
  std::string developerKey;
  json lastNowPlaying = json::object();

  bool pendingBootstrapRefresh = false;
  bool pendingFetchAfterRefresh = false;
  bool volumeRestorePending = false;
  bool wasEnabled = false;
  uint32_t consecutiveAutoNextFailures = 0;
  std::chrono::steady_clock::time_point lastRefreshAttemptAt{};
  std::chrono::steady_clock::time_point lastChannelsFetchAt{};
  std::chrono::steady_clock::time_point lastAutoNextAttemptAt{};
  std::chrono::steady_clock::time_point lastVolumeRestoreAttemptAt{};
  std::chrono::steady_clock::time_point lastWebsiteReachabilityCheckAt{};
  bool lastWebsiteReachable = false;

  mutable std::mutex workerMutex;
  std::condition_variable workerCv;
  std::thread workerThread;
  bool workerStopRequested = false;
  uint64_t workerEpoch = 0;
  std::deque<WorkerRequest> requestQueue;
  std::deque<WorkerResponse> responseQueue;
  std::array<size_t, static_cast<size_t>(WorkerRequestKind::Count)> queuedCounts{};
  std::array<size_t, static_cast<size_t>(WorkerRequestKind::Count)> inflightCounts{};
};
