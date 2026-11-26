# Relatório de testes — 1.0.0-rc1 — atualizado em 2026-11-25

## Resultado executivo

**Candidata de código à validação. Não é uma release 1.0 fisicamente aprovada.**
O núcleo passou em build limpo CMake/CTest no Windows: **9/9, 1,11 s**.
O servidor Python legado passou: **9/9, 0,797 s**. Depois do hotfix X11, o
usuário compilou a aplicação completa no Pi e executou **12/12 testes com
sucesso, 4,46 s**. O agente não acessou o Pi; os resultados ARM64 são do log
fornecido pelo usuário. O binário foi gerado no dispositivo, não neste host.

**Atualização Spotify (25/11/2026):** o usuário forneceu posteriormente o log
de compilação completa ARM64 da integração e **15/15 CTest PASS, 12,26 s**.
O SDL reportou renderização e saída 1920×1080. A evidência detalhada está ao
final deste documento; áudio audível e comandos físicos continuam pendentes.

## Ambiente e procedência

- Host real do agente: Windows, PowerShell, MSVC 19.51.36252, CMake 4.4.4,
  Python 3.14. Código C++17 Release, BUILD_NATIVE_APP=OFF.
- Base verificada: 35d85026a3c1a32b6ce2124a2ce2f91b7851d0b1.
- Importação superficial via conector GitHub devido bloqueio de rede direta;
  SHA de cada blob/árvore e do commit original conferidos. Histórico remoto
  consultado. Publicação da branch de finalização autorizada posteriormente
  pelo usuário, via conector GitHub; sem reescrever a v0.5 nem fazer merge no main.
- O agente não alterou checkout, playlists ou serviços no Raspberry. O usuário
  aplicou o hotfix e executou build/testes/launcher em diretório extraído separado.
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
Windows de I/O foi executado neste host. O mesmo teste de persistência passou
posteriormente no Pi, conforme log do usuário. Todos os testes usam falha explícita,
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

### Diagnóstico ALSA posterior — log do usuário

`aplay -l/-L` identificou Headphones (placa 0) e HDMI `vc4hdmi`, dispositivo 0
(placa 1). `gst-inspect-1.0 alsasink` informou elemento/plugin ausente. A
execução solicitando `--audio-sink alsasink --audio-device default` retornou
FAILED/category=3/retry=0. Isso confirma a falta de uma dependência de áudio
necessária para essa preferência; não demonstra defeito do HDMI. Instalação
manual de gstreamer1.0-alsa e verificação audível continuam pendentes.

### Playlist real e comportamento durante uso — confirmação do usuário

O usuário confirmou que sua playlist real carregou e relatou que alguns canais
não estão mais no ar. A validação offline e o lançamento com ALSA/HDMI por
nome de placa produziram este trecho, com endereços de streams omitidos:

```text
Playlist: 398 canais, 0 invalidos, 0 duplicados; truncada=nao
Renderizacao interna: 1920 x 1080; saida atual: 1920 x 1080
Controle principal: PS4 Controller (0)
Player: state=RECONNECTING category=0 retry=1 (endereco omitido)
Player: state=RECONNECTING category=0 retry=2 (endereco omitido)
Player: state=FAILED category=0 retry=2 (endereco omitido)
```

O log confirma catálogo real validado sem perdas pelo limite do parser e uma
falha categorizada como rede/stream após duas reconexões. Ele não distingue,
sozinho, canal permanentemente fora do ar de falha temporária de rede/provedor.
O usuário também relatou travamentos, que atribui possivelmente à fonte atual.
Com o histórico de subtensão/throttling, essa hipótese é plausível, mas não
houve medição contemporânea de alimentação/FPS/CPU/memória/temperatura para
isolar a causa. Codec, rede e caminho de apresentação também continuam como
variáveis a verificar. Não declarar fluidez diária ou 1080p30 contínuo aprovado.

