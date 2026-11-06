#include "AlbumArtLookup.h"

#include "AlbumArtFiles.h"
#include "nlohmann.h"

#include <curl/curl.h>

#include <algorithm>
#include <cstdio>

using namespace std::chrono_literals;

namespace
{
// MusicBrainz refuses requests without a User-Agent that identifies the application and a way to reach its maker.
// TODO: replace CONTACT with a real e-mail address or URL before shipping.
const char *const USER_AGENT = "Trevally/1.0 ( CONTACT )";

constexpr auto DEBOUNCE = 300ms;         // the track must stay the same this long before it is looked up
constexpr auto MUSICBRAINZ_GAP = 1100ms; // MusicBrainz: 1 request per second
constexpr auto BACKOFF = 60s;            // after a 429 / 503
constexpr auto RETRY_NOT_FOUND = 10min;
constexpr auto RETRY_ERROR = 30s;
constexpr size_t MAX_ENTRIES = 24;
constexpr size_t MAX_BODY = 1024 * 1024; // search answers
constexpr int MIN_SCORE = 80;            // MusicBrainz search score, 0 - 100
constexpr size_t MAX_ALBUM_ART_TRIES = 3;

struct HttpResult
{
  long status = 0;
  CURLcode code = CURLE_FAILED_INIT;
  std::string body;
};

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

HttpResult httpGet(const std::string &url, const std::atomic<bool> &stop)
{
  HttpResult result;
  CURL *curl = curl_easy_init();
  if (curl == nullptr)
    return result;
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
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &stop);
  result.code = curl_easy_perform(curl);
  if (result.code == CURLE_OK)
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);
  curl_easy_cleanup(curl);
  return result;
}

std::string urlEncode(const std::string &text)
{
  CURL *curl = curl_easy_init();
  if (curl == nullptr)
    return "";
  char *escaped = curl_easy_escape(curl, text.c_str(), static_cast<int>(text.size()));
  std::string result = escaped != nullptr ? escaped : "";
  curl_free(escaped);
  curl_easy_cleanup(curl);
  return result;
}

std::string trimmed(const std::string &text)
{
  const size_t first = text.find_first_not_of(" \t");
  if (first == std::string::npos)
    return "";
  return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}

// Title or album without its (...) and [...] parts: "Mona Lisa (feat. Kendrick Lamar)" -> "Mona Lisa"; MusicBrainz lists the
// recording without them.
std::string withoutBrackets(const std::string &text)
{
  std::string result;
  int depth = 0;
  for (const char c : text)
  {
    if (c == '(' || c == '[')
      ++depth;
    else if ((c == ')' || c == ']') && depth > 0)
      --depth;
    else if (depth == 0)
      result += c;
  }
  result = trimmed(result);
  return result.empty() ? text : result;
}

// The main artist of "Lil Wayne, Kendrick Lamar" / "A & B" / "A feat. B"
std::string mainArtist(const std::string &text)
{
  size_t cut = text.size();
  for (const char *separator : {",", " & ", " feat", " Feat", " ft.", " Ft.", " featuring", " Featuring", " with ", " x ", " X "})
    cut = std::min(cut, text.find(separator));
  const std::string result = trimmed(text.substr(0, cut));
  return result.empty() ? text : result;
}

// Lucene phrase for the MusicBrainz search syntax.
std::string quoted(const std::string &text)
{
  std::string result = "\"";
  for (const char c : text)
  {
    if (c == '"' || c == '\\')
      result += '\\';
    result += c;
  }
  return result + "\"";
}

int scoreOf(const json &item)
{
  const auto it = item.find("score");
  return (it != item.end() && it->is_number()) ? it->get<int>() : 0;
}

std::string idOf(const json &item)
{
  const auto it = item.find("id");
  return (it != item.end() && it->is_string()) ? it->get<std::string>() : "";
}

void addUnique(std::vector<std::string> &ids, const std::string &id)
{
  if (!id.empty() && std::find(ids.begin(), ids.end(), id) == ids.end())
    ids.push_back(id);
}

