#include "AmpLinks.h"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <deque>
#include <vector>

using std::string;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

extern boost::asio::io_context io_context;
extern std::shared_ptr<RootObject> root;

static const unsigned short ampPort = 1234;               // the amps' websocket API port
static const auto connectTimeout = std::chrono::seconds(5);
static const auto retryDelay = std::chrono::seconds(3);
// The amp's websocket server closes connections it hasn't heard from in 300 s; amps send us
// notifications but we may have nothing to send them, so ping well inside that.
static const auto pingPeriod = std::chrono::seconds(60);

/* The amp's IP from /discovery/devices, a JSON array of {"ip", "mac", ...}; "" if not discovered. */
static string ipForMac(const string &mac)
{
  if (!root)
    return "";
  auto devices = std::dynamic_pointer_cast<StringSensor>(root->elementFromPath("/discovery/devices"));
  if (!devices)
    return "";
  json list = json::parse(devices->get(), nullptr, false);
  if (!list.is_array())
    return "";
  for (const auto &device : list)
  {
    if (!device.is_object() || !device.contains("mac") || !device.contains("ip"))
      continue;
    if (!device["mac"].is_string() || !device["ip"].is_string())
      continue;
    if (AmpLinks::normalizeMac(device["mac"].get<string>()) == mac)
      return device["ip"].get<string>();
  }
  return "";
}

/* The part of a notification's params for @p leafUrl, given the url the params are relative to
 * (subscription updates come as one tree from "/": {"amp": {"channels": {"1": {"output": {...}}}}}). */
static const json *leafParams(const json &params, const string &url, const string &leafUrl)
{
  if (leafUrl == url)
    return &params;
  const string prefix = (url == "/") ? url : url + "/";
  if (leafUrl.compare(0, prefix.size(), prefix) != 0)
    return nullptr;
  const json *node = &params;
  size_t start = prefix.size();
  while (start <= leafUrl.size())
  {
    size_t end = leafUrl.find('/', start);
    if (end == string::npos)
      end = leafUrl.size();
    const string name = leafUrl.substr(start, end - start);
    if (!node->is_object() || !node->contains(name))
      return nullptr;
    node = &(*node)[name];
    start = end + 1;
  }
  return node;
}

struct AmpLinks::Link : std::enable_shared_from_this<AmpLinks::Link>
{
  enum class State { Connecting, Open, Closed };

  /* One connection attempt; the async handlers hold it, so it outlives a teardown until they finish. */
  struct Conn
  {
    Conn() : ws(io_context) {}
    websocket::stream<beast::tcp_stream> ws;
    beast::flat_buffer readBuffer;
  };
  using Msg = std::shared_ptr<const string>; // nullptr: a ping

  explicit Link(const string &mac) : mac(mac), retryTimer(io_context), pingTimer(io_context) {}

  string mac;
  int refs = 0;
  State state = State::Connecting;
  uint64_t generation = 0; // bumped per connection attempt; handlers from older attempts do nothing
  bool reportedDown = false; // log the first failure only, not every retry
  std::shared_ptr<Conn> conn;
  std::deque<Msg> writeQueue;
  bool writing = false;
  net::steady_timer retryTimer;
  net::steady_timer pingTimer;
  int nextRequestId = 1;
  std::map<int, string> subscribeRequests;             // request id -> leaf url, to route the reply
  std::map<string, std::map<uint64_t, Listener>> leaves; // leaf url -> listeners
  std::map<string, json> leafState;                    // leaf url -> last known element values
  std::map<string, json> pendingSets;                  // leaf url -> elements to set once connected

  void connect()
  {
    if (state == State::Closed)
      return;
    state = State::Connecting;
    const uint64_t gen = ++generation;
    const string ip = ipForMac(mac);
    if (ip.empty())
      return retry(gen, "not discovered yet");
    boost::system::error_code ec;
    auto address = net::ip::make_address(ip, ec);
    if (ec)
      return retry(gen, "bad discovered ip " + ip);

    conn = std::make_shared<Conn>();
    writeQueue.clear();
    writing = false;
    subscribeRequests.clear();

    auto self = shared_from_this();
    auto c = conn;
    beast::get_lowest_layer(c->ws).expires_after(connectTimeout);
    beast::get_lowest_layer(c->ws).async_connect(tcp::endpoint(address, ampPort),
      [self, c, gen, ip](beast::error_code ec) {
        if (gen != self->generation)
          return;
        if (ec)
          return self->retry(gen, "connect to " + ip + ": " + ec.message());
        beast::get_lowest_layer(c->ws).expires_never(); // the websocket timeouts take over
        websocket::stream_base::timeout timeouts = websocket::stream_base::timeout::suggested(beast::role_type::client);
        timeouts.idle_timeout = std::chrono::seconds(20); // pings a silent amp and drops it if it doesn't answer
        timeouts.keep_alive_pings = true;
        c->ws.set_option(timeouts);
        c->ws.async_handshake(ip + ":" + std::to_string(ampPort), "/",
          [self, c, gen](beast::error_code ec) {
            if (gen != self->generation)
              return;
            if (ec)
              return self->retry(gen, "handshake: " + ec.message());
            self->opened(gen);
          });
      });
  }

