#include "CustomChannelsClient.h"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>

using std::string;

namespace {
struct ParsedUrl {
  string scheme;
  string host;
  string port;
  string path;
  bool valid = false;
};

ParsedUrl parseUrl(const string &url)
{
  ParsedUrl out;
  const auto schemeSep = url.find("://");
  if (schemeSep == string::npos) {
    return out;
  }

  out.scheme = url.substr(0, schemeSep);
  const auto hostStart = schemeSep + 3;
  const auto pathStart = url.find('/', hostStart);
  const string hostPort = pathStart == string::npos ? url.substr(hostStart) : url.substr(hostStart, pathStart - hostStart);
  out.path = pathStart == string::npos ? "" : url.substr(pathStart);

  const auto colon = hostPort.find(':');
  if (colon == string::npos) {
    out.host = hostPort;
    out.port = out.scheme == "https" ? "443" : "80";
  } else {
    out.host = hostPort.substr(0, colon);
    out.port = hostPort.substr(colon + 1);
  }

  if (out.path.empty()) {
    out.path = "";
  }

  out.valid = !out.scheme.empty() && !out.host.empty() && !out.port.empty();
  return out;
}

bool isSuccessStatus(int status)
{
  return status >= 200 && status < 300;
}
} // namespace

CustomChannelsClient::CustomChannelsClient(const string &apiUrl, const string &developerKey)
  : apiUrl(apiUrl), developerKey(developerKey)
{
  const ParsedUrl parsed = parseUrl(apiUrl);
  if (!parsed.valid) {
    lastErrorText = "Invalid CUSTOMCHANNELS_API_URL";
    return;
  }

  scheme = parsed.scheme;
  host = parsed.host;
  port = parsed.port;
  basePath = parsed.path;
}

bool CustomChannelsClient::isConfigured() const
{
  return !host.empty() && !developerKey.empty() && (scheme == "http" || scheme == "https");
}

const string &CustomChannelsClient::lastError() const
{
  return lastErrorText;
}

CustomChannelsClient::Response CustomChannelsClient::registerListener(
    const string &identifier,
    const string &redirect,
    const string &success,
    const string &displayName) const
{
  json body = json::object();
  body["identifier"] = identifier;
  body["redirect"] = redirect;
  if (!success.empty()) {
    body["success"] = success;
  }
  if (!displayName.empty()) {
    body["display_name"] = displayName;
  }
  return postJson("/api/auth/new", developerKey, body);
}

CustomChannelsClient::Response CustomChannelsClient::requestAccessToken(const string &authCode) const
{
  json body = json::object();
  body["auth_code"] = authCode;
  return postJson("/api/auth/access", developerKey, body);
}

CustomChannelsClient::Response CustomChannelsClient::refreshAccessToken(const string &refreshToken) const
{
  json body = json::object();
  body["refresh_token"] = refreshToken;
  return postJson("/api/auth/refresh", developerKey, body);
}

CustomChannelsClient::Response CustomChannelsClient::getChannels(const string &accessToken) const
{
  return postJson("/api/channels", accessToken, json::object());
}

CustomChannelsClient::Response CustomChannelsClient::getNowPlaying(
    const string &accessToken,
    uint32_t channelId,
    const string &format,
    const string &zone) const
{
  string target = "/api/channel/" + std::to_string(channelId);
  if (!format.empty()) {
    target += "/" + format;
  }
  if (!zone.empty()) {
    target += "?zone=" + zone;
  }
  return postJson(target, accessToken, json::object());
}

CustomChannelsClient::Response CustomChannelsClient::getStreamMetadata(const string &accessToken, const string &streamUrl) const
{
  json body = json::object();
  body["stream"] = streamUrl;
  return postJson("/api/stream/metadata", accessToken, body);
}

CustomChannelsClient::Response CustomChannelsClient::getUser(const string &accessToken) const
{
  return postJson("/api/user", accessToken, json::object());
}

string CustomChannelsClient::buildTarget(const string &relativeTarget) const
{
  string base = basePath;
  if (base.empty()) {
    base = "";
  }
  if (!base.empty() && base.back() == '/') {
    base.pop_back();
  }
  if (!relativeTarget.empty() && relativeTarget.front() == '/') {
    return base + relativeTarget;
  }
  return base + "/" + relativeTarget;
}

CustomChannelsClient::Response CustomChannelsClient::postJson(
    const string &relativeTarget,
    const string &bearerToken,
    const json &body) const
{
  Response out;
  if (!isConfigured()) {
    out.error = lastErrorText.empty() ? "Custom Channels client not configured" : lastErrorText;
    return out;
  }

  namespace beast = boost::beast;
  namespace http = beast::http;
  namespace net = boost::asio;
  namespace ssl = net::ssl;
  using tcp = net::ip::tcp;

  try {
    net::io_context ioc;
    ssl::context ctx(ssl::context::tls_client);
    ctx.set_default_verify_paths();
    ctx.set_verify_mode(ssl::verify_peer);

    const string target = buildTarget(relativeTarget);
    const string hostHeader = host;
    const int version = 11;

    http::request<http::string_body> req{http::verb::post, target, version};
    req.set(http::field::host, hostHeader);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.set(http::field::authorization, "Bearer " + bearerToken);
    req.set(http::field::content_type, "application/json");
    req.body() = body.dump();
    req.prepare_payload();

    if (scheme == "https") {
      tcp::resolver resolver(ioc);
      beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);

      if(!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
        out.error = "Failed to set TLS SNI hostname";
        return out;
      }

      auto const results = resolver.resolve(host, port);
      beast::get_lowest_layer(stream).connect(results);
      stream.handshake(ssl::stream_base::client);
      http::write(stream, req);

      beast::flat_buffer buffer;
      http::response<http::string_body> res;
      http::read(stream, buffer, res);

      out.transportOk = true;
      out.httpStatus = static_cast<int>(res.result_int());
      if (!res.body().empty()) {
        out.body = json::parse(res.body());
      }

      beast::error_code ec;
      stream.shutdown(ec);
    } else {
      tcp::resolver resolver(ioc);
      beast::tcp_stream stream(ioc);
      auto const results = resolver.resolve(host, port);
      stream.connect(results);

      http::write(stream, req);

      beast::flat_buffer buffer;
      http::response<http::string_body> res;
      http::read(stream, buffer, res);

      out.transportOk = true;
      out.httpStatus = static_cast<int>(res.result_int());
      if (!res.body().empty()) {
        out.body = json::parse(res.body());
      }

      beast::error_code ec;
      stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    }

    if (!isSuccessStatus(out.httpStatus)) {
      if (out.error.empty()) {
        out.error = "HTTP status " + std::to_string(out.httpStatus);
      }
    }
  } catch (const std::exception &e) {
    out.error = e.what();
  }

  return out;
}
