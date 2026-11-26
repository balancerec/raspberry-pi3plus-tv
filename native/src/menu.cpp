#include "tv/menu.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <unordered_set>
#include <utility>
namespace raspberry_tv {
bool TvModel::loadPlaylist(const std::string& path) {
    return applyPlaylist(readPlaylistFile(path),path);
}
bool TvModel::applyPlaylist(PlaylistResult loaded,const std::string& path) {
    if(!loaded) {message_=loaded.error;return false;}
    if(const auto* ch=selectedChannel())rememberedChannel_=channelIdentity(*ch);
    const auto oldGroup=channels_.empty()?rememberedGroup_:activeGroup();
    channels_=std::move(loaded.channels);identities_.clear();
    groups_.clear();std::unordered_set<std::string> found;
    for(const auto& channel:channels_) {
        identities_.push_back(channelIdentity(channel));
        if(found.insert(channel.group).second)groups_.push_back(channel.group);
    }
    groupIndex_=0;
    for(std::size_t i=0;i<groups_.size();++i)if(groups_[i]==oldGroup)groupIndex_=i+1;
    rebuildChannels();selectedRow_=0;selectIdentity(rememberedChannel_);
    playlistPath_=path;
    message_=std::to_string(channels_.size())+" canais; "+std::to_string(loaded.invalid)+" invalidos; "+std::to_string(loaded.duplicates)+" duplicados";
    if(loaded.truncated)message_+=" (limite 50000)";
    return true;
}
void TvModel::restoreState(const UserState& state) {
    favorites_=state.favorites;
    if(state.settings.resumeSelection) {rememberedChannel_=state.settings.lastChannel;rememberedGroup_=state.settings.lastGroup;}
    rebuildChannels();
}
void TvModel::captureState(UserState& state) const {
    state.favorites=favorites_;state.settings.playlist=playlistPath_.empty()?state.settings.playlist:playlistPath_;
    if(channels_.empty())return; // failed initial import must not erase remembered selection
    state.settings.lastGroup=activeGroup();state.settings.lastChannel=rememberedChannel_;
    if(const auto* ch=selectedChannel())state.settings.lastChannel=channelIdentity(*ch);
}
bool TvModel::selectIdentity(const std::string& id) {
    const auto& list=view_==View::Favorites?visibleFavorites_:visibleChannels_;
    for(std::size_t row=0;row<list.size();++row)if(identities_[list[row]]==id) {selectedRow_=row;return true;}
    return false;
}
void TvModel::setFilter(const std::string& filter) {
    filter_=filter;rebuildChannels();selectedRow_=0;
}
const char* TvModel::viewTitle(View view) {
    switch(view) {
        case View::Home: return "INICIO";
        case View::Channels: return "TV AO VIVO";
        case View::Library: return "BIBLIOTECA";
        case View::Favorites: return "FAVORITOS";
        case View::Apps: return "APLICATIVOS";
        case View::System: return "SISTEMA";
        case View::Settings: return "CONFIGURACOES";
    }
    return "RASPBERRY TV";
}
std::size_t TvModel::visibleCount() const {
    if (view_ == View::Apps) return appRowCount_;
    if (view_ == View::Settings) return 5;
    if (view_ == View::Favorites) return visibleFavorites_.size();
    if (view_ == View::Channels) return visibleChannels_.size();
    return 0;
}
void TvModel::setAppRowCount(std::size_t count, bool reset) {
    appRowCount_ = count;
    if(view_==View::Apps && (reset || selectedRow_>=count))selectedRow_=0;
}
void TvModel::selectAppRow(std::size_t row) {
    if(view_==View::Apps)selectedRow_=row<appRowCount_?row:0;
}
std::string TvModel::activeGroup() const {
    return groupIndex_ == 0 || groupIndex_ > groups_.size() ?
        "TODOS" : groups_[groupIndex_ - 1];
}
void TvModel::rebuildChannels() {
    visibleChannels_.clear();visibleFavorites_.clear();
    for(std::size_t i=0;i<channels_.size();++i) {
        if(hasFavorite(i))visibleFavorites_.push_back(i);
        if((groupIndex_==0 || channels_[i].group==groups_[groupIndex_-1]) &&
           (filter_.empty() || channels_[i].name.find(filter_)!=std::string::npos || channels_[i].group.find(filter_)!=std::string::npos))
            visibleChannels_.push_back(i);
    }
}
void TvModel::changeGroup(int direction) {
    if (view_ != View::Channels || groups_.empty()) return;
    const std::size_t count = groups_.size() + 1;
    groupIndex_ = (groupIndex_ + (direction > 0 ? 1 : count - 1)) % count;
    rebuildChannels();
    selectedRow_ = 0;
    message_ = "Grupo: " + activeGroup();
}
bool TvModel::stepChannel(int direction) {
    const std::size_t count = visibleCount();
    if (!count || direction == 0 ||
        (view_ != View::Channels && view_ != View::Favorites)) return false;
    const std::size_t next = (selectedRow_ + (direction > 0 ? 1 : count - 1)) % count;
    const bool changed = next != selectedRow_;
    selectedRow_ = next;
    return changed;
}
std::size_t TvModel::channelIndexAt(std::size_t row) const {
    if(view_==View::Favorites)return row<visibleFavorites_.size()?visibleFavorites_[row]:channels_.size();
    return row < visibleChannels_.size() ? visibleChannels_[row] : channels_.size();
}
const Channel* TvModel::selectedChannel() const {
    if (view_ != View::Channels && view_ != View::Favorites) return nullptr;
    if (selectedRow_ >= visibleCount()) return nullptr;
    const auto idx = channelIndexAt(selectedRow_);
    return idx < channels_.size() ? &channels_[idx] : nullptr;
}
void TvModel::setView(View next) {
    if(const auto* ch=selectedChannel())rememberedChannel_=channelIdentity(*ch);
    view_ = next; selectedRow_ = 0;
    if(next==View::Channels || next==View::Favorites)selectIdentity(rememberedChannel_);
    help_ = false; details_ = false;
}
void TvModel::move(int direction) {
    if (view_ == View::Home) {
        const int current = static_cast<int>(selectedHome_);
        const int proposed = current + direction;
        if (direction == 1 && current % 3 == 2) return;
        if (direction == -1 && current % 3 == 0) return;
        if (proposed >= 0 && proposed < 6) selectedHome_ = static_cast<std::size_t>(proposed);
    } else {
        const auto count = visibleCount();
        if (!count) return;
        if (direction < 0 && selectedRow_ > 0) --selectedRow_;
        if (direction > 0 && selectedRow_ + 1 < count) ++selectedRow_;
    }
}
void TvModel::toggleFavorite() {
    if(const auto* ch=selectedChannel())toggleFavoriteIdentity(channelIdentity(*ch));
}
void TvModel::toggleFavoriteIdentity(const std::string& id) {
    if(favorites_.erase(id))message_="Canal removido dos favoritos";
    else {favorites_.insert(id);message_="Canal adicionado aos favoritos";}
    rebuildChannels();
    if(selectedRow_>=visibleCount())selectedRow_=visibleCount()?visibleCount()-1:0;
}
void TvModel::dispatch(Action action) {
    if (action == Action::None) return;
    if (action == Action::Home) { setView(View::Home); message_ = "Tela inicial"; return; }
    if (action == Action::Menu) { help_ = !help_; details_ = false; return; }
    if (action == Action::Back) {
        if (help_) help_ = false;
        else if (details_) details_ = false;
        else setView(View::Home);
        return;
    }
    if (help_ || details_) return;
    if (view_ == View::Settings || view_ == View::Apps) {
        if(action==Action::Up)move(-1);
        if(action==Action::Down)move(1);
        if(view_==View::Apps && action==Action::Left)for(int i=0;i<6;++i)move(-1);
        if(view_==View::Apps && action==Action::Right)for(int i=0;i<6;++i)move(1);
    }
    if (action == Action::Favorites) {
        if (view_ == View::Channels || view_ == View::Favorites) toggleFavorite();
        else setView(View::Favorites);
        return;
    }
    if (action == Action::Details) {
        if (selectedChannel()) details_ = true;
        else message_ = "Selecione um canal para ver detalhes";
        return;
    }
    if (view_ == View::Home) {
        if (action == Action::Up) move(-3);
        else if (action == Action::Down) move(3);
        else if (action == Action::Left) move(-1);
        else if (action == Action::Right) move(1);
        else if (action == Action::Confirm) {
            constexpr View mapping[] = {View::Channels,View::Library,View::Favorites,
                                        View::Apps,View::System,View::Settings};
            setView(mapping[selectedHome_]);
        }
        return;
    }
    if (action == Action::PreviousCategory || action == Action::NextCategory) {
        if (view_ == View::Channels) {
            changeGroup(action == Action::NextCategory ? 1 : -1);
            return;
        }
        constexpr View categories[] = {View::Channels,View::Library,View::Favorites,
                                       View::Apps,View::System,View::Settings};
        const auto it = std::find(std::begin(categories), std::end(categories), view_);
        int i = it == std::end(categories) ? 0 : static_cast<int>(it - std::begin(categories));
        i = (i + (action == Action::NextCategory ? 1 : 5)) % 6;
        setView(categories[i]);
        return;
    }
    if (view_ == View::Channels || view_ == View::Favorites) {
        if (action == Action::Up) move(-1);
        if (action == Action::Down) move(1);
        if (action == Action::PreviousChannel) stepChannel(-1);
        if (action == Action::NextChannel) stepChannel(1);
        if (action == Action::Left) for (int i = 0; i < 8; ++i) move(-1);
        if (action == Action::Right) for (int i = 0; i < 8; ++i) move(1);
        if (action == Action::Confirm) {
            message_ = selectedChannel() ? "Pressione X para reproduzir" :
                                          "Importe uma playlist M3U com --playlist CAMINHO";
        }
    }
}
} // namespace raspberry_tv
