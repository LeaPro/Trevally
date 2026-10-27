#include "objects.h"
#include "CustomChannelsClient.h"
#include <Common.h>

#include <csignal>
#include <cstdlib>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using std::string;

string getenvOrDefault(const char *name, const string &fallback)
{
  const char *value = std::getenv(name);
  if (value == nullptr || value[0] == '\0') {
    return fallback;
  }
  return value;
}

string jsonStringOrEmpty(const json &j, const char *key)
{
  if (!j.is_object() || !j.contains(key) || !j[key].is_string()) {
    return "";
  }
  return j[key].get<string>();
}

bool jsonStatusSuccess(const json &j)
{
  return j.is_object() && j.contains("status") && j["status"].is_string() && j["status"].get<string>() == "success";
}
namespace {
  using Clock = std::chrono::steady_clock;
  using Seconds = std::chrono::seconds;
  using Minutes = std::chrono::minutes;

  constexpr auto autoRefreshInterval = Minutes(20);
  constexpr auto autoChannelsFetchInterval = Minutes(15);
  constexpr auto autoNextRetryDelay = Seconds(1);
  constexpr auto websiteReachabilityProbeInterval = Seconds(2);
  constexpr uint32_t maxAutoNextFailures = 5;

  std::string chooseIdentifier()
  {
    char hostname[256] = {0};
    if (gethostname(hostname, sizeof(hostname) - 1) == 0 && hostname[0] != '\0')
    {
      return std::string(hostname);
    }
    return "trevally-player";
  }
  std::string chooseDisplayName()
  {
    return chooseIdentifier();
  }
}

class UrlPlaybackProcess {
public:
  bool start(const string &url)
  {
    stop();

    const string baseCmd = getenvOrDefault(
      "CUSTOMCHANNELS_PLAYER_CMD",
      "ffmpeg -nostdin -loglevel warning -i \"%URL%\" -f wav - 2>/tmp/customchannels-ffmpeg.log | aplay -D net_audio 2>/tmp/customchannels-aplay.log");

    string cmd = baseCmd;
    const size_t tokenPos = cmd.find("%URL%");
    if (tokenPos != string::npos) {
      cmd.replace(tokenPos, 5, url);
    } else {
      cmd += " \"" + url + "\"";
    }

    const pid_t child = fork();
    if (child == 0) {
      setpgid(0, 0);
      execl("/bin/sh", "sh", "-c", cmd.c_str(), (char *)nullptr);
      _exit(127);
    }

    if (child < 0) {
      return false;
    }

    // Ensure the launched command runs in its own process group so stop()
    // can terminate shell pipelines and descendants reliably.
    setpgid(child, child);
    pid = child;
    return true;
  }

  bool stop()
  {
    if (pid <= 0) {
      return true;
    }

    int status = 0;

    // Already exited or not a direct child anymore.
    pid_t rc = waitpid(pid, &status, WNOHANG);
    if (rc == pid || (rc < 0 && errno == ECHILD)) {
      pid = -1;
      return true;
    }

    signalPlayback(SIGTERM);

    for (int i = 0; i < 30; ++i) {
      rc = waitpid(pid, &status, WNOHANG);
      if (rc == pid || (rc < 0 && errno == ECHILD)) {
        pid = -1;
        return true;
      }
      usleep(50 * 1000);
    }

    signalPlayback(SIGKILL);

    for (int i = 0; i < 20; ++i) {
      rc = waitpid(pid, &status, WNOHANG);
      if (rc == pid || (rc < 0 && errno == ECHILD)) {
        pid = -1;
        return true;
      }
      usleep(50 * 1000);
    }

    return false;
  }

  bool isRunning()
  {
    if (pid <= 0) {
      return false;
    }

    int status = 0;
    const pid_t rc = waitpid(pid, &status, WNOHANG);
    if (rc == 0) {
      return true;
    }

    if (rc < 0 && errno != ECHILD) {
      return true;
    }

    pid = -1;
    return false;
  }

private:
  void signalPlayback(int signo)
  {
    if (pid <= 0) {
      return;
    }

    // Signal process group first, then direct child as fallback.
    if (kill(-pid, signo) != 0 && errno != ESRCH) {
      (void)kill(pid, signo);
      return;
    }

    (void)kill(pid, signo);
  }

