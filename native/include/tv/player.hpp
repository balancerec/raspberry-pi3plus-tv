#pragma once

#include <gst/gst.h>
#include <string>
struct SDL_Window;

namespace raspberry_tv {

// GStreamer is isolated from TvModel and ControllerInput.
// X11/SDL window integration is deliberately experimental; the existing shell
// remains available if a channel cannot be started.
class TvPlayer {
public:
    TvPlayer() = default;
    ~TvPlayer();

    TvPlayer(const TvPlayer&) = delete;
    TvPlayer& operator=(const TvPlayer&) = delete;

    bool initialize(SDL_Window* window);
    bool play(const std::string& uri);
    void stop();
    void shutdown(); // Idempotent, release GStreamer before SDL/X11 window
    void togglePause();
    bool poll();
    bool active() const { return active_; }
    bool paused() const { return paused_; }
    const std::string& status() const { return status_; }

private:
    bool createPipeline();

    guintptr windowHandle_ = 0;
    GstElement* playbin_ = nullptr;
    GstBus* bus_ = nullptr;
    bool ready_ = false;
    bool active_ = false;
    bool paused_ = false;
    bool desiredPlaying_ = true;
    bool live_ = false;
    std::string status_ = "Player desativado";
};

} // namespace raspberry_tv
