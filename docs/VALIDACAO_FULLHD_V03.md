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
- [ ] Compilar v0.3 no Raspberry Pi, rodar CTest 3/3.
- [ ] Validar cabecalho, cards e foco no sinal HDMI 1920x1080 fisico.
- [ ] Executar `scripts/test-h264-fullhd.sh` e verificar EOS em H.264 1080p30.
- [ ] Executar `scripts/test-h264-fullhd.sh --show` em 1080p30 e avaliar a negociacao real de formatos. O teste GL em 640x360 foi concluido sem erros, mas negociou I420 em memoria convencional (nao zero-copy).
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
