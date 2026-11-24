#include "tv/settings.hpp"
#include "tv/menu.hpp"
#include "tv/state_writer.hpp"
#include "test.hpp"
#include <chrono>
#include <fstream>
#include <sstream>
using namespace raspberry_tv;
int main() {
    auto dir=std::filesystem::temp_directory_path()/("tv-state-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::string error;UserState state,loaded;
    check(loadUserState(dir,state,error),"absent state uses safe defaults");
    check(state.settings.volume==0.65 && !state.settings.muted,"safe initial audio");
    Channel c{"Canal A","News","https://example.invalid/a?token=SECRET","A","a",""};
    state.favorites.insert(channelIdentity(c));state.settings.playlist="/tmp/Lista com espaços.m3u";
    state.settings.volume=0.25;state.settings.muted=true;state.settings.lastChannel=channelIdentity(c);state.settings.lastGroup="News";
    check(saveUserState(dir,state,error) && loadUserState(dir,loaded,error),"atomic state roundtrip");
    check(loaded.favorites==state.favorites && loaded.settings.volume==0.25 && loaded.settings.muted,"audio/favorites restored");
    check(loaded.settings.playlist==state.settings.playlist,"UTF-8/spaces in path roundtrip");
    std::ifstream saved(dir/"state.conf");std::string raw((std::istreambuf_iterator<char>(saved)),{});saved.close();
    check(raw.find("SECRET")==std::string::npos && raw.find("https://")==std::string::npos,"channel addresses never persisted");
    std::istringstream list("#EXTM3U\n#EXTINF:-1 tvg-id=\"a\" tvg-name=\"A\" group-title=\"News\",Canal A\nhttps://example.invalid/a?token=NEW\n#EXTINF:-1,B\nhttps://example.invalid/b\n");
    TvModel model;model.restoreState(loaded);check(model.applyPlaylist(readM3u(list),"test.m3u"),"apply after restoring state");
    model.dispatch(Action::Confirm);check(model.hasFavorite(0) && model.favoriteCount()==1,"favorite remapped across token change");
    model.dispatch(Action::Home);model.dispatch(Action::Favorites);check(model.selectedChannel() && model.selectedChannel()->tvgId=="a","favorite view restored");
    std::istringstream reorder("#EXTM3U\n#EXTINF:-1,B\nhttps://example.invalid/b\n#EXTINF:-1 tvg-id=\"a\" tvg-name=\"A\" group-title=\"News\",Canal A\nhttps://example.invalid/a?token=LATEST\n");
    check(model.applyPlaylist(readM3u(reorder),"test.m3u") && model.hasFavorite(1),"favorites survive reorder/reload");
    check(!model.loadPlaylist("missing-file.m3u") && model.favoriteCount()==1,"bad reload preserves favorites and catalog");
    model.captureState(loaded);check(loaded.settings.lastChannel==channelIdentity(c),"stable last-channel identity captured");
    {std::ofstream f(dir/"state.conf");f<<"raspberry-tv-state 1\nvolume \"nan\"\nmuted \"wrong\"\naudio-sink \"shell\"\n";}
    check(!loadUserState(dir,loaded,error) && loaded.settings.volume==0.65 && !loaded.settings.muted && loaded.settings.audioSink=="autoaudiosink","corruption falls back safely");
    check(saveUserState(dir,state,error),"corrupt file recovered atomically");
    for(int i=0;i<10;++i){state.settings.volume=i/10.0;check(saveUserState(dir,state,error),"repeated atomic replacement");}
    for(const auto& item:std::filesystem::directory_iterator(dir))check(item.path().filename()=="state.conf","temporary files cleaned");
    UserState invalid=state;invalid.settings.playlist="https://provider.invalid/?token=SECRET";
    check(!saveUserState(dir,invalid,error),"remote secret not saved as playlist path");
    check(loadUserState(dir,loaded,error) && loaded.settings.volume==0.9,"failed save retains last valid state");
    {StateWriter writer(dir);for(int i=0;i<20;++i){state.settings.volume=i/20.0;writer.submit(state);}}
    check(loadUserState(dir,loaded,error) && loaded.settings.volume==0.95,"async writer flushes last request at shutdown");
    std::filesystem::remove_all(dir);
}
