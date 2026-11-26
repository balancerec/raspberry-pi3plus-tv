#pragma once
#include "tv/menu.hpp"
#include "tv/playback_state.hpp"
#include "tv/spotify.hpp"
#include <SDL.h>
#include <SDL_ttf.h>
#include <map>
#include <string>
namespace raspberry_tv {
class TvUi {
public:
    bool init();
    bool initOsd(SDL_Window* parent);
    void draw(SDL_Renderer*,const TvModel&,bool controllerConnected);
    void drawOsd(SDL_Window* parent,const Channel*,const PlayerSnapshot&,bool help,bool visible);
    void setSettings(const UserSettings& settings) {settings_=settings;}
    void setSpotify(const SpotifyPanel* panel, bool busy) {spotify_=panel;spotifyBusy_=busy;}
    void close();
    ~TvUi(){close();}
private:
    struct Text {SDL_Texture* texture=nullptr;int w=0,h=0;std::size_t bytes=0;unsigned long used=0;};
    std::map<std::string,Text> cache_;
    std::size_t cacheBytes_=0;unsigned long use_=0;
    SDL_Window* osdWindow_=nullptr;SDL_Renderer* osdRenderer_=nullptr;
    bool osdVisible_=false;
    UserSettings settings_;
    const SpotifyPanel* spotify_ = nullptr;
    bool spotifyBusy_ = false;
    TTF_Font* title_=nullptr;TTF_Font* heading_=nullptr;TTF_Font* body_=nullptr;TTF_Font* small_=nullptr;
    void text(SDL_Renderer*,TTF_Font*,const std::string&,int,int,SDL_Color,int maxWidth=1200);
    void panel(SDL_Renderer*,SDL_Rect,SDL_Color,bool selected=false);
    void header(SDL_Renderer*,bool);
    void home(SDL_Renderer*,const TvModel&);
    void content(SDL_Renderer*,const TvModel&);
    void overlay(SDL_Renderer*,const TvModel&);
};
}