Não houve confirmação explícita posterior de áudio audível, OSD, reconexão
Bluetooth ou todos os comandos físicos. O comando escolheu ALSA/HDMI, mas isso
não prova som na TV nem comprova a instalação do plugin por esse trecho.
Nenhuma URL, credencial, token ou conteúdo da playlist real foi incluído no Git.

No worktree isolado do Pi, rode diagnóstico, build limpo/CTest completo e
`JOBS=1 bash scripts/package-arm64.sh`. Faça a matriz física de
RELEASE_CHECKLIST.md com fonte atual, sem estresse prolongado. Registre
vídeo visível, áudio audível, reconexão DS4, retorno ao menu e ausência de
criticals. Só então marque itens pendentes e considere remover RC1 do nome.
# Integração Spotify — validação adicional de 25/11/2026

Esta seção refere-se à branch `feature/native-spotify-integration`, derivada
da RC1 publicada `e84c664`. As evidências anteriores no Pi permanecem válidas
somente para a versão anterior; não aprovam a nova interface por inferência.

| Verificação executada neste host Windows | Resultado real |
|---|---|
| CTest portátil antes de alterações | 9/9 PASS, 1,09 s |
| Build limpo `build-spotify-final`, MSVC/CMake Release, `BUILD_NATIVE_APP=OFF` | PASS, sem erros; avisos MSVC preexistentes em settings.cpp |
| CTest após integração | 11/11 PASS, 2,08 s |
| Suíte Python Spotify (incluída em um teste CTest) | 39 casos: 38 PASS, 1 SKIP POSIX |
| Regressão do servidor Python legado | 9/9 PASS |

Logs locais: `build-spotify-final-configure.log`,
`build-spotify-final-compile.log` e `build-spotify-final-ctest.log` na raiz do
checkout. Esses logs não são versionados nem contêm credenciais de conta.

Spotify: PKCE/RFC7636, estado/callback inválido, armazenamento atômico,
corrupção, exclusão entre processos, refresh/rotação e limite de uma renovação
em 401; erros 403/404/503, cooldown persistente em 429, endpoints/paginação,
UTF-8, payloads de comandos e ausência de tokens na saída. Soloist: loopback
obrigatório, arquivos de discovery, estado/fila no formato oficial, comandos
sem chave em ctl, volume relativo, entrada oculta, build expirado e interrupção
do launcher. Respostas e credenciais são artificiais; nenhum login, pareamento
ou comando autenticado real foi executado.

No C++ foram executados protocolo/limites, estados, navegação de APLICATIVOS,
seleção de backend, manutenção/limpeza de metadados, serialização de comandos,
cancelamento/descarte de resposta antiga, falha sanitizada e shutdown do worker.
Os nove testes anteriores de parser, entrada, persistência, playback e filas
continuam passando. Não alteramos o ciclo de vida de GStreamer nesta integração.

Na etapa inicial no host Windows, build Linux/ARM64 da UI, adaptador POSIX,
permissões 0700/0600 e CTest completo do destino ainda estavam pendentes.
O usuário executou posteriormente esses testes, conforme nova evidência abaixo.
O workflow Linux está preparado no Git, mas não foi executado nesta sessão.
Não há SDK SDL2/GStreamer Linux neste host, acesso SSH operacional ou cliente
Soloist disponível ao agente para validar som.

Cliente Soloist oficial não é redistribuído. Precisa ser instalado e pareado
pelo usuário; depende de PipeWire/PulseAudio no Linux. O diagnóstico anterior
de PipeWire ausente no DietPi continua relevante. Não modificamos servidores
de áudio nem serviços para contorná-lo. Não certificamos áudio pela alteração
de propriedade, por resposta da API ou por teste com mock.

Chave real fornecida na conversa não foi gravada em fonte, teste, exemplo,
comando, log ou configuração desta entrega. Código e pacote são preparados
sem credenciais. Configuração real deverá ser feita com o prompt oculto do
launcher, em arquivo privado XDG fora do checkout. Limitação de argv da CLI
oficial e demais requisitos: [SPOTIFY.md](SPOTIFY.md).

