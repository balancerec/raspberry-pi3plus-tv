# Relatório de testes — 1.0.0-rc1 — 2026-11-24

## Resultado executivo

**Candidata de código à validação. Não é uma release 1.0 fisicamente aprovada.**
O núcleo passou em build limpo CMake/CTest no Windows: **9/9, 1,11 s**.
O servidor Python legado passou: **9/9, 0,797 s**. Depois do hotfix X11, o
usuário compilou a aplicação completa no Pi e executou **12/12 testes com
sucesso, 4,46 s**. O agente não acessou o Pi; os resultados ARM64 são do log
fornecido pelo usuário. O binário foi gerado no dispositivo, não neste host.

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

## Situação da integração

| Verificação | Resultado real |
|---|---|
| BUILD_NATIVE_APP=ON neste host | Configuração falhou: Could NOT find PkgConfig (PKG_CONFIG_EXECUTABLE) |
| C++ SDL2/GStreamer novo | Build completo no Pi após hotfix, conforme log do usuário; não compilado neste Windows |
| gstreamer_object_lifecycle | PASS no Pi, 0,20 s, criticals fatais habilitados pelo CTest |
| real_player_open_failure_cancel_shutdown | PASS no Pi, 1,59 s; WAV/fakesinks, HTTP 403/404 local, falhas, zapping e shutdown |
| release_scripts_filesystem | PASS na última execução do Pi, 1,26 s; simulação em HOME temporário |
| Scripts Bash | Host Windows bloqueado pelo sandbox; sintaxe aprovada no Pi pelo teste #10 |
| Pacote ARM64 | Ainda não gerado/conferido; o usuário já possui binário compilado no Pi |
| UI/OSD/video/áudio/Bluetooth reais | Log SDL confirma 1920×1080 e PS4 reconhecido; apresentação/navegação/áudio/reconexão físicos pendentes |

No Linux com BUILD_NATIVE_APP=ON, são registrados **12 testes** (9 núcleo,
1 scripts e 2 GStreamer). Todos passaram no log ARM64 de 4,46 s.
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

A primeira tentativa foi seguida pela execução bem-sucedida registrada abaixo.
O agente continua sem acesso SSH; esta evidência é a saída fornecida pelo usuário.

Verificação local específica do hotfix: teste C++ isolado com o mesmo enum e
ordem de inclusão (declaração, macro `None=0L`, referências ao enum). A versão
anterior falhou com o erro de expansão após `::`; a versão `NoRequest`
compilou com `/W4 /WX` e executou com código 0. A macro foi mantida definida.
Isso verifica a colisão, não a aplicação SDL/GStreamer completa.

### Build ARM64 após hotfix e execução gráfica — log do usuário

O build retomado no mesmo diretório gerou tv_player, raspberry-tv-native e
todos os executáveis de teste. **12/12 PASS, 0 falhas, 4,46 s**. É uma retomada
incremental após a correção, não uma segunda compilação integral em diretório novo.
O teste GStreamer usa fakesinks: não prova áudio audível nem vídeo HDMI.

Execução com DISPLAY=:0 e catálogo de teste local informou:

```text
Renderizacao interna: 1920 x 1080; saida atual: 1920 x 1080
Controle principal: PS4 Controller (0)
```

Também foram emitidos avisos PipeWire sobre client.conf ausente e erros OpenAL
ao criar contexto PipeWire/consultar RTKit. Eles exigem investigação da saída
de áudio, mas o trecho não identifica o sink final nem confirma silêncio ou som.
O usuário encerrou com Ctrl+C e retornou ao prompt; não há GStreamer-CRITICAL
no trecho fornecido. Vídeo visível, OSD, áudio audível e ações físicas não foram
confirmados apenas por esse log. Nenhum novo teste de estresse/desempenho.

No worktree isolado do Pi, rode diagnóstico, build limpo/CTest completo e
`JOBS=1 bash scripts/package-arm64.sh`. Faça a matriz física de
RELEASE_CHECKLIST.md com fonte atual, sem estresse prolongado. Registre
vídeo visível, áudio audível, reconexão DS4, retorno ao menu e ausência de
criticals. Só então marque itens pendentes e considere remover RC1 do nome.
