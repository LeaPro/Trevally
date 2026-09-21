#include "objects.h"
#include <cassert>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>
#include <cstdlib>
#include <stdexcept>
#include <unistd.h>

using std::string;

#if SOUNDTRACK_ENABLED

Soundtrack::Soundtrack(string path, Object *parent)
  : LeafObject(path, parent),
    sourceTypeMap({{0, "playlist"}, {1, "schedule"}})
{
  string name;
  name = "sdkReady"; elements[name] = sdkReadyPtr = std::make_shared<BoolSensor>(path + "/" + name, this, false);
  name = "paired"; elements[name] = pairedPtr = std::make_shared<BoolSensor>(path + "/" + name, this, false);
  name = "authStatus"; elements[name] = authStatusPtr = std::make_shared<StringSensor>(path + "/" + name, this, "unknown");
  name = "deviceId"; elements[name] = deviceIdPtr = std::make_shared<StringSensor>(path + "/" + name, this, "", 256);
  name = "isPlaying"; elements[name] = isPlayingPtr = std::make_shared<BoolSensor>(path + "/" + name, this, false);
  name = "isPaused"; elements[name] = isPausedPtr = std::make_shared<BoolSensor>(path + "/" + name, this, false);
  name = "volume"; elements[name] = volumePtr = std::make_shared<UInt32Control>(path + "/" + name, this, 0, 100, 100, "percent");

  name = "pairCode"; elements[name] = pairCodePtr = std::make_shared<StringControl>(path + "/" + name, this, "", 16, false, true);
  name = "pairNow"; elements[name] = pairNowPtr = std::make_shared<BoolControl>(path + "/" + name, this, false, false, true);
  name = "unpair"; elements[name] = unpairPtr = std::make_shared<BoolControl>(path + "/" + name, this, false, false, true);
  name = "play"; elements[name] = playPtr = std::make_shared<BoolControl>(path + "/" + name, this, false, false, true);
  name = "pause"; elements[name] = pausePtr = std::make_shared<BoolControl>(path + "/" + name, this, false, false, true);
  name = "next"; elements[name] = nextPtr = std::make_shared<BoolControl>(path + "/" + name, this, false, false, true);
  name = "previous"; elements[name] = previousPtr = std::make_shared<BoolControl>(path + "/" + name, this, false, false, true);

  name = "playFromSourceId"; elements[name] = playFromSourceIdPtr = std::make_shared<StringControl>(path + "/" + name, this, "", 256, false, true);
  name = "playFromSourceType"; elements[name] = playFromSourceTypePtr = std::make_shared<EnumControl>(path + "/" + name, this, 0, 1, 0, sourceTypeMap, false, true);
  name = "playFromNow"; elements[name] = playFromNowPtr = std::make_shared<BoolControl>(path + "/" + name, this, true, false, true);

  name = "soundzone"; elements[name] = soundzonePtr = std::make_shared<JsonSensor>(path + "/" + name, this, json::object());
  name = "currentTrack"; elements[name] = currentTrackPtr = std::make_shared<JsonSensor>(path + "/" + name, this, json::object());
  name = "library"; elements[name] = libraryPtr = std::make_shared<JsonSensor>(path + "/" + name, this, json::array());
  name = "troubles"; elements[name] = troublesPtr = std::make_shared<JsonSensor>(path + "/" + name, this, json::array());
  name = "lastError"; elements[name] = lastErrorPtr = std::make_shared<StringSensor>(path + "/" + name, this, "", 512);

  methods.insert("pair");
  methods.insert("libraryFetch");
  methods.insert("libraryReset");
  methods.insert("playFrom");
}

Soundtrack::~Soundtrack()
{
  shutdownSdk();
}

void Soundtrack::initialize()
{
  LeafObject::initialize();
  initializeSdk();
  syncSensorState();
}

extern "C" {
#include "audio_output_callbacks.h"
#include "splayerapi/splayerapi-5.h"
#include "splayerapi/splayer_auth_api-1.h"
#include "splayerapi/splayer_controls_api-3.h"
#include "splayerapi/splayer_library_api-1.h"
#include "splayerapi/splayer_metadata_api-4.h"
#include "splayerapi/splayer_troubles_api-2.h"
}

namespace {
const char *chooseCacheDir()
{
  if (access("/mnt/data", W_OK) == 0)
  {
    return "/mnt/data/soundtrack-cache";
  }
  return "/tmp/soundtrack-cache";
}

std::string chooseVendorDeviceName()
{
  char hostname[256] = {0};
  if (gethostname(hostname, sizeof(hostname) - 1) == 0 && hostname[0] != '\0')
  {
    return std::string(hostname);
  }
  return "trevally-player";
}
}

