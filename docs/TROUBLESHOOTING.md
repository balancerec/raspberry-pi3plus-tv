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

## Alimentação/desempenho

### Avisos PipeWire/OpenAL na seleção automática

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
