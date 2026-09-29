#include "objects.h"
#include "CustomChannelsClient.h"

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

  name = "enabled"; elements[name] = enabledPtr = std::make_shared<BoolControl>(path + "/" + name, this, false);
  name = "status"; elements[name] = statusPtr = std::make_shared<StringSensor>(path + "/" + name, this, "not_configured", 64);
  name = "lastError"; elements[name] = lastErrorPtr = std::make_shared<StringSensor>(path + "/" + name, this, "", 1024);
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
  name = "volume"; elements[name] = volumePtr = std::make_shared<UInt32Control>(path + "/" + name, this, 0, 100, 100, "percent", true, true);
  name = "nowPlaying"; elements[name] = nowPlayingPtr = std::make_shared<JsonSensor>(path + "/" + name, this, json::object());
  name = "isPlaying"; elements[name] = isPlayingPtr = std::make_shared<BoolSensor>(path + "/" + name, this, false);
  name = "play"; elements[name] = playPtr = std::make_shared<BoolControl>(path + "/" + name, this, false, true, true);
  name = "next"; elements[name] = nextPtr = std::make_shared<BoolControl>(path + "/" + name, this, false, false, true);
}

CustomChannels::~CustomChannels()
{
  if (playback != nullptr) {
    playback->stop();
  }
}