  pid_t pid = -1;
};

CustomChannels::CustomChannels(string path, Object *parent)
  : LeafObject(path, parent)
{
  string name;

  name = "enable"; elements[name] = enabledPtr = std::make_shared<BoolControl>(path + "/" + name, this, false);
  name = "status"; elements[name] = statusPtr = std::make_shared<StringSensor>(path + "/" + name, this, "not_configured", 64);
  name = "lastError"; elements[name] = lastErrorPtr = std::make_shared<StringSensor>(path + "/" + name, this, "", 1024);
  name = "websiteReachable"; elements[name] = websiteReachablePtr = std::make_shared<BoolSensor>(path + "/" + name, this, false);

  name = "registerListener"; elements[name] = registerListenerPtr = std::make_shared<BoolControl>(path + "/" + name, this, false, false, true);
  name = "authUrl"; elements[name] = authUrlPtr = std::make_shared<StringSensor>(path + "/" + name, this, "", 1024);
  name = "authCode"; elements[name] = authCodePtr = std::make_shared<StringControl>(path + "/" + name, this, "", 256, false, true);
  name = "exchangeAuthCode"; elements[name] = exchangeAuthCodePtr = std::make_shared<BoolControl>(path + "/" + name, this, false, false, true);
  name = "redirectUrl"; elements[name] = redirectUrlPtr = std::make_shared<StringControl>(path + "/" + name, this, "https://localhost/customchannels/callback", 512, true, true);
  name = "successUrl"; elements[name] = successUrlPtr = std::make_shared<StringControl>(path + "/" + name, this, "", 512, true, true);
  name = "accessToken"; elements[name] = accessTokenPtr = std::make_shared<StringControl>(path + "/" + name, this, "", 2048, true, false);
  name = "refreshToken"; elements[name] = refreshTokenValuePtr = std::make_shared<StringControl>(path + "/" + name, this, "", 2048, true, false);
  name = "accessExpiresAt"; elements[name] = accessExpiresAtPtr = std::make_shared<StringControl>(path + "/" + name, this, "", 128, true, false);
  name = "refreshExpiresAt"; elements[name] = refreshExpiresAtPtr = std::make_shared<StringControl>(path + "/" + name, this, "", 128, true, false);
  name = "refreshAccess"; elements[name] = refreshTokenPtr = std::make_shared<BoolControl>(path + "/" + name, this, false, false, true);
 
  name = "zone"; elements[name] = zonePtr = std::make_shared<StringControl>(path + "/" + name, this, "", 128, true, true);
  name = "format"; elements[name] = formatPtr = std::make_shared<StringControl>(path + "/" + name, this, "", 16, true, true);
  name = "fetchChannels"; elements[name] = fetchChannelsPtr = std::make_shared<BoolControl>(path + "/" + name, this, false, false, true);
  name = "channels"; elements[name] = channelsPtr = std::make_shared<JsonSensor>(path + "/" + name, this, json::array());
  name = "channelId"; elements[name] = channelIdPtr = std::make_shared<UInt32Control>(path + "/" + name, this, 0, 99999999, 0, "id", true, true);

  name = "nowPlaying"; elements[name] = nowPlayingPtr = std::make_shared<JsonSensor>(path + "/" + name, this, json::object());
  name = "isPlaying"; elements[name] = isPlayingPtr = std::make_shared<BoolSensor>(path + "/" + name, this, false);
  name = "play"; elements[name] = playPtr = std::make_shared<BoolControl>(path + "/" + name, this, false, true, true);
  name = "next"; elements[name] = nextPtr = std::make_shared<BoolControl>(path + "/" + name, this, false, false, true);
  name = "volume"; elements[name] = volumePtr = std::make_shared<UInt32Control>(path + "/" + name, this, 0, 100, 100, "percent", true, true);
  name = "mute"; elements[name] = mutePtr = std::make_shared<BoolControl>(path + "/" + name, this, false, true, true);
}

CustomChannels::~CustomChannels()
{
  stopWorker();
  if (playback != nullptr) {
    playback->stop();
  }
}

