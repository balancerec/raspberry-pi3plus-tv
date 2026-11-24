# Raspberry TV Native — v0.5: canais IPTV reais e navegacao por grupos

Data: 2026-11-23. Branch: \`feature/native-iptv-channels-v05\`.
Base: v0.4 com video local visivel no HDMI e retorno ao menu pelo botao O aprovados pelo usuario.

## Estado da validacao

- [x] Implementada filtragem de canais M3U por \`group-title\`, \`#EXTGRP\` e categoria "Sem categoria".
- [x] Implementado indice de canais filtrados que preserva indices da playlist na lista de favoritos.
- [x] L1/R1 alterna os grupos na tela TV AO VIVO; no restante da interface, conserva a troca de modulo.
- [x] L2/R2 troca de canal, inclusive durante reproducao; percorre apenas os canais do grupo atual e volta ao primeiro/ultimo nos limites.
- [x] Durante video: seta para cima/baixo altera volume de 5 em 5 pontos percentuais, Quadrado alterna mudo, X/Options pausa ou retoma, O interrompe, PS retorna ao inicio.
- [x] Novo comando \`--check-playlist CAMINHO\`: carrega lista local e imprime somente quantidade de canais e de grupos sem iniciar SDL/GStreamer ou exibir URLs.
- [x] Novo teste de regressao \`iptv_group_and_channel_navigation\`: leitura de BOM/CRLF/EXTGRP, ordenacao de grupos, troca circular, indices de favoritos, lista vazia e falha na recarga.
- [ ] Compilacao e CTest 5/5 no Raspberry (ainda nao executados).
- [ ] Testes fisicos de filtro L1/R1, selecao, troca de canais L2/R2, audio e mute no equipamento.
- [ ] Testar listas IPTV locais com enderecos de canais autorizados (HLS, MPEG-TS, RTSP, conforme codecs e plugins disponiveis).
- [ ] Verificar comportamento de URLs expiradas, 404, falhas de autenticacao, timeouts de rede e buffering.
- [ ] Manter v0.4 como rollback sem merge no main.
- [ ] Otimizar pipeline Full HD 1080p30 apos disponibilidade de fonte estavel.

## Configuracao no DietPi — branch e pasta independentes

O clone inicial da origem pode ser single-branch. Use \`fetch\` por ref e um worktree separado:

\`\`\`bash
cd ~/raspberry-pi3plus-tv
git fetch origin refs/heads/feature/native-iptv-channels-v05
git worktree add -b feature/native-iptv-channels-v05 ~/raspberry-tv-iptv FETCH_HEAD
cd ~/raspberry-tv-iptv

cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j2
ctest --test-dir build/native --output-on-failure
\`\`\`

Esperado: **5 testes**, incluindo \`iptv_group_and_channel_navigation\` e \`gstreamer_object_lifecycle\`. Se houver falha, nao inicie a UI ate examinarmos o erro; 5/5 e apenas criterio previsto, nao resultado confirmado.

## Carregar playlist real

A lista deve existir fisicamente no Raspberry Pi. Para uma playlist ja copiada:

\`\`\`bash
cd ~/raspberry-tv-iptv
chmod 600 "$HOME/Playlist_VLC.m3u"
./build/native/raspberry-tv-native --check-playlist "$HOME/Playlist_VLC.m3u"
DISPLAY=:0 ./build/native/raspberry-tv-native --fullscreen --playlist "$HOME/Playlist_VLC.m3u"
\`\`\`

\`chmod\` nao cria o arquivo: primeiro confira \`ls -lh ~/Playlist_VLC.m3u\`.
Para transferir do Windows pelo PowerShell, use \`scp "C:\\caminho\\Playlist_VLC.m3u" balancerec@IP_DO_RASPBERRY:~/Playlist_VLC.m3u\` (ajuste caminho e IP). Nao cole URLs com tokens em chats, logs, screenshots ou commits Git.

No caso de uma lista com milhares de canais, \`--check-playlist\` faz verificacao da sintaxe/contagem, **nao** testa resolucao DNS, disponibilidade, autorizacao, licenca, codec, bitrate ou qualidade dos canais.

## Comandos

| Contexto | DualShock 4 |
|---|---|
| Tela inicial / listas | D-pad ou analogico navega; X confirma |
| TV AO VIVO | L1/R1 alterna grupo; L2/R2 navega canais |
| FAVORITOS | Triangulo adiciona/remove; L2/R2 navega favoritos |
| Video | L2/R2 troca de canal no grupo/lista atual; X ou Options pausa |
| Video | Cima/baixo sobe/desce volume; Quadrado ativa/desativa mudo |
| Video | O para e volta a lista; PS para e retorna ao inicio |

## Restricoes de hardware e seguranca

A fonte de alimentacao atual sera mantida, por decisao do usuario. O ultimo benchmark registrou \`throttled=0x50005\`, com subtensao e limitacao de frequencia ativas. Portanto, testar com um canal por vez, sessoes curtas e sem overclock. Nao desativar as protecoes, nao alterar XFCE/LightDM, HDMI, SSH, rede ou Bluetooth. Nao declarar 1080p30 estavel com base em abertura/PLAYING/EOS de um canal de baixa resolucao.

O \`playbin\` usa a saida de audio padrao do sistema, nao muda globalmente o servidor de audio. A disponibilidade de audio depende do sink, codecs e da saida configurada no DietPi.

## Checklist proxima validacao

1. [ ] Confirmar \`git log -1 --oneline\` na branch correta e \`ctest\` 5/5.
2. [ ] Rodar \`--check-playlist\` com a lista local sem expor URLs.
3. [ ] Abrir TV AO VIVO e alternar grupos com L1/R1.
4. [ ] Abrir dois canais autorizados com X e trocar entre eles com R2/L2.
5. [ ] Confirmar volume, mute e audio real pela TV.
6. [ ] Interromper com O e verificar o retorno visual e navegacao.
7. [ ] Documentar erros de codecs/stream sem registrar credenciais ou tokens.

Versao v0.4 aprovada para comparacao: \`feature/native-iptv-playback-v04\`. Nenhum merge realizado no \`main\`.
