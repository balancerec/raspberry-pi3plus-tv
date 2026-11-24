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
