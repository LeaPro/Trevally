#pragma once

#include <object.h>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

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
  void loadConfiguration();
  void runAutomation();

  bool isAuthenticated() const;

  void handleRegisterListener();
  void handleExchangeAuthCode();
  void handleRefreshToken();
  void handleFetchChannels();
  void handlePlayNow(bool forceNext);
  bool setMixerVolume(uint32_t percent);
  void setError(const std::string &msg);

  std::shared_ptr<StringSensor> statusPtr;
  std::shared_ptr<StringSensor> lastErrorPtr;
  std::shared_ptr<StringSensor> authUrlPtr;
  std::shared_ptr<JsonSensor> channelsPtr;
  std::shared_ptr<JsonSensor> nowPlayingPtr;
  std::shared_ptr<BoolSensor> isPlayingPtr;
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
  std::shared_ptr<BoolControl> playPtr;
  std::shared_ptr<BoolControl> nextPtr;

  std::unique_ptr<CustomChannelsClient> client;
  std::unique_ptr<UrlPlaybackProcess> playback;

  std::string apiUrl;
  std::string developerKey;
  json lastNowPlaying = json::object();

  bool pendingBootstrapRefresh = false;
  bool wasEnabled = false;
  uint32_t consecutiveAutoNextFailures = 0;
  std::chrono::steady_clock::time_point lastRefreshAttemptAt{};
  std::chrono::steady_clock::time_point lastChannelsFetchAt{};
  std::chrono::steady_clock::time_point lastAutoNextAttemptAt{};
};