std::vector<std::string> releaseIds(const json &answer)
{
  std::vector<std::string> ids;
  const auto releases = answer.find("releases");
  if (releases == answer.end() || !releases->is_array())
    return ids;
  for (const json &release : *releases)
    if (scoreOf(release) >= MIN_SCORE)
      addUnique(ids, idOf(release));
  return ids;
}

// Releases the matching recordings appear on; official releases first.
std::vector<std::string> recordingReleaseIds(const json &answer)
{
  std::vector<std::string> official, other;
  const auto recordings = answer.find("recordings");
  if (recordings == answer.end() || !recordings->is_array())
    return official;
  for (const json &recording : *recordings)
  {
    if (scoreOf(recording) < MIN_SCORE)
      continue;
    const auto releases = recording.find("releases");
    if (releases == recording.end() || !releases->is_array())
      continue;
    for (const json &release : *releases)
    {
      const auto status = release.find("status");
      const bool isOfficial = status != release.end() && status->is_string() && status->get<std::string>() == "Official";
      addUnique(isOfficial ? official : other, idOf(release));
    }
  }
  for (const std::string &id : other)
    addUnique(official, id);
  return official;
}

std::string makeKey(const std::string &artist, const std::string &album, const std::string &title)
{
  if (artist.empty() || title.empty())
    return "";
  return artist + '\x1f' + album + '\x1f' + title;
}
} // namespace

AlbumArtLookup::AlbumArtLookup()
{
  static std::once_flag once;
  std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
  thread_ = std::thread([this] { run(); });
}

AlbumArtLookup::~AlbumArtLookup()
{
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_ = true;
  }
  cv_.notify_all();
  thread_.join();
  for (const auto &entry : cache_)
    AlbumArtFiles::remove(entry.second.file);
}

void AlbumArtLookup::request(const std::string &artist, const std::string &album, const std::string &title)
{
  const std::string key = makeKey(artist, album, title);
  std::lock_guard<std::mutex> lock(mutex_);
  if (key == wantedKey_)
    return;
  wantedKey_ = key;
  wanted_ = {artist, album, title};
  cv_.notify_all();
}

std::string AlbumArtLookup::file() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (wantedKey_.empty())
    return "";
  const auto it = cache_.find(wantedKey_);
  return it == cache_.end() ? "" : it->second.file;
}

void AlbumArtLookup::store(const std::string &key, const Entry &entry)
{
  const auto it = cache_.find(key);
  if (it == cache_.end())
  {
    cache_[key] = entry;
    order_.push_back(key);
  }
  else
    it->second = entry;

  while (order_.size() > MAX_ENTRIES)
  {
    const auto victim = std::find_if(order_.begin(), order_.end(), [&](const std::string &k) { return k != wantedKey_ && k != key; });
    if (victim == order_.end())
      break;
    AlbumArtFiles::remove(cache_[*victim].file);
    cache_.erase(*victim);
    order_.erase(victim);
  }
}

void AlbumArtLookup::run()
{
  std::unique_lock<std::mutex> lock(mutex_);
  while (!stop_)
  {
    const std::string key = wantedKey_;
    Clock::time_point wakeAt{}; // when a track that was not found is looked up again
    bool needed = !key.empty();
    if (needed)
    {
      const auto it = cache_.find(key);
      if (it != cache_.end())
      {
        if (!it->second.file.empty())
          needed = false;
        else if (Clock::now() < it->second.retryAt)
        {
          needed = false;
          wakeAt = it->second.retryAt;
        }
      }
    }
    if (!needed)
    {
      if (wakeAt == Clock::time_point{})
        cv_.wait(lock);
      else
        cv_.wait_until(lock, wakeAt);
      continue;
    }

    // Skipping through tracks changes the wanted one all the time: only look up the one that stays.
    if (cv_.wait_for(lock, DEBOUNCE, [&] { return stop_ || wantedKey_ != key; }))
      continue;

    const Track track = wanted_;
    lock.unlock();
    std::string file;
    const Outcome outcome = this->lookup(track, file);
    lock.lock();

    if (stop_)
    {
      AlbumArtFiles::remove(file);
      break;
    }

    Entry entry;
    entry.file = file;
    if (outcome == Outcome::Found)
      std::fprintf(stderr, "[albumart] %s - %s: %s\n", track.artist.c_str(), track.title.c_str(), file.c_str());
    else if (outcome == Outcome::NotFound)
    {
      entry.retryAt = Clock::now() + RETRY_NOT_FOUND;
      std::fprintf(stderr, "[albumart] %s - %s: not found\n", track.artist.c_str(), track.title.c_str());
    }
    else
    {
      entry.retryAt = std::max(Clock::now() + RETRY_ERROR, backoffUntil_);
      std::fprintf(stderr, "[albumart] %s - %s: lookup failed\n", track.artist.c_str(), track.title.c_str());
    }
    store(key, entry);
  }
}

