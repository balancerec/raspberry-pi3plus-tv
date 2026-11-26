#include "tv/spotify.hpp"
#include "tv/playlist.hpp"
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace raspberry_tv {
namespace {
int hex(char c) {
    if(c>='0' && c<='9')return c-'0';
    if(c>='a' && c<='f')return c-'a'+10;
    if(c>='A' && c<='F')return c-'A'+10;
    return -1;
}
std::string decode(const std::string& encoded) {
    std::string result;
    for(std::size_t i=0;i<encoded.size();++i) {
        unsigned char value=static_cast<unsigned char>(encoded[i]);
        if(value=='%') {
            if(i+2>=encoded.size() || hex(encoded[i+1])<0 || hex(encoded[i+2])<0)throw std::runtime_error("encoding");
            value=static_cast<unsigned char>(hex(encoded[i+1])*16+hex(encoded[i+2]));i+=2;
        }
        if(value<32 || value==127)throw std::runtime_error("control");
        result+=static_cast<char>(value);
    }
    if(!validUtf8(result))throw std::runtime_error("utf8");
    return result;
}
int number(const std::string& text,int minimum,int maximum) {
    int value=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || value<minimum || value>maximum)throw std::runtime_error("number");
    return value;
}
std::vector<std::string> fields(const std::string& line) {
    std::vector<std::string> result;std::size_t start=0;
    do {
        const auto end=line.find('\t',start);
        result.push_back(decode(line.substr(start,end==std::string::npos?end:end-start)));
        if(result.size()>8)throw std::runtime_error("fields");
        if(end==std::string::npos)break;
        start=end+1;
    }while(true);
    return result;
}
SpotifyReply invalid() {return {};}
}
SpotifyReply parseSpotifyReply(const std::string& wire) {
    if(wire.size()>262144)return invalid();
    std::istringstream stream(wire);std::string line;
    if(!std::getline(stream,line) || line!="RTVSPOTIFY\t1")return invalid();
    SpotifyReply reply;bool status=false;
    try {
        while(std::getline(stream,line)) {
            if(line.empty())continue;
            if(line.size()>8192)throw std::runtime_error("line");
            const auto f=fields(line);
            if(f[0]=="status" && f.size()==3 && !status) {
                const std::vector<std::string> allowed{"ready","setup","auth","error","rate_limit","busy","network","storage","request","response","access","unavailable","cancelled"};
                if(std::find(allowed.begin(),allowed.end(),f[1])==allowed.end() || f[2].size()>1200)throw std::runtime_error("status");
                reply.code=f[1];reply.message=f[2];status=true;
            } else if(f[0]=="profile" && f.size()==2)reply.profile=f[1];
            else if(f[0]=="timeline" && f.size()==4 && !reply.hasTimeline) {
                reply.position=number(f[1],0,604800000);reply.duration=number(f[2],1,604800000);
                reply.speed=number(f[3],0,4000);reply.hasTimeline=true;
                if(reply.position>reply.duration)throw std::runtime_error("position");
            } else if(f[0]=="features" && f.size()==7 && !reply.hasFeatures) {
                reply.active=number(f[1],0,1)!=0;reply.canSeek=number(f[2],0,1)!=0;
                reply.canShuffle=number(f[3],0,1)!=0;reply.canRepeat=number(f[4],0,1)!=0;
                reply.shuffle=number(f[5],-1,1);reply.repeat=f[6];reply.hasFeatures=true;
                if(reply.repeat!="off" && reply.repeat!="context" && reply.repeat!="track" && reply.repeat!="unknown")throw std::runtime_error("repeat");
            }
            else if(f[0]=="playback" && f.size()==6 && !reply.hasPlayback) {
                if(f[1]!="playing" && f[1]!="paused" && f[1]!="idle" && f[1]!="buffering")throw std::runtime_error("playback");
                reply.hasPlayback=true;reply.playback=f[1];reply.track=f[2];reply.artist=f[3];reply.device=f[4];reply.volume=number(f[5],-1,100);
            } else if(f[0]=="page" && f.size()==6 && !reply.hasPage) {
                if(f[1]!="playlists" && f[1]!="devices" && f[1]!="items" && f[1]!="search" && f[1]!="queue")throw std::runtime_error("page");
                reply.hasPage=true;reply.page=f[1];reply.offset=number(f[2],0,100000);reply.nextOffset=number(f[3],-1,100000);reply.total=number(f[4],0,100000);reply.parent=f[5];
                if(reply.nextOffset!=-1 && reply.nextOffset<=reply.offset)throw std::runtime_error("pagination");
            } else if(f[0]=="row" && f.size()==6 && reply.entries.size()<50) {
                if(f[1]!="playlist" && f[1]!="track" && f[1]!="episode" && f[1]!="device")throw std::runtime_error("row");
                if(f[2].size()>160 || f[3].size()>960 || f[4].size()>960 || (f[5]!="0" && f[5]!="1"))throw std::runtime_error("row");
                reply.entries.push_back({f[1],f[2],f[3],f[4],f[5]=="1"});
            } else throw std::runtime_error("protocol");
        }
        if(!status || (!reply.entries.empty() && !reply.hasPage) || ((reply.hasTimeline || reply.hasFeatures) && !reply.hasPlayback))return invalid();
        return reply;
    } catch(const std::exception&) {return invalid();}
}

