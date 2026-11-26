# Controles RC1

| Entrada | Lista/menus | Reprodução, abertura e reconexão |
|---|---|---|
| D-pad / analógico esquerdo | Navegação discreta | Cima/baixo: volume 5% |
| × | Confirmar / reproduzir | Pausar/retomar quando o estado permitir |
| ○ | Voltar / fechar ajuda | Parar/cancelar imediatamente a solicitação |
| PS | Início | Parar e voltar ao início |
| L1/R1 | Grupo anterior/próximo em TV ao vivo | Sem ação |
| L2/R2 | Canal anterior/próximo | Trocar dentro da lista/grupo/favoritos de origem |
| △ | Alternar favorito; no início abre Favoritos | Alternar favorito do canal em reprodução |
| □ | Detalhes do canal, sem URL | Mute; volume anterior preservado |
| Options | Ajuda | Ajuda no OSD; sem OSD, para e abre ajuda no menu |
| Esquerda/direita | Pular página de oito canais | Sem ação |

Configurações permite alternar tela cheia, OSD, lembrança da seleção,
recarregar playlist e encerrar o aplicativo. O sempre fica disponível durante
abertura/buffering/backoff; PS faz o mesmo e retorna ao início.

## Teclado simultâneo

Setas, Enter, Escape, Home e F1 equivalem a navegação/×/○/PS/Options.
PageUp/PageDown: grupos. `[`/`]`: canais. F: favorito; M: detalhes/mute.
F5 recarrega a playlist (pare o vídeo antes). Ctrl+Q encerra. Repetição de
teclas do sistema é ignorada; teclado e gamepad têm estados independentes.

## Bluetooth

O app usa o pareamento existente. Não executa bluetoothctl nem altera serviços.
Abre apenas um gamepad principal; ignora eventos de outros IDs e eventos JOY
duplicados. Ao remover o principal, limpa botões/eixos/ações pendentes e tenta
outro controle já conectado. Um evento de adição posterior permite reconectar.
Não existe auto-repeat ao manter direção; analógico usa histerese e 80 ms
de estabilidade, conforme correção validada na v0.5.

Os testes simulam o estado normalizado/desconexão e wrap do relógio SDL.
Reconexão Bluetooth física, PS/Guide e mapeamento real da RC1 estão pendentes.

## APLICATIVOS / Spotify nesta branch

D-pad seleciona; esquerda/direita percorrem seis linhas; × executa a ação da
linha. No modo local Soloist: estado, fila, ativar/desativar dispositivo,
pausa/retomada, próxima/anterior e volume ±5%. × em uma faixa da fila inicia
a faixa. A última ação alterna para o modo Web API opcional, com playlists e
dispositivos da conta. Nesse modo, □ em playlist lista as faixas; × reproduz
no dispositivo ativo ou seleciona um dispositivo sem autoplay.

A linha de pausa/retomada local alterna com × após consultar o estado do
Soloist. Depois de pausar, × na mesma linha retoma. O cabeçalho aguarda a
confirmação do estado; falha ou cancelamento não mantém “Tocando” antigo.
Confirmação de estado não comprova áudio na saída física.

○ retorna e cancela a espera; PS volta ao início; Options mostra ajuda
contextual. Esses botões não desfazem ações já aceitas pelo Spotify nem pausam
música automaticamente. Use a linha Pausar antes de abrir IPTV quando quiser
interromper a música. [Configuração e requisitos](SPOTIFY.md).
