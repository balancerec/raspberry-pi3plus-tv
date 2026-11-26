#include "tv/ui.hpp"
#include "tv/display.hpp"
#include <sys/sysinfo.h>
#include <array>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <vector>
#include <ctime>
#include <fstream>
#include <string>
namespace raspberry_tv {
namespace {
constexpr SDL_Color White{245,249,252,255};
constexpr SDL_Color Muted{158,179,188,255};
constexpr SDL_Color Mint{86,240,178,255};
constexpr SDL_Color Cyan{76,201,255,255};
constexpr SDL_Color Dark{8,18,25,255};
constexpr SDL_Color Panel{20,43,54,255};
void fill(SDL_Renderer* r, SDL_Rect rect, SDL_Color c) {
    SDL_Rect physical{display::toNative(rect.x),display::toNative(rect.y),
                      display::toNative(rect.w),display::toNative(rect.h)};
    SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
    SDL_RenderFillRect(r,&physical);
}
void border(SDL_Renderer* r, SDL_Rect rect, SDL_Color c) {
    SDL_Rect physical{display::toNative(rect.x),display::toNative(rect.y),
                      display::toNative(rect.w),display::toNative(rect.h)};
    SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
    SDL_RenderDrawRect(r,&physical);
}
std::string clockText() {
    char buf[24]="--:--";
    auto now=std::time(nullptr);
    if (auto* tm=std::localtime(&now)) std::strftime(buf,sizeof(buf),"%H:%M",tm);
    return buf;
}
std::string memoryText() {
    struct sysinfo info{};
    if (sysinfo(&info)!=0) return "Indisponivel";
    const unsigned long long mib = static_cast<unsigned long long>(info.freeram)*info.mem_unit/(1024*1024);
    return std::to_string(mib)+" MiB livres (sysinfo)";
}
std::string temperatureText() {
    std::ifstream f("/sys/class/thermal/thermal_zone0/temp");long v=0;
    return (f>>v) ? std::to_string(v/1000)+" C" : "Indisponivel";
}
void line(SDL_Renderer* r,int x1,int y1,int x2,int y2,SDL_Color color) {
    SDL_SetRenderDrawColor(r,color.r,color.g,color.b,color.a);
    const int ax=display::toNative(x1),ay=display::toNative(y1),bx=display::toNative(x2),by=display::toNative(y2);
    for(int offset=-1;offset<=1;++offset) {
        if(std::abs(bx-ax)>=std::abs(by-ay))SDL_RenderDrawLine(r,ax,ay+offset,bx,by+offset);
        else SDL_RenderDrawLine(r,ax+offset,ay,bx+offset,by);
    }
}
void circle(SDL_Renderer* r,int x,int y,int radius,SDL_Color color,bool solid) {
    SDL_SetRenderDrawColor(r,color.r,color.g,color.b,color.a);
    const int cx=display::toNative(x),cy=display::toNative(y),rad=display::toNative(radius);
    if(solid)for(int row=-rad;row<=rad;++row) {
        const int width=static_cast<int>(std::sqrt(rad*rad-row*row));
        SDL_RenderDrawLine(r,cx-width,cy+row,cx+width,cy+row);
    } else {
        std::array<SDL_Point,65> points{};
        for(std::size_t i=0;i<points.size();++i) {const auto a=static_cast<double>(i)*6.283185307/64;points[i]={cx+static_cast<int>(rad*std::cos(a)),cy+static_cast<int>(rad*std::sin(a))};}
        SDL_RenderDrawLines(r,points.data(),static_cast<int>(points.size()));
    }
}
void triangle(SDL_Renderer* r,int x,int y,int size,bool right,SDL_Color color) {
    SDL_SetRenderDrawColor(r,color.r,color.g,color.b,color.a);
    const int width=display::toNative(size),base=display::toNative(x-size/3),center=display::toNative(y);
    for(int i=0;i<width;++i) {
        const int half=(width-i)/2,px=base+(right?i:-i);
        SDL_RenderDrawLine(r,px,center-half,px,center+half);
    }
}
void musicBeam(SDL_Renderer* r,SDL_Color color) {
    SDL_SetRenderDrawColor(r,color.r,color.g,color.b,color.a);
    const int left=display::toNative(178),top=display::toNative(283),width=display::toNative(50);
    for(int x=0;x<=width;++x) {
        const int y=top-x*display::toNative(11)/width;
        SDL_RenderDrawLine(r,left+x,y,left+x,y+display::toNative(8));
    }
}
std::string mediaTime(int ms) {
    if(ms<0)return "--:--";
    const int seconds=ms/1000;return std::to_string(seconds/60)+":"+(seconds%60<10?"0":"")+std::to_string(seconds%60);
}
}
bool TvUi::init() {
    if (TTF_Init()<0) return false;
    constexpr std::array<const char*,3> fonts{
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf"};
    for (auto* font:fonts) {
        std::ifstream exists(font);
        if (!exists) continue;
        title_=TTF_OpenFont(font,78);
        heading_=TTF_OpenFont(font,45);
        body_=TTF_OpenFont(font,32);
        small_=TTF_OpenFont(font,24);
        if (title_ && heading_ && body_ && small_) {
            TTF_SetFontStyle(title_,TTF_STYLE_BOLD);
            TTF_SetFontStyle(heading_,TTF_STYLE_BOLD);
            return true;
        }
        close();
        TTF_Init();
    }
    return false;
}
void TvUi::close() {
    for(auto& item:cache_)SDL_DestroyTexture(item.second.texture);
    cache_.clear();cacheBytes_=0;
    if(osdRenderer_)SDL_DestroyRenderer(osdRenderer_);
    if(osdWindow_)SDL_DestroyWindow(osdWindow_);
    osdRenderer_=nullptr;osdWindow_=nullptr;osdVisible_=false;
    if(title_) TTF_CloseFont(title_);
    if(heading_) TTF_CloseFont(heading_);
    if(body_) TTF_CloseFont(body_);
    if(small_) TTF_CloseFont(small_);
    title_=heading_=body_=small_=nullptr;
    if(TTF_WasInit()) TTF_Quit();
}
void TvUi::text(SDL_Renderer* r,TTF_Font* font,const std::string& value,
                int x,int y,SDL_Color color,int maxWidth) {
    if(!font || value.empty())return;
    const auto key=std::to_string(reinterpret_cast<std::uintptr_t>(r))+":"+std::to_string(reinterpret_cast<std::uintptr_t>(font))+":"+
        std::to_string(color.r)+":"+std::to_string(color.g)+":"+std::to_string(color.b)+":"+std::to_string(color.a)+":"+std::to_string(maxWidth)+":"+value;
    auto found=cache_.find(key);
    if(found==cache_.end()) {
        const int width=display::toNative(maxWidth);
        std::string label=value;int w=0,h=0;
        if(TTF_SizeUTF8(font,label.c_str(),&w,&h)!=0)return;
        if(w>width) {
            std::vector<std::size_t> boundaries{0};
            for(std::size_t i=0;i<label.size();++i)
                if(i+1==label.size() || (static_cast<unsigned char>(label[i+1])&0xc0)!=0x80)boundaries.push_back(i+1);
            std::size_t low=0,high=boundaries.size()-1;
            while(low<high) {
                const auto mid=(low+high+1)/2;
                const auto prefix=label.substr(0,boundaries[mid])+"...";
                if(TTF_SizeUTF8(font,prefix.c_str(),&w,&h)==0 && w<=width)low=mid;else high=mid-1;
            }
            label=label.substr(0,boundaries[low])+"...";
        }
        SDL_Surface* surface=TTF_RenderUTF8_Blended(font,label.c_str(),color);if(!surface)return;
        Text entry;entry.w=surface->w;entry.h=surface->h;entry.bytes=static_cast<std::size_t>(entry.w)*entry.h*4;
        entry.texture=SDL_CreateTextureFromSurface(r,surface);SDL_FreeSurface(surface);if(!entry.texture)return;
        while(!cache_.empty() && (cache_.size()>=192 || cacheBytes_+entry.bytes>12*1024*1024)) {
            auto oldest=std::min_element(cache_.begin(),cache_.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});
            cacheBytes_-=oldest->second.bytes;SDL_DestroyTexture(oldest->second.texture);cache_.erase(oldest);
        }
        cacheBytes_+=entry.bytes;found=cache_.emplace(key,entry).first;
    }
    found->second.used=++use_;
    SDL_Rect dest{display::toNative(x),display::toNative(y),found->second.w,found->second.h};
    SDL_RenderCopy(r,found->second.texture,nullptr,&dest);
}
bool TvUi::initOsd(SDL_Window* parent) {
    int x=0,y=0,w=1920,h=1080;SDL_GetWindowPosition(parent,&x,&y);SDL_GetWindowSize(parent,&w,&h);
    osdWindow_=SDL_CreateWindow("Raspberry TV - Informacoes",x,y+h-144,w,144,
        SDL_WINDOW_HIDDEN|SDL_WINDOW_BORDERLESS|SDL_WINDOW_ALWAYS_ON_TOP|SDL_WINDOW_SKIP_TASKBAR);
    if(!osdWindow_)return false;
    osdRenderer_=SDL_CreateRenderer(osdWindow_,-1,SDL_RENDERER_ACCELERATED);
    if(!osdRenderer_)osdRenderer_=SDL_CreateRenderer(osdWindow_,-1,SDL_RENDERER_SOFTWARE);
    if(!osdRenderer_){SDL_DestroyWindow(osdWindow_);osdWindow_=nullptr;return false;}
    return SDL_RenderSetLogicalSize(osdRenderer_,1920,144)==0;
}
void TvUi::drawOsd(SDL_Window* parent,const Channel* channel,const PlayerSnapshot& state,bool help,bool visible) {
    if(!osdWindow_ || !osdRenderer_)return;
    if(!visible) {if(osdVisible_)SDL_HideWindow(osdWindow_);osdVisible_=false;return;}
    if(!osdVisible_) {
        int x,y,w,h;SDL_GetWindowPosition(parent,&x,&y);SDL_GetWindowSize(parent,&w,&h);
        SDL_SetWindowPosition(osdWindow_,x,y+h-144);SDL_SetWindowSize(osdWindow_,w,144);
        SDL_ShowWindow(osdWindow_);SDL_RaiseWindow(osdWindow_);SDL_SetWindowInputFocus(parent);osdVisible_=true;
    }
    fill(osdRenderer_,{0,0,1280,96},Dark);
    const std::string label=channel?channel->name+" | "+channel->group:"TV AO VIVO";
    text(osdRenderer_,body_,label,20,5,White,900);
    text(osdRenderer_,small_,std::string(playbackStateName(state.state))+" "+std::to_string(state.buffering)+"%",940,12,Mint,320);
    const auto audio=state.muted?"MUDO":"Volume "+std::to_string(static_cast<int>(state.volume*100+0.5))+"%";
    const auto audioInfo=state.audioStreams==0?" | Sem trilha de audio":"";
    text(osdRenderer_,small_,audio+audioInfo+" | "+state.status,20,38,Mint,1230);
    text(osdRenderer_,small_,help?"X Pausa | O Para/cancela | PS Inicio | L2/R2 Canais | Cima/Baixo Volume | Quadrado Mudo | Triangulo Favorito":"Options: controles | O: parar/cancelar",20,68,Muted,1230);
    SDL_RenderPresent(osdRenderer_);
}
void TvUi::panel(SDL_Renderer* r,SDL_Rect rect,SDL_Color color,bool selected) {
    fill(r,rect,color);
    if(selected) {
        border(r,{rect.x-3,rect.y-3,rect.w+6,rect.h+6},Mint);
        border(r,{rect.x-5,rect.y-5,rect.w+10,rect.h+10},{86,240,178,85});
    } else border(r,rect,{48,77,88,255});
}
void TvUi::header(SDL_Renderer* r,bool connected) {
    fill(r,{0,0,1280,105},{10,25,31,255});
    fill(r,{52,23,60,60},Mint);
    text(r,heading_,"R",70,33,Dark,40);
    text(r,heading_,"RASPBERRY TV",132,22,White,520);
    text(r,small_,"NATIVE FULL HD / C++ SDL2",134,69,Muted,500);
    panel(r,{885,31,230,44},{20,46,53,255});
    text(r,small_,connected?"PS4 CONECTADO":"SEM CONTROLE",902,43,connected?Mint:Muted,200);
    text(r,heading_,clockText(),1130,30,White,120);
    fill(r,{0,103,1280,2},{42,98,97,255});
}
void TvUi::home(SDL_Renderer* r,const TvModel& model) {
    panel(r,{53,133,1175,225},{18,44,56,255});
    fill(r,{53,133,6,225},Mint);
    text(r,small_,"SUA CENTRAL MULTIMIDIA",94,158,Mint);
    text(r,title_,"ENTRETENIMENTO",87,192,White,870);
    text(r,heading_,"NA SUA TELEVISAO",94,258,Mint,740);
    text(r,body_,"Tudo pelo controle. Sem navegador pesado.",94,311,Muted,840);
    fill(r,{1057,184,110,110},{29,79,86,255});
    border(r,{1037,163,152,152},Cyan);
    text(r,heading_,"TV",1088,212,Mint,85);
    struct Tile {const char* name;const char* detail;SDL_Color color;};
    constexpr Tile cards[]={
        {"TV AO VIVO","Listas M3U e canais",{22,71,70,255}},
        {"BIBLIOTECA","Midias locais",{37,55,88,255}},
        {"FAVORITOS","Seus canais",{65,43,77,255}},
        {"APLICATIVOS","Servicos disponiveis",{69,43,48,255}},
        {"SISTEMA","Recursos e hardware",{30,61,66,255}},
        {"CONFIGURACOES","Controle e exibicao",{58,54,38,255}}};
    for(std::size_t i=0;i<6;++i) {
        int x=53+static_cast<int>(i%3)*400;
        int y=389+static_cast<int>(i/3)*121;
        panel(r,{x,y,375,103},cards[i].color,model.selectedHome()==i);
        fill(r,{x+17,y+19,5,65},model.selectedHome()==i?Mint:Cyan);
        text(r,body_,cards[i].name,x+39,y+17,White,312);
        text(r,small_,cards[i].detail,x+41,y+61,Muted,305);
    }
}
void TvUi::content(SDL_Renderer* r,const TvModel& model) {
    if(model.view()==View::Apps && spotify_){spotifyContent(r,model);return;}
    text(r,title_,TvModel::viewTitle(model.view()),59,134,White,1090);
    const bool listing=model.view()==View::Channels || model.view()==View::Favorites;
    std::string sub=listing ? std::to_string(model.visibleCount())+" canais" :
                 "CONTROLE PS4 / NAVEGACAO NATIVA";
    if(listing && model.visibleCount())
        sub+="   •   "+std::to_string(model.selectedRow()+1)+" / "+std::to_string(model.visibleCount());
    if(model.view()==View::Channels) {
        std::string group=model.activeGroup();
        sub+="   |   GRUPO: "+group+"  (L1/R1)";
    }
    if(model.view()==View::Apps && spotify_)sub=spotify_->nowPlaying();
    text(r,body_,sub,64,214,Mint,1080);
    panel(r,{60,263,1160,358},Panel);
    if(listing) {
        if(!model.visibleCount()) {
            text(r,heading_,model.view()==View::Channels?
                model.channels().empty() ? "NENHUMA PLAYLIST CARREGADA" : "GRUPO SEM CANAIS" : "SEM FAVORITOS",92,341,White,1000);
            text(r,body_,model.view()==View::Channels?
                "Inicie com --playlist /caminho/Playlist_VLC.m3u":
                "Em TV AO VIVO, pressione TRIANGULO para favoritar.",92,393,Muted,980);
            return;
        }
        const auto first=(model.selectedRow()/8)*8;
        for(auto row=first;row<model.visibleCount() && row<first+8;++row) {
            const auto idx=model.channelIndexAt(row);
            if(idx>=model.channels().size()) continue;
            const Channel& ch=model.channels()[idx];
            const int y=276+static_cast<int>(row-first)*41;
            if(row==model.selectedRow()) panel(r,{76,y,1127,39},{33,81,76,255},true);
            text(r,small_,std::to_string(row+1),90,y+10,Mint,68);
            text(r,body_,ch.name,160,y+4,White,640);
            text(r,small_,ch.group,846,y+10,Muted,220);
            if(model.hasFavorite(idx)) text(r,small_,"*",1152,y+8,Mint,24);
        }
        return;
    }
    if(model.view()==View::Apps && spotify_) {
        text(r,small_,spotifyBusy_?"Spotify: aguardando resposta... O cancela e volta.":spotify_->message(),82,275,Mint,1110);
        const auto& rows=spotify_->rows();
        const auto first=(model.selectedRow()/6)*6;
        for(auto row=first;row<rows.size() && row<first+6;++row) {
            const int y=316+static_cast<int>(row-first)*44;
            if(row==model.selectedRow())panel(r,{77,y,1128,40},{33,81,76,255},true);
            text(r,body_,rows[row].title,94,y+5,rows[row].enabled?White:Muted,740);
            text(r,small_,rows[row].detail,854,y+10,Muted,330);
        }
        text(r,small_,std::to_string(model.selectedRow()+1)+" / "+std::to_string(rows.size())+"   Direita/esquerda: pagina   Quadrado: faixas da playlist",94,593,Muted,1090);
    } else if(model.view()==View::System) {
        text(r,heading_,"STATUS DO DISPOSITIVO",94,299,White);
        text(r,body_,"Memoria: "+memoryText(),96,363,Mint);
        text(r,body_,"Plataforma de destino: Raspberry Pi / ARM64",96,406,White);
        text(r,body_,"Temperatura: "+temperatureText(),96,448,White);
        text(r,body_,"Use o diagnostico para verificar plugins e alimentacao.",96,491,Muted);
    } else if(model.view()==View::Settings) {
        const std::string rows[]={
            std::string("Tela cheia: ")+(settings_.fullscreen?"SIM":"NAO"),
            std::string("Informacoes no video: ")+(settings_.showOsd?"SIM":"NAO"),
            std::string("Lembrar selecao (sem autoplay): ")+(settings_.resumeSelection?"SIM":"NAO"),
            "Recarregar playlist local", "Encerrar Raspberry TV"};
        for(std::size_t i=0;i<5;++i) {
            int y=288+static_cast<int>(i)*55;
            if(model.selectedRow()==i)panel(r,{78,y,1120,47},{33,81,76,255},true);
            text(r,body_,rows[i],96,y+7,i==model.selectedRow()?Mint:White,1080);
        }

    } else {
        text(r,heading_,"FORA DO ESCOPO DA RELEASE 1.0",94,347,White);
        text(r,body_,"Use TV AO VIVO ou FAVORITOS para reproduzir.",96,410,Mint);
        text(r,body_,"Este modulo nao oferece servicos nesta release.",96,457,Muted);
    }
}
void TvUi::spotifyContent(SDL_Renderer* r,const TvModel& model) {
    if(!spotify_)return;
    if(!spotify_->mediaMode()) {
        text(r,heading_,spotify_->browserTitle(),64,135,White,1100);
        text(r,small_,spotifyBusy_?"Aguarde... O cancela.":spotify_->message(),64,193,Mint,1150);
        panel(r,{60,245,1160,377},Panel);
        const auto& rows=spotify_->rows();
        const auto start=spotify_->browsing()?SpotifyPanel::ControlRows:0;
        const auto selected=std::max(start,model.selectedRow());
        const auto first=start+(selected-start)/6*6;
        if(rows.size()<=start)text(r,body_,"Nenhum item disponivel.",94,320,Muted,1000);
        for(auto row=first;row<rows.size() && row<first+6;++row) {
            const int y=266+static_cast<int>(row-first)*49;
            if(row==selected)panel(r,{77,y,1128,45},{33,81,76,255},true);
            text(r,body_,rows[row].title,94,y+6,rows[row].enabled?White:Muted,730);
            text(r,small_,rows[row].detail,850,y+13,Muted,340);
        }
        text(r,small_,"O volta ao player  |  Esquerda/direita: pagina  |  Quadrado: faixas",94,590,Muted,1100);
        return;
    }
    const auto& data=spotify_->media();
    text(r,heading_,"SPOTIFY",64,129,Mint,600);
    text(r,small_,spotifyBusy_?"Atualizando...":spotify_->message(),64,176,Muted,1140);
    panel(r,{60,211,1160,336},Panel);
    fill(r,{95,240,180,180},{11,30,35,255});border(r,{95,240,180,180},{39,96,85,255});
    // A neutral music symbol is used when artwork is not supplied; no fake cover.
    fill(r,{178,283,8,82},Mint);fill(r,{221,272,8,79},Mint);
    musicBeam(r,Mint);
    circle(r,166,365,15,Mint,true);circle(r,209,351,15,Mint,true);
    text(r,heading_,data.track.empty()?"Escolha uma musica":data.track,318,235,White,850);
    text(r,body_,data.artist.empty()?"Selecione Raspberry TV no Spotify":data.artist,320,280,Muted,850);
    const bool elsewhere=spotify_->local() && data.hasFeatures && !data.active;
    const bool playing=data.playback=="playing" && !elsewhere;
    const std::string state=elsewhere?"EM OUTRO DISPOSITIVO":playing?"REPRODUZINDO":data.playback=="paused"?"PAUSADO":data.playback=="buffering"?"CARREGANDO":data.playback=="idle"?"PRONTO PARA OUVIR":"AGUARDANDO CONEXAO";
    text(r,small_,state,320,324,playing?Mint:Muted,510);
    text(r,small_,data.volume<0?"VOLUME --":"VOLUME "+std::to_string(data.volume)+"%",850,350,Muted,300);
    for(std::size_t i=0;i<SpotifyPanel::MediaRects.size();++i) {
        const auto box=SpotifyPanel::MediaRects[i];const bool selected=spotify_->mediaFocus()==i;
        const bool enabled=spotify_->controlEnabled(i);const auto ink=enabled?(selected?Dark:White):Muted;
        if(i>=8) {
            panel(r,{box.x,box.y,box.w,box.h},selected?SDL_Color{33,81,76,255}:Panel,selected);
            text(r,small_,spotify_->controlLabel(i),box.x+22,box.y+17,enabled?White:Muted,box.w-35);
            continue;
        }
        const int x=box.x+box.w/2,y=box.y+box.h/2;
        circle(r,x,y,box.w/2-2,selected?Mint:SDL_Color{26,58,64,255},true);
        if(selected)circle(r,x,y,box.w/2+3,Mint,false);
        if(i==2) {
            if(playing || (!elsewhere && data.playback=="buffering")) {fill(r,{x-13,y-18,9,36},ink);fill(r,{x+5,y-18,9,36},ink);}
            else triangle(r,x,y,38,true,ink);
        } else if(i==1 || i==3) {
            triangle(r,x+(i==1?26:-6),y,28,i==3,ink);
            fill(r,{x+(i==1?-16:16),y-15,4,30},ink);
        } else if(i==6 || i==7) {
            fill(r,{x-11,y-2,22,4},ink);if(i==7)fill(r,{x-2,y-11,4,22},ink);
        } else if(i==5) {
            fill(r,{x-14,y-6,8,12},ink);fill(r,{x-6,y-10,5,20},ink);fill(r,{x-1,y-14,4,28},ink);
            if(data.volume==0){line(r,x+6,y-8,x+17,y+8,ink);line(r,x+6,y+8,x+17,y-8,ink);}
            else {line(r,x+7,y-8,x+13,y-3,ink);line(r,x+13,y-3,x+13,y+3,ink);line(r,x+13,y+3,x+7,y+8,ink);}
        } else if(i==0) {
            line(r,x-15,y-9,x-7,y-9,ink);line(r,x-7,y-9,x+8,y+9,ink);line(r,x+8,y+9,x+15,y+9,ink);
            line(r,x-15,y+9,x-7,y+9,ink);line(r,x-7,y+9,x+8,y-9,ink);line(r,x+8,y-9,x+15,y-9,ink);
            line(r,x+11,y-13,x+16,y-9,ink);line(r,x+11,y-5,x+16,y-9,ink);
            line(r,x+11,y+5,x+16,y+9,ink);line(r,x+11,y+13,x+16,y+9,ink);
            if(data.shuffle==1)circle(r,x,y+23,2,selected?Dark:Mint,true);
        } else if(i==4) {
            line(r,x-15,y-9,x+14,y-9,ink);line(r,x+14,y-9,x+8,y-15,ink);line(r,x+14,y-9,x+8,y-3,ink);
            line(r,x-15,y-9,x-19,y-5,ink);line(r,x-19,y-5,x-19,y+2,ink);
            line(r,x+15,y+9,x-14,y+9,ink);line(r,x-14,y+9,x-8,y+3,ink);line(r,x-14,y+9,x-8,y+15,ink);
            line(r,x+15,y+9,x+19,y+5,ink);line(r,x+19,y+5,x+19,y-2,ink);
            if(data.repeat=="track")text(r,small_,"1",x-4,y-7,ink,15);
            if(data.repeat!="off" && data.repeat!="unknown")circle(r,x,y+23,2,selected?Dark:Mint,true);
        }
    }
    const auto position=spotify_->positionAt(SDL_GetTicks());
    fill(r,{100,499,1080,5},{51,74,81,255});
    if(position>=0 && data.duration>0) {
        const int width=static_cast<int>(static_cast<long long>(position)*1080/data.duration);
        if(width)fill(r,{100,499,width,5},Mint);
        circle(r,100+width,501,4,Mint,true);
    }
    text(r,small_,mediaTime(position),100,515,Muted,120);
    text(r,small_,mediaTime(data.duration),1110,515,Muted,90);
    text(r,small_,spotify_->controlLabel(spotify_->mediaFocus())+(spotify_->controlEnabled(spotify_->mediaFocus())?"":" - indisponivel nesta faixa"),490,621,Muted,700);
}
void TvUi::overlay(SDL_Renderer* r,const TvModel& model) {
    if(!model.showHelp() && !model.showDetails()) return;
    fill(r,{0,0,1280,720},{0,0,0,190});
    panel(r,{212,165,856,385},{18,41,51,255});
    if(model.showHelp()) {
        if(model.view()==View::Apps) {
            text(r,heading_,"SPOTIFY - CONTROLES",250,202,Mint,750);
            text(r,body_,"D-PAD Selecionar    X Executar    O Voltar",250,264,White,760);
            text(r,small_,"L1/R1: anterior/proxima  |  Quadrado: mudo",250,320,White,760);
            text(r,small_,"Triangulo: fila  |  L2/R2: recuar/avancar 10 s",250,356,White,760);
            text(r,small_,"Espaco: pausa/retoma  |  Mouse: botoes e progresso",250,392,White,760);
            text(r,small_,"O/PS cancela a espera; pausar e uma acao separada",250,429,White,760);
            text(r,small_,"O ou OPTIONS fecha a ajuda",252,496,Muted,760);
            return;
        }
        text(r,heading_,"CONTROLE PS4 - COMANDOS",250,202,Mint,750);
        text(r,body_,"D-PAD Navegar    X Reproduzir    O Voltar",250,264,White,760);
        text(r,small_,"L1/R1 Filtrar grupos na lista de TV ao vivo",250,320,White,760);
        text(r,small_,"L2/R2 Trocar canais    TRIANGULO Favoritos",250,356,White,760);
        text(r,small_,"No video: CIMA/BAIXO Volume  QUADRADO Mudo",250,392,White,760);
        text(r,small_,"No video: X Pausa    PS Inicio    O Parar",250,429,White,760);
        text(r,small_,"O ou OPTIONS para fechar",252,496,Muted);
    } else {
        const Channel* ch=model.selectedChannel();
        text(r,heading_,"INFORMACOES DO CANAL",250,207,Mint);
        if(ch) {
            text(r,heading_,ch->name,250,287,White,720);
            text(r,body_,"Grupo: "+ch->group,250,346,Muted,700);
            text(r,small_,"URL nao exibida para proteger dados da playlist",250,406,Muted,700);
        }
        text(r,small_,"Pressione O para voltar",250,498,Mint);
    }
}
void TvUi::draw(SDL_Renderer* r,const TvModel& model,bool controllerConnected) {
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    for(int y=0;y<720;y+=8)
        fill(r,{0,y,1280,8},{7,static_cast<Uint8>(16+y/95),static_cast<Uint8>(22+y/110),255});
    header(r,controllerConnected);
    if(model.view()==View::Home) home(r,model);else content(r,model);
    fill(r,{0,654,1280,66},{6,18,23,245});
    fill(r,{53,654,1176,1},{39,76,80,255});
    text(r,small_,model.view()==View::Apps ? "X CONFIRMAR  O VOLTAR  L1/R1 FAIXAS  QUADRADO MUDO  OPTIONS MAIS" : model.view()==View::Channels ?
         "X ABRIR   L1/R1 GRUPOS   L2/R2 CANAIS   O VOLTAR" :
         "X ABRIR   O VOLTAR   PS INICIO   OPTIONS AJUDA",54,673,Muted,820);
    text(r,small_,model.message(),848,673,Mint,371);
    overlay(r,model);
    SDL_RenderPresent(r);
}
} // namespace raspberry_tv