Não foram medidos consumo de memória, CPU, FPS, temperatura ou alimentação
após esta mudança. HDMI 1080p, DS4, IPTV e áudio exigem regressão física com o
novo build. Nenhuma atualização/instalação de release foi aplicada ao Pi pelo agente.

### Evidência posterior do usuário: configuração e Spotify Connect

Em 25/11/2026, o usuário enviou o log do prompt oculto concluído com a mensagem
de chave Soloist salva em arquivo privado fora do projeto. O valor da chave
não apareceu nesse log e não foi copiado para arquivos Git.

Depois enviou uma captura do Spotify desktop com **Raspberry TV** destacado
no painel Connect e a indicação de dispositivo no rodapé. Isso confirma que
o dispositivo foi descoberto e selecionado no cliente Spotify. O botão central
exibe o ícone de reproduzir (triângulo), compatível com pausa na captura;
a indicação do dispositivo no rodapé não comprova áudio audível ou reprodução
contínua. Não reproduzimos dados pessoais ou a imagem no repositório.

Áudio na TV, saída HDMI, volume/mute físicos, estabilidade e controle pelo
novo módulo SDL permaneciam pendentes nesse momento. A captura, isoladamente,
não validava build ARM64, testes Linux ou versão Soloist. O log de build/CTest
foi recebido posteriormente, conforme registro abaixo. A captura não valida
o login OAuth opcional nem as consultas da Web API.

### Evidência posterior do usuário: build ARM64 completo e 15/15 CTest

O usuário executou CMake, compilação serial (`--parallel 1`), CTest e launcher
em `/home/balancerec/raspberry-tv-spotify`, em diretório separado da RC1 anterior.
O log contém configuração inicial do compilador e compilação de todos os
objetos, bibliotecas, aplicativo e testes, sem erros. O agente não executou
essas operações por SSH; a procedência é o log fornecido pelo usuário.

| Ambiente informado pelo build | Versão |
|---|---|
| GCC / C++ | 14.2.0 |
| SDL2 | 2.32.4 |
| SDL2_ttf | 2.24.0 |
| GStreamer / gstreamer-video | 1.26.2 |
| Python | 3.13.5 |
| pkg-config | 1.8.1 |

Resultado: **100% dos testes passaram, 0 falhas em 15; tempo total 12,26 s**.
Além dos nove testes portáveis de regressão, o log registra:

| Teste | Resultado | Tempo |
|---|---|---|
| spotify_protocol_navigation_and_worker | PASS | 0,03 s |
| spotify_oauth_api_and_private_storage | PASS | 3,28 s |
| spotify_native_process_lifecycle | PASS | 4,11 s |
| release_scripts_filesystem | PASS | 1,37 s |
| gstreamer_object_lifecycle | PASS | 0,38 s |
| real_player_open_failure_cancel_shutdown | PASS | 1,65 s |

O CTest #11 executa a suíte Python também no Linux, incluindo permissões
0700/0600 em arquivos temporários de teste. Isso não equivale a inspecionar
os arquivos privados reais da conta. O #12 usa processos Python reais para
testar argv literal, ciclo de vida, limites, falha e cancelamento. Os testes
GStreamer usam fakesinks, sem comprovar saída física de áudio/vídeo.

Após os testes, a cadeia `&&` avançou ao launcher e informou:

```text
Renderizacao interna: 1920 x 1080; saida atual: 1920 x 1080
```

Esse trecho confirma inicialização do renderer SDL e suas dimensões. Ainda
faltam confirmação visual dos menus, ações físicas do DualShock, estado/fila
Soloist exibidos pela UI, áudio audível, volume e troca de faixas. Nenhum log
de versão Soloist, teste OAuth autenticado ou medição nova de alimentação/FPS
foi recebido. Mantemos a condição de candidata à validação física; esta nova
evidência não exige alteração de código nem repetição do build já aprovado.

