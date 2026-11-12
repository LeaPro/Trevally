#pragma once
#include <cstddef>
#include <string>

// Cover art image files for the audio sources whose metadata does not arrive as a local file (streaming providers).
// The UI loads images from a file path, so providers publish the path of the current image in trackArt /
// trackArtThumbnail. Every image gets a new file name, so a changed path tells the UI the image changed; the owner
// removes the file it replaced. Bluetooth does its own file handling (see Bluetooth.cpp).
namespace ArtFiles
{

// Both functions decode the image (JPEG or PNG), scale it down to at most 256 px and write it as a new PNG file: LVGL cannot
// scale JPEGs, which its decoder produces in pieces.
// Writes an image given as encoded data to a new file. prefix identifies the provider.
// Returns the file path, or an empty string when the data is not a decodable image or could not be written.
std::string write(const std::string &prefix, const void *data, size_t size);

// Downloads an http(s) image to a new file. Blocks for up to ~20 s: only call it from a worker thread.
// userAgent, when not empty, is sent as the User-Agent header; maxPx (when > 0) replaces the default 256 px limit.
// Returns the file path, or an empty string on failure.
std::string download(const std::string &prefix, const std::string &url, const std::string &userAgent = "", int maxPx = 0);

// Removes a file returned by write() / download(); an empty path is ignored.
void remove(const std::string &path);

} // namespace ArtFiles
