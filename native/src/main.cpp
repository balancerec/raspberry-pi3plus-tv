#include "tv/input.hpp"
#include <SDL.h>

#include <cstdio>
#include <cstring>

using raspberry_tv::Action;
using raspberry_tv::Axis;
using raspberry_tv::Button;
using raspberry_tv::ControllerInput;

namespace {
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
void paint(SDL_Renderer* r, unsigned selected, bool gamepadConnected) {
    int width = 960, height = 540;
    SDL_GetRendererOutputSize(r, &width, &height);
    SDL_SetRenderDrawColor(r, 7, 16, 20, 255);
    SDL_RenderClear(r);
    const int margin = width / 18;
    const int gap = width / 48;
    const int tileWidth = (width - 2 * margin - 2 * gap) / 3;
    const int tileHeight = (height - 2 * margin - gap) / 2;
    for (unsigned i = 0; i < 6; ++i) {
        SDL_Rect tile{margin + static_cast<int>(i % 3) * (tileWidth + gap),
                      margin + static_cast<int>(i / 3) * (tileHeight + gap), tileWidth, tileHeight};
        SDL_SetRenderDrawColor(r, 20, 43, 54, 255);
        SDL_RenderFillRect(r, &tile);
        if (i == selected) {
            SDL_SetRenderDrawColor(r, gamepadConnected ? 86 : 130, gamepadConnected ? 240 : 130, 178, 255);
            for (int d = 0; d < 4; ++d) {
                SDL_Rect outline{tile.x + d, tile.y + d, tile.w - 2 * d, tile.h - 2 * d};
                SDL_RenderDrawRect(r, &outline);
            }
        }
    }
    SDL_RenderPresent(r);
}
} // namespace

int main(int argc, char** argv) {
    const bool fullscreen = argc > 1 && std::strcmp(argv[1], "--fullscreen") == 0;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    const Uint32 flags = SDL_WINDOW_SHOWN | (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_RESIZABLE);
    SDL_Window* window = SDL_CreateWindow("Raspberry TV - prototipo Gamepad First", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, 960, 540, flags);
    if (!window) { std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); SDL_Quit(); return 1; }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) { std::fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError()); SDL_DestroyWindow(window); SDL_Quit(); return 1; }

    ControllerInput input;
    openFirstController();
    unsigned selected = 0;
    bool running = true;
    bool dirty = true;
    while (running) {
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
            switch (action) {
                case Action::Up: selected = (selected + 3) % 6; dirty = true; break;
                case Action::Down: selected = (selected + 3) % 6; dirty = true; break;
                case Action::Left: selected = (selected + 5) % 6; dirty = true; break;
                case Action::Right: selected = (selected + 1) % 6; dirty = true; break;
                case Action::Home: selected = 0; dirty = true; break;
                case Action::Back: selected = 0; dirty = true; break;
                default: break; // Future IPTV modules will subscribe to these actions.
            }
        }
        if (dirty) { paint(renderer, selected, controller != nullptr); dirty = false; }
    }
    closeController(input);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
