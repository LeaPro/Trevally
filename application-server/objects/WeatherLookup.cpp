#include "WeatherLookup.h"

#include "nlohmann.h"

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace std::chrono_literals;

namespace
{
// NWS and MET Norway ask for a User-Agent that identifies the application and a way to reach its maker.
// TODO: replace CONTACT with a real e-mail address or URL before shipping.
const char *const USER_AGENT = "Trevally/1.0 ( CONTACT )";
const char *const NWS_PREFIX = "https://api.weather.gov/";
const char *const MET_PREFIX = "https://api.met.no/";

constexpr auto REFRESH = 15min;
constexpr auto RETRY_NOT_FOUND = 1h; // unknown postal code, or no forecast for its place
constexpr auto RETRY_ERROR = 1min;
constexpr auto REQUEST_GAP = 1s;
constexpr auto BACKOFF = 60s; // after a 429 / 503
constexpr size_t MAX_BODY = 1024 * 1024;

size_t appendBody(char *data, size_t size, size_t count, void *user)
{
  auto *body = static_cast<std::string *>(user);
  const size_t n = size * count;
  if (body->size() + n > MAX_BODY)
    return 0;
  body->append(data, n);
  return n;
}

int abortWhenStopping(void *user, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
  return static_cast<const std::atomic<bool> *>(user)->load() ? 1 : 0;
}

std::string stringOf(const json &object, const char *key)
{
  const auto it = object.find(key);
  return (it != object.end() && it->is_string()) ? it->get<std::string>() : "";
}

// Coordinates arrive as strings ("47.6302"); false when it is not a number.
bool coordinate(const json &place, const char *key, double &value)
{
  const std::string text = stringOf(place, key);
  char *end = nullptr;
  value = std::strtod(text.c_str(), &end);
  return !text.empty() && end != nullptr && *end == '\0' && std::isfinite(value);
}

struct Place
{
  std::string country; // upper case ISO 3166 alpha-2, e.g. "US"
  std::string postal;  // e.g. "98102", "SW1A 1AA"
};

// "98102" and "98102-1234" are US codes; any other country is named in front: "GB:SW1A 1AA", "DE:10115".
bool parsePlace(const std::string &text, Place &place)
{
  const auto digits = [&](const std::string &s, size_t from, size_t count)
  {
    return s.size() >= from + count && std::all_of(s.begin() + from, s.begin() + from + count, [](unsigned char c) { return c >= '0' && c <= '9'; });
  };
  const size_t colon = text.find(':');
  if (colon == std::string::npos)
    place = {"US", text};
  else
  {
    if (colon != 2 || !std::isalpha((unsigned char)text[0]) || !std::isalpha((unsigned char)text[1]))
      return false;
    place = {std::string{(char)std::toupper((unsigned char)text[0]), (char)std::toupper((unsigned char)text[1])}, text.substr(3)};
  }

  const std::string &p = place.postal;
  if (place.country == "US")
    return (p.size() == 5 && digits(p, 0, 5)) || (p.size() == 10 && digits(p, 0, 5) && p[5] == '-' && digits(p, 6, 4));
  if (p.size() < 2 || p.size() > 10 || !std::isalnum((unsigned char)p.front()) || !std::isalnum((unsigned char)p.back()))
    return false;
  return std::all_of(p.begin(), p.end(), [](unsigned char c) { return std::isalnum(c) || c == ' ' || c == '-'; });
}

// Readable condition for a MET Norway symbol code ("lightrainshowers_day"); the words are the ones the weather icons are picked by.
std::string metCondition(std::string symbol, bool &day)
{
  day = true;
  for (const char *suffix : {"_night", "_day", "_polartwilight"})
  {
    const std::string s = suffix;
    if (symbol.size() > s.size() && symbol.compare(symbol.size() - s.size(), s.size(), s) == 0)
    {
      day = s != "_night";
      symbol.resize(symbol.size() - s.size());
    }
  }
  const auto has = [&](const char *word) { return symbol.find(word) != std::string::npos; };
  const std::string strength = symbol.rfind("heavy", 0) == 0 ? "Heavy " : symbol.rfind("light", 0) == 0 ? "Light " : "";
  const std::string showers = has("showers") ? " Showers" : "";

  if (has("thunder"))
    return "Thunderstorm";
  if (has("snow"))
    return strength + "Snow" + showers;
  if (has("sleet"))
    return strength + "Sleet" + showers;
  if (has("rain"))
    return strength + "Rain" + showers;
  if (has("fog"))
    return "Fog";
  if (has("partlycloudy"))
    return "Partly Cloudy";
  if (has("cloudy"))
    return "Cloudy";
  if (has("fair"))
    return day ? "Mostly Sunny" : "Mostly Clear";
  if (has("clearsky"))
    return day ? "Sunny" : "Clear";
  return "";
}
} // namespace