void CustomChannels::initialize()
{
  LeafObject::initialize();
  loadConfiguration();
  playback = std::make_unique<UrlPlaybackProcess>();
  startWorker();
  wasEnabled = enabledPtr->get();
}

void CustomChannels::loadConfiguration()
{
  apiUrl = getenvOrDefault("CUSTOMCHANNELS_API_URL", "https://pro.customchannels.net");
  #if true //  use WAP developer key by default for testing
  developerKey = getenvOrDefault("CUSTOMCHANNELS_DEVELOPER_KEY", "4be65ee10394693ef79672a525fb5e48");
  #else
  developerKey = getenvOrDefault("CUSTOMCHANNELS_DEVELOPER_KEY", "");
  #endif

  client = std::make_unique<CustomChannelsClient>(apiUrl, developerKey);
  if (!client->isConfigured()) {
    statusPtr->set("not_configured");
    setError("Custom Channels is not configured. Set CUSTOMCHANNELS_DEVELOPER_KEY and optionally CUSTOMCHANNELS_API_URL");
    return;
  }

  if (isAuthenticated()) {
    statusPtr->set("authenticated");
    pendingBootstrapRefresh = true;
  } else {
    statusPtr->set("ready");
  }
  lastErrorPtr->set("");
}

size_t CustomChannels::requestKindIndex(WorkerRequestKind kind)
{
  return static_cast<size_t>(kind);
}

void CustomChannels::startWorker()
{
  std::lock_guard<std::mutex> lock(workerMutex);
  if (workerThread.joinable()) {
    return;
  }

  workerStopRequested = false;
  workerThread = std::thread(&CustomChannels::workerLoop, this);
}

void CustomChannels::stopWorker()
{
  {
    std::lock_guard<std::mutex> lock(workerMutex);
    workerStopRequested = true;
  }
  workerCv.notify_all();

  if (workerThread.joinable()) {
    workerThread.join();
  }

  std::lock_guard<std::mutex> lock(workerMutex);
  requestQueue.clear();
  responseQueue.clear();
  queuedCounts.fill(0);
  inflightCounts.fill(0);
}

bool CustomChannels::enqueueWorkerRequest(const WorkerRequest &request)
{
  std::lock_guard<std::mutex> lock(workerMutex);
  if (workerStopRequested) {
    return false;
  }

  requestQueue.push_back(request);
  ++queuedCounts[requestKindIndex(request.kind)];
  workerCv.notify_one();
  return true;
}

bool CustomChannels::hasPendingOrInFlight(WorkerRequestKind kind) const
{
  std::lock_guard<std::mutex> lock(workerMutex);
  const size_t idx = requestKindIndex(kind);
  return queuedCounts[idx] > 0 || inflightCounts[idx] > 0;
}

void CustomChannels::cancelPendingWork()
{
  std::lock_guard<std::mutex> lock(workerMutex);
  ++workerEpoch;
  requestQueue.clear();
  responseQueue.clear();
  queuedCounts.fill(0);
}

void CustomChannels::workerLoop()
{
  while (true)
  {
    WorkerRequest request;
    {
      std::unique_lock<std::mutex> lock(workerMutex);
      workerCv.wait(lock, [&]() { return workerStopRequested || !requestQueue.empty(); });
      if (workerStopRequested && requestQueue.empty()) {
        return;
      }

      request = requestQueue.front();
      requestQueue.pop_front();

      const size_t idx = requestKindIndex(request.kind);
      if (queuedCounts[idx] > 0) {
        --queuedCounts[idx];
      }
      ++inflightCounts[idx];
    }

    WorkerResponse response;
    response.kind = request.kind;
    response.epoch = request.epoch;
    response.fromAutomation = request.fromAutomation;
    response.forceNext = request.forceNext;
    response.channelId = request.channelId;

    if (client == nullptr || !client->isConfigured()) {
      response.localError = "Custom Channels client unavailable";
    } else {
      switch (request.kind)
      {
        case WorkerRequestKind::RegisterListener:
          response.apiResponse = client->registerListener(
            chooseIdentifier(), request.redirectUrl, request.successUrl, chooseDisplayName());
          break;

        case WorkerRequestKind::ExchangeAuthCode:
          response.apiResponse = client->requestAccessToken(request.authCode);
          break;

        case WorkerRequestKind::RefreshToken:
          response.apiResponse = client->refreshAccessToken(request.refreshToken);
          break;

        case WorkerRequestKind::FetchChannels:
          response.apiResponse = client->getChannels(request.accessToken);
          break;

        case WorkerRequestKind::PlayNow:
          response.apiResponse = client->getNowPlaying(
            request.accessToken,
            request.channelId,
            request.format,
            request.zone);
          break;

        case WorkerRequestKind::Count:
          response.localError = "Invalid worker request kind";
          break;
      }
    }

    {
      std::lock_guard<std::mutex> lock(workerMutex);
      responseQueue.push_back(std::move(response));
    }
  }
}

