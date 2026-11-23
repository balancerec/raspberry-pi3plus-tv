# Validação DietPi ARM64 — 2026-11-23

## Ambiente reportado pelo teste em equipamento real

- Dispositivo: Raspberry Pi 3 Model B, DietPi (Debian Trixie), arquitetura aarch64.
- Kernel previamente identificado: `6.18.50+rpt-rpi-v8`.
- Compilador: GNU C++ 14.2.0.
- CMake: 3.31.6.
- SDL2: 2.32.4 (pacote `libsdl2-dev`).
- Repositório clonado pela branch `feature/native-cpp-controller-first`.

## Comandos executados no Raspberry

```bash
git clone -b feature/native-cpp-controller-first --single-branch https://github.com/balancerec/raspberry-pi3plus-tv.git
cd raspberry-pi3plus-tv
sudo apt install --no-install-recommends build-essential cmake pkg-config libsdl2-dev
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j2
ctest --test-dir build/native --output-on-failure
```

## Resultados observados

- [x] Clone concluído, sem erro.
- [x] Dependências de desenvolvimento instaladas.
- [x] Configuração CMake concluída.
- [x] `libtv_input.a` gerada.
- [x] `tv-input-tests` gerado.
- [x] `raspberry-tv-native` gerado.
- [x] CTest: `controller_input` aprovado (1/1; 100%; duração reportada 0,02 s).
- [x] Protótipo SDL2 aberto com `DISPLAY=:0` no DietPi, confirmado pelo usuário.
- [x] SDL2 detectou `PS4 Controller`, com instâncias `(0)` e `(1)` em um mesmo teste (conexão/reconexão ou nova instância a esclarecer); transporte USB/Bluetooth não informado.
- [ ] Transporte Bluetooth versus USB e reconexão ainda não isolados em teste específico.
- [x] Movimentos direcionais do controle detectados no console (up/down/left/right).
- [ ] Botão PS, Options, gatilhos e reconexão: **não testados separadamente no hardware**.
- [ ] Desempenho e RAM durante uso: **não medidos**.
- [ ] GStreamer integrado ao protótipo C++: **não implementado**.

## Próximos testes sem modificar a inicialização

Na sessão SSH:

```bash
pgrep -af 'Xorg|lightdm|xfce4-session'
ls -l ~/.Xauthority
cd ~/raspberry-pi3plus-tv
DISPLAY=:0 ./build/native/raspberry-tv-native
```

Se não houver autorização X11, investigar `XAUTHORITY` e o login gráfico antes de qualquer mudança. **Não** executar a aplicação como root por padrão; não desativar o LightDM antes de verificar a janela na sessão atual.

Em seguida testar reconhecimento do controle por USB, comandos de navegação, Bluetooth e reconexão; documentar o resultado por etapa. A versão web da `main` permanece preservada.

## Observações

- O protótipo gráfico atualmente exibe seis cartões demonstrativos, não a interface final.
- Os testes CTest cobrem a lógica de entrada, não a interação real com o hardware ou a interface HDMI.
- A disponibilidade anterior do driver H.264 foi comprovada, mas a integração multimídia com este executável ainda está pendente.

## 2026-11-23 — Primeiro teste físico de controle e correção v0.1.1

O usuário executou `DISPLAY=:0 ./build/native/raspberry-tv-native`, obteve `Controle principal: PS4 Controller (0)` e, posteriormente, `Controle principal: PS4 Controller (1)`. O console mostrou movimentos up/down/left/right, mas também centenas de eventos repetidos durante o teste. A implementação anterior repetia movimentos após 350 ms e a cada 120 ms, explicando o spam direcional observado. Os logs disponíveis **não provam** clique × repetido; são eventos de direção. A mudança de instância pode indicar reconexão; precisa de diagnóstico separado.

**Correção publicada na mesma branch:**
- Desativada a repetição direcional ao manter D-pad ou analógico pressionados.
- Analógico: limiar de ativação ±22000; liberação ±12000; estabilidade 80 ms.
- Botões de ação mantidos com um evento por pressionamento.
- Testes de longa duração, deriva analógica, jitter, reconexão e gatilhos; CTest **1/1 em Debug e Release localmente**, sem SDL2 no ambiente local.
- **Pendente:** recompilar o commit atualizado no Raspberry Pi e validar novamente o controle físico e o HDMI.

```bash
cd ~/raspberry-pi3plus-tv
git pull --ff-only
cmake --build build/native -j2
ctest --test-dir build/native --output-on-failure
DISPLAY=:0 ./build/native/raspberry-tv-native
```
