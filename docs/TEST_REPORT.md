# Relatório de testes — 1.0.0-rc1 — 2026-11-24

## Resultado executivo

**Candidata de código à validação. Não é uma release 1.0 fisicamente aprovada.**
O núcleo passou em build limpo CMake/CTest no Windows: **9/9, 1,11 s**.
O servidor Python legado passou: **9/9, 0,797 s**. Não houve acesso ao Pi nem
build completo SDL/GStreamer; nenhum binário ARM64 é apresentado como compilado.

## Ambiente e procedência

- Host real do agente: Windows, PowerShell, MSVC 19.51.36252, CMake 4.4.4,
  Python 3.14. Código C++17 Release, BUILD_NATIVE_APP=OFF.
- Base verificada: 35d85026a3c1a32b6ce2124a2ce2f91b7851d0b1.
- Importação superficial via conector GitHub devido bloqueio de rede direta;
  SHA de cada blob/árvore e do commit original conferidos. Histórico remoto
  consultado, nenhuma reescrita/push/merge.
- Checkout, playlists, serviços e instalação aprovada no Raspberry intocados.
- SSH/rede direta indisponíveis; WSL negado. Git Bash falhou ao criar signal
  pipe com Win32 error 5. Não houve tentativa de contornar essas restrições.

## Antes de alterar código

CMake do núcleo configurou, mas o build original com MSVC falhou em `-Wextra`.
Os quatro testes foram então compilados diretamente com opções equivalentes,
sem alterar fontes, e executados pelo CTest gerado: **4/4, 0,81 s**.
Depois as flags do CMake foram corrigidas por compilador.

## Build limpo final executado

Diretório novo `build-release-verify`, separado de todos os builds anteriores:

```text
cmake -S native -B build-release-verify -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_NATIVE_APP=OFF
cmake --build build-release-verify
ctest --test-dir build-release-verify --output-on-failure
100% tests passed, 0 tests failed out of 9
Total Test time (real) = 1.11 sec
```

Configuração, compilação e testes concluíram sem erros. Houve quatro avisos
MSVC C4996 sobre getenv/_wopen (APIs portáveis/arquivo exclusivo); nenhum erro
de compilação. Isso compila tv_input/tv_menu e os testes, **não** main/ui/player.

| Teste CTest | Resultado | Cobertura |
|---|---|---|
| controller_input | PASS | Edges, hold sem repeat, drift, reset/reconexão normalizada, triggers e relógio wrap |
| menu_navigation_and_m3u | PASS | Navegação e parser básico existente |
| iptv_group_and_channel_navigation | PASS | Grupos, seleção, favoritos e falha de reload |
| fullhd_native_pixels | PASS | Geometria 1920×1080; não valida TV/HDMI |
| playlist_validation_metadata_and_limits | PASS | Metadados, vírgulas, UTF-8 inválido, HLS, duplicatas, limite/cancelamento |
| atomic_settings_and_favorites | PASS | Arquivo privado no host, corrupção/defaults, reorder/token, falha de I/O, gravação/replace, flush assíncrono |
| playback_timeouts_retries_and_cancel | PASS | Máquina pura com tempo injetado; não valida rede real/GStreamer |
| asynchronous_playlist_reload | PASS | Leitura de 20 mil canais em worker, erro e arquivo >64 MiB |
| playback_group_and_favorites_queue | PASS | Origem filtrada, wrap e favoritos removidos |

Persistência POSIX (0600/fsync/rename) foi implementada, mas somente o ramo
Windows de I/O foi executado aqui. Todos os testes usam falha explícita,
sem depender de assert desabilitado pelo Release.

Componente legado: `python -m unittest discover -s tests -v`: **9/9 PASS**.
Não é prova do novo player; demonstra preservação do componente preexistente.

## Tentativas bloqueadas e testes não executados

