#include "tv/ui.hpp"
#include "tv/display.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>

using namespace raspberry_tv;
void require(bool condition,const char* message) {
    if(!condition){std::cerr<<message<<": "<<SDL_GetError()<<'\n';std::exit(1);}
}
int main(int argc,char** argv) {
    require(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0,"SDL software initialization");
    auto* surface=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_ARGB8888);
    require(surface!=nullptr,"Full HD target");
    auto* renderer=SDL_CreateSoftwareRenderer(surface);require(renderer!=nullptr,"software renderer");
    require(SDL_RenderSetLogicalSize(renderer,1920,1080)==0,"Full HD logical rendering");
    TvUi ui;require(ui.init(),"UI fonts");
    TvModel model;model.dispatch(Action::Down);model.dispatch(Action::Confirm);
    require(model.view()==View::Apps,"Apps navigation");
    SpotifyPanel spotify;ui.setSpotify(&spotify,false);
    const std::filesystem::path output=argc>1?argv[1]:"media-ui";
    std::filesystem::create_directories(output);
    auto frame=[&](const char* name) {
        ui.draw(renderer,model,true);
        require(surface->w==1920 && surface->h==1080,"render stays Full HD");
        require(SDL_SaveBMP(surface,(output/(std::string(name)+".bmp")).string().c_str())==0,"save real SDL render");
    };
    frame("spotify-idle");
    const auto wire="RTVSPOTIFY\t1\nstatus\tready\tEstado%20atualizado\nplayback\tplaying\tMusica%20de%20teste\tArtista%20de%20teste\tRaspberry%20TV\t65\nfeatures\t1\t1\t1\t1\t0\toff\ntimeline\t83000\t241000\t1000\n";
    spotify.accept(parseSpotifyReply(wire),SDL_GetTicks());frame("spotify-playing");
    auto paused=parseSpotifyReply(wire);paused.playback="paused";paused.volume=0;paused.speed=0;
    spotify.accept(paused,SDL_GetTicks());frame("spotify-paused-muted");
    spotify.accept(parseSpotifyReply("RTVSPOTIFY\t1\nstatus\tunavailable\tCliente%20indisponivel%3B%20tente%20atualizar.\n"),SDL_GetTicks());
    require(spotify.positionAt(SDL_GetTicks())<0,"failed readback has no fabricated timeline");frame("spotify-offline");
    spotify.accept(parseSpotifyReply("RTVSPOTIFY\t1\nstatus\tready\tFila%20local\npage\tqueue\t0\t-1\t1\t\nrow\ttrack\tspotify%3Atrack%3AAAAAAAAAAAAAAAAAAAAAAA\tProxima%20musica\tArtista%20de%20teste\t1\n"));
    model.setAppRowCount(spotify.rows().size());model.selectAppRow(SpotifyPanel::ControlRows);frame("spotify-queue");
    spotify.showOptions();model.selectAppRow(0);frame("spotify-options");
    ui.close();SDL_DestroyRenderer(renderer);SDL_FreeSurface(surface);SDL_Quit();
    std::cout<<"Full HD SDL media frames: idle, playing, paused/muted, offline, queue and options\n";
}
