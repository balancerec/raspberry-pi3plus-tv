# Raspberry TV nativa — protótipo de entrada via DualShock 4

Protótipo de **navegação por controle como padrão** para a migração Python/Firefox → C++/SDL2. **Ainda não há player IPTV nem interface visual final.** O programa abre uma janela com seis cartões demonstrativos para validar direção, foco, USB/Bluetooth e comandos. Não altera o DietPi, serviços ou modo de inicialização.

- `include/tv/input.hpp`, `src/input.cpp`: normalização e política independente de SDL2.
- `src/main.cpp`: camada SDL2 de joystick e exibição simples.
- `tests/input_tests.cpp`: testes lógicos de botões, histerese, repetição, gatilhos e desconexão.
- `../docs/CONTROLE_PADRAO.md`: contrato de controle e teclas.

```bash
sudo apt install --no-install-recommends build-essential cmake pkg-config libsdl2-dev
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j2
ctest --test-dir build/native --output-on-failure
./build/native/raspberry-tv-native
```

Para compilar e testar **sem instalar SDL2**, use `cmake -S native -B build/native-tests -DBUILD_NATIVE_APP=OFF` seguido de `cmake --build build/native-tests && ctest --test-dir build/native-tests --output-on-failure`.

Evite executar em DRM/KMS direto enquanto o Xorg/LightDM estiver usando a saída HDMI. Execute na sessão gráfica atual primeiro. A seleção de cards é demonstrativa e serve para medir desempenho; não executa sites, aplicações externas nem vídeo.
