# Raspberry TV Native v0.3 — Full HD nativo

Data: 2026-11-23. Branch: `feature/native-fullhd-v03`. Base: `feature/native-tv-ui-v02`.

## Escopo implementado
- [x] Superficie logica SDL2: 1920 x 1080.
- [x] Janela base: 1920 x 1080 (com `--fullscreen` preserva a resolucao atual da sessao HDMI).
- [x] Coordenadas de layout 1280 x 720 convertidas para pixels 1920 x 1080 antes de desenhar retangulos, bordas e texto.
- [x] Fontes SDL_ttf renderizadas em tamanhos Full HD (78/45/32/24 px) para manter nitidez sem redimensionar glifos 720p.
- [x] Registro no terminal do renderer SDL2 e da resolucao efetiva da saida para diferenciar renderizacao 1080p de sinal HDMI inferior.
- [x] Teste unitario de dimensoes e conversao; total esperado CTest: 3/3.
- [x] Script de diagnostico H.264 1080p30 via V4L2 e opcional `glimagesink`.

## Validacoes ja realizadas em hardware antes desta branch
- [x] Interface da v0.2 no HDMI: cabecalho e seis menus visiveis.
- [x] DualShock 4 funcionando, sem repeticao descontrolada.
- [x] Pipeline H.264 a 640x360, 25 fps, `v4l2h264dec` e saida `DMA_DRM/YU12` ate `fakesink`, EOS normal.

## Gates FHD ainda pendentes
- [ ] Confirmar CTest 3/3 da v0.3 (o log atual demonstra executavel v0.3 funcionando, mas nao inclui saida do CTest desta branch).
- [x] Modo HDMI efetivo: `HDMI-1 connected 1920x1080+0+0`, `1920x1080 60.00*+`; executavel reportou `Renderizador: opengl`, `Renderizacao interna: 1920 x 1080; saida atual: 1920 x 1080`. Ainda falta confirmacao visual explicita da nitidez e de todos os elementos nesta v0.3.
- [x] `scripts/test-h264-fullhd.sh`: 1920x1080@30fps, profile constrained-baseline level 4, `v4l2h264dec` → DMABuf DMA_DRM/YU12 → `fakesink`; EOS normal. Pipeline durou 3,4616s para arquivo de 3s; **nao demonstrou folga de desempenho em tempo real**.
- [x] `scripts/test-h264-fullhd.sh --show`: `v4l2h264dec` → I420 em memoria regular → `glupload` GLMemory/I420 → `glcolorconvert` GLMemory/RGBA → `glimagesink`. EOS normal em 3,8800s, **mas muitos avisos `gst_base_sink_is_too_late`: frames descartados**. Nao foi comprovado zero-copy e nao ha fluidez 1080p30 demonstrada.
- [ ] Medir estabilidade real, temperatura, CPU, memoria e latencia.
- [ ] Integrar stream IPTV e audio no aplicativo C++ (nao implementado neste commit).

## Importante sobre resolucao HDMI
`SDL_RenderSetLogicalSize(1920,1080)` estabelece resolucao interna Full HD, mas caso a sessao esteja em 1280x720, a SDL reduz a imagem para 720p. Identifique o modo atual com `xrandr --current` na sessao X11. Ajuste pela configuracao grafica do XFCE para 1920x1080, se suportado, antes de declarar emissao HDMI em Full HD. O modo de video nao foi alterado automaticamente pelo software.

## Operacao segura
Nao parar LightDM/XFCE, nao remover `raspberry-display.service`, nao alterar rede/Bluetooth/SSH e nao configurar autostart nesta etapa. Para evitar colisao com o workspace antigo `--single-branch`, criar worktree separado a partir do ref desejado: `git fetch origin refs/heads/feature/native-fullhd-v03`; `git worktree add -b feature/native-fullhd-v03 "$HOME/raspberry-tv-fullhd" FETCH_HEAD`. Nao executar app como root.

## Marco: GL/X11 validado a 640x360 (usuario, 2026-11-23)

Com `gstreamer1.0-gl` recem-instalado, o comando a seguir completou normalmente:

```bash
DISPLAY=:0 gst-launch-1.0 -e -v \\
  filesrc location=/tmp/raspberry-tv-test.h264 ! \\
  h264parse ! v4l2h264dec ! glimagesink sync=true
```

- `GstGLDisplayX11` reconhecido;
- fluxo negociado como `video/x-raw, format=I420, 640x360 @25fps` entre V4L2 e `glimagesinkbin`;
- `glupload` converteu o fluxo para `video/x-raw(memory:GLMemory), format=I420`;
- `glcolorconvert` chegou a `GLMemory/RGBA`, `texture-target=2D`;
- `EOS` normal, `Execution ended after 0:00:05.001262706` com `sync=true`.