bool CustomChannels::enqueueRegisterListener(bool fromAutomation)
{
  WorkerRequest request;
  request.kind = WorkerRequestKind::RegisterListener;
  request.fromAutomation = fromAutomation;
  request.forceNext = false;
  request.redirectUrl = redirectUrlPtr->get();
  request.successUrl = successUrlPtr->get();
  {
    std::lock_guard<std::mutex> lock(workerMutex);
    request.epoch = workerEpoch;
  }

  if (!enqueueWorkerRequest(request)) {
    setError("Custom Channels worker unavailable");
    registerListenerPtr->set(false);
    return false;
  }

  return true;
}

bool CustomChannels::enqueueExchangeAuthCode(bool fromAutomation)
{
  const string code = authCodePtr->get();
  if (code.empty()) {
    setError("authCode is empty");
    exchangeAuthCodePtr->set(false);
    return false;
  }

  WorkerRequest request;
  request.kind = WorkerRequestKind::ExchangeAuthCode;
  request.fromAutomation = fromAutomation;
  request.authCode = code;
  {
    std::lock_guard<std::mutex> lock(workerMutex);
    request.epoch = workerEpoch;
  }

  if (!enqueueWorkerRequest(request)) {
    setError("Custom Channels worker unavailable");
    exchangeAuthCodePtr->set(false);
    return false;
  }

  return true;
}

bool CustomChannels::enqueueRefreshToken(bool fromAutomation)
{
  const string refreshToken = refreshTokenValuePtr->get();
  if (refreshToken.empty()) {
    setError("No refresh token available");
    if (!fromAutomation) {
      refreshTokenPtr->set(false);
    }
    return false;
  }

  WorkerRequest request;
  request.kind = WorkerRequestKind::RefreshToken;
  request.fromAutomation = fromAutomation;
  request.refreshToken = refreshToken;
  {
    std::lock_guard<std::mutex> lock(workerMutex);
    request.epoch = workerEpoch;
  }

  if (!enqueueWorkerRequest(request)) {
    setError("Custom Channels worker unavailable");
    if (!fromAutomation) {
      refreshTokenPtr->set(false);
    }
    return false;
  }

  lastRefreshAttemptAt = Clock::now();
  return true;
}

bool CustomChannels::enqueueFetchChannels(bool fromAutomation)
{
  const string accessToken = accessTokenPtr->get();
  if (accessToken.empty()) {
    setError("Access token not available");
    if (!fromAutomation) {
      fetchChannelsPtr->set(false);
    }
    return false;
  }

  WorkerRequest request;
  request.kind = WorkerRequestKind::FetchChannels;
  request.fromAutomation = fromAutomation;
  request.accessToken = accessToken;
  {
    std::lock_guard<std::mutex> lock(workerMutex);
    request.epoch = workerEpoch;
  }

  if (!enqueueWorkerRequest(request)) {
    setError("Custom Channels worker unavailable");
    if (!fromAutomation) {
      fetchChannelsPtr->set(false);
    }
    return false;
  }

  return true;
}

