#include "tv/menu.hpp"
#include <cstdlib>
#include <iostream>
#include <sstream>
using namespace raspberry_tv;
void verify(bool b, const char* message) { if (!b) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); } }
int main() {
    std::istringstream test("\xEF\xBB\xBF#EXTM3U\r\n#EXTINF:-1 tvg-name=\"Rede Um\" group-title=\"Noticias\",Canal Um\r\nhttps://example.org/channel1.m3u8\r\n#EXTINF:-1 group-title=\"Filmes\",Canal Dois\nhttp://example.org/2.ts\n");
    const auto parsed = parseM3u(test);
    verify(parsed.size() == 2, "parse count");
    verify(parsed[0].name == "Canal Um" && parsed[0].group == "Noticias", "EXTINF name/group");
    verify(parsed[1].name == "Canal Dois", "second channel");
    TvModel model;
    verify(model.view() == View::Home && model.selectedHome() == 0, "home initial");
    model.dispatch(Action::Left); verify(model.selectedHome() == 0, "left boundary");
    model.dispatch(Action::Right); verify(model.selectedHome() == 1, "right move");
    model.dispatch(Action::Right); verify(model.selectedHome() == 2, "right move 2");
    model.dispatch(Action::Right); verify(model.selectedHome() == 2, "right row boundary");
    model.dispatch(Action::Down); verify(model.selectedHome() == 5, "down move");
    model.dispatch(Action::Confirm); verify(model.view() == View::Settings, "settings via home");
    model.dispatch(Action::Back); verify(model.view() == View::Home, "back home");
    model.dispatch(Action::Home); verify(model.view() == View::Home, "home hotkey");
    model.dispatch(Action::Menu); verify(model.showHelp(), "help on");
    model.dispatch(Action::Back); verify(!model.showHelp(), "help dismissed");
    model.dispatch(Action::Favorites); verify(model.view() == View::Favorites, "fav shortcut");
    std::cout << "Menu navigation and M3U parse tests passed\n";
}
