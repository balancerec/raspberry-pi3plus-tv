#include "tv/spotify.hpp"
#include "tv/menu.hpp"
#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <thread>
using namespace raspberry_tv;
using namespace std::chrono_literals;
void check(bool value,const char* message){if(!value){std::cerr<<message<<'\n';std::exit(1);}}
template<class Predicate> void eventually(Predicate test,const char* message) {
    const auto deadline=std::chrono::steady_clock::now()+2s;
    while(!test()) {if(std::chrono::steady_clock::now()>deadline){check(false,message);}std::this_thread::yield();}
}
int main() {
    const auto reply=parseSpotifyReply("RTVSPOTIFY\t1\nstatus\tready\tPronto\npage\tplaylists\t0\t20\t40\t\nrow\tplaylist\tspotify%3Aplaylist%3AAAAAAAAAAAAAAAAAAAAAAA\tM%C3%BAsica\tBrasil\t1\n");
    check(reply.code=="ready" && reply.hasPage && reply.entries.size()==1,"valid wire page");
    check(reply.entries[0].title==u8"Música" && reply.nextOffset==20,"utf8 and pagination");
    for(const auto& wire:{std::string("raw failure"),std::string("RTVSPOTIFY\t1\nstatus\tready\t%0Asecret\n"),
        std::string("RTVSPOTIFY\t1\nstatus\tready\t%GG\n"),std::string("RTVSPOTIFY\t1\nstatus\tready\tOK\npage\tplaylists\t20\t10\t40\t\n"),
        std::string(262145,'x'),std::string("RTVSPOTIFY\t1\nstatus\tready\tOK\nunknown\tfield\n")})
        check(parseSpotifyReply(wire).code=="error","bad wire rejected");
    const auto playback=parseSpotifyReply("RTVSPOTIFY\t1\nstatus\tready\tOK\nplayback\tplaying\tFaixa\tArtista\tSala\t65\n");
    check(playback.hasPlayback && playback.volume==65,"playback protocol");
    check(parseSpotifyReply("RTVSPOTIFY\t1\nstatus\tready\tOK\nplayback\tplaying\tFaixa\tArtista\tSala\t101\n").code=="error","bad volume rejected");
    SpotifyPanel panel;check(panel.rows().size()==11 && panel.activate(0)[0]=="soloist-status","default local Soloist controls");
    panel.accept(parseSpotifyReply("RTVSPOTIFY\t1\nstatus\tready\tFila\npage\tqueue\t0\t-1\t1\t\nrow\ttrack\tspotify%3Atrack%3AAAAAAAAAAAAAAAAAAAAAAA\tLocal\tArtista\t1\n"));
    check(panel.activate(11)[0]=="soloist-play","queue plays through local backend");
    panel.useLocal(false);check(panel.rows().size()==11,"backend switch clears old metadata");panel.accept(reply);
    check(panel.rows().size()==13,"page plus next link");
    check(panel.activate(11).size()==2 && panel.activate(11)[0]=="play","playlist starts through explicit action");
    check(panel.activate(11,true)[0]=="items","square opens playlist items");
    check(panel.activate(12).back()=="20","next page offset");
    panel.accept(playback);check(panel.rows().size()==13,"status does not erase browse page");
    check(panel.nowPlaying().find("Sala")!=std::string::npos,"device shown");
    panel.accept(parseSpotifyReply("RTVSPOTIFY\t1\nstatus\tauth\tLogin\n"));
    check(panel.rows().size()==11,"logout clears account metadata");

    SpotifyPanel controls;controls.accept(playback);
    check(controls.rows()[4].title=="Pausar Spotify local" && controls.activate(4)[0]=="soloist-toggle","same action toggles local playback");
    controls.requestStarted("soloist-toggle");
    check(controls.nowPlaying().find("Atualizando")!=std::string::npos,"pending control does not show old playing state");
    controls.accept(parseSpotifyReply("RTVSPOTIFY\t1\nstatus\tready\tEstado\nplayback\tpaused\tFaixa\tArtista\tSala\t65\n"));
    check(controls.nowPlaying().find("Pausado:")==0 && controls.rows()[4].title=="Retomar Spotify local","confirmed pause changes header and same row");
    controls.requestStarted("soloist-toggle");
    controls.accept(parseSpotifyReply("RTVSPOTIFY\t1\nstatus\tnetwork\tSem%20confirmacao\n"));
    check(controls.nowPlaying().find("sem confirmacao")!=std::string::npos,"failed readback invalidates old state");
    controls.accept(playback);controls.requestStarted("soloist-resume");controls.requestCancelled();
    check(controls.nowPlaying().find("sem confirmacao")!=std::string::npos,"cancel does not restore stale playing state");

    TvModel model;model.dispatch(Action::Down);model.dispatch(Action::Confirm);
    check(model.view()==View::Apps && model.visibleCount()==11,"Apps reachable by DS4 actions");
    model.setAppRowCount(12);model.dispatch(Action::Right);check(model.selectedRow()==6,"Apps pagination");
    model.selectAppRow(10);model.dispatch(Action::Down);check(model.selectedRow()==11,"Apps list navigation");
    model.setAppRowCount(10);check(model.selectedRow()==0,"row clamp after logout");
    model.dispatch(Action::Menu);model.dispatch(Action::Down);check(model.selectedRow()==0,"help traps input");
    model.dispatch(Action::Back);model.dispatch(Action::Back);check(model.view()==View::Home,"back returns home");

    std::promise<void> entered;auto started=entered.get_future();
    SpotifyWorker worker([&](const SpotifyArguments& arguments,const SpotifyCancel& cancel) {
        if(arguments[0]=="slow") {
            entered.set_value();while(!cancel->load())std::this_thread::yield();
            return std::string("RTVSPOTIFY\t1\nstatus\tready\tStale\n");
        }
        if(arguments[0]=="throw")throw std::runtime_error("private detail must not escape");
        return std::string("RTVSPOTIFY\t1\nstatus\tready\tFresh\n");
    });
    check(worker.submit({"slow"}),"submit worker");check(started.wait_for(2s)==std::future_status::ready,"worker ran");
    check(!worker.submit({"duplicate"}),"no repeated commands queued");
    worker.cancel();eventually([&]{return !worker.busy();},"cancel completed");
    check(!worker.take(),"cancelled result discarded");
    check(worker.submit({"fresh"}),"can issue after cancel");eventually([&]{return !worker.busy();},"fresh completed");
    auto fresh=worker.take();check(fresh && fresh->message=="Fresh","fresh result only");
    check(worker.submit({"throw"}),"exception request accepted");eventually([&]{return !worker.busy();},"exception completed");
    check(worker.take()->message=="Falha ao executar o cliente Spotify.","exception details redacted");
    worker.shutdown();worker.shutdown();check(!worker.submit({"after"}),"shutdown final and idempotent");
    {
        std::promise<void> ready;auto future=ready.get_future();
        SpotifyWorker ending([&](const SpotifyArguments&,const SpotifyCancel& cancel){ready.set_value();while(!cancel->load())std::this_thread::yield();return std::string{};});
        ending.submit({"wait"});check(future.wait_for(2s)==std::future_status::ready,"ending request started");
        // Destructor must cancel and join a running request.
    }
    std::cout<<"Spotify protocol, controller navigation, serialization, cancellation and shutdown passed\n";
}
