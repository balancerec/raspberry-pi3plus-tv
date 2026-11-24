#pragma once
#include <atomic>
#include <cstddef>
#include <istream>
#include <string>
#include <vector>
namespace raspberry_tv {
struct Channel {
    std::string name, group, url, tvgName, tvgId, tvgLogo;
};
struct PlaylistResult {
    std::vector<Channel> channels;
    std::string error;
    std::size_t invalid = 0, duplicates = 0;
    bool truncated = false;
    explicit operator bool() const { return error.empty() && !channels.empty(); }
};
bool validUtf8(const std::string& value);
bool allowedMediaUri(const std::string& value);
std::string channelIdentity(const Channel& channel);
PlaylistResult readM3u(std::istream&, std::size_t limit = 50000,
                      const std::string& baseDirectory = {}, const std::atomic<bool>* cancel = nullptr);
PlaylistResult readPlaylistFile(const std::string& path, const std::atomic<bool>* cancel = nullptr);
std::vector<Channel> parseM3u(std::istream&, std::size_t limit = 10000);
} // namespace raspberry_tv