  void opened(uint64_t gen)
  {
    fprintf(stderr, "%s.%d(%s): amp %s connected\n", __FILE__, __LINE__, __func__, mac.c_str());
    reportedDown = false;
    state = State::Open;
    for (const auto &[url, listeners] : leaves)
      sendSubscribe(url);
    for (const auto &[url, elements] : pendingSets)
      sendSet(url, elements);
    pendingSets.clear();
    read(gen);
    schedulePing(gen);
  }

  void retry(uint64_t gen, const string &why)
  {
    if (gen != generation || state == State::Closed)
      return;
    if (!reportedDown)
      fprintf(stderr, "%s.%d(%s): amp %s: %s; retrying every %ld s\n", __FILE__, __LINE__, __func__, mac.c_str(), why.c_str(),
              (long)std::chrono::duration_cast<std::chrono::seconds>(retryDelay).count());
    reportedDown = true;
    const uint64_t retryGen = ++generation; // the failed attempt's other handlers now do nothing
    teardown();
    state = State::Connecting;
    auto self = shared_from_this();
    retryTimer.expires_after(retryDelay);
    retryTimer.async_wait([self, retryGen](boost::system::error_code ec) {
      if (!ec && retryGen == self->generation)
        self->connect();
    });
  }

  void teardown()
  {
    pingTimer.cancel();
    if (conn)
    {
      beast::error_code ec;
      beast::get_lowest_layer(conn->ws).socket().close(ec);
      conn.reset();
    }
    writeQueue.clear();
    writing = false;
  }

  void close()
  {
    state = State::Closed;
    ++generation;
    retryTimer.cancel();
    teardown();
  }

  void read(uint64_t gen)
  {
    auto self = shared_from_this();
    auto c = conn;
    c->ws.async_read(c->readBuffer, [self, c, gen](beast::error_code ec, std::size_t) {
      if (gen != self->generation)
        return;
      if (ec)
        return self->retry(gen, "read: " + ec.message());
      const string msg = beast::buffers_to_string(c->readBuffer.data());
      c->readBuffer.consume(c->readBuffer.size());
      self->handleMessage(msg);
      if (gen == self->generation) // a listener may have closed the link
        self->read(gen);
    });
  }

  void schedulePing(uint64_t gen)
  {
    auto self = shared_from_this();
    pingTimer.expires_after(pingPeriod);
    pingTimer.async_wait([self, gen](boost::system::error_code ec) {
      if (ec || gen != self->generation || self->state != State::Open)
        return;
      self->write(Msg()); // ping
      self->schedulePing(gen);
    });
  }

  void write(Msg msg)
  {
    if (state != State::Open)
      return;
    writeQueue.push_back(std::move(msg));
    if (!writing)
      writeNext(generation);
  }

  void write(const string &msg) { write(std::make_shared<const string>(msg)); }

  void writeNext(uint64_t gen)
  {
    if (writeQueue.empty())
      return;
    writing = true;
    auto self = shared_from_this();
    auto c = conn;
    Msg msg = writeQueue.front(); // held by the handler: the queue may be cleared while the write is pending
    auto done = [self, c, msg, gen](beast::error_code ec) {
      if (gen != self->generation)
        return;
      self->writing = false;
      if (ec)
        return self->retry(gen, "write: " + ec.message());
      self->writeQueue.pop_front();
      self->writeNext(gen);
    };
    if (!msg)
    {
      c->ws.async_ping({}, done);
    }
    else
    {
      c->ws.text(true);
      c->ws.async_write(net::buffer(*msg), [done](beast::error_code ec, std::size_t) { done(ec); });
    }
  }

  static string request(const string &url, const string &method, const json &params, int id = -1)
  {
    json j = json::object();
    j["leaApi"] = "1.0";
    j["url"] = url;
    j["method"] = method;
    j["params"] = params;
    if (id >= 0)
      j["id"] = id;
    return j.dump();
  }

  void sendSubscribe(const string &url)
  {
    const int id = nextRequestId++;
    subscribeRequests[id] = url;
    write(request(url, "subscribe", json::object(), id)); // with an id, so the amp replies with the current values
  }

  void sendSet(const string &url, const json &elements)
  {
    write(request(url, "set", elements)); // no id: the amp only replies on error
  }

  void handleMessage(const string &msg)
  {
    json j = json::parse(msg, nullptr, false);
    if (!j.is_object())
      return;
    if (j.contains("error"))
    {
      fprintf(stderr, "%s.%d(%s): amp %s: %s\n", __FILE__, __LINE__, __func__, mac.c_str(), j["error"].dump().c_str());
      return;
    }
    if (j.contains("id") && j["id"].is_number_integer() && j.contains("result"))
    {
      auto it = subscribeRequests.find(j["id"].get<int>());
      if (it != subscribeRequests.end())
      {
        const string url = it->second;
        subscribeRequests.erase(it);
        if (j["result"].is_object())
          deliver(url, j["result"]);
      }
      return;
    }
    if (j.value("method", "") == "notify" && j.contains("params") && j["url"].is_string())
    {
      const string url = j["url"].get<string>();
      std::vector<string> urls;
      for (const auto &[leafUrl, listeners] : leaves)
        urls.push_back(leafUrl);
      for (const auto &leafUrl : urls)
      {
        const json *elements = leafParams(j["params"], url, leafUrl);
        if (elements && elements->is_object() && !elements->empty())
          deliver(leafUrl, *elements);
      }
    }
  }