bool WeatherLookup::isPostalCode(const std::string &text)
{
  Place place;
  return parsePlace(text, place);
}

WeatherLookup::WeatherLookup()
{
  static std::once_flag once;
  std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
  thread_ = std::thread([this] { run(); });
}

WeatherLookup::~WeatherLookup()
{
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_ = true;
  }
  cv_.notify_all();
  thread_.join();
}

void WeatherLookup::setLocations(const std::set<std::string> &postalCodes)
{
  std::lock_guard<std::mutex> lock(mutex_);
  bool changed = false;
  for (auto it = entries_.begin(); it != entries_.end();)
  {
    if (postalCodes.count(it->first) == 0)
    {
      it = entries_.erase(it);
      changed = true;
    }
    else
      ++it;
  }
  for (const std::string &code : postalCodes)
    if (isPostalCode(code) && entries_.emplace(code, Entry{}).second)
      changed = true;
  if (changed)
    cv_.notify_all();
}

std::map<std::string, WeatherConditions> WeatherLookup::conditions() const
{
  std::map<std::string, WeatherConditions> result;
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto &[code, entry] : entries_)
    if (entry.hasConditions)
      result[code] = entry.conditions;
  return result;
}

void WeatherLookup::run()
{
  std::unique_lock<std::mutex> lock(mutex_);
  while (!stop_)
  {
    auto next = entries_.end();
    for (auto it = entries_.begin(); it != entries_.end(); ++it)
      if (next == entries_.end() || it->second.due < next->second.due)
        next = it;
    if (next == entries_.end())
    {
      cv_.wait(lock);
      continue;
    }
    if (Clock::now() < next->second.due)
    {
      cv_.wait_until(lock, next->second.due);
      continue;
    }

    const std::string code = next->first;
    Entry work = next->second;
    lock.unlock();
    const Outcome outcome = refresh(code, work);
    lock.lock();
    if (stop_)
      break;

    const auto it = entries_.find(code);
    if (it == entries_.end())
      continue; // no longer wanted
    const auto now = Clock::now();
    if (outcome == Outcome::Ok)
    {
      work.due = now + REFRESH;
      std::fprintf(stderr, "[weather] %s: %s, %.0fF, %s\n", code.c_str(), work.city.c_str(), work.conditions.temperatureF, work.conditions.condition.c_str());
    }
    else if (outcome == Outcome::NotFound)
    {
      work.due = now + RETRY_NOT_FOUND;
      std::fprintf(stderr, "[weather] %s: not found\n", code.c_str());
    }
    else
    {
      work.due = std::max(now + RETRY_ERROR, backoffUntil_);
      std::fprintf(stderr, "[weather] %s: lookup failed\n", code.c_str());
    }
    it->second = work;
  }
}