**Importante:** neste caminho, nao houve caps `video/x-raw(memory:DMABuf)` na saida do decoder; isto nao comprova zero-copy ponta a ponta. O pipeline estabeleceu a rota GL/X11, mas o usuario ainda nao informou explicitamente se enxergou a imagem na televisao. Isso e diferente da validacao anterior com `fakesink`, que negociou DMABuf/YU12.

### Proximo criterio

Registrar `1920x1080,framerate=30/1` no pad de saida do decoder, `EOS` e imagem fisica no HDMI em 1080p; medir consumo de CPU/memoria e aquecimento em reproducoes mais longas antes de afirmar que o sistema sustenta IPTV Full HD.

## Validacao Full HD no Raspberry 3B+ - relato de 2026-11-23

Trecho de comandos e dados observados no equipamento:

- `DISPLAY=:0 ./build/native/raspberry-tv-native --fullscreen` reportou `Renderizador: opengl` e `Renderizacao interna: 1920 x 1080; saida atual: 1920 x 1080`.
- `xrandr --current` confirmou `HDMI-1 connected 1920x1080+0+0` com modo `1920x1080 60.00*+` selecionado.
- `scripts/test-h264-fullhd.sh` gerou H.264 1920x1080@30, baseline level 4.0, 5,6 MiB aproximadamente, 3 segundos. V4L2 negociou `video/x-raw(memory:DMABuf),format=DMA_DRM,drm-format=YU12` e terminou com EOS; `Execution ended after 0:00:03.461609269`; relogio real `0m4,007s`, `user 0m3,853s`, `sys 0m0,289s`.
- `scripts/test-h264-fullhd.sh --show` repetiu a decodificacao `fakesink` e depois executou o player GL: `v4l2h264dec` negociou `video/x-raw,format=I420` sem DMABuf, `glupload` converteu para `GLMemory/I420` e `glcolorconvert` para `GLMemory/RGBA`.
- `glimagesink sync=true`: avisos repetidos do `gst_base_sink_is_too_late` (`There may be a timestamping problem, or this computer is too slow.`) por excesso de buffers atrasados e descartados; EOS depois de `0:00:03.880011613`.

**Conclusao:** a interface 1080p e o modo HDMI 1080p60 estao comprovados. A decodificacao V4L2 1080p30 terminou, mas nao ha prova de sustentacao de 30 fps sem drops. O caminho GL mapeia quadros na memoria comum e transfere para GL; nao foi zero-copy. Nao atribuir exclusivamente a gargalo GPU/CPU, pois timestamps, schedule de sinks e conversoes podem contribuir. Verificacao explicita do usuario sobre imagem visivel e fluidez ainda e necessaria.

### Gate de diagnostico antes de player IPTV
- [ ] Confirmar `gst-inspect-1.0 fpsdisplaysink` e medir contagem de frames exibidos/descartados no GL real.
- [ ] Testar negociacao forcada de `video/x-raw(memory:DMABuf)` com `glimagesink` para saber se o caminho direto existe neste driver; falha de negociacao nao deve derrubar o app.
- [ ] Analisar diferenca entre `sync=true` e `sync=false` sem confundir a eliminacao dos avisos com ganho de FPS.
- [ ] Preferir 1080p30 com quadros entregues e audio sincronizado antes de integrar streams M3U na shell.

### Script de diagnostico adicionado

`bash scripts/diagnose-1080p-performance.sh` executa dois ensaios nao destrutivos a partir do arquivo de teste existente:

- **A.** `v4l2h264dec ! fpsdisplaysink video-sink=glimagesink text-overlay=false sync=true`: relata `last-message`, `rendered`, `dropped` e FPS medidos pelo sink quando disponiveis.
- **B.** Forca caps `video/x-raw(memory:DMABuf),format=DMA_DRM,drm-format=YU12` na fronteira entre V4L2 e GL. A falha e dado de diagnostico, nao deve disparar alteracoes no modo HDMI ou a desativacao do X11.

Arquivos gerados: `~/raspberry-tv-1080p-diagnostics/fps-gl-1080p.log` e `~/raspberry-tv-1080p-diagnostics/dmabuf-gl-1080p.log`. O teste registra erros e contadores sem exibir URLs de IPTV.

## Diagnostico de desempenho enviado pelo usuario — 2026-11-23

O usuario executou `scripts/diagnose-1080p-performance.sh` e obteve:

