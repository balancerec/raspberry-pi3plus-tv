#include "tv/playlist_loader.hpp"
#include "test.hpp"
#include <chrono>
#include <fstream>
#include <filesystem>
using namespace raspberry_tv;
int main() {
    const auto path=std::filesystem::temp_directory_path()/("tv-loader-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".m3u");
    {std::ofstream f(path);f<<"#EXTM3U\n";for(int i=0;i<20000;++i)f<<"#EXTINF:-1,Channel "<<i<<"\nhttps://example.invalid/"<<i<<"\n";}
    PlaylistLoader loader;check(loader.start(path.u8string()),"background load starts");
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(loader.busy() && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(5));
    auto r=loader.take();check(r && bool(*r) && r->channels.size()==20000,"large background load completes");
    check(!loader.take(),"result consumed once");
    check(loader.start("missing-file.m3u"),"reload after previous load");
    while(loader.busy())std::this_thread::sleep_for(std::chrono::milliseconds(1));
    r=loader.take();check(r && !bool(*r),"background errors returned");
    std::filesystem::remove(path);
    {std::ofstream sparse(path,std::ios::binary);sparse.seekp(64*1024*1024);sparse.put('x');}
    check(!readPlaylistFile(path.u8string()),"oversized file rejected before read");std::filesystem::remove(path);
}
