#include "tv/menu.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <utility>
namespace raspberry_tv {
namespace {
std::string trim(std::string text) {
    const auto a = text.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    const auto b = text.find_last_not_of(" \t\r\n");
    return text.substr(a, b - a + 1);
}
std::string attribute(const std::string& extinf, const std::string& name) {
    const std::string key = name + "=\"";
    const auto at = extinf.find(key);
    if (at == std::string::npos) return "";
    const auto start = at + key.size();
    const auto end = extinf.find('"', start);
    return end == std::string::npos ? "" : extinf.substr(start, end - start);
}
}
std::vector<Channel> parseM3u(std::istream& input, std::size_t limit) {
    std::vector<Channel> result;
    std::string line, pendingName, pendingGroup;
    if (limit == 0) return result;
    while (std::getline(input, line) && result.size() < limit) {
        line = trim(line);
        if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF) line.erase(0, 3);
        if (line.empty()) continue;
        if (line.rfind("#EXTINF", 0) == 0) {
            pendingGroup = attribute(line, "group-title");
            const auto comma = line.find(',');
            pendingName = comma == std::string::npos ? "" : trim(line.substr(comma + 1));
            if (pendingName.empty()) pendingName = attribute(line, "tvg-name");
            continue;
        }
        if (line[0] == '#') continue;
        result.push_back({pendingName.empty() ? "Canal " + std::to_string(result.size() + 1) : pendingName,
                          pendingGroup.empty() ? "Sem categoria" : pendingGroup, line});
        pendingName.clear();
        pendingGroup.clear();
    }
    return result;
}
bool TvModel::loadPlaylist(const std::string& path) {
    std::ifstream file(path);
    if (!file) { message_ = "Nao foi possivel abrir a playlist"; return false; }
    auto loaded = parseM3u(file);
    if (loaded.empty()) { message_ = "Playlist sem canais reconhecidos"; return false; }
    channels_ = std::move(loaded);
    favorites_.clear();
    selectedRow_ = 0;
    playlistPath_ = path;
    message_ = std::to_string(channels_.size()) + " canais carregados";
    return true;
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
    if (view_ == View::Favorites) return favorites_.size();
    if (view_ == View::Channels) return channels_.size();
    return 0;
}
std::size_t TvModel::channelIndexAt(std::size_t row) const {
    if (view_ == View::Favorites) {
        if (row >= favorites_.size()) return channels_.size();
        auto it = favorites_.begin();
        std::advance(it, static_cast<long>(row));
        return *it;
    }
    return row < channels_.size() ? row : channels_.size();
}
const Channel* TvModel::selectedChannel() const {
    if (selectedRow_ >= visibleCount()) return nullptr;
    const auto idx = channelIndexAt(selectedRow_);
    return idx < channels_.size() ? &channels_[idx] : nullptr;
}
void TvModel::setView(View next) {
    view_ = next; selectedRow_ = 0;
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
    if (view_ != View::Channels && view_ != View::Favorites) return;
    if (!selectedChannel()) return;
    const auto index = channelIndexAt(selectedRow_);
    if (favorites_.erase(index)) {
        message_ = "Canal removido dos favoritos";
        const auto count = visibleCount();
        if (count && selectedRow_ >= count) selectedRow_ = count - 1;
        if (count == 0) selectedRow_ = 0;
    } else {
        favorites_.insert(index);
        message_ = "Canal adicionado aos favoritos";
    }
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
        constexpr View categories[] = {View::Channels,View::Library,View::Favorites,
                                       View::Apps,View::System,View::Settings};
        const auto it = std::find(std::begin(categories), std::end(categories), view_);
        int i = it == std::end(categories) ? 0 : static_cast<int>(it - std::begin(categories));
        i = (i + (action == Action::NextCategory ? 1 : 5)) % 6;
        setView(categories[i]);
        return;
    }
    if (view_ == View::Channels || view_ == View::Favorites) {
        if (action == Action::Up || action == Action::PreviousChannel) move(-1);
        if (action == Action::Down || action == Action::NextChannel) move(1);
        if (action == Action::Left) for (int i = 0; i < 8; ++i) move(-1);
        if (action == Action::Right) for (int i = 0; i < 8; ++i) move(1);
        if (action == Action::Confirm) {
            message_ = selectedChannel() ? "Reproducao de video sera integrada na proxima etapa" :
                                          "Importe uma playlist M3U com --playlist CAMINHO";
        }
    }
}
} // namespace raspberry_tv