// Asks MusicBrainz for a list of matches; waits as long as its limits require.
bool AlbumArtLookup::search(const std::string &endpoint, const std::string &query, std::string &body)
{
  const auto earliest = std::max(lastRequestAt_ + MUSICBRAINZ_GAP, backoffUntil_);
  while (Clock::now() < earliest)
  {
    if (stop_)
      return false;
    std::this_thread::sleep_for(100ms);
  }
  lastRequestAt_ = Clock::now();

  const HttpResult result = httpGet("https://musicbrainz.org/ws/2/" + endpoint + "/?query=" + urlEncode(query) + "&limit=5&fmt=json", stop_);
  if (result.status == 429 || result.status == 503)
    backoffUntil_ = Clock::now() + BACKOFF;
  if (result.status != 200)
  {
    std::fprintf(stderr, "[albumart] MusicBrainz %s search failed: HTTP %ld, %s\n", endpoint.c_str(), result.status, curl_easy_strerror(result.code));
    return false;
  }
  body = result.body;
  return true;
}

// Downloads the front cover of the first release that has one. transientFailure is set when a download failed in a way that
// may not happen next time (network error, timeout, 5xx), as opposed to the release having no cover.
std::string AlbumArtLookup::fetchFront(const std::vector<std::string> &releaseIds, std::vector<std::string> &tried, bool &transientFailure)
{
  size_t downloads = 0;
  for (const std::string &id : releaseIds)
  {
    if (stop_ || downloads >= MAX_ALBUM_ART_TRIES)
      break;
    if (std::find(tried.begin(), tried.end(), id) != tried.end())
      continue;
    tried.push_back(id);
    ++downloads;
    bool transient = false;
    const std::string file = AlbumArtFiles::download("bluetooth", "https://coverartarchive.org/release/" + id + "/front-500", USER_AGENT, &transient);
    if (!file.empty())
      return file;
    if (transient)
      transientFailure = true;
  }
  return "";
}

AlbumArtLookup::Outcome AlbumArtLookup::lookup(const Track &tagged, std::string &file)
{
  const Track track{mainArtist(tagged.artist), withoutBrackets(tagged.album), withoutBrackets(tagged.title)};
  std::vector<std::string> tried;
  std::string body;
  bool transientFailure = false;

  if (!track.album.empty())
  {
    if (!search("release", "release:" + quoted(track.album) + " AND artist:" + quoted(track.artist), body))
      return Outcome::Error;
    const json answer = json::parse(body, nullptr, false);
    if (!answer.is_discarded())
    {
      file = fetchFront(releaseIds(answer), tried, transientFailure);
      if (!file.empty())
        return Outcome::Found;
    }
  }
  if (stop_)
    return Outcome::Error;

  // No album, or its release has no cover: the recording knows other releases of the track.
  if (!search("recording", "recording:" + quoted(track.title) + " AND artist:" + quoted(track.artist), body))
    return Outcome::Error;
  const json answer = json::parse(body, nullptr, false);
  if (answer.is_discarded())
    return Outcome::Error;
  file = fetchFront(recordingReleaseIds(answer), tried, transientFailure);
  if (!file.empty())
    return Outcome::Found;
  // A cover that could not be downloaded for a reason that may pass is retried soon, not after the not-found wait.
  return transientFailure ? Outcome::Error : Outcome::NotFound;
}