### Correção posterior: pausa/retomada e ausência de som

A foto enviada depois mostra o painel APLICATIVOS na TV, indicador PS4
conectado e um cabeçalho “Tocando”. O usuário informou que conseguiu pausar,
que a retomada não funcionou como esperava e, depois, confirmou **sem som**.
Não há confirmação audível anterior. A causa da ausência de som permanece
pendente; o sink ALSA do player IPTV não configura a saída do Soloist.

A auditoria encontrou um defeito reproduzível no código: controles locais
retornavam apenas “comando aceito”, mantendo o cabeçalho da leitura anterior.
A linha Pausar também não oferecia retomada ao apertar × novamente. Agora
consultamos o estado depois do controle, com cinco leituras no máximo e
prazo compartilhado de 12 segundos, sem reenviar o comando. A mesma linha
alterna pausa/retomada; retomada ativa o dispositivo quando explicitamente
inativo. Falha/cancelamento elimina a indicação antiga de reprodução.

Validação local da correção, em build isolado `build-spotify-controls-final`:

- Build limpo CMake/MSVC do núcleo portável: aprovado.
- CTest portável: **11/11 PASS**, 2,33 s; após os casos adicionais de seleção
  de áudio, **11/11 PASS**, 1,86 s.
- Suíte Python atual: **51 casos, 50 PASS e 1 SKIP POSIX** no Windows, 1,067 s.
- Casos novos cobrem confirmação assíncrona, pausa/retomada, ativação antes de
  play, estados não confirmados, prazo, erro de leitura, cancelamento na UI,
  preservação da chave e ambiente de áudio restrito ao processo filho.

A preferência de nó PipeWire/fallback Pulse pode ser salva sem solicitar a
chave outra vez. O diagnóstico de áudio consulta saídas, mute, volume, fluxos
e estado Soloist. Nenhum nó foi selecionado por inferência; nenhum servidor,
serviço, saída global ou arquivo privado real foi alterado pelo agente.

Os **15/15 CTest no ARM64 acima antecedem esta correção**. Novo build nativo,
retomada física e som HDMI após a correção estão pendentes. A tentativa de
observação direta pelo agente falhou porque o runtime de Computer Use
informou indisponibilidade do pipe nativo; só as imagens e logs enviados
pelo usuário foram observados. Não houve teste físico inventado.

### Ícone para abertura manual — 26/11/2026

Adicionado launcher opcional XFCE com `Terminal=false`, instalação em XDG,
identificação de arquivos gerenciados e uso do executável existente. A ponte
Windows delega autenticação ao SSH e não guarda senha, chave ou sessão.
O primeiro uso conectado instala o ícone no desktop do Pi e abre a TV.
Não inicia Soloist, serviços ou autostart e não altera áudio global.

No host: sintaxe Python e comando remoto verificados sem conexão; comando
PowerShell do instalador aceito pelo parser; ícone ICO multirresolução
gerado e prévia inspecionada. Um .lnk foi gerado na pasta de entrega e relido
por WScript.Shell, confirmando destino Python, helper e ícone. O acesso SSH
e a abertura no Pi não foram executados pelo agente.

A gravação direta na área de trabalho e a criação de pastas permanentes do
Windows retornaram Acesso negado nesta sessão. Foi preparado um instalador
de duplo clique para a sessão normal do usuário. A simulação POSIX de
instalação no host também esbarrou nas permissões de criação dos diretórios
temporários e não é registrada como aprovada. Permissões reais Linux,
ícone visível e abertura física no Pi seguem pendentes.

### Consolidação e publicação do pacote central — 26/11/2026

TV, Spotify, diagnóstico e atalhos reunidos no pacote central. CMake passa
a instalar os dois helpers de desktop, o instalador Windows e o ICO junto
ao helper Spotify e diagnóstico. A instalação dos ícones continua manual.
O exportador de fontes exige commit limpo, confere o manifesto e preserva
entregas anteriores; a recusa a checkout sujo foi executada e aprovada.