bool spotifyPrivateAudioEnabled() {
    const auto* configured=std::getenv("XDG_CONFIG_HOME");
    const auto* home=std::getenv("HOME");
    if((!configured || !*configured) && (!home || !*home))return false;
    const auto base=configured && *configured?std::filesystem::u8path(configured):std::filesystem::u8path(home)/".config";
    std::ifstream file(base/"raspberry-tv"/"spotify"/"audio-focus-v1");
    char line[64]{};file.getline(line,sizeof(line));
    return std::string(line)=="raspberry-tv-private-hdmi-v1";
}

SpotifyWorker::SpotifyWorker(SpotifyRunner runner):runner_(std::move(runner)) {thread_=std::thread(&SpotifyWorker::run,this);}
SpotifyWorker::~SpotifyWorker(){shutdown();}
bool SpotifyWorker::submit(SpotifyArguments arguments) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(exit_ || busy_ || arguments.empty())return false;
    cancel_=std::make_shared<std::atomic<bool>>(false);pending_=std::move(arguments);result_.reset();busy_=true;wake_.notify_one();return true;
}
void SpotifyWorker::cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    if(cancel_)cancel_->store(true);
    result_.reset();
}
bool SpotifyWorker::busy() const {std::lock_guard<std::mutex> lock(mutex_);return busy_;}
std::optional<SpotifyReply> SpotifyWorker::take() {
    std::lock_guard<std::mutex> lock(mutex_);auto reply=std::move(result_);result_.reset();return reply;
}
void SpotifyWorker::shutdown() {
    {std::lock_guard<std::mutex> lock(mutex_);exit_=true;if(cancel_)cancel_->store(true);wake_.notify_one();}
    if(thread_.joinable())thread_.join();
}
void SpotifyWorker::run() {
    while(true) {
        SpotifyArguments arguments;SpotifyCancel cancelled;
        {
            std::unique_lock<std::mutex> lock(mutex_);wake_.wait(lock,[this]{return exit_ || pending_.has_value();});
            if(exit_)return;
            arguments=std::move(*pending_);pending_.reset();cancelled=cancel_;
        }
        SpotifyReply reply;
        try {if(!cancelled->load())reply=parseSpotifyReply(runner_(arguments,cancelled));}
        catch(const std::exception&){reply.message="Falha ao executar o cliente Spotify.";}
        {std::lock_guard<std::mutex> lock(mutex_);if(!cancelled->load() && !exit_)result_=std::move(reply);busy_=false;}
    }
}

