#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace raspberry_tv {
struct SpotifyEntry {
    std::string kind, identity, title, detail;
    bool enabled = false;
};
struct SpotifyReply {
    std::string code = "error", message = "Resposta Spotify invalida.";
    std::string profile, playback, track, artist, device;
    int volume = -1;
    bool hasPlayback = false, hasPage = false;
    std::string page, parent;
    int offset = 0, nextOffset = -1, total = 0;
    std::vector<SpotifyEntry> entries;
};
SpotifyReply parseSpotifyReply(const std::string& wire);

using SpotifyArguments = std::vector<std::string>;
using SpotifyCancel = std::shared_ptr<std::atomic<bool>>;
using SpotifyRunner = std::function<std::string(const SpotifyArguments&, const SpotifyCancel&)>;

// One in-flight request; no queue of repeated controller commands. Cancel makes
// stale results unobservable; the process adapter terminates its child promptly.
class SpotifyWorker {
public:
    explicit SpotifyWorker(SpotifyRunner);
    ~SpotifyWorker();
    bool submit(SpotifyArguments);
    void cancel();
    bool busy() const;
    std::optional<SpotifyReply> take();
    void shutdown();
private:
    SpotifyRunner runner_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::thread thread_;
    bool exit_ = false, busy_ = false;
    std::optional<SpotifyArguments> pending_;
    std::optional<SpotifyReply> result_;
    SpotifyCancel cancel_;
    void run();
};

struct SpotifyMenuRow {
    std::string title, detail;
    SpotifyArguments arguments;
    bool enabled = true;
};
class SpotifyPanel {
public:
    SpotifyPanel();
    void useLocal(bool local);
    void requestStarted(const std::string& command);
    void requestCancelled();
    static constexpr std::size_t ControlRows = 11;
    void accept(const SpotifyReply&);
    const std::vector<SpotifyMenuRow>& rows() const { return rows_; }
    const std::string& message() const { return message_; }
    const std::string& nowPlaying() const { return nowPlaying_; }
    SpotifyArguments activate(std::size_t row, bool details = false) const;
private:
    std::vector<SpotifyMenuRow> rows_;
    std::optional<SpotifyReply> page_;
    bool local_ = true;
    bool confirming_ = false;
    std::string playback_ = "unknown";
    std::string message_ = "Soloist: configure e inicie pelo terminal; consulte docs/SPOTIFY.md.", nowPlaying_ = "Spotify Soloist - dispositivo Raspberry TV";
    void rebuild();
};

// Implemented by the native Linux adapter. No shell, token argument or daemon.
SpotifyRunner makeSpotifyRunner(const std::string& helper);
} // namespace raspberry_tv
