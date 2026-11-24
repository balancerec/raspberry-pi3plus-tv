# Raspberry TV Native v0.5 — Canais IPTV e navegacao por grupos

A nova branch `feature/native-iptv-channels-v05` conserva a interface nativa Full HD (1920x1080) e o player GStreamer validado visualmente na v0.4. Agora suporta:

- `--check-playlist CAMINHO_M3U` para carregar e contar canais e grupos sem abrir a TV.
- `--playlist CAMINHO_M3U` para abrir lista M3U local (incluindo enderecos de stream de rede).
- `L1/R1` alternam os grupos na lista TV AO VIVO; `L2/R2` trocam canais durante playback e navegacao.
- `D-pad cima/baixo` altera volume enquanto o video roda; `Quadrado` ativa/desativa mudo; `Circulo` para e retorna ao menu.
- Teste de regressao `iptv_group_and_channel_navigation` adicionado ao CTest (esperado 5/5 no Raspberry).

Para preparar ambiente em worktree separado e executar:

```bash
cd ~/raspberry-pi3plus-tv
git fetch origin refs/heads/feature/native-iptv-channels-v05
git worktree add -b feature/native-iptv-channels-v05 ~/raspberry-tv-iptv FETCH_HEAD
cd ~/raspberry-tv-iptv
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j2
ctest --test-dir build/native --output-on-failure
./build/native/raspberry-tv-native --check-playlist "$HOME/Playlist_VLC.m3u"
DISPLAY=:0 ./build/native/raspberry-tv-native --fullscreen --playlist "$HOME/Playlist_VLC.m3u"
```

Os dois ultimos comandos exigem que a playlist ja exista em `~/Playlist_VLC.m3u`. Nunca envie tokens ou URLs privadas de canais para logs/commits. O desempenho 1080p30 sustentado permanece sem comprovacao, com `throttled=0x50005` registrado anteriormente. Preserve X11, Bluetooth, rede e servicos do DietPi.

**Plano e checklist:** `docs/VALIDACAO_IPTV_V05.md`.

---

# Raspberry TV Native v0.3 — Full HD 1920x1080

Esta branch renderiza a UI **nativamente em 1920x1080**, incluindo textos do SDL_ttf em alta resolucao. Mantem a navegacao DualShock 4 validada na v0.2 e a lista de canais M3U. A resolucao fisica do HDMI depende do modo selecionado no XFCE.

## Validar no Raspberry Pi

Como o clone inicial usou `--single-branch`, prefira trabalhar num worktree proprio, preservando o projeto existente:

```bash
cd ~/raspberry-pi3plus-tv
git fetch origin refs/heads/feature/native-fullhd-v03
git worktree add -b feature/native-fullhd-v03 ~/raspberry-tv-fullhd FETCH_HEAD
cd ~/raspberry-tv-fullhd
sudo apt install --no-install-recommends build-essential cmake pkg-config libsdl2-dev libsdl2-ttf-dev fonts-dejavu-core
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j2
ctest --test-dir build/native --output-on-failure
DISPLAY=:0 ./build/native/raspberry-tv-native --fullscreen
```

Verifique o modo real do HDMI **em outra sessao SSH**, com a TV logada:

```bash
DISPLAY=:0 xrandr --current
```

Se a TV estiver em 1280x720, a interface e renderizada internamente em 1080p mas reduzida na apresentacao. Ajuste a resolucao para 1920x1080 pelas configuracoes de exibicao do XFCE quando a opcao estiver disponivel.

Para testar **video H.264 Full HD 1080p30** usando V4L2:

```bash
bash scripts/test-h264-fullhd.sh
```

Para testar a exibicao na TV (requer `glimagesink`/gstreamer1.0-gl):

```bash
DISPLAY=:0 bash scripts/test-h264-fullhd.sh --show
```

**Ainda nao ha playback de canais no aplicativo nativo.** Esta versao atualiza a interface para Full HD e disponibiliza a prova de video separada. Nao e uma declaracao de que reproduz qualquer IPTV em 1080p. Nao desligue LightDM, SSH ou servicos Python.

---

## Historico: v0.2

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
