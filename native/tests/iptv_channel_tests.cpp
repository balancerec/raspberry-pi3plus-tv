#include "tv/menu.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace raspberry_tv;

static void verify(bool ok, const char* label) {
    if (!ok) {
        std::cerr << "FAIL: " << label << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main() {
    const std::string playlistFile = "iptv-v05-test-list.m3u";
    {
        std::ofstream file(playlistFile, std::ios::binary);
        verify(static_cast<bool>(file), "create temporary fixture");
        file << "\xEF\xBB\xBF#EXTM3U\r\n";
        file << "#EXTINF:-1 tvg-id=\"news1\" group-title=\"Noticias\",News One\r\n";
        file << "https://example.invalid/news1.m3u8\r\n";
        file << "#EXTINF:-1 group-title=\"Esportes\",Sports One\n";
        file << "https://example.invalid/sport1.m3u8\n";
        file << "#EXTINF:-1 group-title=\"Noticias\",News Two\n";
        file << "https://example.invalid/news2.m3u8\n";
        file << "#EXTINF:-1,Kids One\n#EXTGRP:Infantil\n";
        file << "https://example.invalid/kids.ts\n";
    }
    TvModel model;
    verify(model.loadPlaylist(playlistFile), "load local extended M3U");
    verify(model.channels().size() == 4, "all channels parsed");
    verify(model.groupCount() == 3, "unique groups collected");
    verify(model.activeGroup() == "TODOS", "default group is all");
    verify(model.channels()[3].group == "Infantil", "EXTGRP parser");
    verify(model.view() == View::Home, "start home");
    model.dispatch(Action::Confirm);
    verify(model.view() == View::Channels, "open live TV via Cross");
    verify(model.visibleCount() == 4, "all channels visible");
    verify(model.selectedChannel() && model.selectedChannel()->name == "News One", "first channel");

    model.dispatch(Action::NextCategory);
    verify(model.view() == View::Channels, "L1/R1 stays in live TV");
    verify(model.activeGroup() == "Noticias", "R1 enters Noticias");
    verify(model.visibleCount() == 2, "news filter count");
    verify(model.channelIndexAt(0) == 0 && model.channelIndexAt(1) == 2,
           "filtered indices preserve source order");
    verify(model.selectedChannel() && model.selectedChannel()->name == "News One", "selected news");

    model.dispatch(Action::NextChannel);
    verify(model.selectedChannel() && model.selectedChannel()->name == "News Two",
           "R2 changes to next filtered channel");
    model.dispatch(Action::NextChannel);
    verify(model.selectedChannel() && model.selectedChannel()->name == "News One",
           "R2 wraps within current group");
    model.dispatch(Action::PreviousChannel);
    verify(model.selectedChannel() && model.selectedChannel()->name == "News Two",
           "L2 wraps backward within current group");

    model.dispatch(Action::Favorites);
    verify(model.favoriteCount() == 1, "favorite from filtered channel");
    verify(model.hasFavorite(2), "favorite source index not filtered row");

    model.dispatch(Action::NextCategory);
    verify(model.activeGroup() == "Esportes", "next group Esportes");
    verify(model.visibleCount() == 1, "one sport channel");
    verify(model.selectedRow() == 0, "group resets row");
    verify(!model.stepChannel(+1), "one channel does not switch");

    model.dispatch(Action::PreviousCategory);
    verify(model.activeGroup() == "Noticias", "reverse group");
    model.dispatch(Action::PreviousCategory);
    verify(model.activeGroup() == "TODOS", "previous group returns all");
    verify(model.visibleCount() == 4, "restored all channels");

    model.dispatch(Action::Favorites);
    model.dispatch(Action::Home);
    model.dispatch(Action::Favorites);
    verify(model.view() == View::Favorites, "open favorite list via triangle on home");
    verify(model.visibleCount() == 1, "favorite appears in favorites");
    verify(model.selectedChannel() && model.selectedChannel()->name == "News Two",
           "favorite mapping stable across group filters");
    verify(!model.stepChannel(-1), "single favorite cannot wrap to another");
    model.dispatch(Action::Favorites);
    verify(model.visibleCount() == 0, "removing favorite leaves valid empty view");
    verify(model.selectedChannel() == nullptr, "no stale selected favorite");

    // Existing playlist must remain usable if an invalid replacement is requested.
    verify(!model.loadPlaylist("nonexistent-file-iptv-v05.m3u"), "missing playlist rejected");
    verify(model.channels().size() == 4, "failed reload retains original channels");

    std::remove(playlistFile.c_str());
    std::cout << "IPTV v0.5 group/filter/favorites/channel switching passed\n";
}
