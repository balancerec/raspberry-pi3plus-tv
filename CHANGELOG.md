# Changelog

## 1.0.0-rc1 — 2026-11-24 — candidata à validação

### Implementado

- Parser limitado/UTF-8, tvg-id/name/logo, categorias, duplicatas, filtro e
  distinção entre catálogo IPTV e manifesto HLS; recarga em worker.
- Favoritos por identidade estável e settings XDG privados/atômicos;
  último catálogo/grupo/canal, volume/mute e preferências lembrados sem autoplay.
- Worker GStreamer único, comandos coalescidos por geração, estados explícitos,
  abertura/buffering/stall timeouts e reconexão limitada/cancelável.
- Detecção de trilhas e erros de áudio, seleção de sink/dispositivo por app.
- OSD X11 separado, cache de texto limitado e reticências UTF-8.
- Preferências e saída pelo controle; teclado simultâneo; testes de hotplug lógico.
- Scripts nativos de diagnóstico/build ARM64/instalação opcional/rollback/
  desinstalação e documentação de validação.

### Correções

- Hotfix do primeiro build no Pi: `Command::NoRequest` evita colisão com a
  macro `None` do X11 incluída por SDL_syswm.h. Rebuild ARM64 ainda necessário.

- Reload não apaga favoritos nem troca por lista inválida.
- Identidade deixa de depender da posição do canal.
- Transições e fsync deixam o loop SDL responsivo.
- Zapping mantém grupo/lista de origem e ignora favoritos removidos.
- Estado antigo do pipeline não ressuscita solicitação cancelada.
- Pausa não gera espera ocupada após falha/reconexão.
- Flags CMake compatíveis com MSVC para testar o núcleo.
- Launcher Linux passa a executar a aplicação nativa, preservando legado separado.

### Validação/limitações

Núcleo portável e testes legados executados no Windows. Build completo Linux,
GStreamer novo, pacote ARM64 e testes físicos pendentes por falta de acesso
SSH/runtime Linux neste ambiente. Não há medição nova de FPS/memória no Pi.
Nenhuma alteração no DietPi ou instalação v0.5. Não aprovada como 1.0 final.

## 0.5 — base 35d8502

Interface Full HD, M3U, grupos e GStreamer/VideoOverlay; vídeo e retorno ao
menu relatados como funcionando no Raspberry pelo usuário. Ver registros
históricos em docs/VALIDACAO_IPTV_V05.md e docs/PLAYER_IPTV_V04.md.
