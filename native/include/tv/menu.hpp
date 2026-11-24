#pragma once
#include "tv/input.hpp"
#include <cstddef>
#include <istream>
#include <set>
#include <string>
#include <vector>
namespace raspberry_tv {
enum class View { Home, Channels, Library, Favorites, Apps, System, Settings };
struct Channel { std::string name; std::string group; std::string url; };
std::vector<Channel> parseM3u(std::istream& input, std::size_t limit = 10000);
class TvModel {
public:
    bool loadPlaylist(const std::string& path);
    void dispatch(Action action);
    View view() const { return view_; }
    std::size_t selectedHome() const { return selectedHome_; }
    std::size_t selectedRow() const { return selectedRow_; }
    bool showHelp() const { return help_; }
    bool showDetails() const { return details_; }
    bool hasFavorite(std::size_t index) const { return favorites_.count(index) != 0; }
    const std::string& message() const { return message_; }
    void setStatusMessage(const std::string& value) { message_ = value; }
    const std::string& playlistPath() const { return playlistPath_; }
    const std::vector<Channel>& channels() const { return channels_; }
    std::size_t visibleCount() const;
    std::size_t channelIndexAt(std::size_t visibleRow) const;
    const Channel* selectedChannel() const;
    std::size_t favoriteCount() const { return favorites_.size(); }
    static const char* viewTitle(View view);
private:
    View view_ = View::Home;
    std::size_t selectedHome_ = 0;
    std::size_t selectedRow_ = 0;
    std::vector<Channel> channels_;
    std::set<std::size_t> favorites_;
    std::string playlistPath_;
    std::string message_ = "Controle PS4 pronto";
    bool help_ = false;
    bool details_ = false;
    void setView(View next);
    void move(int direction);
    void toggleFavorite();
};
} // namespace raspberry_tv
