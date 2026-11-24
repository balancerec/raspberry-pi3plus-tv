#include "tv/playlist_loader.hpp"
namespace raspberry_tv {
bool PlaylistLoader::start(const std::string& path) {
    if(busy_.exchange(true))return false;
    if(worker_.joinable())worker_.join(); // previous operation already completed
    cancel_=false;
    worker_=std::thread([this,path] {
        PlaylistResult r;
        try {r=readPlaylistFile(path,&cancel_);} catch(...) {r.error="Falha ao carregar playlist";}
        {std::lock_guard<std::mutex> lock(mutex_);result_=std::move(r);}
        busy_=false;
    });
    return true;
}
std::optional<PlaylistResult> PlaylistLoader::take() {
    if(busy_)return {};
    std::lock_guard<std::mutex> lock(mutex_);auto r=std::move(result_);result_.reset();return r;
}
}
