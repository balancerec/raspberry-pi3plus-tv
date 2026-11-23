# Validacao da interface nativa v0.2

Data: 2026-11-23. Base da branch: `feature/native-cpp-controller-first`.

## Hardware, baseado no teste do usuario
- [x] Raspberry Pi 3 Model B, DietPi aarch64, X11 `:0`.
- [x] SDL2 abriu o executavel anterior na sessao grafica existente.
- [x] DualShock 4 identificado como PS4 Controller; direcoes e botoes detectados.
- [x] Problema de repeticoes descontroladas da v0.1 corrigido e validado fisicamente na v0.1.1.
- [x] Build e CTest 1/1 da v0.1.1 aprovados no equipamento.

## Escopo da v0.2
- [x] Shell da UI nativa com seis menus, selecao e legenda de comandos.
- [x] Modelo de navegacao independente da UI grafica.
- [x] Parser M3U, grupos e favoritos temporarios.
- [x] Testes locais SDL-independent de entrada e menu: 2/2 passaram.
- [x] Build grafico completo com `SDL2_ttf` no Raspberry; usuario reportou compilacao 100% e CTest 2/2 aprovados.
- [x] Exibicao v0.2 no HDMI confirmada pelo usuario: cabecalho e seis menus identificados (`TV AO VIVO`, `BIBLIOTECA`, `FAVORITOS`, `APLICATIVOS`, `SISTEMA`, `CONFIGURACOES`); navegacao pelo DualShock operacional.
- [ ] Integracao e teste de reproducao real GStreamer/H.264 — **pendentes**.
- [ ] Persistencia dos favoritos, EPG, audio e UX do player — **pendentes**.
- [ ] Teste de DRM/KMS sem Xorg e otimizacao de memoria — **pendente**.

## Requisitos
`libsdl2-ttf-dev`, `fonts-dejavu-core`, `libsdl2-dev`, CMake, GCC e pkg-config.

## Seguranca/operacao
A branch v0.2 nao modifica configuracoes do DietPi, autostart, SSH, rede, Bluetooh, LightDM ou servico Python SPI/NRF. O parse da playlist e local; as URLs sao armazenadas em RAM mas nao exibidas/logadas. A reproduccao nao e disparada nesta versao. Nao executar o aplicativo como root.

## Marco fisico v0.2: confirmado pelo usuario em 2026-11-23

O usuario executou o aplicativo no Raspberry conectado a TV e confirmou visualmente o cabecalho, os seis menus titulados e a navegacao funcional pelo DualShock 4, sem relato de repeticao excessiva apos a correcao v0.1.1.

Observacao sobre rastreabilidade: no mesmo diagnostico a branch local de Git ainda aparecia como `feature/native-cpp-controller-first` / `41cf4f4`, embora o build do workspace incluísse `tv_menu`, `src/ui.cpp` e SDL2_ttf. A origem e o estado efetivo do codigo local devem ser verificados separadamente antes de gerar novos commits a partir do dispositivo. O relato confirma os recursos observados em execucao, nao o SHA do executavel.

### Gate seguinte: decodificacao H.264 local antes de integrar IPTV

- [x] `h264parse` e `v4l2h264dec` funcionaram juntos no pipeline real; plugins reconhecidos.
- [x] H.264 local, 640x360@25, 5s, `filesrc ! h264parse ! v4l2h264dec ! fakesink sync=false`: chegou a EOS normalmente, sem erro, 0,738743333s de execucao do pipeline, `time real` 1,300s.
- [ ] Em caso de sucesso, testar video HDMI, formato de cor, memoria e carga de CPU.
- [ ] So depois integrar pipeline de reproducao e controles de playback ao shell C++.

Nao parar LightDM nem remover a aplicacao Python/SPI/LCD/NRF nesta fase.

## Validacao real de H.264 por V4L2 — 2026-11-23

Usuario executou:

```bash
time gst-launch-1.0 -v -e \\
  filesrc location=/tmp/raspberry-tv-test.h264 ! \\
  h264parse ! \\
  v4l2h264dec ! \\
  fakesink sync=false
```

Resultado observado:

- Arquivo de teste: H.264, 640x360, 25/1 fps, 8-bit, 4:2:0, profile constrained-baseline, level 3.
- Parser: caps de saida `video/x-h264`, `stream-format=byte-stream`, `alignment=au`.
- Decoder: elemento `v4l2h264dec` recebeu essas caps; produziu `video/x-raw(memory:DMABuf)`, `format=DMA_DRM`, `drm-format=YU12`, `colorimetry=bt709`.
- Sink: `fakesink` aceitou DMABuf e pipeline terminou em `EOS`, sem erro.
- Tempo de processamento GStreamer: 0,738743333s. Tempo de relogio do comando completo: 1,300s; `sync=false` descarta temporizacao de apresentacao.

Conclusao precisa: **decodificacao H.264 V4L2 operacional**, com saida DMA-BUF. Este teste nao confirma renderizacao via HDMI, audio, sincronismo A/V, desempenho IPTV continuo nem zero-copy ponta-a-ponta com renderer real.

### Teste seguinte

1. Verificar disponibilidade de `glimagesink` (plugin `gstreamer1.0-gl` em Debian).
2. Com X11/LightDM ainda ativos, testar `DISPLAY=:0 gst-launch-1.0 -e -v filesrc location=/tmp/raspberry-tv-test.h264 ! h264parse ! v4l2h264dec ! glimagesink sync=true`.
3. Verificar imagem real no HDMI, negociacao DMA-BUF/GL e ausencia de erros; capturar logs em falhas para ajustes.
4. Somente apos isso implementar integracao de playback com o aplicativo SDL2 e passar aos testes de IPTV e audio.