void Soundtrack::initializeSdk()
{
  if (sdkInitialized)
  {
    return;
  }

  api = &SPLAYER_API;
  controlsApi = api->get_controls_api();
  authApi = api->get_auth_api();
  metadataApi = api->get_metadata_api();
  libraryApi = api->get_library_api();
  troublesApi = api->get_troubles_api();

  if (controlsApi == nullptr || authApi == nullptr || metadataApi == nullptr || libraryApi == nullptr || troublesApi == nullptr)
  {
    lastErrorPtr->set("Soundtrack API module lookup failed");
    return;
  }

  audioCallbacks = audio_api_allocate();
  if (audioCallbacks == nullptr)
  {
    lastErrorPtr->set("Soundtrack audio callback allocation failed");
    return;
  }

  splayer_config_t config = {SPLAYER_SDK_VERSION};
  const std::string vendorDeviceName = chooseVendorDeviceName();
  config.audio_api_callbacks = audioCallbacks;
  config.diskcache_dir = chooseCacheDir();
  config.diskcache_max_mb = 1024;
  config.diskcache_remain_mb = 256;
  config.output_sample_channels = 2;
  config.output_sample_rate = 48000;
  config.vendor_device_name = vendorDeviceName.c_str();
  config.app_version = "1.0";
  config.disable_audio_caching = false;

  const int rc = api->create(config, &splayer);
  if (rc != 0 || splayer == nullptr)
  {
    lastErrorPtr->set("Soundtrack SDK create failed");
    return;
  }

  sdkInitialized = true;
  sdkReadyPtr->set(true);
  lastErrorPtr->set("");
}

void Soundtrack::shutdownSdk()
{
  if (!sdkInitialized || api == nullptr)
  {
    if (audioCallbacks != nullptr)
    {
      free(audioCallbacks);
      audioCallbacks = nullptr;
    }
    return;
  }

  if (splayer != nullptr)
  {
    api->request_exit(splayer, SPLAYER_EXIT_NORMAL);
    for (int i = 0; i < 20 && !api->should_exit(splayer); ++i)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    api->free(splayer);
    splayer = nullptr;
  }

  if (audioCallbacks != nullptr)
  {
    free(audioCallbacks);
    audioCallbacks = nullptr;
  }

  sdkInitialized = false;
  sdkReadyPtr->set(false);
}

void Soundtrack::update(bool sensorsOnly, bool refreshVolatileElements)
{
  (void)refreshVolatileElements;

  if (!sdkInitialized || splayer == nullptr)
  {
    return;
  }

  // Drive the SDK lifecycle so auth/playback events progress between polls.
  api->loop_iteration(splayer);

  syncPairingResult();

  if (sensorsOnly)
  {
    syncSensorState();
  }
  else
  {
    handleControls();
  }
}

