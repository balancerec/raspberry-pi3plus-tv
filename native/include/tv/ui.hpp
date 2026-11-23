#pragma once
#include "tv/menu.hpp"
#include <SDL.h>
#include <SDL_ttf.h>
#include <string>
namespace raspberry_tv {
class TvUi {
public:
    bool init();
    void draw(SDL_Renderer* renderer, const TvModel& model, bool controllerConnected);
    void close();
    ~TvUi() { close(); }
private:
    TTF_Font* title_ = nullptr;
    TTF_Font* heading_ = nullptr;
    TTF_Font* body_ = nullptr;
    TTF_Font* small_ = nullptr;
    void text(SDL_Renderer*, TTF_Font*, const std::string&, int x, int y, SDL_Color, int maxWidth = 1200);
    void panel(SDL_Renderer*, SDL_Rect, SDL_Color, bool selected = false);
    void header(SDL_Renderer*, bool connected);
    void home(SDL_Renderer*, const TvModel&);
    void content(SDL_Renderer*, const TvModel&);
    void overlay(SDL_Renderer*, const TvModel&);
};
} // namespace raspberry_tv
