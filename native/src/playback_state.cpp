#include "tv/playback_state.hpp"
#include <algorithm>
namespace raspberry_tv {
const char* playbackStateName(PlaybackState s) {
    switch(s) {
        case PlaybackState::Idle:return "IDLE";case PlaybackState::Opening:return "OPENING";
        case PlaybackState::Buffering:return "BUFFERING";case PlaybackState::Playing:return "PLAYING";
        case PlaybackState::Paused:return "PAUSED";case PlaybackState::Stopping:return "STOPPING";
        case PlaybackState::RetryWait:return "RECONNECTING";case PlaybackState::Failed:return "FAILED";
        case PlaybackState::Shutdown:return "SHUTDOWN";
    }return "UNKNOWN";
}
const char* playbackFailureMessage(PlaybackFailure f) {
    switch(f) {
        case PlaybackFailure::Network:return "Rede/stream indisponivel ou tempo esgotado";
        case PlaybackFailure::Access:return "Acesso negado ou canal inexistente; recarregue a playlist";
        case PlaybackFailure::Codec:return "Codec ou plugin indisponivel";
        case PlaybackFailure::Audio:return "Falha na saida de audio; confira o dispositivo";
        case PlaybackFailure::Decode:return "Falha de decodificacao";
        case PlaybackFailure::Internal:return "Falha ao abrir o player";
    }return "Falha de reproducao";
}
void PlaybackStateMachine::open(Time now,bool retry) {
    if(!retry)attempts_=0;
    userPaused_=false;percent_=0;state_=PlaybackState::Opening;deadline_=now+20000;stableSince_=0;
}
void PlaybackStateMachine::playing(Time now) {
    if(state_==PlaybackState::Stopping || state_==PlaybackState::RetryWait || !active())return;
    if(state_!=PlaybackState::Playing)stableSince_=now;
    state_=userPaused_?PlaybackState::Paused:PlaybackState::Playing;
    percent_=100;deadline_=0;
}
void PlaybackStateMachine::buffering(int percent,Time now) {
    if(!active() || state_==PlaybackState::Stopping || state_==PlaybackState::RetryWait)return;
    percent_=std::clamp(percent,0,100);
    if(userPaused_)return;
    if(percent_<100) {
        if(state_!=PlaybackState::Buffering)deadline_=now+30000; // progress cannot extend indefinitely
        state_=PlaybackState::Buffering;
    } // PLAYING must be confirmed by the pipeline, not inferred from 100%.
}
void PlaybackStateMachine::pause(bool paused,Time now) {
    if(state_!=PlaybackState::Paused && state_!=PlaybackState::Playing && state_!=PlaybackState::Buffering)return;
    userPaused_=paused;
    if(paused){state_=PlaybackState::Paused;deadline_=0;}
    else {state_=PlaybackState::Buffering;deadline_=now+30000;}
}
void PlaybackStateMachine::fail(PlaybackFailure f,Time now,bool reconnectable) {
    if(reconnectable && f==PlaybackFailure::Network && attempts_<2) {
        deadline_=now+(attempts_==0?1000:3000);++attempts_;state_=PlaybackState::RetryWait;
    } else {state_=PlaybackState::Failed;deadline_=0;}
}
PlaybackTick PlaybackStateMachine::tick(Time now) {
    if(state_==PlaybackState::Playing && stableSince_ && now-stableSince_>=30000)attempts_=0;
    if(!deadline_ || now<deadline_)return PlaybackTick::None;
    if(state_==PlaybackState::RetryWait) {deadline_=0;return PlaybackTick::Retry;}
    if(state_==PlaybackState::Opening || state_==PlaybackState::Buffering) {deadline_=0;return PlaybackTick::Timeout;}
    return PlaybackTick::None;
}
} // namespace raspberry_tv
