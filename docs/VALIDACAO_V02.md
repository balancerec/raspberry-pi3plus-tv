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
- [ ] Build grafico completo v0.2 no Raspberry — **aguardando validacao fisica**.
- [ ] Exibicao v0.2 em HDMI — **aguardando validacao**.
- [ ] Integracao e teste de reproducao real GStreamer/H.264 — **pendentes**.
- [ ] Persistencia dos favoritos, EPG, audio e UX do player — **pendentes**.
- [ ] Teste de DRM/KMS sem Xorg e otimizacao de memoria — **pendente**.

## Requisitos
`libsdl2-ttf-dev`, `fonts-dejavu-core`, `libsdl2-dev`, CMake, GCC e pkg-config.

## Seguranca/operacao
A branch v0.2 nao modifica configuracoes do DietPi, autostart, SSH, rede, Bluetooh, LightDM ou servico Python SPI/NRF. O parse da playlist e local; as URLs sao armazenadas em RAM mas nao exibidas/logadas. A reproduccao nao e disparada nesta versao. Nao executar o aplicativo como root.
