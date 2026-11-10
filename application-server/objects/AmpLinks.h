#pragma once
#include <object.h>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>

/*
 * Websocket links from this device to LEA amps, shared by every leaf that mirrors or drives amp
 * elements (OutputFaders now; LeaInput / LeaControl / LeaSensor later).
 *
 * - One connection per amp (keyed by MAC; the IP comes from /discovery/devices). It reconnects on
 *   its own and stays up while any Handle for that amp exists.
 * - Subscriptions are reference counted per (amp, leaf url): the amp gets one subscribe however
 *   many local users want a leaf, and an unsubscribe only when the last one lets go. Amp
 *   subscriptions belong to the connection, so without this one user unsubscribing would cut off
 *   the others.
 * - Everything runs on the shared io_context, like the rest of the application.
 */
class AmpLinks
{
public:
  /* Element name/value pairs of the subscribed amp leaf: all of them once on subscribe, then the ones that changed. */
  using Listener = std::function<void(const json &elements)>;

  /* Keeps an amp connected (and, if made by subscribe(), a leaf subscribed) until destroyed. */
  class Handle
  {
  public:
    Handle() = default;
    Handle(Handle &&other) noexcept { *this = std::move(other); }
    Handle &operator=(Handle &&other) noexcept;
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    ~Handle() { reset(); }
    void reset();
  private:
    friend class AmpLinks;
    Handle(const std::string &mac, const std::string &leafUrl, uint64_t listenerId)
      : mac(mac), leafUrl(leafUrl), listenerId(listenerId), active(true) {}
    std::string mac;
    std::string leafUrl;      // empty for a plain connection handle
    uint64_t listenerId = 0;
    bool active = false;
  };

  static AmpLinks &instance();

  /* Keeps the amp connected, e.g. to send it sets. */
  Handle use(const std::string &mac);
  /* Subscribes to an amp leaf (e.g. "/amp/channels/1/output"); the listener also gets the leaf's
   * current values once they are known. */
  Handle subscribe(const std::string &mac, const std::string &leafUrl, Listener listener);
  /* Sets elements of an amp leaf. Needs a Handle for the amp; while it is disconnected the latest
   * values per leaf are kept and sent once it connects. */
  void set(const std::string &mac, const std::string &leafUrl, const json &elements);

  static std::string normalizeMac(const std::string &mac);

private:
  struct Link;
  AmpLinks() = default;
  std::shared_ptr<Link> acquire(const std::string &mac);
  void release(const Handle &handle);
  std::map<std::string, std::shared_ptr<Link>> links; // by normalized MAC
  uint64_t nextListenerId = 1;
};
