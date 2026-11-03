#pragma once
#include <cstddef>
#include <string>

// Album art files for the audio sources whose track metadata does not arrive as a local file (streaming providers, Bluetooth).
// The UI loads images from a file path, so every source publishes the path of the current album art in its trackAlbumArt
// element. Every image gets a new file name, so a changed path tells the UI the image changed; the owner removes the file it
// replaced.
namespace AlbumArtFiles
{

// Both functions decode the image (JPEG or PNG), scale it down to at most 500 px (the largest album art area on the display)
// and write it as a new PNG file: LVGL cannot scale JPEGs, which its decoder produces in pieces.
// Writes an image given as encoded data to a new file. prefix identifies the source.
// Returns the file path, or an empty string when the data is not a decodable image or could not be written.
std::string write(const std::string &prefix, const void *data, size_t size);

// Downloads an http(s) image to a new file. Blocks for up to ~20 s: only call it from a worker thread.
// userAgent, when not empty, is sent as the User-Agent header.
// Returns the file path, or an empty string on failure. When transient is not null it is set on failure: true when trying
// again later may succeed (network error, timeout, 5xx, 408, 429), false when the image is not there (other 4xx, or the
// data is not a decodable image).
std::string download(const std::string &prefix, const std::string &url, const std::string &userAgent = "", bool *transient = nullptr);

// Removes a file returned by write() / download(); an empty path is ignored.
void remove(const std::string &path);

} // namespace AlbumArtFiles