- `fpsdisplaysink` / `glimagesink`, `sync=true`: ultima amostra de `last-message` = **rendered 3, dropped 29**, apos amostras que iniciaram em rendered 2 e dropped 0. Dos 32 quadros contados ate a ultima linha publicada, 29 descartados (**90,625%**). Isto nao e necessariamente a estatistica final nem inclui todos os 90 quadros do video fonte de 3s.
- O teste A terminou com codigo zero e `Execution ended after 0:00:04.695189686`; sucesso de pipeline nao significa taxa minima sustentada.
- Caps DMABuf explicitamente exigidas antes do `glimagesink`: erro no `h264parse` (`Internal data stream error`, `not-negotiated (-4)`) e status 1. Logo, o caminho diretamente negociado sem copias nao foi estabelecido com este DRM/GL/driver e esta caps restritiva; nao extrapolar impossibilidade absoluta de DMABuf em outras configuracoes.

### Separacao de gargalos: proximo ensaio

`scripts/benchmark-1080p-stages.sh` (branch v0.3) testa quatro pipelines controlados com video de 1080p:

1. `v4l2h264dec ! fpsdisplaysink video-sink=fakesink sync=false`: vazao de saida do decodificador sem OpenGL (teste diagnostico, nao fluidez).
2. `v4l2h264dec ! fpsdisplaysink video-sink=glimagesink sync=false`: custo do caminho OpenGL sem descartar atrasados por temporizacao.
3. `v4l2h264dec ! queue ! fpsdisplaysink video-sink=glimagesink sync=true`: verificar efeito de desacoplar decoder e renderer sem mudar X11.
4. `videotestsrc 1920x1080 I420 ! fpsdisplaysink video-sink=glimagesink sync=true`: estabelecer referencia da apresentacao OpenGL sem o decodificador H.264.

Os resultados sao gravados em `~/raspberry-tv-1080p-diagnostics/01-decoder-fakesink.log`, `02-decoder-gl-nosync.log`, `03-decoder-queue-gl-sync.log` e `04-synthetic-i420-gl-sync.log`. O script nao altera servicos, modo HDMI nem credenciais.

**Pendencia:** nenhum dos 4 pipelines foi executado em hardware ainda. Nao prometer 1080p30 fluido, nem atribuir com certeza o gargalo exclusivamente a CPU ou ao driver.


## Alerta prioritario: subtensao e throttling ativos durante benchmark 2026-11-23

O benchmark `scripts/benchmark-1080p-stages.sh` trouxe um resultado de alimentacao que invalida a comparacao como medida definitiva da capacidade do hardware:
- `vcgencmd get_throttled`: **throttled=0x50005**, no inicio e ao final.
  - Bits atuais 0x1 e 0x4: subtensao **ativa** e throttling **ativo**.
  - Bits historicos 0x10000 e 0x40000: subtensao e throttling ocorreram desde o ultimo boot.
- Temperatura: 41,3 C antes, 44,5 C depois, sem evidencia de limite termico.
- `01-decoder-fakesink`: cerca de 25,9 FPS medidos com `sync=false`, 0 drops, tempo total 3947ms; abaixo da meta 30 FPS.
- `02-decoder-gl-nosync`: ~6,5 FPS, 0 drops com `sync=false` (nao significa reproducao fluida), total 15076ms.
- `03-decoder-queue-gl-sync`: eventos de descarte intenso, fila nao resolveu; contadores alteram entre mensagens, entao nao converter uma linha individual em taxa final do arquivo.
- `04-synthetic-i420-gl-sync`: descarte de quadros sem decoder H264, confirmando que a apresentacao OpenGL tambem demanda investigacao.
- A meta continua sendo 1080p/30fps; a prova de reproduccao sustentada **continua pendente**. Nao afirmar que todo gargalo foi causado pela alimentacao nem declarar impossibilidade definitiva de 1080p.

Prioridade: fonte de alimentacao de boa qualidade **5,1 V / 2,5 A micro-USB**, cabo de baixa resistencia, nao usar USB da TV para alimentar. Ver recomendacao oficial: https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#power-supply e bits documentados em https://www.raspberrypi.com/documentation/computers/os.html#vcgencmd. Sempre desligar corretamente (`sudo poweroff`) antes de trocar fonte/cabo. Nao tentar "desabilitar throttling" via software.

Novo script: `bash scripts/check-power-health.sh` amostra `get_throttled`, temperatura e frequencia ARM em 10 iteracoes e grava `~/raspberry-tv-1080p-diagnostics/power-status.log`. Se houver flag atual durante a medicao retorna codigo 1. Historico 0x50000 pode permanecer mesmo depois que a subtensao cessar, ate o proximo boot; o importante e que bits atuais (mascara 0xF) fiquem em 0, em repouso e em carga.

Repetir os benchmarks de video **somente depois de estabilizar a fonte**, e comparar com 25,9 FPS (decoder) / 6,5 FPS (caminho GL) registrados sob subtensao. Servicos LightDM, SSH, Bluetooth e `raspberry-display.service` permanecem inalterados.
