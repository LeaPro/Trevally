#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>

// Current conditions of a postal code.
struct WeatherConditions
{
  std::string city;        // place name of the postal code, e.g. "Seattle"
  std::string condition;   // short description, e.g. "Mostly Cloudy"
  double temperatureF = 0; // degrees Fahrenheit
  bool isDaytime = true;   // the sun is up (picks the day or night icon)
};

// Looks up the city, current temperature and conditions of postal codes (the binding of weather tiles) on a worker thread of
// its own, so setLocations() and conditions() never block.
//
// All services are free for commercial use and need no API key:
//  - Zippopotam.us (data: GeoNames, CC BY 4.0) turns the postal code into a place name and coordinates; done once per code.
//  - The National Weather Service API (api.weather.gov, US government open data) gives the forecast grid of US coordinates
//    and the hourly forecast whose current hour is the temperature and conditions.
//  - MET Norway's Locationforecast (api.met.no, CC BY 4.0 / NLOD) does the same for anywhere else, and for US places NWS
//    does not cover.
//    Both want a User-Agent that tells who is asking, and answer 429 / 503 when asked too often.
//
// Postal codes: a US code is written as is ("98102", "98102-1234"), the code of another country has its ISO 3166 alpha-2 country
// code in front: "GB:SW1A 1AA", "DE:10115", "CA:M5V 3L9". Which countries are known is up to Zippopotam.us.
//
// Conditions are refreshed every 15 minutes and cached; the last known conditions stay available when a refresh fails.
class WeatherLookup
{
public:
  WeatherLookup();
  ~WeatherLookup();
  WeatherLookup(const WeatherLookup &) = delete;
  WeatherLookup &operator=(const WeatherLookup &) = delete;

  // The postal codes whose conditions are wanted. Codes that are no longer listed are dropped.
  void setLocations(const std::set<std::string> &postalCodes);

  // Conditions found so far, keyed by postal code; codes that were not found (yet) are missing.
  std::map<std::string, WeatherConditions> conditions() const;

  // A US postal code (5 digits, optionally with the ZIP+4 extension), or "CC:" and a postal code of another country.
  static bool isPostalCode(const std::string &text);

private:
  using Clock = std::chrono::steady_clock;
  struct Entry
  {
    bool located = false;      // city and hourlyUrl are known
    std::string city;
    std::string hourlyUrl;     // NWS hourly forecast of the postal code's grid cell (US)
    std::string metUrl;        // MET Norway forecast of its coordinates (everywhere else)
    bool hasConditions = false;
    WeatherConditions conditions;
    Clock::time_point due{};   // when to refresh; epoch: now
  };
  enum class Outcome { Ok, NotFound, Error };
  struct HttpResult
  {
    long status = 0;
    std::string body;
  };

  void run();
  Outcome refresh(const std::string &postalCode, Entry &entry);
  Outcome locate(const std::string &postalCode, Entry &entry);
  Outcome refreshMet(Entry &entry);
  bool request(const std::string &url, HttpResult &result); // false: no answer, or stopping; spaces and backs off requests

  std::thread thread_;
  mutable std::mutex mutex_; // guards entries_
  std::condition_variable cv_;
  std::atomic<bool> stop_{false};
  std::map<std::string, Entry> entries_;

  // worker thread only
  Clock::time_point lastRequestAt_{};
  Clock::time_point backoffUntil_{};
};
