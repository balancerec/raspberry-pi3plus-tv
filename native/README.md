# Raspberry TV Native Edition — v0.2

Nova interface nativa C++17 + SDL2 + SDL_ttf, construida sobre o prototipo de DualShock 4 da v0.1.1. **Controle PS4 e a entrada principal.** Nenhum browser, runtime Python ou servidor HTTP e necessario para a UI. A versao web original segue na main.

## Implementado
- Interface de sala ("10-foot UI"), 1280x720 logicos, seis menus legiveis e foco visivel;
- D-pad/analogico, X confirmar, O voltar, PS inicio, Options ajuda, L1/R1 secao, L2/R2 canal, triangulo favoritos e quadrado detalhes;
- Parser de arquivo M3U local via `--playlist /caminho/lista.m3u`, incluindo metadados `#EXTINF`, BOM UTF-8 e grupos;
- Lista paginada por 8 canais, favoritos temporarios, sobreposicao de detalhes;
- Tela de status basico (temperatura e RAM livre); redesenho apenas quando muda estado ou minuto do relogio;
- Testes unitarios de controlador e navegacao / M3U.

**Limites:** O player de video IPTV *ainda nao foi integrado*. Apertar X num canal nao inicia a transmissao. Favoritos ainda nao sao persistidos em disco. Itens Biblioteca / Apps sao telas em preparacao. O app ainda usa X11 por padrao: nao desligue LightDM antes de validar a tela real.

## Instalar dependencias
```bash
sudo apt install --no-install-recommends build-essential cmake pkg-config libsdl2-dev libsdl2-ttf-dev fonts-dejavu-core
```

## Compilar e testar
```bash
cd ~/raspberry-pi3plus-tv
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j2
ctest --test-dir build/native --output-on-failure
DISPLAY=:0 ./build/native/raspberry-tv-native
```
Com playlist ja copiada para o Raspberry:
```bash
DISPLAY=:0 ./build/native/raspberry-tv-native --playlist /home/balancerec/Playlist_VLC.m3u
```
Para TV em tela cheia: adicionar `--fullscreen`.

Para testar somente a logica (sem dependencia SDL):
```bash
cmake -S native -B build/native-testes -DBUILD_NATIVE_APP=OFF
cmake --build build/native-testes -j2
ctest --test-dir build/native-testes --output-on-failure
```

Nao usar `sudo` para executar a interface; autentique no XFCE e utilize a sessao X11 corretamente. `Ctrl+C` encerra o aplicativo. O firmware H.264 V4L2 esta habilitado no DietPi, mas o motor de reproducao sera integrado na proxima etapa.
