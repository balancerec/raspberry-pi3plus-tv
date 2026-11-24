#include "tv/input.hpp"
#include "tv/menu.hpp"
#include "tv/ui.hpp"
#include "tv/display.hpp"
#include "tv/player.hpp"
#include "tv/playlist_loader.hpp"
#include "tv/state_writer.hpp"
#include "tv/channel_queue.hpp"
#include <filesystem>
#include <vector>
#include <SDL.h>

#include <cstdio>
#include <csignal>
#include <cstring>
#include <ctime>
#include <string>

using raspberry_tv::Action;
using raspberry_tv::Axis;
using raspberry_tv::Button;
using raspberry_tv::ControllerInput;

namespace {
// SIGINT handler must not touch SDL, GStreamer or allocation APIs.
volatile std::sig_atomic_t interrupted = 0;
void onInterrupt(int) { interrupted = 1; }
SDL_GameController* controller = nullptr;
SDL_JoystickID instanceId = -1;

bool openController(int deviceIndex) {
    if (controller || !SDL_IsGameController(deviceIndex)) return false;
    controller = SDL_GameControllerOpen(deviceIndex);
    if (!controller) {
        std::fprintf(stderr, "SDL controller: %s\n", SDL_GetError());
        return false;
    }
    instanceId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
    std::printf("Controle principal: %s (%d)\n", SDL_GameControllerName(controller), instanceId);
    return true;
}
void openFirstController() {
    for (int i = 0; !controller && i < SDL_NumJoysticks(); ++i) openController(i);
}
void closeController(ControllerInput& input) {
    if (controller) SDL_GameControllerClose(controller);
    controller = nullptr;
    instanceId = -1;
    input.disconnect();
}

bool buttonFromSDL(Uint8 id, Button& result) {
    switch (id) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP: result = Button::DpadUp; return true;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: result = Button::DpadDown; return true;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: result = Button::DpadLeft; return true;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: result = Button::DpadRight; return true;
        // SDL's A/B/X/Y are Cross/Circle/Square/Triangle on the DualShock 4.
        case SDL_CONTROLLER_BUTTON_A: result = Button::Cross; return true;
        case SDL_CONTROLLER_BUTTON_B: result = Button::Circle; return true;
        case SDL_CONTROLLER_BUTTON_X: result = Button::Square; return true;
        case SDL_CONTROLLER_BUTTON_Y: result = Button::Triangle; return true;
        case SDL_CONTROLLER_BUTTON_GUIDE: result = Button::PS; return true;
        case SDL_CONTROLLER_BUTTON_START: result = Button::Options; return true;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: result = Button::L1; return true;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: result = Button::R1; return true;
        default: return false;
    }
}
bool axisFromSDL(Uint8 id, Axis& result) {
    switch (id) {
        case SDL_CONTROLLER_AXIS_LEFTX: result = Axis::LeftX; return true;
        case SDL_CONTROLLER_AXIS_LEFTY: result = Axis::LeftY; return true;
        case SDL_CONTROLLER_AXIS_TRIGGERLEFT: result = Axis::TriggerLeft; return true;
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: result = Axis::TriggerRight; return true;
        default: return false;
    }
}
bool keyFromSDL(SDL_Keycode id, Button& result) {
    switch (id) {
        case SDLK_UP: result = Button::DpadUp; return true;
        case SDLK_DOWN: result = Button::DpadDown; return true;
        case SDLK_LEFT: result = Button::DpadLeft; return true;
        case SDLK_RIGHT: result = Button::DpadRight; return true;
        case SDLK_RETURN: result = Button::Cross; return true;
        case SDLK_ESCAPE: result = Button::Circle; return true;
        case SDLK_HOME: result = Button::PS; return true;
        case SDLK_F1: result = Button::Options; return true;
        case SDLK_PAGEUP: result = Button::L1; return true;
        case SDLK_PAGEDOWN: result = Button::R1; return true;
        case SDLK_LEFTBRACKET: result = Button::L2; return true;
        case SDLK_RIGHTBRACKET: result = Button::R2; return true;
        case SDLK_f: result = Button::Triangle; return true;
        case SDLK_m: result = Button::Square; return true;
        default: return false;
    }
}
} // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT,onInterrupt);std::signal(SIGTERM,onInterrupt);
    std::string playlist,checkPlaylist,filter,error;
    auto stateDirectory=raspberry_tv::defaultStateDirectory();
    // Resolve the optional private state directory before reading preferences.
    for(int i=1;i<argc;++i)if(std::strcmp(argv[i],"--state-dir")==0 && i+1<argc)stateDirectory=std::filesystem::u8path(argv[++i]);
    raspberry_tv::UserState state;
    raspberry_tv::loadUserState(stateDirectory,state,error);
    auto& settings=state.settings;
    for(int i=1;i<argc;++i) {
        const std::string arg=argv[i];
        if(arg=="--fullscreen")settings.fullscreen=true;
        else if(arg=="--windowed")settings.fullscreen=false;
        else if(arg=="--no-osd")settings.showOsd=false;
        else if((arg=="--playlist" || arg=="--check-playlist" || arg=="--filter" || arg=="--state-dir" || arg=="--audio-sink" || arg=="--audio-device") && i+1<argc) {
            const std::string value=argv[++i];
            if(arg=="--playlist")playlist=value;
            if(arg=="--check-playlist")checkPlaylist=value;
            if(arg=="--filter")filter=value;
            if(arg=="--audio-sink")settings.audioSink=value;
            if(arg=="--audio-device")settings.audioDevice=value;
        } else if(arg=="--version") {std::puts("Raspberry TV Native 1.0.0-rc1");return 0;}
        else if(arg=="--help") {
            std::puts("Uso: raspberry-tv-native [--fullscreen|--windowed] [--playlist ARQUIVO] [--filter TEXTO]");
            std::puts("     [--state-dir DIRETORIO] [--no-osd] [--audio-sink autoaudiosink|alsasink|pulsesink|pipewiresink] [--audio-device ID]");
            std::puts("     raspberry-tv-native --check-playlist ARQUIVO | --version");return 0;
        } else {std::fprintf(stderr,"Opcao desconhecida ou argumento ausente; use --help\n");return 2;}
    }
    if(!checkPlaylist.empty()) {
        const auto result=raspberry_tv::readPlaylistFile(checkPlaylist);
        if(!result){std::fprintf(stderr,"%s (caminho omitido)\n",result.error.c_str());return 2;}
        std::printf("Playlist: %zu canais, %zu invalidos, %zu duplicados; truncada=%s\n",result.channels.size(),result.invalid,result.duplicates,result.truncated?"sim":"nao");return 0;
    }
    if(!raspberry_tv::validAudioSink(settings.audioSink)){std::fputs("Saida de audio invalida\n",stderr);return 2;}
    if(playlist.empty())playlist=settings.playlist;
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER)!=0){std::fprintf(stderr,"SDL_Init: %s\n",SDL_GetError());return 1;}
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    const Uint32 flags=SDL_WINDOW_SHOWN|(settings.fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:SDL_WINDOW_RESIZABLE);
    SDL_Window* window=SDL_CreateWindow("Raspberry TV - Native 1.0 RC1",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        raspberry_tv::display::NativeWidth,raspberry_tv::display::NativeHeight,flags);
    if(!window){SDL_Quit();return 1;}
    SDL_Renderer* renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!renderer)renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    if(!renderer || SDL_RenderSetLogicalSize(renderer,1920,1080)!=0){if(renderer)SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 1;}
    int outputWidth=0,outputHeight=0;SDL_GetRendererOutputSize(renderer,&outputWidth,&outputHeight);
    std::printf("Renderizacao interna: 1920 x 1080; saida atual: %d x %d\n",outputWidth,outputHeight);
    raspberry_tv::TvUi ui;
    if(!ui.init()){std::fputs("Falha ao abrir fontes SDL_ttf\n",stderr);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 1;}
    const bool osdAvailable=ui.initOsd(window);
    raspberry_tv::TvModel model;model.restoreState(state);model.setFilter(filter);
    if(!error.empty())model.setStatusMessage(error);
    raspberry_tv::StateWriter writer(stateDirectory);
    raspberry_tv::PlaylistLoader loader;
    if(!playlist.empty()){loader.start(playlist);model.setStatusMessage("Carregando playlist local...");}
    raspberry_tv::TvPlayer player;
    if(!player.initialize(window,settings))model.setStatusMessage("Player requer X11 e plugins GStreamer; menus disponiveis");
    if(!osdAvailable)std::fputs("OSD indisponivel; informacoes retornam ao menu apos O\n",stderr);
    ControllerInput input,keyboard;openFirstController();
    bool running=true,dirty=true,savePending=false,playbackHelp=false,loadPending=!playlist.empty();
    raspberry_tv::ChannelQueue queue;
    std::uint32_t lastAction=SDL_GetTicks(),osdUntil=0;
    auto previousSnapshot=player.snapshot();
    auto shownMinute=std::time(nullptr)/60;
    auto reload=[&] {
        if(player.snapshot().active()){model.setStatusMessage("Pare a reproducao antes de recarregar");return;}
        const auto path=model.playlistPath().empty()?playlist:model.playlistPath();
        if(path.empty())model.setStatusMessage("Inicie com --playlist ARQUIVO local");
        else if(loader.start(path)){loadPending=true;playlist=path;model.setStatusMessage("Recarregando playlist...");}
    };
    auto beginPlayback=[&] {
        queue.start(model);playbackHelp=false;
        if(const auto* channel=queue.current(model);channel && !player.play(channel->url))model.setStatusMessage(player.snapshot().status);
    };
    while(running && !interrupted) {
        SDL_Event event;const auto tick=SDL_GetTicks();
        if(SDL_WaitEventTimeout(&event,20))do {
            if(event.type==SDL_QUIT)running=false;
            else if(event.type==SDL_CONTROLLERDEVICEADDED) {if(openController(event.cdevice.which))dirty=true;}
            else if(event.type==SDL_CONTROLLERDEVICEREMOVED && event.cdevice.which==instanceId) {
                closeController(input);openFirstController();model.setStatusMessage(controller?"Controle reconectado":"Controle desconectado; teclado disponivel");dirty=true;
            } else if((event.type==SDL_CONTROLLERBUTTONDOWN || event.type==SDL_CONTROLLERBUTTONUP) && event.cbutton.which==instanceId) {
                Button b;if(buttonFromSDL(event.cbutton.button,b))input.onButton(b,event.type==SDL_CONTROLLERBUTTONDOWN,tick);
            } else if(event.type==SDL_CONTROLLERAXISMOTION && event.caxis.which==instanceId) {
                Axis a;if(axisFromSDL(event.caxis.axis,a))input.onAxis(a,event.caxis.value,tick);
            } else if(event.type==SDL_KEYDOWN || event.type==SDL_KEYUP) {
                if(event.type==SDL_KEYDOWN && event.key.repeat)continue;
                if(event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_F5){reload();dirty=true;continue;}
                if(event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_q && (event.key.keysym.mod&KMOD_CTRL)){running=false;continue;}
                Button b;if(keyFromSDL(event.key.keysym.sym,b))keyboard.onButton(b,event.type==SDL_KEYDOWN,tick);
            } else if(event.type==SDL_WINDOWEVENT) {
                if(event.window.event==SDL_WINDOWEVENT_FOCUS_LOST)keyboard.disconnect();
                if(event.window.event==SDL_WINDOWEVENT_EXPOSED || event.window.event==SDL_WINDOWEVENT_SIZE_CHANGED)dirty=true;
            }
        }while(SDL_PollEvent(&event));
        auto actions=input.poll(SDL_GetTicks());auto keys=keyboard.poll(SDL_GetTicks());actions.insert(actions.end(),keys.begin(),keys.end());
        for(const auto action:actions) {
            lastAction=SDL_GetTicks();osdUntil=lastAction+5000;savePending=true;dirty=true;
            const auto snap=player.snapshot();
            if(snap.active()) {
                if(action==Action::Back){player.stop();playbackHelp=false;}
                else if(action==Action::Home){player.stop();model.dispatch(Action::Home);playbackHelp=false;}
                else if(action==Action::Confirm)player.togglePause();
                else if(action==Action::Menu) {
                    if(osdAvailable && settings.showOsd)playbackHelp=!playbackHelp;
                    else {player.stop();model.dispatch(Action::Menu);}
                }
                else if(action==Action::Up)player.changeVolume(0.05);
                else if(action==Action::Down)player.changeVolume(-0.05);
                else if(action==Action::Details)player.toggleMute();
                else if(action==Action::Favorites) {if(const auto* channel=queue.current(model))model.toggleFavoriteIdentity(raspberry_tv::channelIdentity(*channel));}
                else if(action==Action::NextChannel || action==Action::PreviousChannel) {
                    if(queue.step(action==Action::NextChannel?1:-1,model)) {
                        const auto* channel=queue.current(model);
                        model.selectIdentity(raspberry_tv::channelIdentity(*channel));player.play(channel->url);
                    }
                }
                continue;
            }
            if(action==Action::Confirm && model.view()==raspberry_tv::View::Settings && !model.showHelp()) {
                switch(model.selectedRow()) {
                    case 0:
                        if(SDL_SetWindowFullscreen(window,settings.fullscreen?0:SDL_WINDOW_FULLSCREEN_DESKTOP)==0)settings.fullscreen=!settings.fullscreen;
                        else model.setStatusMessage("Nao foi possivel alterar a janela");
                        break;
                    case 1:settings.showOsd=!settings.showOsd;break;
                    case 2:settings.resumeSelection=!settings.resumeSelection;break;
                    case 3:reload();break;
                    case 4:running=false;break;
                }continue;
            }
            if(action==Action::Confirm && model.selectedChannel() && !model.showHelp() && !model.showDetails() && !loadPending)beginPlayback();
            else model.dispatch(action);
        }
        if(auto result=loader.take()) {loadPending=false;model.applyPlaylist(std::move(*result),playlist);dirty=true;savePending=true;lastAction=SDL_GetTicks();}
        const auto snap=player.snapshot();
        if(snap.state!=previousSnapshot.state || snap.status!=previousSnapshot.status || snap.buffering!=previousSnapshot.buffering || snap.volume!=previousSnapshot.volume || snap.muted!=previousSnapshot.muted || snap.audioStreams!=previousSnapshot.audioStreams) {
            dirty=true;osdUntil=SDL_GetTicks()+5000;
            if(!snap.active())model.setStatusMessage(snap.status);
            previousSnapshot=snap;
        }
        if(savePending && static_cast<std::uint32_t>(SDL_GetTicks()-lastAction)>=500) {
            model.captureState(state);settings.volume=snap.volume;settings.muted=snap.muted;writer.submit(state);savePending=false;
        }
        if(auto saveError=writer.takeError();!saveError.empty()){model.setStatusMessage(saveError);dirty=true;}
        const auto minute=std::time(nullptr)/60;if(minute!=shownMinute){shownMinute=minute;dirty=true;}
        const bool showOsd=snap.active() && settings.showOsd && (playbackHelp || snap.state!=raspberry_tv::PlaybackState::Playing || static_cast<std::int32_t>(osdUntil-SDL_GetTicks())>0);
        if(dirty) {
            ui.setSettings(settings);
            if(!snap.active())ui.draw(renderer,model,controller!=nullptr);
            ui.drawOsd(window,queue.current(model),snap,playbackHelp,showOsd);
            dirty=false;
        } else if(!showOsd)ui.drawOsd(window,nullptr,snap,false,false);
    }
    // Join the GStreamer owner before destroying any X11 drawable.
    const auto snap=player.snapshot();model.captureState(state);settings.volume=snap.volume;settings.muted=snap.muted;writer.submit(state);
    player.shutdown();closeController(input);ui.close();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
