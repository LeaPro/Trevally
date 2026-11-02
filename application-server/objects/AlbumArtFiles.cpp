#include "AlbumArtFiles.h"

#include <curl/curl.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <system_error>
#include <vector>

#include <unistd.h>

// Images are decoded here and republished as PNG: LVGL's JPEG decoder (TJPGD) decodes in pieces, and LVGL cannot scale
// or rotate such partially decoded images, so a JPEG file would not render in an album art widget that fits it to its box.
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_MAX_DIMENSIONS 4096
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

namespace
{

constexpr long MAX_IMAGE_BYTES = 8L * 1024 * 1024;
constexpr int MAX_ALBUM_ART_PX = 500; // longest side of the published image; larger images are scaled down. The biggest album art area is 498 px wide

std::atomic<unsigned> fileCounter{0};

// /run is a tmpfs on the board; fall back to /tmp where /run is not writable (host builds)
const std::string &albumArtDirectory()
{
  static std::string dir;
  static std::once_flag once;
  std::call_once(once, []() {
    dir = access("/run", W_OK) == 0 ? "/run/trevally-album-art" : "/tmp/trevally-album-art";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec); // images of an earlier run; nothing of this run exists yet
    std::filesystem::create_directories(dir, ec);
  });
  return dir;
}

std::string newFilePath(const std::string &prefix, const std::string &ext)
{
  return albumArtDirectory() + "/" + prefix + "-" + std::to_string(fileCounter.fetch_add(1)) + "." + ext;
}

// Writes decoded RGB pixels (stbi_load with 3 channels) as a PNG, scaled down (box filter) to at most MAX_ALBUM_ART_PX.
// Returns false when the file could not be written.
bool writePng(const std::string &path, const unsigned char *rgb, int w, int h)
{
  const int longest = w > h ? w : h;
  if (longest <= MAX_ALBUM_ART_PX)
    return stbi_write_png(path.c_str(), w, h, 3, rgb, w * 3) != 0;

  const int ow = w * MAX_ALBUM_ART_PX / longest > 0 ? w * MAX_ALBUM_ART_PX / longest : 1;
  const int oh = h * MAX_ALBUM_ART_PX / longest > 0 ? h * MAX_ALBUM_ART_PX / longest : 1;
  std::vector<unsigned char> out(static_cast<size_t>(ow) * oh * 3);
  for (int y = 0; y < oh; y++)
  {
    const int y0 = static_cast<int>(static_cast<long long>(y) * h / oh);
    const int y1 = std::max(y0 + 1, static_cast<int>(static_cast<long long>(y + 1) * h / oh));
    for (int x = 0; x < ow; x++)
    {
      const int x0 = static_cast<int>(static_cast<long long>(x) * w / ow);
      const int x1 = std::max(x0 + 1, static_cast<int>(static_cast<long long>(x + 1) * w / ow));
      unsigned sum[3] = {0, 0, 0};
      for (int sy = y0; sy < y1; sy++)
        for (int sx = x0; sx < x1; sx++)
          for (int c = 0; c < 3; c++)
            sum[c] += rgb[(static_cast<size_t>(sy) * w + sx) * 3 + c];
      const unsigned count = static_cast<unsigned>((y1 - y0) * (x1 - x0));
      for (int c = 0; c < 3; c++)
        out[(static_cast<size_t>(y) * ow + x) * 3 + c] = static_cast<unsigned char>((sum[c] + count / 2) / count);
    }
  }
  return stbi_write_png(path.c_str(), ow, oh, 3, out.data(), ow * 3) != 0;
}

// Decodes image data (JPEG or PNG) and writes it as a new PNG file; "" when it is not a decodable image
std::string publishDecoded(const std::string &prefix, unsigned char *rgb, int w, int h)
{
  if (rgb == nullptr)
    return "";
  const std::string path = newFilePath(prefix, "png");
  const bool ok = writePng(path, rgb, w, h);
  stbi_image_free(rgb);
  if (!ok)
  {
    ::remove(path.c_str());
    return "";
  }
  return path;
}

size_t writeToFile(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  return fwrite(ptr, size, nmemb, static_cast<FILE *>(userdata));
}

} // namespace

namespace AlbumArtFiles
{

std::string write(const std::string &prefix, const void *data, size_t size)
{
  if (data == nullptr || size == 0 || size > static_cast<size_t>(MAX_IMAGE_BYTES))
    return "";
  int w = 0, h = 0, n = 0;
  unsigned char *rgb = stbi_load_from_memory(static_cast<const unsigned char *>(data), static_cast<int>(size), &w, &h, &n, 3);
  return publishDecoded(prefix, rgb, w, h);
}

std::string download(const std::string &prefix, const std::string &url, const std::string &userAgent)
{
  if (url.rfind("https://", 0) != 0 && url.rfind("http://", 0) != 0)
    return "";

  static std::once_flag curlInit;
  std::call_once(curlInit, []() { curl_global_init(CURL_GLOBAL_DEFAULT); }); // not thread-safe if left to curl_easy_init

  const std::string path = newFilePath(prefix, "download"); // temporary: replaced by the decoded PNG
  FILE *f = fopen(path.c_str(), "wb");
  if (f == nullptr)
    return "";

  CURL *curl = curl_easy_init();
  if (curl == nullptr)
  {
    fclose(f);
    ::remove(path.c_str());
    return "";
  }
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToFile);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
  curl_easy_setopt(curl, CURLOPT_MAXFILESIZE, MAX_IMAGE_BYTES);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L); // required when called from threads
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  if (!userAgent.empty())
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());

  const CURLcode rc = curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  const bool closed = fclose(f) == 0;

  if (rc != CURLE_OK || !closed)
  {
    ::remove(path.c_str());
    return "";
  }
  int w = 0, h = 0, n = 0;
  unsigned char *rgb = stbi_load(path.c_str(), &w, &h, &n, 3);
  ::remove(path.c_str());
  return publishDecoded(prefix, rgb, w, h);
}

void remove(const std::string &path)
{
  if (!path.empty())
    ::remove(path.c_str());
}

} // namespace AlbumArtFiles
