#pragma once
#include "tv/input.hpp"
#include "tv/playlist.hpp"
#include "tv/settings.hpp"
#include <cstddef>
#include <istream>
#include <set>
#include <string>
#include <vector>
namespace raspberry_tv {
enum class View { Home, Channels, Library, Favorites, Apps, System, Settings };
class TvModel {
public:
    bool loadPlaylist(const std::string& path);
    bool applyPlaylist(PlaylistResult result, const std::string& path);
    void restoreState(const UserState& state);
    void captureState(UserState& state) const;
    void setFilter(const std::string& filter);
    const std::string& filter() const { return filter_; }
    void toggleFavoriteIdentity(const std::string& id);
    bool selectIdentity(const std::string& id);
    void dispatch(Action action);
    View view() const { return view_; }
    std::size_t selectedHome() const { return selectedHome_; }
    std::size_t selectedRow() const { return selectedRow_; }
    void setAppRowCount(std::size_t count, bool reset = false);
    void selectAppRow(std::size_t row);
    bool showHelp() const { return help_; }
    bool showDetails() const { return details_; }
    bool hasFavorite(std::size_t index) const { return index < identities_.size() && favorites_.count(identities_[index]) != 0; }
    const std::string& message() const { return message_; }
    void setStatusMessage(const std::string& value) { message_ = value; }
    const std::string& playlistPath() const { return playlistPath_; }
    const std::vector<Channel>& channels() const { return channels_; }
    std::size_t visibleCount() const;
    std::size_t channelIndexAt(std::size_t visibleRow) const;
    const Channel* selectedChannel() const;
    std::size_t favoriteCount() const { return visibleFavorites_.size(); }
    std::size_t groupCount() const { return groups_.size(); }
    std::string activeGroup() const;
    bool stepChannel(int direction); // +/-1, wraps in current visible list
    static const char* viewTitle(View view);
private:
    View view_ = View::Home;
    std::size_t selectedHome_ = 0;
    std::size_t selectedRow_ = 0;
    std::size_t appRowCount_ = 11;
    std::vector<Channel> channels_;
    std::vector<std::string> groups_;
    std::vector<std::size_t> visibleChannels_;
    std::size_t groupIndex_ = 0; // 0 = all groups, 1..N = groups_[index-1]
    std::set<std::string> favorites_;
    std::vector<std::string> identities_;
    std::vector<std::size_t> visibleFavorites_;
    std::string filter_, rememberedChannel_, rememberedGroup_ = "TODOS";
    std::string playlistPath_;
    std::string message_ = "Controle PS4 pronto";
    bool help_ = false;
    bool details_ = false;
    void setView(View next);
    void move(int direction);
    void toggleFavorite();
    void changeGroup(int direction);
    void rebuildChannels();
};
} // namespace raspberry_tv
