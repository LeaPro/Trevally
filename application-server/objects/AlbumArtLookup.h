#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Album art for sources that only tell the artist, album and title of the track (Bluetooth): the track is looked up on
// MusicBrainz and its front cover is downloaded from the Cover Art Archive. The lookup runs on a worker thread of its own, so
// request() and file() never block.
//
// MusicBrainz allows 1 request per second per IP and wants a descriptive User-Agent: the worker waits for the track to stay
// unchanged for a moment before it looks it up (skipping through tracks costs nothing), spaces its MusicBrainz requests, and backs
// off for a minute when the server says it is overloaded. Results are cached, so replaying a track needs no network.
class AlbumArtLookup
{
public:
  AlbumArtLookup();
  ~AlbumArtLookup();
  AlbumArtLookup(const AlbumArtLookup &) = delete;
  AlbumArtLookup &operator=(const AlbumArtLookup &) = delete;

  // The track whose cover is wanted. An empty artist or title means there is none. Does nothing when it is the track
  // requested last.
  void request(const std::string &artist, const std::string &album, const std::string &title);

  // PNG file with the cover of the requested track; empty while it is being looked up or when none was found. The file name
  // is different for every image.
  std::string file() const;

  // URL file() was downloaded from (a Cover Art Archive URL); empty when file() is.
  std::string url() const;

private:
  using Clock = std::chrono::steady_clock;
  struct Track
  {
    std::string artist, album, title;
  };
  struct Entry
  {
    std::string file;          // empty: nothing found
    std::string url;           // where file was downloaded from
    Clock::time_point retryAt; // when to look up a track again that has no file
  };
  enum class Outcome { Found, NotFound, Error };

  void run();
  Outcome lookup(const Track &track, std::string &file, std::string &url);
  bool search(const std::string &endpoint, const std::string &query, std::string &body); // false: no usable answer
  std::string fetchFront(const std::vector<std::string> &releaseIds, std::vector<std::string> &tried, bool &transientFailure, std::string &url);
  void store(const std::string &key, const Entry &entry); // mutex_ held

  std::thread thread_;
  mutable std::mutex mutex_; // guards everything below
  std::condition_variable cv_;
  std::atomic<bool> stop_{false};
  std::string wantedKey_;    // empty: nothing wanted
  Track wanted_;
  std::map<std::string, Entry> cache_;
  std::deque<std::string> order_; // keys of cache_, oldest first

  // worker thread only
  Clock::time_point lastRequestAt_{};
  Clock::time_point backoffUntil_{};
};