bool CustomChannels::enqueuePlayNow(bool forceNext, bool fromAutomation)
{
  const string accessToken = accessTokenPtr->get();
  if (accessToken.empty()) {
    if (!refreshTokenValuePtr->get().empty()) {
      pendingBootstrapRefresh = true;
      setError("Access token not available; attempting token refresh");
    } else {
      setError("Access token not available");
    }
    if (forceNext) {
      nextPtr->set(false);
    }
    return false;
  }

  const uint32_t channelId = channelIdPtr->get();
  if (channelId == 0) {
    setError("channelId must be set before play");
    if (forceNext) {
      nextPtr->set(false);
    }
    return false;
  }

  WorkerRequest request;
  request.kind = WorkerRequestKind::PlayNow;
  request.fromAutomation = fromAutomation;
  request.forceNext = forceNext;
  request.accessToken = accessToken;
  request.channelId = channelId;
  request.format = formatPtr->get();
  request.zone = zonePtr->get();
  {
    std::lock_guard<std::mutex> lock(workerMutex);
    request.epoch = workerEpoch;
  }

  if (!enqueueWorkerRequest(request)) {
    setError("Custom Channels worker unavailable");
    if (forceNext) {
      nextPtr->set(false);
    }
    return false;
  }

  return true;
}

void CustomChannels::applyRegisterListenerResponse(const WorkerResponse &response)
{
  registerListenerPtr->set(false);

  if (!response.localError.empty()) {
    setError(response.localError);
    statusPtr->set("auth_required");
    return;
  }

  if (!response.apiResponse.transportOk || !jsonStatusSuccess(response.apiResponse.body)) {
    const string apiMessage = jsonStringOrEmpty(response.apiResponse.body, "message");
    setError(apiMessage.empty() ?
      (response.apiResponse.error.empty() ? "register listener failed" : response.apiResponse.error) :
      apiMessage);
    statusPtr->set("auth_required");
    return;
  }

  authUrlPtr->set(jsonStringOrEmpty(response.apiResponse.body, "auth_url"));
  const string authCode = jsonStringOrEmpty(response.apiResponse.body, "auth_code");
  if (!authCode.empty()) {
    authCodePtr->set(authCode);
  }
  statusPtr->set("awaiting_user_authorization");
  setError("");
}

void CustomChannels::applyExchangeAuthCodeResponse(const WorkerResponse &response)
{
  exchangeAuthCodePtr->set(false);

  if (!response.localError.empty()) {
    setError(response.localError);
    statusPtr->set("auth_required");
    return;
  }

  if (!response.apiResponse.transportOk || !jsonStatusSuccess(response.apiResponse.body)) {
    const string apiMessage = jsonStringOrEmpty(response.apiResponse.body, "message");
    setError(apiMessage.empty() ?
      (response.apiResponse.error.empty() ? "access token exchange failed" : response.apiResponse.error) :
      apiMessage);
    statusPtr->set("auth_required");
    return;
  }

  const string accessToken = jsonStringOrEmpty(response.apiResponse.body, "access_token");
  const string refreshToken = jsonStringOrEmpty(response.apiResponse.body, "refresh_token");
  const string accessExpiresAt = jsonStringOrEmpty(response.apiResponse.body, "access_expires_at");
  const string refreshExpiresAt = jsonStringOrEmpty(response.apiResponse.body, "refresh_expires_at");

  if (accessToken.empty()) {
    setError("missing access token in response");
    statusPtr->set("auth_required");
    return;
  }

  accessTokenPtr->set(accessToken);
  refreshTokenValuePtr->set(refreshToken);
  accessExpiresAtPtr->set(accessExpiresAt);
  refreshExpiresAtPtr->set(refreshExpiresAt);

  statusPtr->set("authenticated");
  setError("");
}