SpotifyPanel::SpotifyPanel(){rebuild();}
void SpotifyPanel::useLocal(bool local) {
    local_=local;page_.reset();confirming_=false;playback_="unknown";media_={};showMedia();mediaFocus_=2;
    nowPlaying_=local?"Spotify Soloist - dispositivo Raspberry TV":"Spotify Web API - dispositivo da conta";
    message_=local?"Escolha Raspberry TV nos dispositivos do Spotify.":"Conecte sua conta para ver playlists e dispositivos.";
    rebuild();
}
void SpotifyPanel::requestStarted(const std::string& command) {
    static const std::vector<std::string> commands{
        "soloist-status","soloist-play","soloist-resume","soloist-toggle","soloist-pause",
        "soloist-next","soloist-previous","soloist-volume-up","soloist-volume-down",
        "soloist-activate","soloist-deactivate","soloist-mute","soloist-shuffle","soloist-repeat",
        "soloist-seek","soloist-seek-back","soloist-seek-forward","status","play","resume","pause","toggle",
        "next","previous","volume-up","volume-down","transfer"};
    if(std::find(commands.begin(),commands.end(),command)==commands.end())return;
    confirming_=true;nowPlaying_="Atualizando estado do Spotify...";
}
void SpotifyPanel::requestCancelled() {
    if(!confirming_)return;
    confirming_=false;playback_="unknown";
    media_.playback="unknown";media_.hasTimeline=false;
    nowPlaying_="Estado do Spotify sem confirmacao; atualize";
    message_="Espera cancelada. Um comando ja aceito pelo Spotify pode ter sido executado.";
    rebuild();
}
void SpotifyPanel::accept(const SpotifyReply& reply,std::uint32_t now) {
    message_=reply.message;
    if((confirming_ || reply.code!="ready") && !reply.hasPlayback) {playback_="unknown";media_.playback="unknown";media_.hasTimeline=false;nowPlaying_="Estado do Spotify sem confirmacao; atualize";}
    confirming_=false;
    if(reply.code=="auth" || reply.code=="setup") {page_.reset();showMedia();playback_="unknown";nowPlaying_="Spotify Connect - conta desconectada";}
    if(reply.hasPlayback) {
        media_=reply;mediaAt_=now;
        playback_=reply.playback;
        nowPlaying_=local_ && reply.hasFeatures && !reply.active?"Em outro dispositivo: ":reply.playback=="playing"?"Tocando: ":reply.playback=="paused"?"Pausado: ":reply.playback=="buffering"?"Buffering: ":"Sem reproducao ativa";
        if(!reply.track.empty())nowPlaying_+=(reply.playback=="idle"?" | ":"")+reply.track;
        if(!reply.artist.empty())nowPlaying_+=" / "+reply.artist;
        if(!reply.device.empty())nowPlaying_+=" | "+reply.device;
        if(reply.volume>=0)nowPlaying_+=" | "+std::to_string(reply.volume)+"%";
    }
    if(reply.hasPage && reply.code=="ready") {page_=reply;mediaMode_=false;browsing_=true;}
    rebuild();
}
void SpotifyPanel::rebuild() {
    rows_={
        {"Atualizar estado da conta", "Spotify Connect", {"status"}},
        {"Minhas playlists", "X listar", {"playlists"}},
        {"Escolher dispositivo", "X listar", {"devices"}},
        {"Retomar reproducao", "Premium", {"resume"}},
        {"Pausar", "Dispositivo Spotify", {"pause"}},
        {"Faixa anterior", "", {"previous"}},
        {"Proxima faixa", "", {"next"}},
        {"Diminuir volume em 5%", "Dispositivo Spotify", {"volume-down"}},
        {"Aumentar volume em 5%", "Dispositivo Spotify", {"volume-up"}},
        {"Desconectar Web API neste aparelho", "Remove tokens OAuth locais", {"logout"}},
        {"Controlar Soloist no Raspberry", "Conta via Spotify Connect", {"use-local"}}
    };
    if(local_)rows_={
        {"Atualizar estado local", "Spotify Soloist", {"soloist-status"}},
        {"Fila local de reproducao", "X listar", {"soloist-queue"}},
        {"Ativar Raspberry TV no Spotify", "Dispositivo local", {"soloist-activate"}},
        {"Retomar reproducao local", "Soloist", {"soloist-resume"}},
        {media_.hasFeatures && !media_.active?"Reproduzir no Raspberry TV":playback_=="paused"?"Retomar Spotify local":
         playback_=="playing" || playback_=="buffering"?"Pausar Spotify local":"Pausar / retomar Spotify local",
         "X alterna pausa/reproducao", {"soloist-toggle"}},
        {"Faixa anterior", "Soloist", {"soloist-previous"}},
        {"Proxima faixa", "Soloist", {"soloist-next"}},
        {"Diminuir volume em 5%", "Soloist", {"soloist-volume-down"}},
        {"Aumentar volume em 5%", "Soloist", {"soloist-volume-up"}},
        {"Desativar dispositivo local", "Cede a outro dispositivo", {"soloist-deactivate"}},
        {"Abrir playlists e dispositivos da conta", "Web API / OAuth opcional", {"use-web"}}
    };
    if(!page_)return;
    for(const auto& entry:page_->entries) {
        const auto command=entry.kind=="device"?"transfer":local_?"soloist-play":"play";
        rows_.push_back({entry.title,entry.detail,{command,entry.identity},entry.enabled});
    }
    const auto pageArguments=[this](int offset) {
        SpotifyArguments result{page_->page};
        if(page_->page=="items" || page_->page=="search")result.push_back(page_->parent);
        result.push_back("--offset");result.push_back(std::to_string(offset));return result;
    };
    const int size=page_->page=="search"?10:20;
    if(page_->offset>0)rows_.push_back({"Pagina anterior", "",pageArguments(std::max(0,page_->offset-size))});
    if(page_->nextOffset>=0)rows_.push_back({"Proxima pagina", "",pageArguments(page_->nextOffset)});
}
SpotifyArguments SpotifyPanel::activate(std::size_t row,bool details) const {
    if(row>=rows_.size() || !rows_[row].enabled)return {};
    if(details) {
        const auto& arguments=rows_[row].arguments;
        if(arguments.size()==2 && arguments[0]=="play" && arguments[1].rfind("spotify:playlist:",0)==0)return {"items",arguments[1]};
        return {};
    }
    return rows_[row].arguments;
}
SpotifyArguments SpotifyPanel::statusCommand() const {return {local_?"soloist-status":"status","--media"};}
bool SpotifyPanel::controlEnabled(std::size_t control) const {
    if(control==0)return local_ && media_.canShuffle;
    if(control==4)return local_ && media_.canRepeat;
    if(control==5)return media_.volume>=0 && local_;
    if(control==6 || control==7)return media_.volume>=0;
    return control<MediaRects.size();
}
std::string SpotifyPanel::controlLabel(std::size_t control) const {
    static const std::array<const char*,12> labels{{"Aleatorio","Anterior","Reproduzir","Proxima","Repetir","Mudo","Volume -","Volume +","Fila","Dispositivo","Mais","Atualizar"}};
    if(control>=labels.size())return {};
    if(control==2 && !(local_ && media_.hasFeatures && !media_.active) && (playback_=="playing" || playback_=="buffering"))return "Pausar";
    if(control==5 && media_.volume==0)return "Ativar som";
    return labels[control];
}
SpotifyArguments SpotifyPanel::activateMedia() const {
    if(!controlEnabled(mediaFocus_))return {};
    static const std::array<const char*,12> commands{{"shuffle","previous","toggle","next","repeat","mute","volume-down","volume-up","queue","activate","show-options","status"}};
    if(mediaFocus_==10)return {"show-options"};
    std::string command=commands[mediaFocus_];
    if(!local_ && mediaFocus_==8)command="playlists";
    if(!local_ && mediaFocus_==9)command="devices";
    return {(local_?"soloist-":"")+command,"--media"};
}
SpotifyArguments SpotifyPanel::mediaShortcut(Action action) const {
    std::string command;
    if(action==Action::Confirm)command="toggle";
    else if(action==Action::PreviousCategory)command="previous";
    else if(action==Action::NextCategory)command="next";
    else if(action==Action::Favorites)command=local_?"queue":"playlists";
    else if(action==Action::Details && local_ && media_.volume>=0)command="mute";
    else if(local_ && media_.canSeek && action==Action::PreviousChannel)command="seek-back";
    else if(local_ && media_.canSeek && action==Action::NextChannel)command="seek-forward";
    if(command.empty())return {};
    return {(local_?"soloist-":"")+command,"--media"};
}
void SpotifyPanel::navigateMedia(Action action) {
    const std::size_t first=mediaFocus_<8?0:8, count=mediaFocus_<8?8:4;
    if(action==Action::Left)mediaFocus_=first+(mediaFocus_-first+count-1)%count;
    else if(action==Action::Right)mediaFocus_=first+(mediaFocus_-first+1)%count;
    else if((action==Action::Down && mediaFocus_<8) || (action==Action::Up && mediaFocus_>=8)) {
        const int x=MediaRects[mediaFocus_].x+MediaRects[mediaFocus_].w/2;
        int best=10000;const std::size_t begin=action==Action::Down?8:0,end=action==Action::Down?12:8;
        for(auto i=begin;i<end;++i) {const int distance=std::abs(MediaRects[i].x+MediaRects[i].w/2-x);if(distance<best){best=distance;mediaFocus_=i;}}
    }
}
bool SpotifyPanel::selectMediaAt(int x,int y) {
    for(std::size_t i=0;i<MediaRects.size();++i) {const auto r=MediaRects[i];if(x>=r.x && x<r.x+r.w && y>=r.y && y<r.y+r.h){mediaFocus_=i;return true;}}
    return false;
}
SpotifyArguments SpotifyPanel::seekAt(int x) const {
    if(!local_ || !media_.canSeek || !media_.hasTimeline || x<100 || x>1180)return {};
    const auto position=static_cast<long long>(x-100)*media_.duration/1080;
    return {"soloist-seek",std::to_string(position),"--media"};
}
int SpotifyPanel::positionAt(std::uint32_t now) const {
    const auto elapsed=static_cast<std::uint32_t>(now-mediaAt_);
    if(!media_.hasTimeline || elapsed>20000 || media_.playback=="unknown")return -1;
    const auto advance=playback_=="playing"?static_cast<long long>(elapsed)*media_.speed/1000:0;
    return static_cast<int>(std::min<long long>(media_.duration,media_.position+advance));
}
std::string SpotifyPanel::browserTitle() const {
    if(!browsing_ || !page_)return "MAIS CONTROLES";
    if(page_->page=="queue")return "FILA DE REPRODUCAO";
    if(page_->page=="devices")return "DISPOSITIVOS";
    if(page_->page=="playlists")return "MINHAS PLAYLISTS";
    return "FAIXAS";
}
} // namespace raspberry_tv
