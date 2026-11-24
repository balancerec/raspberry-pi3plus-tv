#pragma once
#include "tv/playlist.hpp"
#include <mutex>
#include <optional>
#include <thread>
namespace raspberry_tv {
// One bounded local-file operation; never joins an active load from the SDL loop.
class PlaylistLoader {
public:
    ~PlaylistLoader() {cancel_=true;if(worker_.joinable())worker_.join();}
    bool start(const std::string& path);
    std::optional<PlaylistResult> take();
    bool busy() const {return busy_;}
private:
    std::atomic<bool> cancel_{false},busy_{false};
    std::thread worker_; std::mutex mutex_; std::optional<PlaylistResult> result_;
};
}