Reconfiguração e **compilação limpa** Release/MSVC no diretório isolado
`build-spotify-controls-final`: sem erros. **11/11 CTest PASS, 2,99 s**.
A suíte Python completa registrou **60 casos, 59 PASS e 1 SKIP, 2,026 s**;
o skip POSIX no Windows não é convertido em aprovação de permissões Linux.
Os logs locais são
`central-configure.log`, `central-build.log`, `central-ctest.log` e
`central-python.log`, ignorados pelo Git. Nenhum teste físico novo foi feito.

Publicação da integração e da branch de finalização autorizada pelo usuário.
O main remoto observado antes da publicação estava em `7fc92e9` (merge
anterior do usuário), e a branch de finalização em `e84c664`. O agente não
realiza merge ou atualização do main. Áudio Spotify continua sem confirmação.

O exportador gerou ZIP e TAR do commit local `ae50b8a`: **97 arquivos Git,
96 hashes de fontes conferidos**. Ambos foram reabertos e tiveram seus
arquivos comparados byte a byte; foram gerados SHA-256 dos dois pacotes e
JSON de procedência. O pacote não inclui binário ARM64, cliente Soloist,
playlist privada, chave ou sessão. Uma auditoria prévia dos oito commits
desde a RC1 examinou 75 blobs distintos sem encontrar a chave real ou
configuração privada. Arquivos de relatório posteriores mantêm a mesma
implementação; a entrega final recebe o identificador do commit publicado.

### Evidência CI Linux após publicação — 26/11/2026

O commit publicado **64aa777e92f34705a85e4c3bd5b5739680b67f52** foi confirmado
nas branches `feature/native-spotify-integration` e `feature/native-tv-release-v10`.
As nove árvores de commits publicados coincidem exatamente com as árvores
locais auditadas. O histórico local anterior foi preservado em branch archive;
o checkout foi alinhado aos commits publicados após verificar seus hashes.
Main permaneceu em `7fc92e977ac53156a52ace191c9d02a1a4320ea9`.

As duas execuções GitHub Actions concluíram com **success**:

