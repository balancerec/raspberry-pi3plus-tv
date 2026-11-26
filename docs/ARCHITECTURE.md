# Arquitetura 1.0 RC1

Base mantida: aplicação C++17, SDL2/SDL_ttf, GStreamer playbin, glimagesink e
GstVideoOverlay sobre janela X11. Sem navegador ou novos serviços.

## Responsabilidades

| Componente | Responsabilidade |
|---|---|
| input | Eventos normalizados, edges, histerese e estabilidade de 80 ms; sem repetição por hold |
| menu / channel_queue | Catálogo, índices filtrados, categorias, favoritos e fila da lista de origem |
| playlist / playlist_loader | Parser limitado, metadados/validação e leitura local em worker |
| settings / state_writer | Estado XDG e escrita atômica em worker, último pedido prevalece |
| playback_state | Máquina de estados/deadlines/backoff independente de GStreamer, relógio injetável |
| player | Única thread proprietária de playbin, sinks e bus; abertura, rede, áudio e teardown |
| ui | Full HD, cache LRU limitado de texto, menus e janela OSD separada |
| main | Ciclo SDL, roteamento contextual, hotplug, shutdown e integração dos módulos |

Não há uma classe Network Manager separada: os source plugins tratam rede;
o player controla timeout/backoff e traduz erros para categorias seguras.

## Threads e ownership

A thread SDL não chama set_state nem espera abertura/teardown de streams.
`play`, `stop` e `shutdown` postam um comando; um contador de geração impede
resultados antigos de sobrescrever a solicitação mais recente. Comandos ainda
não iniciados são coalescidos. Nenhum segundo pipeline é aberto em paralelo.

O worker encerra o pipeline anterior antes de iniciar o próximo. Toda referência
flutuante é reclamada com `gst_object_ref_sink`; playbin retém suas referências
dos sinks, e o app libera as suas. Bus e playbin são liberados exatamente uma
vez. SDL/X11 só são destruídos após o join do worker. Shutdown é idempotente.

Timeouts de fonte: HTTP 10 s quando a propriedade é compatível; RTSP TCP 10 s;
UDP emite timeout em 15 s. Abertura tem deadline de 20 s, buffering 30 s sem
extensão ilimitada por progresso parcial. Posição consultável estagnada em
PLAYING por 20 s é tratada como falha. Streams live sem posição dependem do
timeout da fonte/bus; nenhum prazo é imposto à pausa do usuário.

Falhas de rede/EOS remoto têm no máximo duas reconexões, com backoff de 1 s e
3 s. Uma execução estável por 30 s restaura esse orçamento. Erros de acesso,
codec, áudio e decodificação retornam ao menu. Arquivos locais não reconectam.
O cancela qualquer abertura/retry, sem esperar no SDL. Uma chamada de driver
que bloqueie dentro do plugin ainda pode atrasar a execução efetiva do stop ou
o join final; não é seguro matar essa thread e liberar o drawable por baixo dela.

Estados: IDLE → OPENING → BUFFERING/PLAYING ↔ PAUSED → STOPPING → IDLE;
RECONNECTING e FAILED representam falhas; SHUTDOWN é terminal. Alguns streams
não enviam buffering, portanto OPENING → PLAYING é válido.

## UI e vídeo

Renderização lógica e rasterização continuam 1920×1080; coordenadas de design
são multiplicadas por 1,5. HDMI permanece sob controle do XFCE. Redraw somente
por alterações/eventos/minuto. Texto UTF-8 é truncado com reticências por largura,
sem reduzir a fonte para acomodar strings enormes. Cache: até 192 texturas e
aproximadamente 12 MiB de pixels, compartilhado entre os renderers da UI/OSD.

Durante reprodução, nenhum SDL_RenderPresent é feito no drawable do vídeo.
O OSD é uma janela X11 borderless/always-on-top de 144 px, renderizada por SDL
em outro drawable. Mostra canal/grupo/estado/buffer/volume/mute/mensagens;
desaparece após 5 s em PLAYING. Options o reabre com controles. WM/compositor
precisam ser validados no destino. Se indisponível/desabilitado, Options para
o vídeo e abre ajuda no menu. Nenhuma alteração global de compositor é feita.

## Catálogo/persistência

Índices de grupos e favoritos são preparados uma vez por alteração. O desenho
itera apenas oito linhas. A fila de reprodução guarda índices, não cópias de
50 mil canais; não é permitido recarregar catálogo durante a reprodução.
Filtros/ordem da lista de origem são preservados, e favoritos removidos são
ignorados no próximo L2/R2.

Estado em `$XDG_CONFIG_HOME/raspberry-tv/state.conf` (fallback `~/.config`).
POSIX: diretório 0700, temporário exclusivo 0600, write/fsync/rename/fsync do
diretório. No Windows, `_commit` + MoveFileEx com replace/write-through.
Escritas são agrupadas após 500 ms sem ações e fora do SDL; a saída faz flush
do último estado. Abrir duas instâncias usa política último escritor prevalece.
Arquivo inválido recupera campos válidos/padrões, nunca habilita autoplay.
Detalhes das identidades e seus limites estão em PLAYLISTS.md.

## Diagnóstico e privacidade

Mensagens do app não contêm URI, debug strings GStreamer ou caminhos privados.
Identificadores de favoritos são hashes opacos, não mecanismos de criptografia.
O wrapper desativa logs GST_DEBUG herdados. Logs verbosos de plugins podem
expor tokens se o usuário os habilitar diretamente; não publique esses logs.

Sem medição de desempenho nesta RC1 no Pi. Dados históricos de v0.3/v0.5
permanecem nos documentos originais e não são extrapolados para esta versão.

## Integração Spotify opcional (25/11/2026)

`spotify.cpp` mantém protocolo limitado, painel de ações e worker serializado,
com uma operação ativa e cancelamento. Nenhuma credencial atravessa esse
protocolo. `spotify_process.cpp` inicia Python sem shell, com argv literal,
stdout limitado, stderr suprimido, grupo de processos exclusivo e prazo de
20 segundos. Cancelamento/encerramento matam e recolhem esse grupo; não
interrompem a thread SDL. O teste Linux específico está preparado, não
executado neste host Windows.

`integrations/spotify/spotify_client.py` usa somente biblioteca padrão Python.
O adaptador Soloist chama a CLI oficial `ctl` contra loopback; o cliente de
áudio oficial é instalado e iniciado manualmente pelo usuário. Não há novo
player de áudio no processo C++, serviço de boot ou dependência web na UI.
O launcher Soloist em primeiro plano é separado da UI e possui seu próprio
encerramento Ctrl+C. IPTV mantém seu worker GStreamer e sua seleção de áudio.

A Web API opcional usa PKCE S256, callback loopback e state, token/refresh
atômicos privados, bloqueio entre operações e cooldown persistente em 429.
API aceita somente destinos HTTPS oficiais e não segue redirecionamentos
portando Bearer. Consultas são paginadas, sem baixar toda a biblioteca e sem
polling contínuo. A chave Soloist tem armazenamento separado dos tokens OAuth.
Segredos reais não integram código/testes/pacotes. Limitação de argv do
executável Soloist, áudio e instruções estão em [SPOTIFY.md](SPOTIFY.md).
