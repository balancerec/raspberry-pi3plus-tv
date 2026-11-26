# Solução de problemas RC1

Comece com `bash scripts/diagnose-native.sh`. Ele não instala dependências,
não altera serviços nem abre streams. Nunca publique playlists ou GST_DEBUG
com URLs privadas.

| Sintoma | Verificação / recuperação |
|---|---|
| `codex: comando não encontrado` | Não é dependência da TV. Use CMake/CTest e o launcher diretamente. |
| Player requer X11 | Execute na sessão XFCE real da TV; não use backend SDL dummy/Wayland para validar HDMI. |
| Falta playbin/glimagesink | Confira plugins/dependências. A UI permanece disponível; nenhum serviço é alterado. |
| 403/404/lista expirada | Atualize seu arquivo local de forma privada e recarregue. Erros de acesso não são tratados como codec. |
| Rede/DNS/timeout | App limita duas reconexões, 1 s/3 s; O cancela e restaura o menu após teardown. |
| Buffering prolongado | Deadline de 30 s; O pode cancelar a qualquer momento. Confira stream/rede/fonte. |
| Codec indisponível | Consulte plugins e formato do canal. Pi 3 não tem HEVC/AV1 presumidos; tente H.264 compatível. |
| Vídeo sem áudio | Verifique se há trilha, mute/volume e saída selecionada. `n-audio>0` não confirma áudio audível. |
| Falha de saída de áudio | Use autoaudiosink ou um sink/dispositivo existente, conferido por diagnóstico; não reconfigure ALSA globalmente. |
| Controle desconectado | Reconecte pelo pareamento já existente. Teclado continua disponível. Não recrie o pareamento automaticamente. |
| Não importa URL M3U | Importação remota fica fora da RC1. Forneça arquivo local. |
| Manifesto HLS rejeitado | A URL de mídia HLS deve ser uma entrada do catálogo, não o catálogo em si. |
| Lista antiga após reload | A nova lista falhou e foi preservada a anterior. Confira mensagem/validação offline. |
| Favorito não reaparece | Confira tvg-id/nome/grupo estáveis e limites descritos em PLAYLISTS.md. Ausentes ficam armazenados. |
| Falha ao salvar | Verifique espaço e permissões do diretório XDG. A gravação anterior é preservada em falha antes do rename. |
| Preferências inválidas | Feche o app, guarde cópia de state.conf e use um `--state-dir` novo. Padrões não fazem autoplay. |
| OSD não visível / conflito WM | Teste `--no-osd`; Options para e exibe ajuda no menu. Não altere compositor/XFCE globalmente. |
| Encerramento lento | Um source/driver pode bloquear teardown. Não destrua a sessão gráfica/serviços; registre estado sem URLs e investigue o plugin. |

## Áudio: plugin ALSA e seleção de dispositivo

Se aparecerem `can't load config client.conf`, `Failed to create PipeWire event
context` ou `Could not query RTKit`, o log não confirma qual saída final de áudio
foi escolhida. autoaudiosink procura sinks disponíveis; um candidato pode falhar
durante essa busca. Não instale/reconfigure PipeWire ou RTKit apenas para
silenciar esses avisos.