  void deliver(const string &url, const json &elements)
  {
    auto leafIt = leaves.find(url);
    if (leafIt == leaves.end())
      return;
    json &state = leafState[url];
    if (!state.is_object())
      state = json::object();
    for (auto it = elements.begin(); it != elements.end(); ++it)
      state[it.key()] = it.value();
    // copy: a listener may add or drop subscriptions
    std::vector<Listener> listeners;
    for (const auto &[id, listener] : leafIt->second)
      listeners.push_back(listener);
    for (const auto &listener : listeners)
      if (listener)
        listener(elements);
  }
};

AmpLinks &AmpLinks::instance()
{
  // never destroyed: Handles held by elements that outlive main() (e.g. a UI live binding of an open page)
  // release into it during static destruction; the process exit closes the sockets
  static AmpLinks *links = new AmpLinks;
  return *links;
}

string AmpLinks::normalizeMac(const string &mac)
{
  string result = mac;
  std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return std::toupper(c); });
  return result;
}

std::shared_ptr<AmpLinks::Link> AmpLinks::acquire(const string &mac)
{
  auto &link = links[mac];
  if (!link)
  {
    link = std::make_shared<Link>(mac);
    link->connect();
  }
  link->refs++;
  return link;
}

AmpLinks::Handle AmpLinks::use(const string &mac)
{
  const string key = normalizeMac(mac);
  acquire(key);
  return Handle(key, "", 0);
}

AmpLinks::Handle AmpLinks::subscribe(const string &mac, const string &leafUrl, Listener listener)
{
  const string key = normalizeMac(mac);
  auto link = acquire(key);
  const uint64_t id = nextListenerId++;
  auto &listeners = link->leaves[leafUrl];
  const bool first = listeners.empty();
  listeners[id] = std::move(listener);
  if (first)
  {
    link->leafState.erase(leafUrl);
    if (link->state == Link::State::Open)
      link->sendSubscribe(leafUrl);
  }
  else if (link->leafState.count(leafUrl))
  {
    // already subscribed: hand the new listener the known values (later, so it isn't called from inside subscribe())
    net::post(io_context, [link, leafUrl, id]() {
      auto leafIt = link->leaves.find(leafUrl);
      if (leafIt == link->leaves.end())
        return;
      auto it = leafIt->second.find(id);
      if (it != leafIt->second.end() && it->second && link->leafState.count(leafUrl))
        it->second(link->leafState[leafUrl]);
    });
  }
  return Handle(key, leafUrl, id);
}

void AmpLinks::set(const string &mac, const string &leafUrl, const json &elements)
{
  auto it = links.find(normalizeMac(mac));
  if (it == links.end())
  {
    fprintf(stderr, "%s.%d(%s): no link to amp %s; set of %s dropped\n", __FILE__, __LINE__, __func__, mac.c_str(), leafUrl.c_str());
    return;
  }
  auto &link = it->second;
  if (link->state == Link::State::Open)
  {
    link->sendSet(leafUrl, elements);
  }
  else
  {
    json &pending = link->pendingSets[leafUrl];
    if (!pending.is_object())
      pending = json::object();
    for (auto e = elements.begin(); e != elements.end(); ++e)
      pending[e.key()] = e.value();
  }
}

void AmpLinks::release(const Handle &handle)
{
  auto it = links.find(handle.mac);
  if (it == links.end())
    return;
  auto link = it->second;
  if (!handle.leafUrl.empty())
  {
    auto leafIt = link->leaves.find(handle.leafUrl);
    if (leafIt != link->leaves.end())
    {
      leafIt->second.erase(handle.listenerId);
      if (leafIt->second.empty())
      {
        link->leaves.erase(leafIt);
        link->leafState.erase(handle.leafUrl);
        if (link->state == Link::State::Open)
          link->write(Link::request(handle.leafUrl, "unsubscribe", json::object()));
      }
    }
  }
  if (--link->refs <= 0)
  {
    link->close();
    links.erase(it);
  }
}

AmpLinks::Handle &AmpLinks::Handle::operator=(Handle &&other) noexcept
{
  if (this != &other)
  {
    reset();
    mac = std::move(other.mac);
    leafUrl = std::move(other.leafUrl);
    listenerId = other.listenerId;
    active = other.active;
    other.active = false;
  }
  return *this;
}

void AmpLinks::Handle::reset()
{
  if (!active)
    return;
  active = false;
  AmpLinks::instance().release(*this);
}
