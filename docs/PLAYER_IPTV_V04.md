# Raspberry TV Native v0.4 — primeiro player IPTV embutido (experimental)

**Status:** codigo publicado para compilar e validar no Raspberry Pi; nao representa reproducao 1080p fluida.

## Decisao de operacao
A alimentacao atual do Raspberry Pi 3B+ foi mantida a pedido do proprietario, apesar de `vcgencmd get_throttled` ter indicado `0x50005` (subtensao e throttling ativos). Nao tentar desativar protecao por software, fazer overclock ou forcar repeticao de benchmarks sob subtensao. Preservar fonte, SSH, XFCE, LightDM, rede, Bluetooth, `raspberry-display.service`, e worktree `~/raspberry-tv-fullhd` existente.

## Arquitetura adicionada

- `native/include/tv/player.hpp`: classe `TvPlayer`, isolada da UI e do leitor M3U.
- `native/src/player.cpp`: GStreamer `playbin` e `glimagesink`, com `GstVideoOverlay` usando o identificador X11 da janela SDL. Nao abre shell externo, nao exige root e nao inicia player ate pressionar × num canal.
- `native/src/main.cpp`: converte a selecao de canal e os comandos do DualShock 4 em eventos do player. SDL nao redesenha sobre os quadros do GStreamer enquanto estiver reproduzindo; ao parar, volta ao shell.
- `native/CMakeLists.txt`: dependencias `gstreamer-1.0`, `gstreamer-video-1.0` via pkg-config.
- **Qualidade de imagem:** a UI conserva renderizacao interna 1920x1080. O codec e a taxa de quadros do canal dependem da origem e dos plugins; os atuais gargalos de OpenGL continuam em aberto.

## Comandos do DualShock 4 na v0.4

| Contexto | Botao | Resultado |
|---|---|---|
| Lista TV AO VIVO ou FAVORITOS | × | Inicia o canal selecionado |
| Video | × / Options | Alterna pausa / reproduccao |
| Video | ○ | Para video e devolve controle ao menu |
| Video | PS | Para video e retorna ao inicio |
| Video | L2 / R2 | Tenta trocar para canal anterior / seguinte |
| Video | △ | Marca/desmarca favorito do canal selecionado |
| Menus | D-pad / analogico | Navegar, um comando por acionamento |

URLs M3U devem ser URIs absolutas com esquema `http`, `https`, `rtsp`, `rtmp`, `udp` ou `file`. A app nao imprime URLs de canais nem segredos de tokens. Nao insira listas nao confiaveis.

## Instalacao no Raspberry (branch separada)

```bash
cd ~/raspberry-pi3plus-tv
git fetch origin refs/heads/feature/native-iptv-playback-v04
git worktree add -b feature/native-iptv-playback-v04 ~/raspberry-tv-player FETCH_HEAD
cd ~/raspberry-tv-player

sudo apt install --no-install-recommends \
  libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
  gstreamer1.0-gl gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
  gstreamer1.0-libav

cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j2
ctest --test-dir build/native --output-on-failure
```

Se a compilacao falhar, parar e inspecionar o erro. Nao configurar autostart nem executar como root.

## Smoke test local, preferencialmente leve

Crie uma lista com arquivo H.264 de teste ja presente no Raspberry (evita rede, autenticação e cargas imprevisiveis). Reempacotar para MP4 pode melhorar o reconhecimento sem recodificar:

```bash
ffmpeg -hide_banner -loglevel error -y -framerate 25 \
  -i /tmp/raspberry-tv-test.h264 -c:v copy -movflags +faststart \
  /tmp/raspberry-tv-teste.mp4

cat > /tmp/raspberry-tv-teste.m3u <<'M3U'
#EXTM3U
#EXTINF:-1 group-title="Diagnostico",Teste de video local
file:///tmp/raspberry-tv-teste.mp4
M3U

DISPLAY=:0 ./build/native/raspberry-tv-native \
  --fullscreen --playlist /tmp/raspberry-tv-teste.m3u
```

Se o video ficar lento, pressionar ○ e retornar ao menu. Um video de 5 segundos e suficiente para este teste funcional; ainda nao e um teste de fluidez Full HD. O arquivo `/tmp/raspberry-tv-teste.mp4` e temporario.

## Pendencias / gates

