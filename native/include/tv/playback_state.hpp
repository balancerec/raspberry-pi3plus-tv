#pragma once
#include <cstdint>
#include <string>
namespace raspberry_tv {
enum class PlaybackState {Idle,Opening,Buffering,Playing,Paused,Stopping,RetryWait,Failed,Shutdown};
enum class PlaybackFailure {Network,Access,Codec,Audio,Decode,Internal};
enum class PlaybackTick {None,Timeout,Retry};
const char* playbackStateName(PlaybackState);
const char* playbackFailureMessage(PlaybackFailure);
class PlaybackStateMachine {
public:
    using Time=std::uint64_t; // monotonic milliseconds, injectable in tests
    void open(Time now,bool retry=false);
    void playing(Time now);
    void buffering(int percent,Time now);
    void pause(bool paused,Time now);
    void fail(PlaybackFailure failure,Time now,bool reconnectable);
    void stopping() {state_=PlaybackState::Stopping;}
    void stopped() {state_=PlaybackState::Idle;attempts_=0;}
    void shutdown() {state_=PlaybackState::Shutdown;}
    PlaybackTick tick(Time now);
    PlaybackState state() const {return state_;}
    unsigned attempts() const {return attempts_;}
    int percent() const {return percent_;}
    bool active() const {return state_!=PlaybackState::Idle && state_!=PlaybackState::Failed && state_!=PlaybackState::Shutdown;}
private:
    PlaybackState state_=PlaybackState::Idle;
    Time deadline_=0,stableSince_=0;
    unsigned attempts_=0; int percent_=0;
    bool userPaused_=false;
};
struct PlayerSnapshot {
    PlaybackState state=PlaybackState::Idle;
    std::string status="Player pronto";
    int buffering=0, audioStreams=-1;
    double volume=0.65;bool muted=false;
    bool active() const {return state!=PlaybackState::Idle && state!=PlaybackState::Failed && state!=PlaybackState::Shutdown;}
};
} // namespace raspberry_tv