// GET with the spacing and back-off the services ask for.
bool WeatherLookup::request(const std::string &url, HttpResult &result)
{
  const auto earliest = std::max(lastRequestAt_ + REQUEST_GAP, backoffUntil_);
  while (Clock::now() < earliest)
  {
    if (stop_)
      return false;
    std::this_thread::sleep_for(100ms);
  }
  lastRequestAt_ = Clock::now();

  result = HttpResult{};
  CURL *curl = curl_easy_init();
  if (curl == nullptr)
    return false;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, appendBody);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.body);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, abortWhenStopping);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &stop_);
  const CURLcode code = curl_easy_perform(curl);
  if (code == CURLE_OK)
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);
  else
    std::fprintf(stderr, "[weather] %s: %s\n", url.c_str(), curl_easy_strerror(code));
  curl_easy_cleanup(curl);

  if (result.status == 429 || result.status == 503)
    backoffUntil_ = Clock::now() + BACKOFF;
  return code == CURLE_OK;
}

// Postal code -> place name, and the forecast of its coordinates: NWS hourly in the US, MET Norway anywhere else.
WeatherLookup::Outcome WeatherLookup::locate(const std::string &postalCode, Entry &entry)
{
  Place where;
  if (!parsePlace(postalCode, where))
    return Outcome::NotFound;
  const bool us = where.country == "US";

  // Zippopotam lists the US territories as countries of their own. Some countries are only listed by the start of their
  // codes (the outward code of GB, the first 3 characters of CA), so a code that is not found is retried shortened.
  std::vector<std::string> countries;
  std::vector<std::string> candidates;
  if (us)
  {
    countries = {"us", "pr", "vi", "gu", "as", "mp"};
    candidates = {where.postal.substr(0, 5)};
  }
  else
  {
    countries = {std::string{(char)std::tolower((unsigned char)where.country[0]), (char)std::tolower((unsigned char)where.country[1])}};
    candidates = {where.postal};
    const std::string token = where.postal.substr(0, where.postal.find_first_of(" -"));
    for (const std::string &shorter : {token, where.postal.substr(0, 3)})
      if (shorter.size() >= 2 && std::find(candidates.begin(), candidates.end(), shorter) == candidates.end())
        candidates.push_back(shorter);
  }
  HttpResult result;
  for (const std::string &country : countries)
  {
    for (std::string candidate : candidates)
    {
      for (size_t space = candidate.find(' '); space != std::string::npos; space = candidate.find(' '))
        candidate.replace(space, 1, "%20");
      if (!request("https://api.zippopotam.us/" + country + "/" + candidate, result))
        return Outcome::Error;
      if (result.status != 404)
        break;
    }
    if (result.status != 404)
      break;
  }
  if (result.status != 200)
    return result.status == 404 ? Outcome::NotFound : Outcome::Error;
  const json places = json::parse(result.body, nullptr, false);
  if (!places.is_object() || !places.contains("places") || !places["places"].is_array() || places["places"].empty())
    return Outcome::Error;
  const json &place = places["places"][0];
  double latitude = 0, longitude = 0;
  if (!place.is_object() || !coordinate(place, "latitude", latitude) || !coordinate(place, "longitude", longitude))
    return Outcome::Error;
  const std::string city = stringOf(place, "place name");

  // Both services want coordinates of at most 4 decimals
  char point[64];
  std::snprintf(point, sizeof(point), "%.4f,%.4f", latitude, longitude);
  std::string hourlyUrl;
  if (us)
  {
    if (!request(std::string(NWS_PREFIX) + "points/" + point, result) || (result.status != 200 && result.status != 404))
      return Outcome::Error;
    if (result.status == 200) // 404: outside NWS coverage, MET Norway then
    {
      const json answer = json::parse(result.body, nullptr, false);
      if (!answer.is_object() || !answer.contains("properties") || !answer["properties"].is_object())
        return Outcome::Error;
      hourlyUrl = stringOf(answer["properties"], "forecastHourly");
      if (hourlyUrl.rfind(NWS_PREFIX, 0) != 0)
        return Outcome::Error;
    }
  }

  entry.city = city;
  entry.hourlyUrl = hourlyUrl;
  entry.metUrl.clear();
  if (hourlyUrl.empty())
  {
    char lat[32], lon[32];
    std::snprintf(lat, sizeof(lat), "%.4f", latitude);
    std::snprintf(lon, sizeof(lon), "%.4f", longitude);
    entry.metUrl = std::string(MET_PREFIX) + "weatherapi/locationforecast/2.0/compact?lat=" + lat + "&lon=" + lon;
  }
  entry.located = true;
  return Outcome::Ok;
}

