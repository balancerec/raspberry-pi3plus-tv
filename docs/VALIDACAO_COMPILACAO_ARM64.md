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
- [ ] Janela SDL2 exibida na televisão: **não testado**.
- [ ] Entrada real de DualShock 4 por USB: **não testado**.
- [ ] Entrada real de DualShock 4 por Bluetooth: **não testado**.
- [ ] Botão PS, Options, gatilhos e reconexão: **não testados no hardware**.
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
