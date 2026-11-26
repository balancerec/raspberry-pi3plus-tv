#include "tv/spotify.hpp"
#include "tv/playlist.hpp"
#include <algorithm>
#include <charconv>
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
        if(!status || (!reply.entries.empty() && !reply.hasPage))return invalid();
        return reply;
    } catch(const std::exception&) {return invalid();}
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
    local_=local;page_.reset();confirming_=false;playback_="unknown";
    nowPlaying_=local?"Spotify Soloist - dispositivo Raspberry TV":"Spotify Web API - dispositivo da conta";
    message_=local?"Inicie Soloist no terminal e selecione Raspberry TV no Spotify.":"Playlists e dispositivos: configure Client ID e login OAuth (opcional).";
    rebuild();
}
void SpotifyPanel::requestStarted(const std::string& command) {
    static const std::vector<std::string> commands{
        "soloist-status","soloist-play","soloist-resume","soloist-toggle","soloist-pause",
        "soloist-next","soloist-previous","soloist-volume-up","soloist-volume-down",
        "soloist-activate","soloist-deactivate","status","play","resume","pause",
        "next","previous","volume-up","volume-down","transfer"};
    if(std::find(commands.begin(),commands.end(),command)==commands.end())return;
    confirming_=true;nowPlaying_="Atualizando estado do Spotify...";
}
void SpotifyPanel::requestCancelled() {
    if(!confirming_)return;
    confirming_=false;playback_="unknown";
    nowPlaying_="Estado do Spotify sem confirmacao; atualize";
    message_="Espera cancelada. Um comando ja aceito pelo Spotify pode ter sido executado.";
    rebuild();
}
void SpotifyPanel::accept(const SpotifyReply& reply) {
    message_=reply.message;
    if(confirming_ && !reply.hasPlayback) {playback_="unknown";nowPlaying_="Estado do Spotify sem confirmacao; atualize";}
    confirming_=false;
    if(reply.code=="auth" || reply.code=="setup") {page_.reset();playback_="unknown";nowPlaying_="Spotify Connect - conta desconectada";}
    if(reply.hasPlayback) {
        playback_=reply.playback;
        nowPlaying_=reply.playback=="playing"?"Tocando: ":reply.playback=="paused"?"Pausado: ":reply.playback=="buffering"?"Buffering: ":"Sem reproducao ativa";
        if(!reply.track.empty())nowPlaying_+=(reply.playback=="idle"?" | ":"")+reply.track;
        if(!reply.artist.empty())nowPlaying_+=" / "+reply.artist;
        if(!reply.device.empty())nowPlaying_+=" | "+reply.device;
        if(reply.volume>=0)nowPlaying_+=" | "+std::to_string(reply.volume)+"%";
    }
    if(reply.hasPage && reply.code=="ready")page_=reply;
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
        {playback_=="paused"?"Retomar Spotify local":
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
} // namespace raspberry_tv