void CustomChannels::applyRefreshTokenResponse(const WorkerResponse &response)
{
  refreshTokenPtr->set(false);

  if (!response.localError.empty()) {
    setError(response.localError);
    pendingFetchAfterRefresh = false;
    return;
  }

  if (!response.apiResponse.transportOk || !jsonStatusSuccess(response.apiResponse.body)) {
    const string apiMessage = jsonStringOrEmpty(response.apiResponse.body, "message");
    setError(apiMessage.empty() ?
      (response.apiResponse.error.empty() ? "token refresh failed" : response.apiResponse.error) :
      apiMessage);

    // Keep existing tokens for transient transport/service failures.
    // Only force re-auth when the provider explicitly rejects credentials.
    const bool authRejected = response.apiResponse.transportOk &&
      (response.apiResponse.httpStatus == 401 || response.apiResponse.httpStatus == 403);
    if (authRejected) {
      accessTokenPtr->set("");
      refreshTokenValuePtr->set("");
      accessExpiresAtPtr->set("");
      refreshExpiresAtPtr->set("");
      statusPtr->set("auth_required");
      pendingBootstrapRefresh = false;
    } else {
      pendingBootstrapRefresh = !refreshTokenValuePtr->get().empty();
    }
    pendingFetchAfterRefresh = false;
    return;
  }

  const string refreshedAccessToken = jsonStringOrEmpty(response.apiResponse.body, "access_token");
  const string refreshedRefreshToken = jsonStringOrEmpty(response.apiResponse.body, "refresh_token");
  if (refreshedAccessToken.empty() || refreshedRefreshToken.empty()) {
    setError("token refresh response missing token fields");
    pendingBootstrapRefresh = !refreshTokenValuePtr->get().empty();
    pendingFetchAfterRefresh = false;
    return;
  }

  accessTokenPtr->set(refreshedAccessToken);
  refreshTokenValuePtr->set(refreshedRefreshToken);
  accessExpiresAtPtr->set(jsonStringOrEmpty(response.apiResponse.body, "access_expires_at"));
  refreshExpiresAtPtr->set(jsonStringOrEmpty(response.apiResponse.body, "refresh_expires_at"));

  statusPtr->set("authenticated");
  pendingFetchAfterRefresh = true;
  setError("");
}

void CustomChannels::applyFetchChannelsResponse(const WorkerResponse &response)
{
  fetchChannelsPtr->set(false);

  if (!response.localError.empty()) {
    setError(response.localError);
    return;
  }

  if (!response.apiResponse.transportOk || !jsonStatusSuccess(response.apiResponse.body)) {
    const string apiMessage = jsonStringOrEmpty(response.apiResponse.body, "message");
    setError(apiMessage.empty() ?
      (response.apiResponse.error.empty() ? "fetch channels failed" : response.apiResponse.error) :
      apiMessage);
    return;
  }

  if (response.apiResponse.body.contains("channels") && response.apiResponse.body["channels"].is_array()) {
    channelsPtr->set(response.apiResponse.body["channels"]);
  }

  lastChannelsFetchAt = Clock::now();
  pendingFetchAfterRefresh = false;
  setError("");
}

void CustomChannels::applyPlayNowResponse(const WorkerResponse &response)
{
  if (response.forceNext) {
    nextPtr->set(false);
  }

  auto markAutoFailure = [&]() {
    if (!response.fromAutomation) {
      return;
    }

    ++consecutiveAutoNextFailures;
    if (consecutiveAutoNextFailures >= maxAutoNextFailures) {
      playPtr->set(false);
      setError("Auto-next failed repeatedly; playback stopped");
    }
  };

  if (!response.localError.empty()) {
    setError(response.localError);
    markAutoFailure();
    return;
  }

  if (!response.apiResponse.transportOk || !jsonStatusSuccess(response.apiResponse.body)) {
    const string apiMessage = jsonStringOrEmpty(response.apiResponse.body, "message");
    setError(apiMessage.empty() ?
      (response.apiResponse.error.empty() ? "now playing request failed" : response.apiResponse.error) :
      apiMessage);
    markAutoFailure();
    return;
  }

  const string streamUrl = jsonStringOrEmpty(response.apiResponse.body, "url");
  if (streamUrl.empty()) {
    setError("Now Playing response did not include a URL");
    markAutoFailure();
    return;
  }

  lastNowPlaying = response.apiResponse.body;
  nowPlayingPtr->set(lastNowPlaying);

  if (playback == nullptr) {
    setError("Playback process unavailable");
    markAutoFailure();
    return;
  }

  if (response.forceNext) {
    if (!playback->stop()) {
      setError("Failed to stop existing playback process");
      markAutoFailure();
      return;
    }
  }

  if (!setMixerVolume(volumePtr->get())) {
    volumeRestorePending = true;
    setError("Waiting for net_audio volume restore before playback");
    markAutoFailure();
    return;
  }

  volumeRestorePending = false;

  if (!playback->start(streamUrl)) {
    setError("Failed to start local playback process");
    markAutoFailure();
    return;
  }

  lastAutoNextAttemptAt = Clock::now();
  playPtr->set(true);
  consecutiveAutoNextFailures = 0;
  setError("");
}