json Soundtrack::processJson(const string &method, json &j, int client)
{
  if (!sdkInitialized || splayer == nullptr)
  {
    throw std::runtime_error("Soundtrack SDK is not initialized");
  }

  if (method == "pair")
  {
    if (!j.is_string())
    {
      throw std::runtime_error("pair expects string pairing code");
    }
    const string code = j.get<string>();
    if (code.empty())
    {
      throw std::runtime_error("pair code cannot be empty");
    }
    const int rc = authApi->initiate_pair_with_code(splayer, code.c_str());
    if (rc != 0)
    {
      lastErrorPtr->set("Soundtrack pair initiation failed");
      return false;
    }
    pairCodePtr->set(code);
    pairNowPtr->set(false);
    return true;
  }

  if (method == "libraryReset")
  {
    libraryApi->reset_library(splayer);
    libraryPtr->set(json::array());
    return true;
  }

  if (method == "libraryFetch")
  {
    int limit = 50;
    if (!j.is_null())
    {
      if (!j.is_number_integer())
      {
        throw std::runtime_error("libraryFetch expects integer limit or null");
      }
      limit = std::max(1, std::min(500, j.get<int>()));
    }

    splayer_library_result_t *result = nullptr;
    const int rc = libraryApi->fetch_library_sync(splayer, limit, &result);
    if (rc != 0 || result == nullptr)
    {
      lastErrorPtr->set("Soundtrack library fetch failed");
      return json::array();
    }

    json output = json::array();
    const int count = libraryApi->get_result_count(result);
    for (int i = 0; i < count; ++i)
    {
      const splayer_music_source_t *item = libraryApi->get_library_result_item(result, i);
      if (item == nullptr)
      {
        continue;
      }
      json row = json::object();
      row["id"] = libraryApi->get_source_id(item);
      row["name"] = libraryApi->get_source_name(item);
      row["type"] = sourceTypeToString(static_cast<int>(libraryApi->get_source_type(item)));
      row["imageUri"] = libraryApi->get_source_image_uri(item);
      output.push_back(row);
    }

    libraryApi->free_library_result(result);
    libraryPtr->set(output);
    return output;
  }

  if (method == "playFrom")
  {
    if (!j.is_object())
    {
      throw std::runtime_error("playFrom expects object with sourceId and sourceType");
    }

    if (!j.contains("sourceId") || !j["sourceId"].is_string())
    {
      throw std::runtime_error("playFrom.sourceId must be string");
    }

    const string sourceId = j["sourceId"].get<string>();
    const string sourceTypeStr = j.value("sourceType", string("playlist"));
    const int sourceType = sourceTypeFromString(sourceTypeStr);
    const int playNow = j.value("playNow", true) ? 1 : 0;

    const int rc = libraryApi->set_play_from(splayer, sourceId.c_str(), static_cast<splayer_source_type_t>(sourceType), playNow);
    if (rc != 0)
    {
      lastErrorPtr->set("Soundtrack set_play_from failed");
      return false;
    }

    playFromSourceIdPtr->set(sourceId);
    playFromSourceTypePtr->setEnumValue(sourceType);
    playFromNowPtr->set(playNow != 0);
    return true;
  }

  return LeafObject::processJson(method, j, client);
}

void Soundtrack::syncSensorState()
{
  if (!sdkInitialized || splayer == nullptr)
  {
    return;
  }

  const int authStatus = static_cast<int>(authApi->get_auth_status(splayer));
  authStatusPtr->set(authStatusToString(authStatus));
  pairedPtr->set(authStatus == SPLAYER_AUTH_STATUS_PAIRED);
  isPlayingPtr->set(controlsApi->is_playing(splayer) != 0);
  isPausedPtr->set(controlsApi->is_paused(splayer) != 0);

  const int volume = controlsApi->get_volume(splayer);
  if (volume >= 0 && volume <= 100)
  {
    volumePtr->set(static_cast<uint32_t>(volume));
  }

  splayer_soundzone_t *soundzone = authApi->get_soundzone(splayer);
  if (soundzone != nullptr)
  {
    json zone = json::object();
    zone["id"] = soundzone->id == nullptr ? "" : soundzone->id;
    zone["name"] = soundzone->name == nullptr ? "" : soundzone->name;
    soundzonePtr->set(zone);
    authApi->free_soundzone(soundzone);
  }

  syncMetadata();
  syncTroubles();

  if (api->should_exit(splayer))
  {
    lastErrorPtr->set("Soundtrack requested process exit");
  }
}

void Soundtrack::syncPairingResult()
{
  splayer_pair_result_t *result = authApi->get_pairing_result(splayer);
  if (result == nullptr)
  {
    return;
  }

  if (result->success)
  {
    if (result->device_id != nullptr)
    {
      deviceIdPtr->set(result->device_id);
    }
    lastErrorPtr->set("");
  }
  else
  {
    const char *msg = result->message == nullptr ? "Soundtrack pairing failed" : result->message;
    lastErrorPtr->set(msg);
  }

  authApi->free_pairing_result(result);
}

void Soundtrack::syncTroubles()
{
  splayer_troubles_array_t *arr = nullptr;
  const int rc = troublesApi->get_troubles(splayer, &arr);
  if (rc != 0 || arr == nullptr)
  {
    return;
  }

  json out = json::array();
  for (size_t i = 0; i < arr->size; ++i)
  {
    const splayer_trouble_t &t = arr->splayer_trouble_array[i];
    json row = json::object();
    row["name"] = t.trouble_name == nullptr ? "" : t.trouble_name;
    row["comment"] = t.trouble_comment == nullptr ? "" : t.trouble_comment;
    row["performed"] = t.trouble_test_performed != 0;
    row["ok"] = t.trouble_test_value != 0;
    row["severity"] = static_cast<int>(t.trouble_severity);
    out.push_back(row);
  }
  troublesApi->free_troubles(arr);
  troublesPtr->set(out);
}

