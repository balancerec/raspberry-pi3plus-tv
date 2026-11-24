# Playlists RC1

Importe um arquivo local legível por `--playlist ARQUIVO`. Não há download
remoto automático. `.m3u` e `.m3u8` são aceitos pelo conteúdo; use UTF-8.
Validação sem janela, rede ou reprodução:

```bash
./build-native/raspberry-tv-native --check-playlist "$HOME/iptv/playlist.m3u"
```

Exemplo público sintético (não é um canal funcional):

```m3u
#EXTM3U
#EXTINF:-1 tvg-id="news.hd" tvg-name="Notícias HD" tvg-logo="https://example.invalid/logo.png" group-title="Notícias, Brasil",Notícias HD
https://example.invalid/live.m3u8
```

EXTINF é interpretado respeitando vírgulas entre aspas; nome exibido após a
vírgula tem prioridade, com fallback tvg-name. São preservados group-title,
tvg-name, tvg-id e tvg-logo. Logo é somente metadado, não é baixado/renderizado.
EXTGRP também é aceito. Sem grupo: “Sem categoria”; sem nome: “Canal N”.
BOM inicial e CRLF são tratados; linhas com UTF-8 inválido são ignoradas.

URIs aceitas: http, https, rtsp, rtmp, udp e file. Suporte efetivo depende dos
plugins/codecs. Arquivo local relativo/absoluto existente é convertido em file
URI; espaços/UTF-8 são escapados. Não são executados handlers arbitrários.

Duplicatas são identificadas pela URI completa exata; a primeira ocorrência
prevalece, mesmo se repetida em outro grupo. URLs com queries diferentes não
são deduplicadas, pois podem representar canais diferentes.

Limites: 64 MiB por catálogo, 16 KiB por linha, 50 mil canais na importação.
O limite de canais gera aviso de truncamento. Linhas inválidas/duplicatas são
contadas e a mensagem de resultado informa as quantidades. Arquivo ausente,
vazio, corrompido ou manifesto HLS não substitui o catálogo em uso.

Um manifesto com `#EXT-X-...` é HLS de mídia/variantes, não catálogo IPTV:
adicione a **URL desse manifesto como canal** em um catálogo M3U. Não trate
uma URL de catálogo fornecida pelo provedor como canal; baixe o arquivo de
forma privada fora do app e importe localmente. A RC1 recusa URLs em --playlist.

L1/R1 percorrem grupos, incluindo TODOS. Esquerda/direita paginam oito linhas.
`--filter TEXTO` aplica substring exata e sensível a maiúsculas ao nome/grupo
da TV ao vivo; UTF-8 é mantido. Favoritos mostram sua própria lista. A recarga
manual usa F5 ou Configurações; leitura ocorre em background. Durante vídeo,
pare antes de recarregar para preservar a fila de índices do catálogo.

## Identidades e privacidade

Favoritos não usam índices. Material da identidade:

1. tvg-id, rótulo tvg-name (fallback nome exibido) e grupo, quando há tvg-id;
2. caso contrário, URL sem query/fragmento/credenciais na autoridade, com
   scheme/host em minúsculas, mais o rótulo e o grupo.

O material passa por dois hashes de 64 bits e só o identificador opaco é
gravado. Isso **não é criptografia**. URLs originais e tokens não são salvos
nos favoritos nem no último canal. Somente o caminho do catálogo é lembrado.

Reordenação e rotação de query tokens preservam a identidade. Alterar nome ou
grupo pode mudar a identidade; URLs com tokens no path, sem tvg-id confiável,
também podem mudar. Variantes idênticas em tvg-id/nome/grupo ou em URL-base/
nome/grupo compartilham favorito; o app não pode inferir se uma query é token
ou seletor de canal. Prefira metadados estáveis e distintos para variantes.
Favoritos de canais temporariamente ausentes são mantidos e reaparecem quando
o catálogo volta a incluí-los.

Mantenha playlists em diretório privado, fora do Git; `.gitignore` ignora
M3U/M3U8. Nunca publique listas, URLs de provedores, tokens ou logs verbosos.
