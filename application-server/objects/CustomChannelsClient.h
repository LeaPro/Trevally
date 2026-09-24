#pragma once

#include <object.h>
#include <string>

class CustomChannelsClient {
public:
  struct Response {
    bool transportOk = false;
    int httpStatus = 0;
    json body = json::object();
    std::string error;
  };

  CustomChannelsClient(const std::string &apiUrl, const std::string &developerKey);

  bool isConfigured() const;
  const std::string &lastError() const;

  Response registerListener(const std::string &identifier,
                            const std::string &redirect,
                            const std::string &success,
                            const std::string &displayName) const;
  Response requestAccessToken(const std::string &authCode) const;
  Response refreshAccessToken(const std::string &refreshToken) const;

  Response getChannels(const std::string &accessToken) const;
  Response getNowPlaying(const std::string &accessToken,
                         uint32_t channelId,
                         const std::string &format,
                         const std::string &zone) const;
  Response getStreamMetadata(const std::string &accessToken, const std::string &streamUrl) const;
  Response getUser(const std::string &accessToken) const;

private:
  Response postJson(const std::string &target, const std::string &bearerToken, const json &body) const;
  std::string buildTarget(const std::string &relativeTarget) const;

  std::string apiUrl;
  std::string developerKey;
  std::string scheme;
  std::string host;
  std::string port;
  std::string basePath;
  mutable std::string lastErrorText;
};