- [Pacote central — execução 45814458812](https://github.com/balancerec/raspberry-pi3plus-tv/actions/runs/45814458812).
- [Integração Spotify — execução 45814400462](https://github.com/balancerec/raspberry-pi3plus-tv/actions/runs/45814400462).

O job Linux central teve configuração e compilação limpa da aplicação completa
com `BUILD_NATIVE_APP=ON`, **15/15 CTest PASS, 1,19 s** e **60/60 testes Python
PASS, 0,591 s**, sem skip. Os logs do job 139890449008 registram SDL2 2.30.0,
SDL2_ttf 2.22.0, GStreamer/gstreamer-video 1.24.2 e Python 3.12.3. Isso valida
também o adaptador POSIX, fixtures de permissões e ciclo de vida GStreamer
da correção atual no runner Linux. Não substitui build ARM64, Bluetooth,
renderização física ou áudio audível no Raspberry; os testes do player usam
fakesinks. Nenhum serviço do dispositivo foi alterado nesta publicação.

### Soloist pelo ícone — implementação posterior

A captura seguinte do usuário mostra a TV aberta e a mensagem “Soloist
indisponível”. O launcher anterior iniciava apenas a UI, deixando a partida
do cliente Spotify dependente de um terminal separado. Corrigido o fluxo:
o ícone inicia ou reutiliza o cliente configurado, inclusive com a TV já aberta.
Um supervisor do usuário roda em sessão separada, com lock, identidade de PID,
duas tentativas de recuperação e pausa de 30 segundos após esgotá-las.
O menu do ícone permite parar somente o grupo de processos gerenciado.

O launcher reaproveita o helper Spotify da instalação existente e encaminha
os comandos do painel pelo protocolo anterior. Não exige recompilar o binário
do Pi, não transporta a chave e não apaga identidade/cache Connect. Não instala
serviço de boot ou altera áudio global. A janela SDL e o player permanecem
com o código anterior; a mudança está no launcher e no supervisor.

Host Windows: **12/12 CTest PASS, 3,24 s**; suíte completa **70 casos, 63 PASS,
7 SKIP, 2,218 s**. Os novos testes portáveis verificam reutilização sem nova
leitura de chave, ausência de configuração, redaction e transporte comprimido
Windows. Seis testes de processos Linux estão preparados para partida após
saída do pai, concorrência, recuperação de queda, limite/backoff, cliente
manual preservado, UI já aberta e ação de parada. Esses seis testes foram
pulados no Windows; não são registrados como executados até passar no Linux.
Docker não está disponível no PATH deste host. Validação física e som ainda
dependem do dispositivo real e não foram inferidos da captura.

A primeira execução Linux do novo teste rodou os dez casos e encontrou um
erro ao instalar o ícone depois de abrir diretamente pelo launcher: o próprio
launcher criava seu lock sem antes marcar o diretório como gerenciado. A
criação foi unificada para marcar o diretório desde o primeiro uso. O teste
também expôs um ResourceWarning do processo de partida; agora um processo
intermediário é aguardado e recolhido, enquanto o supervisor continua em sua
própria sessão. Foi incluída proteção contra duplicar um cliente manual vivo
com API lenta, verificando PID, usuário e executável sem ler sua linha de
comando. A execução Linux deve ser repetida antes de aplicar a atualização.

Repetição aprovada no commit **237ddc1877a231a852b139926650f08d79938ae7**:
[CI central 45823870221](https://github.com/balancerec/raspberry-pi3plus-tv/actions/runs/45823870221)
e [CI Spotify 45823865192](https://github.com/balancerec/raspberry-pi3plus-tv/actions/runs/45823865192)
concluíram com sucesso. No job Linux 139922845515, **16/16 CTest PASS, 10,32 s**;
o teste desktop/supervisão passou em 9,00 s. A suíte Python executou **70/70
casos, sem skip, em 9,572 s**. O ResourceWarning não reapareceu. Foram
executados os seis casos POSIX, incluindo preservação de cliente manual
cuja API está temporariamente lenta.

O instalador Windows existente recebeu os dois helpers atualizados; o
transporte comprimido tem 8.528 caracteres e foi conferido sem abrir SSH.
A pasta instalada Windows retornou Acesso negado à escrita do agente e seus
arquivos anteriores foram relidos e confirmados intactos. A aplicação da
atualização será feita pelo instalador na sessão normal do usuário. O
agente não executou SSH, não iniciou o Soloist real e não confirmou áudio.

---

### Controles de mídia — 26/11/2026

O pedido do usuário compara o painel de onze opções com uma barra de mídia.
A implementação agora apresenta um player SDL com ações, tempo e volume;
fila e opções complementares têm telas próprias. A UI Full HD e o player
IPTV/OSD existentes foram preservados.

Host Windows: compilação limpa portável Release/MSVC no diretório isolado
`build-spotify-controls-final`: **12/12 CTest PASS, 3,28 s** antes do updater.
Após adicionar seus testes, a suíte Python completa executou **86 casos,
79 PASS e 7 SKIP POSIX, 2,039 s**. Os casos novos verificam protocolo opt-in,
âncora/velocidade/expiração do tempo, foco DS4/mouse, mute/restauração,
capacidade e confirmação de busca/shuffle/repeat, manifesto/caminhos seguros,
preservação da versão anterior em falha e rollback. O payload Windows foi
decodificado em teste sem SSH. Sintaxe PowerShell do instalador conferida
sem executar a instalação. Novos CMake/CTest e render SDL Linux aguardam CI.

O updater não foi executado contra o Raspberry nesta etapa. O acesso SSH do
agente está indisponível; a entrega prepara a ação explícita do usuário.
Nenhuma reprodução ou medição física de áudio/FPS/alimentação foi inventada.
