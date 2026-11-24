#include "tv/input.hpp"
#include "tv/menu.hpp"
#include "tv/ui.hpp"
#include "tv/display.hpp"
#include "tv/player.hpp"
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
        default: return false;
    }
}
} // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, onInterrupt);
    std::signal(SIGTERM, onInterrupt);
    bool fullscreen = false;
    std::string playlist;
    for (int i=1;i<argc;++i) {
        if (std::strcmp(argv[i],"--fullscreen")==0) fullscreen=true;
        else if (std::strcmp(argv[i],"--playlist")==0 && i+1<argc) playlist=argv[++i];
        else if (std::strcmp(argv[i],"--help")==0) { std::puts("Uso: raspberry-tv-native [--fullscreen] [--playlist CAMINHO_M3U]"); return 0; }
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    const Uint32 flags = SDL_WINDOW_SHOWN | (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_RESIZABLE);
    SDL_Window* window = SDL_CreateWindow("Raspberry TV - Native IPTV v0.4", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, raspberry_tv::display::NativeWidth, raspberry_tv::display::NativeHeight, flags);
    if (!window) { std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); SDL_Quit(); return 1; }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) { std::fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError()); SDL_DestroyWindow(window); SDL_Quit(); return 1; }

    if (SDL_RenderSetLogicalSize(renderer,raspberry_tv::display::NativeWidth,
                                raspberry_tv::display::NativeHeight) != 0) {
        std::fprintf(stderr,"Falha ao configurar renderizador Full HD: %s\n",SDL_GetError());
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 1;
    }
    SDL_RendererInfo rendererInfo{};
    if (SDL_GetRendererInfo(renderer,&rendererInfo)==0)
        std::printf("Renderizador: %s\n",rendererInfo.name);
    int outputWidth=0,outputHeight=0;
    SDL_GetRendererOutputSize(renderer,&outputWidth,&outputHeight);
    std::printf("Renderizacao interna: %d x %d; saida atual: %d x %d\n",
                raspberry_tv::display::NativeWidth,raspberry_tv::display::NativeHeight,
                outputWidth,outputHeight);
    if(outputWidth<raspberry_tv::display::NativeWidth ||
       outputHeight<raspberry_tv::display::NativeHeight)
        std::fprintf(stderr,
                     "AVISO: saida HDMI abaixo de Full HD: verifique o modo de video do XFCE.\n");
    raspberry_tv::TvUi ui;
    if (!ui.init()) {
        std::fprintf(stderr,"Falha ao iniciar SDL_ttf: %s\n", TTF_GetError());
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 1;
    }
    raspberry_tv::TvModel model;
    if (!playlist.empty()) {
        if (!model.loadPlaylist(playlist)) std::fprintf(stderr,"Nao foi possivel carregar a playlist.\n");
        else std::printf("Playlist carregada: %zu canais\n",model.channels().size());
    }
    raspberry_tv::TvPlayer player;
    if (!player.initialize(window)) {
        model.setStatusMessage("Player X11 indisponivel; menus permanecem ativos");
        std::fprintf(stderr,"AVISO: player IPTV indisponivel nesta sessao.\n");
    }
    ControllerInput input;
    openFirstController();
    bool running = true;
    bool dirty = true;
    std::time_t shownMinute = std::time(nullptr) / 60;
    while (running && !interrupted) {
        SDL_Event event;
        const bool hasEvent = SDL_WaitEventTimeout(&event, 20) != 0;
        if (hasEvent) {
            do {
                const std::uint32_t now = SDL_GetTicks();
                if (event.type == SDL_QUIT) running = false;
                else if (event.type == SDL_CONTROLLERDEVICEADDED) {
                    if (openController(event.cdevice.which)) dirty = true;
                } else if (event.type == SDL_CONTROLLERDEVICEREMOVED && event.cdevice.which == instanceId) {
                    closeController(input);
                    openFirstController();
                    dirty = true;
                } else if ((event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP) &&
                           event.cbutton.which == instanceId) {
                    Button btn;
                    if (buttonFromSDL(event.cbutton.button, btn)) input.onButton(btn, event.type == SDL_CONTROLLERBUTTONDOWN, now);
                } else if (event.type == SDL_CONTROLLERAXISMOTION && event.caxis.which == instanceId) {
                    Axis axis;
                    if (axisFromSDL(event.caxis.axis, axis)) input.onAxis(axis, event.caxis.value, now);
                } else if (!controller && (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)) {
                    Button btn;
                    if (keyFromSDL(event.key.keysym.sym, btn)) input.onButton(btn, event.type == SDL_KEYDOWN, now);
                } else if (event.type == SDL_WINDOWEVENT &&
                           (event.window.event == SDL_WINDOWEVENT_EXPOSED || event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)) {
                    dirty = true;
                }
            } while (SDL_PollEvent(&event));
        }
        for (const Action action : input.poll(SDL_GetTicks())) {
            std::printf("Acao: %s\n", raspberry_tv::actionName(action));
            if (player.active()) {
                // The SDL event loop remains active while GStreamer owns the
                // X11 video area. Never redraw SDL over a playing frame.
                if (action == Action::Back) {
                    player.stop();
                    model.setStatusMessage("Reproducao encerrada");
                    dirty = true;
                } else if (action == Action::Home) {
                    player.stop();
                    model.dispatch(Action::Home);
                    dirty = true;
                } else if (action == Action::Confirm || action == Action::Menu) {
                    player.togglePause();
                } else if (action == Action::NextChannel ||
                           action == Action::PreviousChannel) {
                    model.dispatch(action);
                    if (const auto* channel = model.selectedChannel()) {
                        if (!player.play(channel->url)) {
                            model.setStatusMessage(player.status());
                            dirty = true;
                        }
                    }
                } else if (action == Action::Favorites) {
                    model.dispatch(action);
                }
                continue;
            }
            const bool browsing = model.view()==raspberry_tv::View::Channels ||
                                  model.view()==raspberry_tv::View::Favorites;
            if (action == Action::Confirm && browsing &&
                !model.showHelp() && !model.showDetails() &&
                model.selectedChannel()) {
                if (!player.play(model.selectedChannel()->url)) {
                    model.setStatusMessage(player.status());
                    std::fprintf(stderr,"Player: falha de abertura (URL omitida)\n");
                    dirty = true;
                } else {
                    std::puts("Player: abertura solicitada ao GStreamer (URL omitida)");
                }
                continue;
            }
            model.dispatch(action);
            dirty = true;
        }
        if (player.poll() && !player.active()) {
            model.setStatusMessage(player.status());
            dirty = true;
        }
        const auto minute=std::time(nullptr)/60;
        if(minute!=shownMinute) {shownMinute=minute;dirty=true;}
        if (dirty && !player.active()) {
            ui.draw(renderer, model, controller != nullptr);
            dirty = false;
        }
    }
    // Release the overlay before destroying SDL's X11 drawable.
    player.shutdown();
    closeController(input);
    ui.close();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
