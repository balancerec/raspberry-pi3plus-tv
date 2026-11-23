#include "tv/ui.hpp"
#include "tv/display.hpp"
#include <sys/sysinfo.h>
#include <array>
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
    if(title_) TTF_CloseFont(title_);
    if(heading_) TTF_CloseFont(heading_);
    if(body_) TTF_CloseFont(body_);
    if(small_) TTF_CloseFont(small_);
    title_=heading_=body_=small_=nullptr;
    if(TTF_WasInit()) TTF_Quit();
}
void TvUi::text(SDL_Renderer* r,TTF_Font* font,const std::string& value,
                int x,int y,SDL_Color color,int maxWidth) {
    if(!font || value.empty()) return;
    SDL_Surface* surface=TTF_RenderUTF8_Blended(font,value.c_str(),color);
    if(!surface) return;
    SDL_Texture* texture=SDL_CreateTextureFromSurface(r,surface);
    if(texture) {
        const int physicalMaxWidth = display::toNative(maxWidth);
        const double scale=surface->w>physicalMaxWidth ?
            static_cast<double>(physicalMaxWidth)/surface->w : 1.0;
        SDL_Rect dest{display::toNative(x),display::toNative(y),
                      static_cast<int>(surface->w*scale),static_cast<int>(surface->h*scale)};
        SDL_RenderCopy(r,texture,nullptr,&dest);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
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
    text(r,title_,TvModel::viewTitle(model.view()),59,134,White,1090);
    const bool listing=model.view()==View::Channels || model.view()==View::Favorites;
    std::string sub=listing ? std::to_string(model.visibleCount())+" canais" :
                 "CONTROLE PS4 / NAVEGACAO NATIVA";
    if(listing && model.visibleCount())
        sub+="   •   "+std::to_string(model.selectedRow()+1)+" / "+std::to_string(model.visibleCount());
    text(r,body_,sub,64,214,Mint,1080);
    panel(r,{60,263,1160,358},Panel);
    if(listing) {
        if(!model.visibleCount()) {
            text(r,heading_,model.view()==View::Channels?
                "NENHUMA PLAYLIST CARREGADA":"SEM FAVORITOS",92,341,White,1000);
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
    if(model.view()==View::System) {
        text(r,heading_,"STATUS DO DISPOSITIVO",94,299,White);
        text(r,body_,"Memoria: "+memoryText(),96,363,Mint);
        text(r,body_,"Arquitetura: Raspberry Pi / ARM64",96,406,White);
        text(r,body_,"Temperatura: "+temperatureText(),96,448,White);
        text(r,body_,"H.264 V4L2: dispositivo esperado em /dev/video10",96,491,Muted);
    } else if(model.view()==View::Settings) {
        text(r,heading_,"CONTROLE COMO NAVEGACAO PADRAO",94,300,White);
        text(r,body_,"PS4 por USB / Bluetooth; teclado como alternativa",96,362,Mint);
        text(r,body_,"D-pad: um movimento por pressionamento",96,405,White);
        text(r,body_,"Options: ajuda   PS: inicio   O: voltar",96,448,Muted);
        text(r,body_,"Esta interface nao altera as configuracoes do DietPi.",96,491,Muted);
    } else {
        text(r,heading_,"MODULO EM PREPARACAO",94,347,White);
        text(r,body_,"Os menus ja estao integrados ao motor C++.",96,410,Mint);
        text(r,body_,"A proxima etapa conecta os servicos e o player.",96,457,Muted);
    }
}
void TvUi::overlay(SDL_Renderer* r,const TvModel& model) {
    if(!model.showHelp() && !model.showDetails()) return;
    fill(r,{0,0,1280,720},{0,0,0,190});
    panel(r,{212,165,856,385},{18,41,51,255});
    if(model.showHelp()) {
        text(r,heading_,"CONTROLE PS4 - COMANDOS",250,202,Mint,750);
        text(r,body_,"D-PAD / ANALOGICO     Navegar",250,274,White);
        text(r,body_,"X Confirmar    O Voltar    PS Inicio",250,326,White);
        text(r,body_,"L1/R1 Categorias    L2/R2 Canais",250,378,White);
        text(r,body_,"TRIANGULO Favorito    QUADRADO Detalhes",250,429,White);
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
    text(r,small_,"X ABRIR   O VOLTAR   PS INICIO   OPTIONS AJUDA",54,673,Muted,820);
    text(r,small_,model.message(),848,673,Mint,371);
    overlay(r,model);
    SDL_RenderPresent(r);
}
} // namespace raspberry_tv
