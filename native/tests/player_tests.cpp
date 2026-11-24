#include "tv/player.hpp"
#include "test.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
using namespace raspberry_tv;
template<class Predicate> bool waitFor(Predicate pred) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(std::chrono::steady_clock::now()<deadline){if(pred())return true;std::this_thread::sleep_for(std::chrono::milliseconds(10));}return false;
}
int main() {
    TvPlayer p;check(!p.play("file:///nonexistent"),"uninitialized player rejects open");
    check(p.initializeHeadless(),"headless real GStreamer worker");
    check(!p.play("javascript://invalid"),"invalid URI rejected");
    const auto file=std::filesystem::temp_directory_path()/"raspberry-tv-player-fixture.wav";
    {std::ofstream out(file,std::ios::binary);
    auto word=[&](unsigned value,int bytes){while(bytes--){out.put(static_cast<char>(value&255));value>>=8;}};
    out.write("RIFF",4);word(36+48000*2*6,4);out.write("WAVEfmt ",8);word(16,4);word(1,2);word(1,2);
    word(48000,4);word(96000,4);word(2,2);word(16,2);out.write("data",4);word(48000*2*6,4);
    for(int i=0;i<48000*2*6;++i)out.put(0);}
    gchar* fixtureUri=gst_filename_to_uri(file.c_str(),nullptr);check(fixtureUri!=nullptr,"fixture URI");
    check(p.play(fixtureUri),"actual local WAV open");g_free(fixtureUri);
    check(waitFor([&]{return p.snapshot().state==PlaybackState::Playing;}),"actual pipeline confirms PLAYING");
    p.togglePause();check(waitFor([&]{return p.snapshot().state==PlaybackState::Paused;}),"actual pause");
    p.togglePause();check(waitFor([&]{return p.snapshot().state==PlaybackState::Playing;}),"actual resume");
    p.stop();check(waitFor([&]{return p.snapshot().state==PlaybackState::Idle;}),"actual playing-to-idle teardown");
    std::filesystem::remove(file);
    for(int i=0;i<8;++i) {
        check(p.play("file:///nonexistent-raspberry-tv-test.wav"),"async missing-file request accepted");
        check(waitFor([&]{return p.snapshot().state==PlaybackState::Failed;}),"actual open failure returns control");
        check(p.play("http://127.0.0.1:1/unavailable"),"network failure request");
        p.stop();check(waitFor([&]{return p.snapshot().state==PlaybackState::Idle;}),"stop cancels pending open/retry");
    }
    // Last request wins, then stop: no obsolete request may resurrect playback.
    for(int i=0;i<100;++i)p.play("file:///obsolete-raspberry-tv.wav");
    p.stop();check(waitFor([&]{return p.snapshot().state==PlaybackState::Idle;}),"rapid zapping is coalesced and cancellable");
    p.changeVolume(-0.2);p.toggleMute();check(p.snapshot().volume>0.44 && p.snapshot().volume<0.46 && p.snapshot().muted,"audio state exposed (not proof of audible audio)");
    p.shutdown();p.shutdown();check(p.snapshot().state==PlaybackState::Shutdown,"idempotent shutdown");
}
