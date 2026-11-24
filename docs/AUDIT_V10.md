# Auditoria da base v0.5 — 2026-11-24

Base: `35d85026a3c1a32b6ce2124a2ce2f91b7851d0b1`, branch
`feature/native-iptv-channels-v05`. Histórico consultado pelo conector GitHub
(80 commits até a raiz). Cópia local importada superficialmente pela API:
blobs, árvores e objeto do commit verificados pelos SHA-1 originais. A nova
branch tem esse commit como ancestral, sem alterar o remoto.

O checkout do Raspberry e suas alterações locais não puderam ser inspecionados:
o agente está no Windows, sem SSH/rede direta/WSL autorizados pelo ambiente.
Nenhum arquivo ou serviço do dispositivo foi alterado.

## Verificação anterior às mudanças

- CMake configurou o núcleo sem SDL/GStreamer com MSVC 19.51.
- Build original falhou por `-Wextra` incompatível com MSVC.
- Compilação direta dos quatro testes originais, sem alterar fontes, seguida
  pelo CTest gerado: **4/4 aprovados** (0,81 s).
- Teste GStreamer original: indisponível neste host; não executado.

## Problemas confirmados no código

1. Favoritos por índice, apagados no reload; nenhuma configuração persistida.
2. Parser aceita qualquer linha como URL; perde tvg-id/name/logo e não distingue
   um manifesto HLS de catálogo IPTV. Sem deduplicação/limite de bytes.
3. Leitura da playlist e transições/teardown de GStreamer no loop SDL.
4. Sem deadline de abertura/buffering ou política limitada de reconexão.
5. Loop não desenha durante reprodução: HUD SDL não pode sobrepor o mesmo
   drawable X11 com segurança. Preservar glimagesink e implementar OSD separado.
6. Recriação de todas as texturas de texto a cada redraw.
7. Options pausa durante vídeo, sem ajuda contextual; teclado só sem controle.
8. Script `run-linux.sh` executa o servidor web legado, não o aplicativo nativo.
9. Teste antigo de ownership testa objetos avulsos, não a implementação do player.

## Decisões

Preservar C++17/SDL2, resolução interna 1920×1080, glimagesink/VideoOverlay,
preferência V4L2 e ownership explícito. Isolar chamadas GStreamer numa única
thread; comandos novos substituem pedidos antigos ainda não iniciados.
Persistência XDG privada/atômica; carregamento de arquivo local em worker
com limites. Não importar playlists remotas nesta release. Validar o núcleo
no host e fornecer testes reais do player e pacote ARM64 gerável no destino.