Com o app fechado, confira `gst-inspect-1.0 alsasink`, `aplay -l` e `aplay -L`.
O app já aceita `--audio-sink alsasink --audio-device default`; isso seleciona
ALSA no aplicativo, mas `default` não garante HDMI. Para HDMI, use o identificador
real mostrado por `aplay -L`, sem presumir número da placa. O dispositivo pode
estar ocupado por outro aplicativo/servidor; registre o erro antes de alterar
serviços. Um teste curto com audiotestsrc deve usar volume baixo e precisa ser
ouvido na TV. Detalhes: [autoaudiosink](https://gstreamer.freedesktop.org/documentation/autodetect/autoaudiosink.html)
e [alsasink](https://gstreamer.freedesktop.org/documentation/alsa/alsasink.html).

No dispositivo do usuário, `aplay -l/-L` identificou Headphones na placa 0 e
`vc4hdmi`, dispositivo 0, como HDMI. Porém gst-inspect informou que **alsasink
não está instalado**. Com esse sink explicitamente solicitado, o player retorna
FAILED/category=3 antes de iniciar o stream. O pacote Debian que o fornece é
[`gstreamer1.0-alsa`](https://packages.debian.org/trixie/gstreamer1.0-alsa),
já listado nas dependências da instalação. A instalação deve ser executada
manualmente pelo usuário; não é necessário recompilar o aplicativo.

Depois de confirmar a disponibilidade do plugin, a preferência por aplicação
`--audio-sink alsasink --audio-device 'plughw:CARD=vc4hdmi,DEV=0'` seleciona
a placa HDMI identificada, usando conversão PCM do plug ALSA. A presença da
placa e do plugin ainda não confirma áudio audível; testar um tom curto/baixo
e um canal com trilha antes de marcar o requisito como concluído.

## Alimentação/desempenho

O usuário forneceu registro de `get_throttled=0x50005`: bits 0 e 2 ativos
(subtensão e throttling), e 16/18 históricos. A fonte foi mantida por decisão
do usuário. Não desativamos proteção e não aplicamos overclock.

Execute uma amostra curta, sem estresse:

```bash
bash scripts/validate-device.sh
```

Se houver quedas de quadros, compare alimentação/temperatura e o perfil real
do canal. Escolher outro stream H.264 menos exigente é diferente de reduzir
a resolução da interface. Mantenha UI 1920×1080; não troque para kmssink sob
X11 nem altere HDMI globalmente para produzir um benchmark artificial.

O MOTD fornecido identifica “RPi 3 Model B”, enquanto o alvo informado é 3B+.
O diagnóstico lê o modelo do device tree para confirmar; a RC1 não presume
que essa identificação já foi resolvida.

## Verificação de erros críticos

No Raspberry, execute primeiro CTest. Os testes de ownership e player usam
`G_DEBUG=fatal-criticals` para falhar em GStreamer/GLib CRITICAL. O teste real
do player usa fakesinks e WAV sintético: isso valida pipeline/lifecycle, não
vídeo HDMI, áudio audível ou decodificação H.264 do hardware.

Use o launcher para logs normais sem debug de URLs. Ao abrir um chamado,
inclua somente estado/categoria do erro, versões de plugins e a amostra de
saúde; remova caminhos privados, tokens e credenciais.

## Spotify opcional

- Soloist ausente/build expirado: obtenha o build atual ARM64 no link oficial
  de [SPOTIFY.md](SPOTIFY.md), fora do repositório. Não há download automático.
- Nenhum estado local: abra novamente pelo ícone atualizado Raspberry TV.
  Ele inicia o cliente configurado em segundo plano, mesmo com a UI já aberta.
  O painel também tenta recuperar o backend antes de comandos locais. Depois
  selecione Raspberry TV no Spotify. Após três falhas do processo, aguarde
  30 segundos antes de tentar novamente; a TV continua utilizável.
- Soloist sem som: ele usa PipeWire ou PulseAudio, enquanto o IPTV pode usar
  ALSA. `--audio-sink alsasink` do IPTV não configura a saída Soloist. Verifique
  `wpctl status`/`pactl info` se disponíveis. Não mudamos serviços globais.
- Chave Soloist não autentica Web API: são interfaces distintas. Consultar
  playlists exige Client ID e OAuth PKCE opcionais; comandos Soloist não.
- Web API 403: confira Premium, scopes e usuários permitidos no Developer.
  404: confira conteúdo/dispositivo. 429: aguarde o prazo mostrado; não há
  reconexões automáticas infinitas. Sessão revogada: execute login novamente.
- Não publique arquivos privados, tokens, argv do daemon ou logs brutos.
  A UI recebe apenas estado/metadados e mensagens sanitizadas.

## Controles de mídia Spotify

- Ainda aparece a lista antiga: ela pertence ao executável antigo. Execute o
  atualizador explícito do pacote central e aguarde CMake/CTest. A atualização
  só do helper não troca a interface compilada. Depois abra o ícone normal.
- `--:--` ou botão desabilitado: posição/duração ou a capacidade daquela ação
  não foram informadas pelo Soloist. Atualizar tenta obter um novo snapshot.
  Não simulamos tempo nem enviamos busca/aleatório/repetição sem suporte.
- Comando em andamento: ○ cancela a espera; PS retorna ao início. Comandos
  já aceitos pelo Spotify podem ter sido executados. A fila não acumula cliques.
- Novo build não abre: a seleção anterior é restaurada; use o ícone para abrir
  novamente. Também existe Voltar a versão anterior no menu contextual do Pi.
- Não houve som: a barra de mídia não configura a saída de áudio. Preserve o
  diagnóstico PipeWire/PulseAudio descrito em SPOTIFY.md; volume confirmado
  pelo Soloist ainda não prova áudio audível no hardware.

## Connect entra em pausa ao selecionar Raspberry TV

A coleta de 26/11 mostrou o SDK vivo, sem servidor PipeWire/Pulse acessível,
e transição buffering → paused/ativo. O áudio IPTV confirmado usa ALSA direto.
Execute a correção explícita do pacote central descrita em
[SPOTIFY.md](SPOTIFY.md#saída-hdmi-privada). Ela cria saída privada para vc4hdmi,
preserva a chave e não configura o controle PS4 como destino de áudio.

`hdmi_ocupado`: feche a reprodução IPTV ou outro app que detenha esse HDMI e
repita a correção. Ela não encerra servidores de áudio de terceiros.
`modulo_ou_biblioteca_ausente` ou falha APT: o runtime privado não ficou
disponível; a preferência anterior é preservada. Não use sudo nem instale
pacotes sugeridos aleatoriamente. Guarde a mensagem da etapa que falhou.
`Audio ocupado ou cliente externo`: a liberação antes de IPTV não conseguiu
parar um cliente gerenciado. O menu volta ao controle do usuário; o launcher
não encerra processos desconhecidos para forçar o canal.

Após aplicar, o coletor também consulta o socket privado autenticado, sem
expor cookie/token. playing com is_active=false representa estado remoto da
conta; a UI mostra Em outro dispositivo e Play ativa o Raspberry. Não use
esse estado como confirmação de áudio local. Se falhar, mantenha o relatório
e use Voltar a versão anterior no ícone do Pi.