// MET Norway: the current hour of the forecast, in Celsius with a symbol code.
WeatherLookup::Outcome WeatherLookup::refreshMet(Entry &entry)
{
  HttpResult result;
  if (!request(entry.metUrl, result) || (result.status != 200 && result.status != 203))
    return Outcome::Error;
  const json answer = json::parse(result.body, nullptr, false);
  if (!answer.is_object() || !answer.contains("properties") || !answer["properties"].is_object())
    return Outcome::Error;
  const json &properties = answer["properties"];
  if (!properties.contains("timeseries") || !properties["timeseries"].is_array() || properties["timeseries"].empty())
    return Outcome::Error;
  const json &now = properties["timeseries"][0];
  if (!now.is_object() || !now.contains("data") || !now["data"].is_object())
    return Outcome::Error;
  const json &data = now["data"];
  if (!data.contains("instant") || !data["instant"].is_object() || !data["instant"].contains("details") || !data["instant"]["details"].is_object())
    return Outcome::Error;
  const json &details = data["instant"]["details"];
  if (!details.contains("air_temperature") || !details["air_temperature"].is_number())
    return Outcome::Error;

  std::string symbol;
  for (const char *span : {"next_1_hours", "next_6_hours", "next_12_hours"})
  {
    const auto it = data.find(span);
    if (it != data.end() && it->is_object() && it->contains("summary") && (*it)["summary"].is_object())
    {
      symbol = stringOf((*it)["summary"], "symbol_code");
      if (!symbol.empty())
        break;
    }
  }

  bool day = true;
  entry.conditions.city = entry.city;
  entry.conditions.condition = metCondition(symbol, day);
  entry.conditions.temperatureF = details["air_temperature"].get<double>() * 9.0 / 5.0 + 32.0;
  entry.conditions.isDaytime = day;
  entry.hasConditions = true;
  return Outcome::Ok;
}

WeatherLookup::Outcome WeatherLookup::refresh(const std::string &postalCode, Entry &entry)
{
  if (!entry.located)
  {
    const Outcome located = locate(postalCode, entry);
    if (located != Outcome::Ok)
      return located;
  }
  if (!entry.metUrl.empty())
    return refreshMet(entry);

  HttpResult result;
  if (!request(entry.hourlyUrl, result) || result.status != 200)
  {
    if (result.status == 404)
      entry.located = false; // the grid cell changed: find it again next time
    return Outcome::Error;
  }
  const json answer = json::parse(result.body, nullptr, false);
  if (!answer.is_object() || !answer.contains("properties") || !answer["properties"].is_object())
    return Outcome::Error;
  const json &properties = answer["properties"];
  if (!properties.contains("periods") || !properties["periods"].is_array() || properties["periods"].empty())
    return Outcome::Error;
  const json &now = properties["periods"][0]; // the current hour
  if (!now.is_object() || !now.contains("temperature") || !now["temperature"].is_number())
    return Outcome::Error;

  double temperature = now["temperature"].get<double>();
  if (stringOf(now, "temperatureUnit") == "C")
    temperature = temperature * 9.0 / 5.0 + 32.0;

  entry.conditions.city = entry.city;
  entry.conditions.condition = stringOf(now, "shortForecast");
  entry.conditions.temperatureF = temperature;
  entry.conditions.isDaytime = !now.contains("isDaytime") || !now["isDaytime"].is_boolean() || now["isDaytime"].get<bool>();
  entry.hasConditions = true;
  return Outcome::Ok;
}
