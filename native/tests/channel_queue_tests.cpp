#include "tv/channel_queue.hpp"
#include "test.hpp"
#include <sstream>
using namespace raspberry_tv;
int main() {
    std::istringstream in("#EXTM3U\n#EXTINF:-1 group-title=\"G1\",A\nhttps://example.invalid/a\n#EXTINF:-1 group-title=\"G2\",B\nhttps://example.invalid/b\n#EXTINF:-1 group-title=\"G1\",C\nhttps://example.invalid/c\n");
    TvModel m;check(m.applyPlaylist(readM3u(in),"queue.m3u"),"fixture");m.dispatch(Action::Confirm);m.dispatch(Action::NextCategory);
    ChannelQueue q;q.start(m);check(q.current(m)->name=="A","queue selected channel");
    check(q.step(1,m) && q.current(m)->name=="C","queue stays in group");
    check(q.step(1,m) && q.current(m)->name=="A","forward group wrap");
    check(q.step(-1,m) && q.current(m)->name=="C","backward group wrap");check(!q.step(0,m),"zero is no-op");
    m.toggleFavoriteIdentity(channelIdentity(m.channels()[0]));m.toggleFavoriteIdentity(channelIdentity(m.channels()[2]));
    m.dispatch(Action::Home);m.dispatch(Action::Favorites);q.start(m);
    check(q.step(1,m) && q.current(m)->name=="C","favorites queue");
    m.toggleFavoriteIdentity(channelIdentity(m.channels()[0]));check(!q.step(1,m),"removed favorites skipped");
    m.toggleFavoriteIdentity(channelIdentity(m.channels()[2]));check(!q.step(-1,m),"empty favorites no change");
    m.dispatch(Action::Home);m.dispatch(Action::Confirm);m.setFilter("C");q.start(m);
    check(q.current(m)->name=="C" && !q.step(1,m),"name filter preserved in queue");
}
