#include "tv/playback_state.hpp"
#include "test.hpp"
using namespace raspberry_tv;
int main() {
    PlaybackStateMachine m;check(!m.active(),"idle default");m.open(100);
    check(m.state()==PlaybackState::Opening && m.tick(20099)==PlaybackTick::None,"opening deadline");
    check(m.tick(20100)==PlaybackTick::Timeout,"opening timeout");
    m.fail(PlaybackFailure::Network,20100,true);check(m.state()==PlaybackState::RetryWait && m.attempts()==1,"first bounded retry");
    check(m.tick(21099)==PlaybackTick::None && m.tick(21100)==PlaybackTick::Retry,"one second backoff");
    m.open(21100,true);m.fail(PlaybackFailure::Network,21200,true);
    check(m.attempts()==2 && m.tick(24199)==PlaybackTick::None && m.tick(24200)==PlaybackTick::Retry,"three second backoff");
    m.open(24200,true);m.fail(PlaybackFailure::Network,24300,true);check(!m.active() && m.state()==PlaybackState::Failed,"exhausted attempts");
    m.open(30000);m.buffering(20,31000);m.buffering(80,60000);check(m.tick(61000)==PlaybackTick::Timeout,"progress cannot extend buffering forever");
    m.open(70000);m.playing(71000);m.pause(true,72000);check(m.state()==PlaybackState::Paused && m.tick(999999)==PlaybackTick::None,"user pause has no network deadline");
    m.pause(false,1000000);m.playing(1000010);check(m.state()==PlaybackState::Playing,"resume confirmed by playing");
    m.fail(PlaybackFailure::Network,1000020,true);m.stopping();m.stopped();check(!m.active() && m.tick(2000000)==PlaybackTick::None,"cancel cancels backoff");
    m.open(3000000);m.fail(PlaybackFailure::Codec,3000100,true);check(!m.active() && m.attempts()==0,"codec errors do not retry");
    m.open(4000000);m.fail(PlaybackFailure::Access,4000010,true);check(!m.active(),"403/404 category no retries");
    m.open(5000000);m.fail(PlaybackFailure::Network,5000010,false);check(!m.active(),"local files no retries");
    m.open(6000000);m.stopping();m.playing(6000010);check(m.state()==PlaybackState::Stopping,"late playing cannot undo stop");m.shutdown();check(!m.active(),"shutdown terminal");
}
