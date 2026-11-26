#include "tv/spotify.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <thread>
#include <unistd.h>
using namespace raspberry_tv;
using namespace std::chrono_literals;
void require(bool value,const char* message){if(!value){std::cerr<<message<<'\n';std::exit(1);}}
int main() {
    namespace fs=std::filesystem;
    const auto directory=fs::temp_directory_path()/("rtv-spotify-process-"+std::to_string(getpid()));
    require(fs::create_directory(directory),"isolated process fixture directory");
    struct Cleanup {fs::path path;~Cleanup(){std::error_code ec;fs::remove_all(path,ec);}} cleanup{directory};
    const auto script=directory/"fixture.py";
    {
        std::ofstream file(script);
        file<<"import pathlib, sys, time, urllib.parse\n"
               "if sys.argv[1] == 'wait':\n"
               "    pathlib.Path(sys.argv[2]).write_text('ready')\n"
               "    time.sleep(30)\n"
               "elif sys.argv[1] == 'overflow':\n"
               "    print('x' * 300000)\n"
               "else:\n"
               "    print('RTVSPOTIFY\\t1')\n"
               "    print('status\\tready\\t' + urllib.parse.quote(sys.argv[2], safe=''))\n";
    }
    auto runner=makeSpotifyRunner(script.string());
    auto cancelled=std::make_shared<std::atomic<bool>>(false);
    const auto literal="argument with spaces; $(never-a-shell)";
    for(int i=0;i<8;++i) {
        const auto reply=parseSpotifyReply(runner({"ready",literal},cancelled));
        require(reply.code=="ready" && reply.message==literal,"argv stays literal; repeated child lifecycle");
    }
    require(parseSpotifyReply(runner({"overflow"},cancelled)).code!="ready","output bounded");
    require(parseSpotifyReply(makeSpotifyRunner((directory/"missing.py").string())({"ready"},cancelled)).code=="error","missing helper safe error");
    const auto marker=directory/"started";
    auto work=std::async(std::launch::async,[&]{return runner({"wait",marker.string()},cancelled);});
    const auto deadline=std::chrono::steady_clock::now()+3s;
    while(!fs::exists(marker) && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(5ms);
    require(fs::exists(marker),"child started");
    cancelled->store(true);
    require(work.wait_for(2s)==std::future_status::ready,"native cancellation kills and reaps child promptly");
    require(parseSpotifyReply(work.get()).code=="network","cancel response safe");
    std::cout<<"Native Spotify process: argv, repeated lifecycle, output limit, failure and cancellation passed\n";
}
