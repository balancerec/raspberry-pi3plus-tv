#include "tv/playlist.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_set>
namespace raspberry_tv {
namespace {
constexpr std::size_t MaxBytes = 64 * 1024 * 1024, MaxLine = 16384;
std::string trim(const std::string& s) {
    auto a=s.find_first_not_of(" \t\r\n");
    return a==std::string::npos ? "" : s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);
}
std::string attribute(const std::string& line,const std::string& key) {
    std::size_t at=0;
    while ((at=line.find(key,at))!=std::string::npos) {
        if (at && !std::isspace(static_cast<unsigned char>(line[at-1]))) { at+=key.size(); continue; }
        std::size_t p=at+key.size();
        while(p<line.size() && line[p]==' ') ++p;
        if(p>=line.size() || line[p++]!='=') {at=p;continue;}
        while(p<line.size() && line[p]==' ') ++p;
        if(p>=line.size()) return {};
        const char quote=line[p];
        if(quote!='\"' && quote!='\'') {
            auto end=line.find_first_of(" ,\t",p); return line.substr(p,end-p);
        }
        std::string out;
        for(++p;p<line.size();++p) {
            if(line[p]==quote) return out;
            if(line[p]=='\\' && p+1<line.size() && (line[p+1]==quote || line[p+1]=='\\')) ++p;
            out+=line[p];
        }
        return {};
    }
    return {};
}
std::size_t separator(const std::string& s) {
    char quote=0;
    for(std::size_t i=0;i<s.size();++i) {
        if(s[i]=='\\' && quote && i+1<s.size()) {++i;continue;}
        if(quote && s[i]==quote) quote=0;
        else if(!quote && (s[i]=='\"' || s[i]=='\'')) quote=s[i];
        else if(!quote && s[i]==',') return i;
    }
    return std::string::npos;
}
std::string fileUri(const std::filesystem::path& path) {
    const std::string s=path.generic_u8string();
    std::ostringstream out; out<<"file://";
    for(unsigned char c:s) {
        if(std::isalnum(c) || c=='/' || c=='-' || c=='_' || c=='.' || c=='~' || c==':') out<<c;
        else out<<'%'<<std::uppercase<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<int>(c);
    }
    return out.str();
}
std::string normalizedAddress(std::string url) {
    url=url.substr(0,url.find_first_of("?#")); // temporary query tokens are never identity
    auto scheme=url.find("://");
    if(scheme!=std::string::npos) {
        const auto end=url.find('/',scheme+3);
        auto at=url.rfind('@',end);
        if(at!=std::string::npos && at>scheme) url.erase(scheme+3,at-scheme-2);
        auto hostEnd=url.find('/',scheme+3);
        std::transform(url.begin(),hostEnd==std::string::npos?url.end():url.begin()+hostEnd,
                       url.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    }
    return url;
}
}
bool validUtf8(const std::string& s) {
    for(std::size_t i=0;i<s.size();) {
        auto c=static_cast<unsigned char>(s[i++]);
        if(c<0x80) {if(c<32 && c!='\t' && c!='\r') return false; continue;}
        unsigned n=0; unsigned cp=0, min=0;
        if(c>=0xc2 && c<=0xdf) {n=1;cp=c&31;min=0x80;}
        else if(c>=0xe0 && c<=0xef) {n=2;cp=c&15;min=0x800;}
        else if(c>=0xf0 && c<=0xf4) {n=3;cp=c&7;min=0x10000;} else return false;
        if(i+n>s.size()) return false;
        while(n--) {auto t=static_cast<unsigned char>(s[i++]);if((t&0xc0)!=0x80)return false;cp=(cp<<6)|(t&63);}
        if(cp<min || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) return false;
    }
    return true;
}
bool allowedMediaUri(const std::string& s) {
    if(s.empty() || s.find_first_of("\r\n\t ")!=std::string::npos) return false;
    const auto end=s.find("://"); if(end==std::string::npos || end+3>=s.size()) return false;
    const auto scheme=s.substr(0,end);
    return scheme=="http" || scheme=="https" || scheme=="rtsp" || scheme=="rtmp" || scheme=="udp" || scheme=="file";
}
std::string channelIdentity(const Channel& c) {
    // Non-cryptographic opaque identifier, NOT encryption. No URL/token is written.
    const auto label=c.tvgName.empty()?c.name:c.tvgName;
    const std::string material=(c.tvgId.empty()?"url:"+normalizedAddress(c.url):"id:"+c.tvgId)+"\n"+label+"\n"+c.group;
    std::uint64_t a=14695981039346656037ULL,b=7809847782465536322ULL;
    for(unsigned char ch:material) {a=(a^ch)*1099511628211ULL;b=(b^ch)*14029467366897019727ULL;}
    std::ostringstream out;out<<"v1-"<<std::hex<<std::setfill('0')<<std::setw(16)<<a<<std::setw(16)<<b;
    return out.str();
}
PlaylistResult readM3u(std::istream& in,std::size_t limit,const std::string& base,const std::atomic<bool>* cancel) {
    PlaylistResult result; Channel pending; std::string line;
    std::unordered_set<std::string> seen; std::size_t bytes=0;
    bool first=true;
    if(!limit) {result.error="Limite de canais invalido";return result;}
    // Read bounded lines; do not let getline allocate an attacker-controlled line.
    while(in.good()) {
        if(cancel && cancel->load()) {result.error="Leitura cancelada";return result;}
        line.clear(); bool tooLong=false; char c;
        while(in.get(c)) {
            if((bytes%4096)==0 && cancel && cancel->load()) {result.error="Leitura cancelada";return result;}
            if(++bytes>MaxBytes) {result.error="Playlist excede 64 MiB";result.channels.clear();return result;}
            if(c=='\n') break;
            if(line.size()<MaxLine) line+=c; else tooLong=true;
        }
        if(first && line.compare(0,3,"\xEF\xBB\xBF")==0) line.erase(0,3);
        first=false;
        if(tooLong || !validUtf8(line)) {++result.invalid;pending={};continue;}
        line=trim(line);if(line.empty())continue;
        if(line.rfind("#EXT-X-",0)==0) {result.error="Manifesto HLS: adicione sua URL a um catalogo M3U";result.channels.clear();return result;}
        if(line.rfind("#EXTINF:",0)==0) {
            pending={}; const auto sep=separator(line);
            if(sep==std::string::npos) {++result.invalid;continue;}
            const auto meta=line.substr(0,sep);
            pending.name=trim(line.substr(sep+1));pending.group=attribute(meta,"group-title");
            pending.tvgName=attribute(meta,"tvg-name");pending.tvgId=attribute(meta,"tvg-id");pending.tvgLogo=attribute(meta,"tvg-logo");
            if(pending.name.empty())pending.name=pending.tvgName;
            continue;
        }
        if(line.rfind("#EXTGRP:",0)==0) {pending.group=trim(line.substr(8));continue;}
        if(line[0]=='#')continue;
        std::string uri=line;
        if(!allowedMediaUri(uri) && !base.empty() && uri.find("://")==std::string::npos) {
            std::error_code ec;
            auto path=std::filesystem::u8path(uri);if(path.is_relative()) path=std::filesystem::u8path(base)/path;
            if(std::filesystem::is_regular_file(path,ec)) uri=fileUri(std::filesystem::absolute(path,ec));
        }
        if(!allowedMediaUri(uri)) {++result.invalid;pending={};continue;}
        if(!seen.insert(uri).second) {++result.duplicates;pending={};continue;}
        if(result.channels.size()>=limit) {result.truncated=true;break;}
        pending.url=uri;
        if(pending.name.empty())pending.name="Canal "+std::to_string(result.channels.size()+1);
        if(pending.group.empty())pending.group="Sem categoria";
        result.channels.push_back(std::move(pending));pending={};
    }
    if(in.bad()) {result.error="Falha de leitura da playlist";result.channels.clear();}
    if(result.channels.empty() && result.error.empty())result.error="Playlist sem canais validos";
    return result;
}
PlaylistResult readPlaylistFile(const std::string& path,const std::atomic<bool>* cancel) {
    PlaylistResult r;std::error_code ec;const auto p=std::filesystem::u8path(path);
    if(path.find("://")!=std::string::npos) {r.error="Importacao remota nao suportada; use arquivo local";return r;}
    if(!std::filesystem::is_regular_file(p,ec)) {r.error="Arquivo de playlist indisponivel";return r;}
    if(std::filesystem::file_size(p,ec)>MaxBytes || ec) {r.error="Playlist excede 64 MiB ou nao pode ser lida";return r;}
    std::ifstream file(p,std::ios::binary);if(!file){r.error="Nao foi possivel abrir a playlist";return r;}
    return readM3u(file,50000,std::filesystem::absolute(p,ec).parent_path().u8string(),cancel);
}
std::vector<Channel> parseM3u(std::istream& in,std::size_t limit) {return readM3u(in,limit).channels;}
} // namespace raspberry_tv
