#include "tv/settings.hpp"
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace raspberry_tv {
namespace {
bool safeText(const std::string& s) {return s.size()<=16384 && s.find_first_of("\r\n") == std::string::npos && s.find('\0')==std::string::npos;}
bool identity(const std::string& s) {
    return s.size()==35 && s.substr(0,3)=="v1-" && s.find_first_not_of("0123456789abcdef",3)==std::string::npos;
}
}
bool validAudioSink(const std::string& s) {return s=="autoaudiosink" || s=="alsasink" || s=="pulsesink" || s=="pipewiresink";}
std::filesystem::path defaultStateDirectory() {
    if(const char* p=std::getenv("XDG_CONFIG_HOME")) {
        auto path=std::filesystem::u8path(p);if(path.is_absolute())return path/"raspberry-tv";
    }
    if(const char* p=std::getenv("HOME"))return std::filesystem::u8path(p)/".config"/"raspberry-tv";
#ifdef _WIN32
    if(const char* p=std::getenv("USERPROFILE"))return std::filesystem::u8path(p)/".config"/"raspberry-tv";
#endif
    return {}; // fail safely; never write into the working directory
}
bool loadUserState(const std::filesystem::path& dir,UserState& state,std::string& error) {
    state={};error.clear();std::error_code ec;
    if(dir.empty()){error="Diretorio de configuracao indisponivel";return false;}
    const auto path=dir/"state.conf";
    if(!std::filesystem::exists(path,ec) && !ec)return true;
    if(ec || std::filesystem::file_size(path,ec)>4*1024*1024 || ec) {error="Configuracao invalida; usando padroes";return false;}
    std::ifstream in(path,std::ios::binary);std::string line;
    if(!in || !std::getline(in,line) || line!="raspberry-tv-state 1") {error="Configuracao invalida; usando padroes";return false;}
    UserState next;bool invalid=false;
    while(std::getline(in,line)) {
        std::istringstream row(line);std::string key,value;
        if(!(row>>key>>std::quoted(value)) || !safeText(value)) {invalid=true;continue;}
        row>>std::ws;if(!row.eof()){invalid=true;continue;}
        auto boolean=[&](bool& dest) {if(value=="0" || value=="1")dest=value=="1";else invalid=true;};
        if(key=="playlist") {if(value.find("://")==std::string::npos)next.settings.playlist=value;else invalid=true;}
        else if(key=="channel") {if(value.empty() || identity(value))next.settings.lastChannel=value;else invalid=true;}
        else if(key=="group")next.settings.lastGroup=value;
        else if(key=="volume") {
            try {std::size_t n=0;auto v=std::stod(value,&n);if(n==value.size() && std::isfinite(v) && v>=0 && v<=1)next.settings.volume=v;else invalid=true;} catch(...) {invalid=true;}
        }
        else if(key=="muted")boolean(next.settings.muted);
        else if(key=="fullscreen")boolean(next.settings.fullscreen);
        else if(key=="osd")boolean(next.settings.showOsd);
        else if(key=="resume-selection")boolean(next.settings.resumeSelection);
        else if(key=="audio-sink") {if(validAudioSink(value))next.settings.audioSink=value;else invalid=true;}
        else if(key=="audio-device")next.settings.audioDevice=value;
        else if(key=="favorite") {if(identity(value) && next.favorites.size()<50000)next.favorites.insert(value);else invalid=true;}
    }
    if(in.bad()){error="Falha ao ler configuracao; usando padroes";return false;}
    state=std::move(next);
    if(invalid)error="Algumas configuracoes invalidas foram ignoradas";
    return !invalid;
}
bool saveUserState(const std::filesystem::path& dir,const UserState& state,std::string& error) {
    error.clear();
    if(dir.empty()){error="Diretorio de configuracao indisponivel";return false;}
    const auto& s=state.settings;
    if(!safeText(s.playlist) || s.playlist.find("://")!=std::string::npos || !safeText(s.lastGroup) ||
       !safeText(s.audioDevice) || !validAudioSink(s.audioSink) || !std::isfinite(s.volume) || s.volume<0 || s.volume>1 ||
       (!s.lastChannel.empty() && !identity(s.lastChannel)) || state.favorites.size()>50000) {error="Configuracao nao pode ser salva";return false;}
    std::ostringstream out;out<<"raspberry-tv-state 1\n";
    auto field=[&](const char* key,const std::string& v){out<<key<<' '<<std::quoted(v)<<'\n';};
    field("playlist",s.playlist);field("channel",s.lastChannel);field("group",s.lastGroup);
    field("volume",std::to_string(s.volume));field("muted",s.muted?"1":"0");
    field("fullscreen",s.fullscreen?"1":"0");field("osd",s.showOsd?"1":"0");
    field("resume-selection",s.resumeSelection?"1":"0");field("audio-sink",s.audioSink);field("audio-device",s.audioDevice);
    for(const auto& id:state.favorites) {if(!identity(id)){error="Favorito invalido";return false;}field("favorite",id);}
    const auto data=out.str();std::error_code ec;
    std::filesystem::create_directories(dir,ec);
    if(ec){error="Nao foi possivel criar diretorio de configuracao";return false;}
#ifndef _WIN32
    if(chmod(dir.c_str(),0700)!=0){error="Nao foi possivel proteger configuracao";return false;}
#endif
    static std::atomic<unsigned> counter{0};
#ifdef _WIN32
    const auto pid=GetCurrentProcessId();
#else
    const auto pid=getpid();
#endif
    const auto temp=dir/(".state-"+std::to_string(pid)+"-"+std::to_string(++counter));
    const auto target=dir/"state.conf";
#ifdef _WIN32
    int fd=_wopen(temp.c_str(),_O_WRONLY|_O_CREAT|_O_EXCL|_O_BINARY,_S_IREAD|_S_IWRITE);
#else
    int fd=open(temp.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
#endif
    if(fd<0){error="Nao foi possivel iniciar gravacao atomica";return false;}
    bool ok=true;std::size_t done=0;
    while(done<data.size()) {
#ifdef _WIN32
        int n=_write(fd,data.data()+done,static_cast<unsigned>(data.size()-done));
#else
        auto n=write(fd,data.data()+done,data.size()-done);
#endif
        if(n<0 && errno==EINTR)continue;
        if(n<=0){ok=false;break;}done+=static_cast<std::size_t>(n);
    }
#ifdef _WIN32
    if(_commit(fd)!=0)ok=false;if(_close(fd)!=0)ok=false;
    if(ok)ok=MoveFileExW(temp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    if(fsync(fd)!=0)ok=false;
    if(close(fd)!=0)ok=false;
    if(ok) {std::filesystem::rename(temp,target,ec);ok=!ec;}
    if(ok) {int d=open(dir.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(d>=0){ok=fsync(d)==0;close(d);}else ok=false;}
#endif
    if(!ok){std::filesystem::remove(temp,ec);error="Falha ao salvar configuracao atomicamente";}
    return ok;
}
} // namespace raspberry_tv
