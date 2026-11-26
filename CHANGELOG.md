# Changelog

## Não lançado — integração Spotify — 2026-11-25

- Funções Soloist oficiais para reprodução local/estado/fila/controle,
  configuração privada por entrada oculta e launcher manual sem serviços.
- Web API opcional: OAuth PKCE, renovação de token, perfil, playlists/faixas,
  busca, dispositivos, reprodução, pausa, faixas e volume; endpoints 2026.
- Painel Spotify controller-first em APLICATIVOS com navegação paginada,
  worker serializado, cancelamento, prazos e mensagens sem segredos.
- Testes offline de autenticação/API/armazenamento/Soloist e C++ de protocolo,
  navegação e concorrência. Adaptador POSIX e áudio físico pendentes no Pi.
- Código, histórico e pacote sem chave real; `.gitignore` protege arquivos
  privados. Download do cliente oficial e ativação de áudio não automatizados.


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
  macro `None` do X11 incluída por SDL_syswm.h. Build ARM64 e 12/12 testes
  aprovados depois da correção, conforme log do usuário.

- Reload não apaga favoritos nem troca por lista inválida.
- Identidade deixa de depender da posição do canal.
- Transições e fsync deixam o loop SDL responsivo.
- Zapping mantém grupo/lista de origem e ignora favoritos removidos.
- Estado antigo do pipeline não ressuscita solicitação cancelada.
- Pausa não gera espera ocupada após falha/reconexão.
- Flags CMake compatíveis com MSVC para testar o núcleo.
- Launcher Linux passa a executar a aplicação nativa, preservando legado separado.

### Validação/limitações

Núcleo portável e testes legados executados no Windows. Build completo ARM64
e 12/12 testes aprovados no Raspberry pelo usuário (4,46 s); UI reporta
1920×1080 e reconhece PS4. Playlist privada real validada: 398 canais,
0 inválidos, 0 duplicados, sem truncamento. A lista permanece fora do Git.

Usuário relata alguns canais fora do ar e travamentos durante o uso. O log
registra duas reconexões de rede/stream e falha após esgotar esse limite.
A fonte inadequada/subtensão histórica pode contribuir para o desempenho,
mas não houve medição contemporânea que prove ser a única causa. Não há
FPS/memória/temperatura novos ou confirmação final de áudio audível.
Pacote ARM64, build integral em novo diretório e matriz física permanecem
pendentes. Nenhuma alteração automática de serviços do DietPi ou instalação
v0.5. Não aprovada como 1.0 final. Publicação somente na branch de finalização,
autorizada pelo usuário; sem merge no main.

## 0.5 — base 35d8502

Interface Full HD, M3U, grupos e GStreamer/VideoOverlay; vídeo e retorno ao
menu relatados como funcionando no Raspberry pelo usuário. Ver registros
históricos em docs/VALIDACAO_IPTV_V05.md e docs/PLAYER_IPTV_V04.md.
