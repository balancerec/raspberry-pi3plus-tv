# Raspberry TV — Contrato de navegação Gamepad First (v1)

**Decisão de produto:** o DualShock 4 é a forma de navegação **padrão** para a futura Raspberry TV nativa. Funciona via USB ou Bluetooth quando o Linux expõe o dispositivo à SDL2. O teclado é modo alternativo de recuperação e testes, não a entrada principal; mouse não é obrigatório.

## Mapeamento fixo

| DualShock 4 | Ação | Disponibilidade |
|---|---|---|
| D-pad ↑ ↓ ← → | Navegar pelos menus | Protótipo |
| Analógico esquerdo | Navegar pelos menus | Protótipo |
| × (SDL A) | Confirmar/abrir | Evento implementado; integração com apps pendente |
| ○ (SDL B) | Voltar/cancelar | Protótipo |
| PS (SDL Guide) | Ir ao início | Protótipo, depende de evento disponibilizado pelo sistema |
| Options (SDL Start) | Abrir menu | Evento implementado; tela pendente |
| L1 / R1 | Categoria anterior/próxima | Evento implementado; categorias pendentes |
| L2 / R2 | Canal IPTV anterior/próximo | Evento implementado; IPTV pendente |
| △ (SDL Y) | Favoritos | Evento implementado; favoritos pendentes |
| □ (SDL X) | Detalhes do canal | Evento implementado; EPG pendente |

## Regras de ergonomia

- **Controller First:** conectar o primeiro controle compatível detectado pela SDL2. Não abrir teclado virtual ou exigir mouse para navegar.
- Hotplug: aceitar conexão e desconexão em execução; após desconexão, tentar o próximo controle disponível.
- D-pad tem prioridade sobre analógico esquerdo para impedir comandos duplicados.
- Analógico possui zona morta com histerese: ativa em ±22000, libera abaixo de ±12000; mudança precisa permanecer estável por 80 ms.
- Navegação por direção: **uma ação por pressionamento**, sem repetição automática ao segurar D-pad ou analógico; o D-pad responde imediatamente e o analógico aguarda estabilidade de 80 ms.
- Botões de ação: uma emissão por pressionamento; gatilhos ativam acima de 20000 e liberam abaixo de 12000.
- Quando nenhum controle estiver conectado, usar teclado como fallback: setas, Enter, Esc, Home, F1, PageUp e PageDown.
- Não assumir que o botão PS chega ao programa em todos os sistemas Linux: alguns ambientes podem interceptá-lo. Testar no Raspberry real.
- No futuro, menus e player recebem eventos de uma única camada `ControllerInput`, não acessam botões físicos diretamente.

## Estado do protótipo

O código em `native/` estabelece a política de entrada, testa o mapeamento lógico e oferece **uma janela demonstrativa** SDL2 com seis blocos selecionáveis. **Não é a interface final, player IPTV, nem substitui o Firefox automaticamente.** Nenhum serviço, autostart ou sistema instalado é modificado.

## Execução futura no DietPi

```bash
sudo apt install --no-install-recommends build-essential cmake pkg-config libsdl2-dev
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j2
ctest --test-dir build/native --output-on-failure
./build/native/raspberry-tv-native
```

Para exibir em tela cheia dentro de um desktop existente, use `--fullscreen`. Para usar DRM/KMS direto no futuro, precisamos validar separadamente desligamento do LightDM, grupos de permissão e `SDL_VIDEODRIVER=kmsdrm`, sem colocar isso no autostart antes dos testes.

### Validação do DualShock 4 no DietPi

```bash
ls -l /dev/input/event* /dev/input/js* 2>/dev/null
bluetoothctl devices Connected
```

Testar USB primeiro, depois Bluetooth. Se o controle não for detectado, verificar log de dispositivo, mapeamento da SDL e permissões de entrada; **não** executar o aplicativo inteiro como root como solução genérica.