| Verificação | Resultado real |
|---|---|
| BUILD_NATIVE_APP=ON neste host | Configuração falhou: Could NOT find PkgConfig (PKG_CONFIG_EXECUTABLE) |
| C++ SDL2/GStreamer novo | Não compilado neste ambiente; exige dependências/runtime Linux do destino |
| gstreamer_object_lifecycle | Preparado; não executado na RC1 |
| real_player_open_failure_cancel_shutdown | Preparado; não executado; WAV sintético/fakesinks, HTTP 403/404 local, falhas, zapping e shutdown |
| release_scripts_filesystem | PASS no Pi, 1,31 s, conforme log fornecido pelo usuário; simulação em HOME temporário |
| Scripts Bash | Host Windows bloqueado pelo sandbox; sintaxe aprovada no Pi pelo teste #10 |
| Pacote ARM64 | Gerador pronto; pacote com executável não gerado sem build aarch64 |
| UI/OSD/video/áudio/Bluetooth reais | Pendentes; nenhum resultado físico inventado |

No Linux com BUILD_NATIVE_APP=ON, são registrados **12 testes** (9 núcleo,
1 scripts e 2 GStreamer). Execute todos com CTest antes de aprovar a candidata.
O teste de scripts simula a arquitetura apenas para exercer o gerenciamento
de arquivos; não é validação ARM64. Em root, esse teste é explicitamente skipped;
rode como usuário normal, conforme exigência de execução do produto.

## Desempenho e alimentação

Nenhum FPS, CPU, RAM ou temperatura nova foi medido no Raspberry. Os 0,30 s
do teste de loader e 1,11 s da suíte são tempos neste Windows, não benchmarks
do Pi. Não demonstram reprodução 1080p30 nem ausência de vazamento no hardware.

Informações fornecidas pelo usuário: subtensão/throttling, registro histórico
0x50005, sessão DietPi ARM64. Resultados antigos de V4L2/GL e vídeo físico v0.5
continuam históricos. A RC1 mantém resolução da interface e glimagesink sem
alterar proteções elétricas/térmicas, HDMI, X11 ou serviços.

## Próxima validação objetiva

### Primeira tentativa ARM64 — log fornecido pelo usuário

No checkout extraído no Pi, GNU 14.2.0 configurou SDL2 2.32.4, SDL2_ttf
2.24.0 e GStreamer 1.26.2. tv_input e tv_menu compilaram, mas tv_player
parou: SDL_syswm.h inclui Xlib, cuja macro `None` expandia `Command::None`
para uma constante numérica. Correção: enumerador privado `NoRequest` em
player.hpp/player.cpp; sem undef global, alteração de pipeline ou dependências.

Nesse mesmo log, `release_scripts_filesystem` **PASS (1,31 s)**: sintaxe Bash e
simulação de instalação/atualização/rollback/desinstalação. Os outros 11 testes
foram **Not Run**, pois o build interrompido não gerou seus executáveis;
não são 11 falhas funcionais. O teste dos scripts não valida o binário ARM64.

A correção ainda exige recompilar a aplicação no Pi e executar os 12 testes.
O agente continua sem acesso SSH; esta evidência é a saída fornecida pelo usuário.

Verificação local específica do hotfix: teste C++ isolado com o mesmo enum e
ordem de inclusão (declaração, macro `None=0L`, referências ao enum). A versão
anterior falhou com o erro de expansão após `::`; a versão `NoRequest`
compilou com `/W4 /WX` e executou com código 0. A macro foi mantida definida.
Isso verifica a colisão, não a aplicação SDL/GStreamer completa.

No worktree isolado do Pi, rode diagnóstico, build limpo/CTest completo e
`JOBS=1 bash scripts/package-arm64.sh`. Faça a matriz física de
RELEASE_CHECKLIST.md com fonte atual, sem estresse prolongado. Registre
vídeo visível, áudio audível, reconexão DS4, retorno ao menu e ausência de
criticals. Só então marque itens pendentes e considere remover RC1 do nome.