void CustomChannels::drainWorkerResponses()
{
  std::deque<WorkerResponse> drained;
  {
    std::lock_guard<std::mutex> lock(workerMutex);
    drained.swap(responseQueue);
  }

  for (auto &response : drained)
  {
    {
      std::lock_guard<std::mutex> lock(workerMutex);
      const size_t idx = requestKindIndex(response.kind);
      if (inflightCounts[idx] > 0) {
        --inflightCounts[idx];
      }

      if (response.epoch != workerEpoch) {
        continue;
      }
    }

    switch (response.kind)
    {
      case WorkerRequestKind::RegisterListener:
        applyRegisterListenerResponse(response);
        break;

      case WorkerRequestKind::ExchangeAuthCode:
        applyExchangeAuthCodeResponse(response);
        break;

      case WorkerRequestKind::RefreshToken:
        applyRefreshTokenResponse(response);
        break;

      case WorkerRequestKind::FetchChannels:
        applyFetchChannelsResponse(response);
        break;

      case WorkerRequestKind::PlayNow:
        applyPlayNowResponse(response);
        break;

      case WorkerRequestKind::Count:
        break;
    }
  }
}

void CustomChannels::update(bool sensorsOnly, bool refreshVolatileElements)
{
  (void)refreshVolatileElements;
  drainWorkerResponses();

  const bool nowEnabled = enabledPtr->get();

  if (sensorsOnly)
  {
    isPlayingPtr->set(nowEnabled && playback != nullptr && playback->isRunning());
  }
  else // non-sensors
  {
    if (enabledPtr->isModified() && nowEnabled && !wasEnabled) {
      pendingBootstrapRefresh = true;
      volumeRestorePending = true;
      lastVolumeRestoreAttemptAt = {};
      consecutiveAutoNextFailures = 0;
      lastAutoNextAttemptAt = {};
    }
    wasEnabled = nowEnabled;

    if (!enabledPtr->get())
    {
      websiteReachablePtr->set(false);
      lastWebsiteReachable = false;
      lastWebsiteReachabilityCheckAt = {};
      cancelPendingWork();
      if (playback != nullptr)
        playback->stop();
      playPtr->set(false);
      mutePtr->set(false);
      nextPtr->set(false);
      registerListenerPtr->set(false);
      exchangeAuthCodePtr->set(false);
      refreshTokenPtr->set(false);
      fetchChannelsPtr->set(false);
      lastErrorPtr->reset();
      authUrlPtr->reset();
      zonePtr->reset();
      formatPtr->reset();
      channelsPtr->reset();
      nowPlayingPtr->reset();
      isPlayingPtr->reset();
      pendingFetchAfterRefresh = false;
      pendingBootstrapRefresh = false;
      consecutiveAutoNextFailures = 0;
      lastAutoNextAttemptAt = {};
      return;
    }
    if (registerListenerPtr->isModified() && registerListenerPtr->get()) {
      enqueueRegisterListener(false);
    }

    if (exchangeAuthCodePtr->isModified() && exchangeAuthCodePtr->get()) {
      enqueueExchangeAuthCode(false);
    }

    if (refreshTokenPtr->isModified() && refreshTokenPtr->get()) {
      enqueueRefreshToken(false);
    }

    if (fetchChannelsPtr->isModified() && fetchChannelsPtr->get()) {
      enqueueFetchChannels(false);
    }

    if (playPtr->isModified()) {
      if (playPtr->get()) {
        // Treat explicit user play as a fresh recovery attempt.
        consecutiveAutoNextFailures = 0;
        lastAutoNextAttemptAt = {};
        enqueuePlayNow(false, false);
      } else {
        if (playback != nullptr && !playback->stop()) {
          setError("Failed to stop local playback process");
        }
      }
    }

    if (nextPtr->isModified() && nextPtr->get()) {
      enqueuePlayNow(true, false);
    }

    if (volumePtr->isModified()) {
      volumeRestorePending = true;
      lastVolumeRestoreAttemptAt = {};
    }

    if (mutePtr->isModified()) {
      volumeRestorePending = true;
      lastVolumeRestoreAttemptAt = {};
    }
  }

  runAutomation();
}

