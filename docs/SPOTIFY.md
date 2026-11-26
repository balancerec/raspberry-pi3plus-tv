# Spotify na Raspberry TV

Implementação opcional na branch `feature/native-spotify-integration`. Não é
parte da aprovação física anterior da RC1. Não altera o player IPTV, o áudio
global, os serviços, a sessão gráfica ou o autostart do DietPi.

## Duas interfaces oficiais

**Soloist é o caminho para áudio no Raspberry.** É o cliente oficial headless
do Spotify, disponível para Linux ARM64. Usa uma chave Soloist particular e
pareamento pelo Spotify Connect. Premium é necessário para gerar a chave e
configurar o cliente; depois, contas Free e Premium podem conectar, segundo
a documentação atual. A chave Soloist não é um Client ID nem um token OAuth.

**Web API é opcional para consultar playlists, faixas e dispositivos da conta.**
Requer Client ID público e login OAuth PKCE separado. Os comandos de player da
Web API exigem Premium e um dispositivo Spotify disponível; não decodificam
áudio no GStreamer. Não requisitamos email, senha, Client Secret ou permissão
para modificar playlists. `playlist-read-private`, `playlist-read-collaborative`,
`user-read-playback-state` e `user-modify-playback-state` são os escopos usados.

Referências oficiais, consultadas em 25/11/2026:

- [Soloist e requisitos](https://developer.spotify.com/documentation/soloist).
- [Autenticação Soloist](https://developer.spotify.com/documentation/soloist/concepts/authentication).
- [Downloads por arquitetura](https://developer.spotify.com/documentation/soloist/reference/downloads-and-updates).
- [Comandos ctl](https://developer.spotify.com/documentation/soloist/reference/soloist-ctl).
- [Áudio Linux](https://developer.spotify.com/documentation/soloist/concepts/overview).
- [OAuth PKCE](https://developer.spotify.com/documentation/web-api/tutorials/code-pkce-flow).
- [Mudanças da Web API em 2026](https://developer.spotify.com/documentation/web-api/references/changes/february-2026).

## Preparar o Soloist no Raspberry

Baixe **pessoalmente** o arquivo ARMv8/AArch64 Linux da página oficial acima.
Extraia em um diretório novo do usuário. Não redistribuímos seu binário ou
arquivo de download e não instalamos serviços. Os builds expiram: a saída 10
é traduzida para uma solicitação de atualização do cliente oficial.

No terminal do Pi, a partir da pasta deste projeto, configure o caminho do
executável que extraiu. O exemplo supõe que você o extraiu nesse diretório:

```bash
export SOLOIST_BIN="$HOME/.local/opt/spotify-soloist/soloist"
test -x "$SOLOIST_BIN" && "$SOLOIST_BIN" --version
bash scripts/spotify.sh soloist-configure
bash scripts/spotify.sh soloist-run
```

`soloist-configure` lê a chave com **entrada oculta**; não a cole em comandos,
arquivos do projeto ou no chat. Se não houver terminal capaz de ocultar a
entrada, a operação é recusada. O segundo campo é um nó PipeWire opcional;
deixe vazio para usar a saída padrão do usuário, sem alterar sua configuração.

O comando manual `soloist-run` mantém o Soloist em primeiro plano, com nome **Raspberry TV**,
volume inicial 35%, cache limitado a 100 MiB e API vinculada apenas a
`127.0.0.1`, em porta dinâmica. Em um Spotify da mesma rede, selecione
**Raspberry TV** em dispositivos e confirme o som. Ctrl+C encerra o processo
iniciado por esse comando. No uso diário, prefira o ícone **Raspberry TV**:
ele inicia o cliente configurado em segundo plano, sem outro terminal.

### Abertura pelo ícone e recuperação

O launcher consulta a API local antes de iniciar o Soloist. Reutiliza um
cliente existente, inclusive um iniciado manualmente; sem cliente acessível,
usa a configuração privada já salva e inicia um supervisor do usuário em
sessão separada. O cliente permanece disponível após fechar a conexão SSH.
Isso também ocorre ao clicar no ícone quando a interface TV já está aberta.

O supervisor mantém uma única instância, tenta recuperar até duas quedas
com esperas de 1 e 3 segundos e aplica pausa de 30 segundos se as tentativas
se esgotarem. O painel utiliza um adaptador compatível com o protocolo atual
para tentar iniciar o cliente quando uma ação local encontra o backend parado.
Nenhum comando de reprodução é enviado apenas por iniciar o cliente.

No Raspberry, o menu contextual do ícone oferece **Parar Spotify**. Essa ação
encerra somente o grupo de processos criado pelo supervisor, após verificar
usuário, PID e instante de criação. Clientes iniciados manualmente são
preservados. Estado de supervisão fica em
`~/.local/state/raspberry-tv/soloist-runtime` (ou XDG_STATE_HOME), com diretório
0700 e arquivos 0600. Chave, identidade Connect e cache permanecem nos mesmos
locais anteriores. Nenhum serviço de boot é instalado.

Se a instalação não tiver chave ou binário válidos, a TV continua disponível
e o painel apresenta falha resumida. Cliente online não comprova áudio na TV;
o diagnóstico de PipeWire/PulseAudio continua necessário quando há silêncio.

O comando abaixo continua disponível para execução manual e diagnóstico:

```bash
DISPLAY=:0 bash scripts/run-native.sh --fullscreen \
  --playlist "$HOME/Playlist_VLC.m3u" \
  --audio-sink alsasink --audio-device 'plughw:CARD=vc4hdmi,DEV=0'
```

ALSA/HDMI acima continua sendo a seleção do **player IPTV**. No Linux, o
Soloist usa PipeWire e, em caso de indisponibilidade, PulseAudio. Sua CLI não
oferece um sink ALSA direto. O DietPi apresentou previamente configuração
PipeWire ausente; não consideramos o áudio Soloist aprovado e não instalamos,
ativamos ou reconfiguramos um servidor de áudio automaticamente. Verificações
curtas, sem mudar serviços: `wpctl status` e/ou `pactl info`, se disponíveis.
Um erro nesses comandos deve ser resolvido antes do teste audível Soloist.

### Diagnosticar ausência de som

Com uma faixa selecionada e o Soloist em execução, rode no Raspberry:

```bash
cd "$HOME/raspberry-tv-spotify"
python3 scripts/diagnose-spotify.py
```

O diagnóstico consulta saídas, mute, volume, fluxos e estado local. Não lê a
chave, sessões privadas ou argumentos dos processos. Não altera configurações.
Em 25/11/2026, o usuário confirmou ausência de som; a saída efetiva ainda
precisa ser identificada. O texto “Tocando” informa o estado do provedor,
sem comprovar som audível na TV.

Depois de identificar o nó real, é possível salvar uma saída apenas para o
Soloist, preservando a chave e o pareamento. Substitua o identificador do
exemplo pelo resultado do diagnóstico:

```bash
bash scripts/spotify.sh soloist-audio --pipewire-device NOME_OU_ID_DO_NO
```

Para o fallback PulseAudio, a preferência do processo pode ser salva com:

```bash
bash scripts/spotify.sh soloist-audio --pulse-sink NOME_DO_SINK
```

Reinicie somente o launcher Soloist com Ctrl+C e `bash scripts/spotify.sh
soloist-run`. A preferência Pulse usa `PULSE_SINK` no ambiente do processo
filho, conforme o [manual PulseAudio](https://manpages.debian.org/trixie/pulseaudio/pulseaudio.1.en.html).
Isso não altera a saída padrão do sistema e não força o backend Pulse quando
PipeWire está disponível. Nomes estáveis são preferíveis a IDs transitórios.
Para voltar ao padrão do usuário, salve `--pipewire-device '' --pulse-sink ''`.
O launcher também preenche o runtime/DBus da sessão existente quando essas
variáveis estão ausentes; não inicia servidores ou serviços de áudio.

## Controle na interface nativa

Abra **APLICATIVOS**. O painel inicia no modo local Soloist:

- D-pad/analógico: selecionar; esquerda/direita: seis linhas por página.
- ×: atualizar estado, consultar fila, ativar dispositivo, retomar, pausar,
  faixa anterior/próxima, volume ±5% ou desativar o dispositivo, conforme linha.
- × em uma faixa da fila: iniciar essa faixa no Soloist.
- Na linha de pausa/retomada, × alterna a ação conforme o estado consultado.
  Depois de pausar, a mesma linha passa a oferecer “Retomar Spotify local”.
- “Abrir playlists e dispositivos da conta”: alterna para a Web API opcional.
- No modo Web API, × em playlist inicia no dispositivo ativo; □ lista suas
  faixas; × em dispositivo transfere a sessão sem iniciar música (`play=false`).
- ○: cancela a solicitação local pendente e retorna; PS: início; Options: ajuda.

Cancelar interrompe a espera e descarta resultados antigos. Um comando já
aceito pelo Spotify pode ter efeito na conta; cancelar não desfaz essa ação.
○/PS não pausam o Spotify automaticamente. Para interromper música, escolha
“Pausar Spotify local” ou “Pausar” antes de voltar à TV. Não associamos nem
sincronizamos músicas com os canais de vídeo.

Após controles locais, consultamos o estado em até cinco leituras, sob prazo
compartilhado de 12 segundos, sem repetir o comando. A retomada tenta ativar
o dispositivo quando ele está explicitamente inativo. Durante a solicitação,
o cabeçalho informa a atualização; uma falha ou cancelamento deixa o estado
sem confirmação, em vez de manter um “Tocando” antigo. Não há polling contínuo.
Confirmação pelo provedor ainda exige verificação audível. Os controles IPTV
permanecem como antes.

## Web API opcional: login PKCE e consultas

No painel Spotify Developer, cadastre exatamente esta Redirect URI:

```text
http://127.0.0.1:8765/spotify/callback
```

Com o Client ID público de 32 caracteres hexadecimais:

```bash
bash scripts/spotify.sh configure
bash scripts/spotify.sh login
```

O login abre o navegador externo e aguarda até 120 segundos em loopback. Não
existe navegador embutido. Para um Pi acessado por SSH, abra uma conexão no
Windows que encaminhe somente essa porta, mantendo-a aberta durante o login:

```powershell
ssh -L 8765:127.0.0.1:8765 balancerec@192.168.100.17
```

Nessa sessão, entre na pasta do projeto e execute
`bash scripts/spotify.sh login --no-browser`. Abra a URL de autorização
mostrada no navegador do Windows. O callback retorna pelo túnel. Não publique
URLs de callback, arquivos privados ou logs de depuração. O servidor só escuta
loopback e verifica `state`, caminho e Host; não registra o código OAuth.

Exemplos de funções disponíveis no terminal, sem imprimir credenciais:

```bash
bash scripts/spotify.sh profile
bash scripts/spotify.sh playlists --offset 0
bash scripts/spotify.sh devices
bash scripts/spotify.sh status
bash scripts/spotify.sh search 'música brasileira'
bash scripts/spotify.sh pause
bash scripts/spotify.sh resume
bash scripts/spotify.sh next
bash scripts/spotify.sh previous
bash scripts/spotify.sh volume 35
bash scripts/spotify.sh logout
```

`items URI_DA_PLAYLIST --offset 20`, `play URI_SPOTIFY` e `transfer ID_DISPOSITIVO`
completam a API. Use URIs oficiais `spotify:playlist:...` ou `spotify:track:...`.
As consultas devolvem protocolo textual percent-encoded, compartilhado com o
C++; não devolvem tokens ou dumps completos da conta. Paginação limitada a
20 registros (busca: 10); as URLs `next` recebidas nunca são seguidas diretamente.
Usamos `/playlists/{id}/items`, com suporte aos campos `item` e legado `track`.
No modo de desenvolvimento, Spotify pode restringir conteúdo a playlists do
próprio usuário, usuários autorizados e permissões do aplicativo.

## Armazenamento, segurança e encerramento

No Pi, usando os padrões XDG:

| Dado | Caminho |
|---|---|
| Chave Soloist | `/home/balancerec/.config/raspberry-tv/spotify/soloist.private.json` |
| Client ID | `/home/balancerec/.config/raspberry-tv/spotify/config.private.json` |
| Tokens OAuth | `/home/balancerec/.config/raspberry-tv/spotify/tokens.private.json` |
| Cooldown da Web API | `/home/balancerec/.config/raspberry-tv/spotify/cooldown.private.json` |
| Sessão/discovery Soloist | `/home/balancerec/.local/share/raspberry-tv/soloist/` |
| Cache Soloist | `/home/balancerec/.cache/raspberry-tv/soloist/` |

As variáveis XDG substituem os diretórios padrão. Diretórios gerenciados POSIX
0700, arquivos de credenciais 0600, substituição atômica com fsync. Não é um
cofre criptografado: proteja a conta do sistema e seus backups. No Windows,
os testes usam ACL herdada do perfil; não certificamos equivalência ao 0600.

**Limitação da CLI oficial Soloist:** exige a chave na opção `--api-key` ao
iniciar. Nosso launcher a lê do arquivo privado e não imprime comando, chave,
stdout/stderr bruto ou exceções com argv. Mesmo assim, os argumentos desse
processo podem ser acessíveis localmente a usuários autorizados pelo sistema.
Não gere screenshots ou publique listagens detalhadas desse processo. Essa
limitação não existe no protocolo entre o helper e a UI; `soloist ctl` não
recebe a chave. Consulte a página oficial de autenticação acima.

O processo de controle é separado do loop SDL, com uma solicitação por vez,
limite de saída 256 KiB, prazo total de 20 s e cancelamento do grupo de
processos. Chamadas `ctl`: até 8 s, resposta até 2 MiB; HTTP: timeout de 5 s
por operação de socket e orçamento de 15 s, além do limite externo do worker.
Nenhum segredo é aceito por argumento do nosso helper.

Tokens OAuth são renovados antes de expirar; um 401 permite uma única renovação.
429 persiste o prazo `Retry-After`, sem loop de repetição. 403 explica Premium,
permissões/allowlist; 404 orienta verificar conteúdo/dispositivo. Respostas de
erro remotas e exceções de rede não são exibidas. Não usamos logs HTTP verbosos.

`logout` remove tokens OAuth locais e preserva o Client ID; não encerra nem
remove a sessão Soloist. Para desfazer esta integração, feche a UI, encerre o
launcher Soloist com Ctrl+C e volte ao checkout RC1 já existente. Para revogar
o acesso remoto, use o painel/conta Spotify; não apagamos estado de outros apps.
A chave fornecida na conversa não foi gravada no projeto, exemplos ou testes.

## Validação

- [x] Testes offline de PKCE, callback, refresh, permissões HTTP, cooldown,
  serialização, metadados, paginação, armazenamento e comandos Soloist.
- [x] Testes C++ de protocolo, navegação, exclusão de comandos duplicados,
  descarte de resultados cancelados e encerramento do worker.
- [x] Teste POSIX de permissões 0700/0600 em arquivos temporários de teste no Pi,
  integrante da suíte Python aprovada pelo CTest #11.
- [x] Teste Linux real do adaptador de processos/cancelamento e CTest completo:
  15/15 PASS, 12,26 s, conforme log do usuário em 25/11/2026.
- [x] Build completo ARM64 da interface alterada, com GCC 14.2 e GStreamer 1.26.2.
- [x] Inicialização SDL reporta renderização interna e saída 1920×1080.
- [ ] Regressão física IPTV/DualShock e comandos Spotify pelo controle.
- [x] Chave Soloist configurada pelo usuário no prompt oculto, conforme log enviado.
- [x] Dispositivo Raspberry TV descoberto e selecionado no Spotify Connect,
  conforme captura enviada pelo usuário em 25/11/2026.
- [ ] Versão e validade do build Soloist registradas por log do Raspberry.
- [ ] Reprodução efetiva e áudio audível pelo dispositivo de saída do Raspberry.
- [ ] OAuth real, playlists da conta e controle de dispositivo real.

Os testes automatizados usam exclusivamente dados artificiais. A evidência do
usuário sobre configuração e seleção do dispositivo é registrada separadamente;
a captura não comprova áudio audível. Consulte `TEST_REPORT.md`.
