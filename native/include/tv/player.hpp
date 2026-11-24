#pragma once
#include "tv/playback_state.hpp"
#include "tv/settings.hpp"
#include <gst/gst.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
struct SDL_Window;
namespace raspberry_tv {
// Only the worker owns GstObject references and calls GStreamer state APIs.
// SDL owns the X11 drawable until shutdown() joins that worker.
class TvPlayer {
public:
    TvPlayer()=default;
    ~TvPlayer();
    TvPlayer(const TvPlayer&)=delete;TvPlayer& operator=(const TvPlayer&)=delete;
    bool initialize(SDL_Window*,const UserSettings& settings=UserSettings{});
    bool initializeHeadless(); // real GStreamer integration tests, fakesinks only
    bool play(const std::string& uri);
    void stop();
    void shutdown();
    void togglePause();
    void changeVolume(double delta);
    void toggleMute();
    PlayerSnapshot snapshot() const;
private:
    // Xlib defines the macro None; do not use it as an enumerator here.
    enum class Command {NoRequest,Play,Stop,Shutdown};
    void run();
    bool createPipeline();
    void releasePipeline();
    bool startStream(bool retry);
    void handleFailure(PlaybackFailure);
    void publish(const std::string& status={});
    void inspectBus();
    void applyAudio();
    static std::uint64_t now();
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::thread worker_;
    Command pending_=Command::NoRequest;
    std::string pendingUri_;
    std::atomic<unsigned long> generation_{0};
    unsigned long currentGeneration_=0;
    bool ready_=false,exit_=false,headless_=false;
    bool desiredPause_=false,audioDirty_=false;
    PlayerSnapshot snapshot_;
    UserSettings settings_;
    guintptr windowHandle_=0;
    GstElement* playbin_=nullptr;
    GstBus* bus_=nullptr;
    PlaybackStateMachine machine_;
    PlaybackFailure pipelineFailure_=PlaybackFailure::Internal;
    std::string uri_;
    bool live_=false,paused_=false,buffering_=false;
    gint64 lastPosition_=-1;
    std::uint64_t lastProgress_=0,lastProbe_=0;
};
} // namespace raspberry_tv