void CustomChannels::initialize()
{
  LeafObject::initialize();
  loadConfiguration();
  playback = std::make_unique<UrlPlaybackProcess>();
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

void CustomChannels::update(bool sensorsOnly, bool refreshVolatileElements)
{
  (void)refreshVolatileElements;

  if (sensorsOnly)
  {
    isPlayingPtr->set(playback != nullptr && playback->isRunning());
  }
  else // non-sensors
  {
    const bool nowEnabled = enabledPtr->get();
    if (enabledPtr->isModified() && nowEnabled && !wasEnabled) {
      pendingBootstrapRefresh = true;
      if (!setMixerVolume(volumePtr->get())) {
        setError("Failed to apply net_audio volume");
      }
    }
    wasEnabled = nowEnabled;

    if (!enabledPtr->get())
    {
      if (playback != nullptr)
        playback->stop();
      playPtr->set(false);
      lastErrorPtr->reset();
      authUrlPtr->reset();
      zonePtr->reset();
      formatPtr->reset();
      channelsPtr->reset();
      nowPlayingPtr->reset();
      isPlayingPtr->reset();
      return;
    }
    if (registerListenerPtr->isModified() && registerListenerPtr->get()) {
      handleRegisterListener();
      registerListenerPtr->set(false);
    }

    if (exchangeAuthCodePtr->isModified() && exchangeAuthCodePtr->get()) {
      handleExchangeAuthCode();
      exchangeAuthCodePtr->set(false);
    }

    if (refreshTokenPtr->isModified() && refreshTokenPtr->get()) {
      handleRefreshToken();
      refreshTokenPtr->set(false);
    }

    if (fetchChannelsPtr->isModified() && fetchChannelsPtr->get()) {
      handleFetchChannels();
      fetchChannelsPtr->set(false);
    }

    if (playPtr->isModified()) {
      if (playPtr->get()) {
        handlePlayNow(false);
      } else {
        if (playback != nullptr && !playback->stop()) {
          setError("Failed to stop local playback process");
        }
      }
    }

    if (nextPtr->isModified() && nextPtr->get()) {
      handlePlayNow(true);
      nextPtr->set(false);
    }

    if (volumePtr->isModified()) {
      if (!setMixerVolume(volumePtr->get())) {
        setError("Failed to set net_audio volume");
      }
    }
  }

  runAutomation();
}

void CustomChannels::runAutomation()
{
  if (playback == nullptr) {
    return;
  }

  const auto now = Clock::now();
  const bool isRunning = playback->isRunning();

  if (pendingBootstrapRefresh && !refreshTokenValuePtr->get().empty()) {
    pendingBootstrapRefresh = false;
    handleRefreshToken();
    if (isAuthenticated()) {
      handleFetchChannels();
    }
  } else if (isAuthenticated()) {
    if (lastRefreshAttemptAt.time_since_epoch().count() == 0 ||
        (now - lastRefreshAttemptAt) >= autoRefreshInterval) {
      handleRefreshToken();
      if (isAuthenticated()) {
        handleFetchChannels();
      }
    } else if (lastChannelsFetchAt.time_since_epoch().count() == 0 ||
               (now - lastChannelsFetchAt) >= autoChannelsFetchInterval ||
               channelsPtr->get().empty()) {
      handleFetchChannels();
    }
  }

  if (!isRunning && playPtr->get() && isAuthenticated() && channelIdPtr->get() != 0) {
    if (lastAutoNextAttemptAt.time_since_epoch().count() == 0 ||
        (now - lastAutoNextAttemptAt) >= autoNextRetryDelay) {
      lastAutoNextAttemptAt = now;
      handlePlayNow(false);
      if (playback->isRunning()) {
        consecutiveAutoNextFailures = 0;
      } else {
        ++consecutiveAutoNextFailures;
        if (consecutiveAutoNextFailures >= maxAutoNextFailures) {
          playPtr->set(false);
          setError("Auto-next failed repeatedly; playback stopped");
        }
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

void CustomChannels::handleRegisterListener()
{
  if (client == nullptr || !client->isConfigured()) {
    setError("Custom Channels client unavailable");
    return;
  }

  const auto res = client->registerListener(
      chooseIdentifier(),
      redirectUrlPtr->get(),
      successUrlPtr->get(),
      chooseDisplayName());

  if (!res.transportOk || !jsonStatusSuccess(res.body)) {
    const string apiMessage = jsonStringOrEmpty(res.body, "message");
    setError(apiMessage.empty() ? (res.error.empty() ? "register listener failed" : res.error) : apiMessage);
    statusPtr->set("auth_required");
    return;
  }

  authUrlPtr->set(jsonStringOrEmpty(res.body, "auth_url"));
  const string authCode = jsonStringOrEmpty(res.body, "auth_code");
  if (!authCode.empty()) {
    authCodePtr->set(authCode);
  }
  statusPtr->set("awaiting_user_authorization");
  setError("");
}

void CustomChannels::handleExchangeAuthCode()
{
  if (client == nullptr || !client->isConfigured()) {
    setError("Custom Channels client unavailable");
    return;
  }

  const string code = authCodePtr->get();
  if (code.empty()) {
    setError("authCode is empty");
    return;
  }

  const auto res = client->requestAccessToken(code);
  if (!res.transportOk || !jsonStatusSuccess(res.body)) {
    const string apiMessage = jsonStringOrEmpty(res.body, "message");
    setError(apiMessage.empty() ? (res.error.empty() ? "access token exchange failed" : res.error) : apiMessage);
    statusPtr->set("auth_required");
    return;
  }

  const string accessToken = jsonStringOrEmpty(res.body, "access_token");
  const string refreshToken = jsonStringOrEmpty(res.body, "refresh_token");
  const string accessExpiresAt = jsonStringOrEmpty(res.body, "access_expires_at");
  const string refreshExpiresAt = jsonStringOrEmpty(res.body, "refresh_expires_at");

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

void CustomChannels::handleRefreshToken()
{
  if (client == nullptr || !client->isConfigured()) {
    setError("Custom Channels client unavailable");
    return;
  }

  const string refreshToken = refreshTokenValuePtr->get();
  if (refreshToken.empty()) {
    setError("No refresh token available");
    return;
  }

  lastRefreshAttemptAt = Clock::now();

  const auto res = client->refreshAccessToken(refreshToken);
  if (!res.transportOk || !jsonStatusSuccess(res.body)) {
    const string apiMessage = jsonStringOrEmpty(res.body, "message");
    setError(apiMessage.empty() ? (res.error.empty() ? "token refresh failed" : res.error) : apiMessage);
    accessTokenPtr->set("");
    refreshTokenValuePtr->set("");
    accessExpiresAtPtr->set("");
    refreshExpiresAtPtr->set("");
    statusPtr->set("auth_required");
    return;
  }

  accessTokenPtr->set(jsonStringOrEmpty(res.body, "access_token"));
  refreshTokenValuePtr->set(jsonStringOrEmpty(res.body, "refresh_token"));
  accessExpiresAtPtr->set(jsonStringOrEmpty(res.body, "access_expires_at"));
  refreshExpiresAtPtr->set(jsonStringOrEmpty(res.body, "refresh_expires_at"));

  statusPtr->set("authenticated");
  setError("");
}

void CustomChannels::handleFetchChannels()
{
  if (client == nullptr || !client->isConfigured()) {
    setError("Custom Channels client unavailable");
    return;
  }

  const string accessToken = accessTokenPtr->get();
  if (accessToken.empty()) {
    setError("Access token not available");
    return;
  }

  const auto res = client->getChannels(accessToken);
  if (!res.transportOk || !jsonStatusSuccess(res.body)) {
    const string apiMessage = jsonStringOrEmpty(res.body, "message");
    setError(apiMessage.empty() ? (res.error.empty() ? "fetch channels failed" : res.error) : apiMessage);
    return;
  }

  if (res.body.contains("channels") && res.body["channels"].is_array()) {
    channelsPtr->set(res.body["channels"]);
  }

  lastChannelsFetchAt = Clock::now();

  setError("");
}

void CustomChannels::handlePlayNow(bool forceNext)
{
  if (client == nullptr || !client->isConfigured()) {
    setError("Custom Channels client unavailable");
    return;
  }

  const string accessToken = accessTokenPtr->get();
  if (accessToken.empty()) {
    setError("Access token not available");
    return;
  }

  const uint32_t channelId = channelIdPtr->get();
  if (channelId == 0) {
    setError("channelId must be set before play");
    return;
  }

  const auto res = client->getNowPlaying(accessToken, channelId, formatPtr->get(), zonePtr->get());
  if (!res.transportOk || !jsonStatusSuccess(res.body)) {
    const string apiMessage = jsonStringOrEmpty(res.body, "message");
    setError(apiMessage.empty() ? (res.error.empty() ? "now playing request failed" : res.error) : apiMessage);
    return;
  }

  const string streamUrl = jsonStringOrEmpty(res.body, "url");
  if (streamUrl.empty()) {
    setError("Now Playing response did not include a URL");
    return;
  }

  lastNowPlaying = res.body;
  nowPlayingPtr->set(lastNowPlaying);

  if (playback == nullptr) {
    setError("Playback process unavailable");
    return;
  }

  if (forceNext) {
    if (!playback->stop()) {
      setError("Failed to stop existing playback process");
      return;
    }
  }

  if (!playback->start(streamUrl)) {
    setError("Failed to start local playback process");
    return;
  }

  lastAutoNextAttemptAt = Clock::now();
  playPtr->set(true);
  setError("");
}

void CustomChannels::setError(const string &msg)
{
  lastErrorPtr->set(msg);
}

bool CustomChannels::setMixerVolume(uint32_t percent)
{
  const int normalized = percent > 100 ? 100 : static_cast<int>(percent);
  const string command =
    "amixer -D net_audio sset 'NetAudio' " + std::to_string(normalized) + "% >/tmp/customchannels-amixer.log 2>&1";
  return std::system(command.c_str()) == 0;
}