void CustomChannels::runAutomation()
{
  if (!enabledPtr->get() || playback == nullptr) {
    websiteReachablePtr->set(false);
    lastWebsiteReachable = false;
    lastWebsiteReachabilityCheckAt = {};
    return;
  }

  const auto now = Clock::now();
  const bool isRunning = playback->isRunning();

  if (lastWebsiteReachabilityCheckAt.time_since_epoch().count() == 0 ||
      (now - lastWebsiteReachabilityCheckAt) >= websiteReachabilityProbeInterval) {
    lastWebsiteReachabilityCheckAt = now;
    const string probeUrl = apiUrl.empty() ? "https://pro.customchannels.net" : apiUrl;
    lastWebsiteReachable = isWebsiteReachable(probeUrl);
    websiteReachablePtr->set(lastWebsiteReachable);
  }

  if (!lastWebsiteReachable) {
    if (isRunning) {
      if (!playback->stop()) {
        setError("Provider unreachable and playback stop failed");
      }
    }
    if (playPtr->get()) {
      // Keep play intent latched so automation can resume when connectivity returns.
      setError("Provider unreachable; waiting for automatic resume");
    }
    isPlayingPtr->set(false);
    return;
  }

  if (volumeRestorePending) {
    if (lastVolumeRestoreAttemptAt.time_since_epoch().count() == 0 ||
        (now - lastVolumeRestoreAttemptAt) >= Seconds(1)) {
      lastVolumeRestoreAttemptAt = now;
      if (setMixerVolume(volumePtr->get())) {
        volumeRestorePending = false;
      }
    }
  }

  const bool hasRefreshToken = !refreshTokenValuePtr->get().empty();
  const bool hasAccessToken = !accessTokenPtr->get().empty();
  if ((pendingBootstrapRefresh || (hasRefreshToken && !hasAccessToken)) && hasRefreshToken) {
    const bool refreshDue = lastRefreshAttemptAt.time_since_epoch().count() == 0 ||
      (now - lastRefreshAttemptAt) >= Seconds(5);
    if (refreshDue && !hasPendingOrInFlight(WorkerRequestKind::RefreshToken)) {
      if (enqueueRefreshToken(true)) {
        pendingBootstrapRefresh = false;
      }
    }
  } else if (isAuthenticated()) {
    if (lastRefreshAttemptAt.time_since_epoch().count() == 0 ||
        (now - lastRefreshAttemptAt) >= autoRefreshInterval) {
      if (!hasPendingOrInFlight(WorkerRequestKind::RefreshToken)) {
        enqueueRefreshToken(true);
      }
    }

    const bool channelsDue = pendingFetchAfterRefresh ||
      lastChannelsFetchAt.time_since_epoch().count() == 0 ||
               (now - lastChannelsFetchAt) >= autoChannelsFetchInterval ||
               channelsPtr->get().empty();
    if (channelsDue && !hasPendingOrInFlight(WorkerRequestKind::FetchChannels)) {
      enqueueFetchChannels(true);
    }
  }

  if (!isRunning && playPtr->get() && isAuthenticated() && channelIdPtr->get() != 0) {
    if (lastAutoNextAttemptAt.time_since_epoch().count() == 0 ||
        (now - lastAutoNextAttemptAt) >= autoNextRetryDelay) {
      if (!hasPendingOrInFlight(WorkerRequestKind::PlayNow)) {
        lastAutoNextAttemptAt = now;
        enqueuePlayNow(false, true);
      }
    }
  } else if (isRunning) {
    consecutiveAutoNextFailures = 0;
  }
}

bool CustomChannels::isAuthenticated() const
{
  return !accessTokenPtr->get().empty() && !refreshTokenValuePtr->get().empty();
}

void CustomChannels::setError(const string &msg)
{
  lastErrorPtr->set(msg);
}

bool CustomChannels::setMixerVolume(uint32_t percent)
{
  const uint32_t requested = percent > 100 ? 100 : percent;
  const uint32_t effective = mutePtr->get() ? 0U : requested;
  const int normalized = static_cast<int>(effective);
  const string command =
    "amixer -D net_audio sset 'NetAudio Playback Volume' " + std::to_string(normalized) + "% >/tmp/customchannels-amixer.log 2>&1";
  return std::system(command.c_str()) == 0;
}