- [x] Branch e modulo GStreamer de player estruturados.
- [x] Mapeamento de comandos de playback no shell SDL.
- [x] Build C++ GStreamer no Raspberry (confirmado pelo usuario: 100% Built target raspberry-tv-native).
- [x] CTest 3/3 (controller_input, menu_navigation_and_m3u, fullhd_native_pixels), 0 falhas em 0,07s, confirmado no Raspberry.
- [ ] Teste funcional com arquivo local, abertura do canal e retorno pelo ○ (nao executado ainda).
- [ ] Confirmar X11 `GstVideoOverlay` sobre a janela SDL com o driver real.
- [ ] Testar fontes de canais M3U com permissao de uso, protocolos e audio.
- [ ] Validar troca de canais e buffering real.
- [ ] Resolver subtensao e perda de quadros antes da aprovacao 1080p30 sustentado.
- [ ] Persistencia de favoritos, EPG, HUD do player e tratamento avancado de reconexao.

## Referencias da API

- GstVideoOverlay: https://gstreamer.freedesktop.org/documentation/video/gstvideooverlay.html
- playbin: https://gstreamer.freedesktop.org/documentation/playback/playbin.html
- glimagesink: https://gstreamer.freedesktop.org/documentation/opengl/glimagesink.html

## Validacao de compilacao em hardware — 2026-11-23

Usuario executou `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`, `cmake --build build/native -j2` e `ctest --test-dir build/native --output-on-failure` em `~/raspberry-tv-player`. O executavel nativo foi construido, 3/3 testes aprovados (`controller_input`, `menu_navigation_and_m3u`, `fullhd_native_pixels`), tempo CTest 0,07 s. As bibliotecas `libgstreamer1.0-dev 1.26.2-2` e `libgstreamer-plugins-base1.0-dev 1.26.2-1+rpt3+deb13u2` estavam previamente instaladas. Esta validacao **nao testa o pipeline de exibicao real**, integrado por GstVideoOverlay/X11, nem o retorno do video ao menu. O primeiro smoke test deve usar arquivo pequeno de 640x360 e M3U `file:///tmp/...`, evitando forcar fluxo Full HD durante a subtensao.

## Correcao do ciclo de vida GStreamer — 2026-11-23

### Relato em hardware

Primeiro teste da v0.4 com `DISPLAY=:0 ./build/native/raspberry-tv-native --fullscreen --playlist /tmp/raspberry-tv-teste.m3u`:

- `Renderizador: opengl`; dimensoes internas/saida `1920x1080`.
- `Playlist carregada: 1 canais`.
- `Controle principal: PS4 Controller (0)`; eventos `confirm`, `up`, `menu`, `menu`, `confirm`.
- Ao encerrar com `Ctrl+C`: `GStreamer-CRITICAL gst_element_set_state: assertion 'GST_IS_ELEMENT (element)' failed` e `gst_object_unref: assertion '((GObject *) object)->ref_count > 0' failed`.
- **O log nao comprova que o video apareceu**, somente funcionamento da interface e do parser.

### Correcao implementada (pendente de reteste no Raspberry)

- O codigo anterior usava `g_object_set(playbin,"video-sink",sink)` seguido de `gst_object_unref(sink)` sem afirmar posse explicita de sua referencia flutuante. Agora tanto o `playbin` quanto o `glimagesink` sao estabilizados por `gst_object_ref_sink()` antes de transferencias de propriedade. O padrao foi conferido com o proprio codigo fonte da implementacao GStreamer.
- Adicionado metodo idempotente `TvPlayer::shutdown()`: `GST_STATE_NULL`, espera limitada de estado, libera `GstBus` e `playbin` **antes** de `SDL_DestroyWindow`, preservando validade do XID.
- Sinais SIGINT/SIGTERM so alteram `sig_atomic_t` e a limpeza ocorre no loop principal; sem chamadas de GStreamer dentro do handler.
- Marcadores seguros no console `Player: abertura solicitada...`, `GStreamer: transicao para PLAYING confirmada`, `GStreamer: fim de fluxo recebido (EOS)`, sem URLs/token.
- Novo teste automatizado `gstreamer_object_lifecycle`: 24 ciclos de criar `playbin` e `fakesink`, atribuir propriedade, mudar estado, desalocar. Qualquer CRITICAL do GLib vira falha de teste.
- Esperado na nova compilacao: **CTest 4/4**, mas ainda nao executado fisicamente. Esse teste de referencias isoladas **nao substitui** smoke test de video na janela SDL/X11.

### Procedimento de reteste

```bash
cd ~/raspberry-tv-player
git fetch origin refs/heads/feature/native-iptv-playback-v04
git merge --ff-only FETCH_HEAD
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j2
ctest --test-dir build/native --output-on-failure
DISPLAY=:0 ./build/native/raspberry-tv-native --fullscreen --playlist /tmp/raspberry-tv-teste.m3u
```

Aperte × para entrar em TV AO VIVO e × novamente para abrir o arquivo MP4. Verifique se o video e exibido; ○ para voltar. `Ctrl+C` apenas depois de concluir o teste para validar a limpeza. Nao alterar LightDM, xrandr, fonte, Bluetooth ou outros servicos.
