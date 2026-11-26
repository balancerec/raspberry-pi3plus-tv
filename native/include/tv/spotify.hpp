#pragma once
#include <atomic>
#include <array>
#include <cstdint>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include "tv/input.hpp"

namespace raspberry_tv {
struct SpotifyEntry {
    std::string kind, identity, title, detail;
    bool enabled = false;
};
struct SpotifyReply {
    std::string code = "error", message = "Resposta Spotify invalida.";
    std::string profile, playback, track, artist, device;
    int volume = -1;
    int position = -1, duration = -1, speed = 0;
    bool hasTimeline = false, hasFeatures = false, active = false, canSeek = false, canShuffle = false, canRepeat = false;
    int shuffle = -1;
    std::string repeat = "unknown";
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
struct SpotifyControlRect {int x,y,w,h;};
class SpotifyPanel {
public:
    SpotifyPanel();
    void useLocal(bool local);
    void requestStarted(const std::string& command);
    void requestCancelled();
    static constexpr std::size_t ControlRows = 11;
    void accept(const SpotifyReply&, std::uint32_t now = 0);
    const std::vector<SpotifyMenuRow>& rows() const { return rows_; }
    const std::string& message() const { return message_; }
    const std::string& nowPlaying() const { return nowPlaying_; }
    SpotifyArguments activate(std::size_t row, bool details = false) const;
    static constexpr std::array<SpotifyControlRect,12> MediaRects{{
        {290,384,60,60},{370,375,78,78},{475,360,108,108},{610,375,78,78},
        {710,384,60,60},{858,387,62,62},{944,387,62,62},{1030,387,62,62},
        {95,560,235,54},{380,560,235,54},{665,560,235,54},{950,560,235,54}}};
    bool mediaMode() const {return mediaMode_;}
    bool browsing() const {return browsing_;}
    bool local() const {return local_;}
    void showMedia() {mediaMode_=true;browsing_=false;}
    void showOptions() {mediaMode_=false;browsing_=false;}
    void navigateMedia(Action);
    std::size_t mediaFocus() const {return mediaFocus_;}
    bool selectMediaAt(int x,int y);
    SpotifyArguments activateMedia() const;
    SpotifyArguments mediaShortcut(Action) const;
    SpotifyArguments statusCommand() const;
    SpotifyArguments seekAt(int x) const;
    bool controlEnabled(std::size_t) const;
    const SpotifyReply& media() const {return media_;}
    int positionAt(std::uint32_t now) const;
    std::string controlLabel(std::size_t) const;
    std::string browserTitle() const;
private:
    std::vector<SpotifyMenuRow> rows_;
    std::optional<SpotifyReply> page_;
    bool local_ = true;
    bool confirming_ = false;
    bool mediaMode_ = true, browsing_ = false;
    std::size_t mediaFocus_ = 2;
    SpotifyReply media_;
    std::uint32_t mediaAt_ = 0;
    std::string playback_ = "unknown";
    std::string message_ = "Escolha Raspberry TV nos dispositivos do Spotify.", nowPlaying_ = "Spotify - Raspberry TV";
    void rebuild();
};

// Implemented by the native Linux adapter. No shell, token argument or daemon.
SpotifyRunner makeSpotifyRunner(const std::string& helper);
} // namespace raspberry_tv
