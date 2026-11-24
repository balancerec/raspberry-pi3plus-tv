#pragma once
#include "tv/settings.hpp"
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
namespace raspberry_tv {
// SD-card fsync never runs in the SDL loop; final destructor flushes the last state.
class StateWriter {
public:
    explicit StateWriter(std::filesystem::path path):path_(std::move(path)),worker_(&StateWriter::run,this) {}
    ~StateWriter();
    void submit(UserState state);
    std::string takeError();
private:
    void run();
    std::filesystem::path path_;
    std::mutex mutex_;std::condition_variable wake_;
    std::optional<UserState> pending_;std::string error_;bool stopping_=false;
    std::thread worker_;
};
}