void Soundtrack::syncMetadata()
{
  splayer_track_metadata_t *md = metadataApi->get_current_track_metadata(splayer);
  if (md == nullptr)
  {
    return;
  }

  json artists = json::array();
  for (int i = 0; i < md->num_artists; ++i)
  {
    artists.push_back(md->artists[i] == nullptr ? "" : md->artists[i]);
  }

  json out = json::object();
  out["title"] = md->track_title == nullptr ? "" : md->track_title;
  out["album"] = md->album_name == nullptr ? "" : md->album_name;
  out["artists"] = artists;
  out["isrc"] = md->isrc == nullptr ? "" : md->isrc;
  out["durationMs"] = md->duration_ms;
  out["positionSec"] = metadataApi->get_current_track_position(splayer);
  out["albumImageUri"] = md->album_image_uri == nullptr ? "" : md->album_image_uri;
  out["albumImageCached"] = md->is_album_image_cached;

  currentTrackPtr->set(out);
  metadataApi->free_track_metadata(md);
}

void Soundtrack::handleControls()
{
  if (pairNowPtr->isModified() && pairNowPtr->get())
  {
    const string code = pairCodePtr->get();
    if (!code.empty())
    {
      const int rc = authApi->initiate_pair_with_code(splayer, code.c_str());
      if (rc != 0)
      {
        lastErrorPtr->set("Soundtrack pair initiation failed");
      }
    }
    pairNowPtr->set(false);
  }

  if (unpairPtr->isModified() && unpairPtr->get())
  {
    authApi->unpair(splayer);
    unpairPtr->set(false);
  }

  if (playPtr->isModified() && playPtr->get())
  {
    if (controlsApi->play(splayer) != 0)
    {
      lastErrorPtr->set("Soundtrack play failed");
    }
    playPtr->set(false);
  }

  if (pausePtr->isModified() && pausePtr->get())
  {
    if (controlsApi->pause(splayer) != 0)
    {
      lastErrorPtr->set("Soundtrack pause failed");
    }
    pausePtr->set(false);
  }

  if (nextPtr->isModified() && nextPtr->get())
  {
    if (controlsApi->skip_tracks(splayer, 1) != 0)
    {
      lastErrorPtr->set("Soundtrack next failed");
    }
    nextPtr->set(false);
  }

  if (previousPtr->isModified() && previousPtr->get())
  {
    if (controlsApi->skip_tracks(splayer, -1) != 0)
    {
      lastErrorPtr->set("Soundtrack previous failed");
    }
    previousPtr->set(false);
  }

  if (volumePtr->isModified())
  {
    const uint32_t volume = volumePtr->get();
    if (controlsApi->set_volume(splayer, static_cast<int>(volume)) != 0)
    {
      lastErrorPtr->set("Soundtrack volume set failed");
    }
  }

  if (isModified({playFromSourceIdPtr, playFromSourceTypePtr, playFromNowPtr}, false))
  {
    const string sourceId = playFromSourceIdPtr->get();
    if (!sourceId.empty())
    {
      const int sourceType = playFromSourceTypePtr->getEnumValue();
      const int playNow = playFromNowPtr->get() ? 1 : 0;
      const int rc = libraryApi->set_play_from(splayer, sourceId.c_str(), static_cast<splayer_source_type_t>(sourceType), playNow);
      if (rc != 0)
      {
        lastErrorPtr->set("Soundtrack set_play_from failed");
      }
    }
    playFromSourceIdPtr->isModified();
    playFromSourceTypePtr->isModified();
    playFromNowPtr->isModified();
  }
}

const char *Soundtrack::authStatusToString(int status) const
{
  switch (status)
  {
    case SPLAYER_AUTH_STATUS_PAIRED: return "paired";
    case SPLAYER_AUTH_STATUS_NOT_PAIRED: return "not_paired";
    case SPLAYER_AUTH_STATUS_UPDATE_REQUIRED: return "update_required";
    case SPLAYER_AUTH_STATUS_UNKNOWN:
    default:
      return "unknown";
  }
}

const char *Soundtrack::sourceTypeToString(int type) const
{
  auto it = sourceTypeMap.find(type);
  if (it == sourceTypeMap.end())
  {
    return "playlist";
  }
  return it->second;
}

int Soundtrack::sourceTypeFromString(const string &type) const
{
  if (type == "schedule")
  {
    return 1;
  }
  return 0;
}

#else // SOUNDTRACK_ENABLED

Soundtrack::Soundtrack(string path, Object *parent)
  : LeafObject(path, parent)
{
  assert(false && "Soundtrack SDK unsupported on this architecture");
}

#endif // SOUNDTRACK_ENABLED
