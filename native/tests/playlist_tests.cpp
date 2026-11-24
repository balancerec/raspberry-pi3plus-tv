#include "tv/playlist.hpp"
#include "test.hpp"
#include <sstream>
using namespace raspberry_tv;
int main() {
    std::istringstream in("\xEF\xBB\xBF#EXTM3U\r\n#EXTINF:-1 tvg-id=\"n1\" tvg-name=\"Notícias, Brasil\" tvg-logo=\"https://example.invalid/logo.png\" group-title=\"News, BR\",Notícias HD\nhttps://example.invalid/live?token=secret\nhttps://example.invalid/live?token=secret\ngarbage\n#EXTINF:-1,\nfile:///tmp/video.mp4\n");
    auto r=readM3u(in);check(bool(r) && r.channels.size()==2,"valid channels, duplicate/garbage ignored");
    check(r.invalid==1 && r.duplicates==1,"diagnostic counts");
    const auto& c=r.channels[0];check(c.tvgId=="n1" && c.tvgName=="Notícias, Brasil" && c.group=="News, BR","all metadata preserved");
    check(c.tvgLogo=="https://example.invalid/logo.png","logo preserved without downloading");
    auto other=c;other.url="https://other.invalid/changed?token=new";
    check(channelIdentity(c)==channelIdentity(other),"tvg identity survives provider token and address change");
    other=c;other.tvgId.clear();auto original=other;other.url="https://example.invalid/live?token=changed";
    check(channelIdentity(original)==channelIdentity(other),"fallback identity ignores query token");
    other.name="Other channel";other.tvgName="Other";check(channelIdentity(original)!=channelIdentity(other),"labels distinguish URL query variants");
    std::istringstream hls("#EXTM3U\n#EXT-X-TARGETDURATION:6\n#EXTINF:6,\nsegment.ts\n");
    auto h=readM3u(hls);check(!h && h.channels.empty(),"HLS manifest is not an IPTV catalog");
    std::istringstream empty("#EXTM3U\n# comment\n");check(!readM3u(empty),"empty list rejected");
    std::istringstream bad("#EXTM3U\n\xc0\xaf\nhttps://example.invalid/a\n");
    check(readM3u(bad).invalid==1,"invalid UTF-8 rejected");
    check(!validUtf8("\xed\xa0\x80") && !validUtf8("\xf4\x90\x80\x80"),"surrogate/out of range rejected");
    std::istringstream hugeLine(std::string(20000,'x')+"\nhttps://example.invalid/a\n");
    check(readM3u(hugeLine).invalid==1,"line allocation bounded");
    std::ostringstream many;many<<"#EXTM3U\n";
    for(int i=0;i<12000;++i)many<<"#EXTINF:-1 group-title=\"G\",Canal "<<i<<"\nhttps://example.invalid/"<<i<<"\n";
    std::istringstream big(many.str());auto b=readM3u(big,10000);
    check(b.channels.size()==10000 && b.truncated,"large catalog bounded with explicit truncation");
    std::atomic<bool> cancel{true};std::istringstream cancelled(many.str());check(!readM3u(cancelled,50000,"",&cancel),"read cancellation");
    check(!allowedMediaUri("javascript://x") && !allowedMediaUri("https://host/x\nsecret"),"dangerous schemes/control characters rejected");
    check(!readPlaylistFile("https://example.invalid/list.m3u"),"remote import explicitly refused");
}
