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

A tela principal agora é um player de mídia. D-pad/analógico seleciona os
botões; × confirma o botão destacado. Ao abrir, o foco está em reproduzir/pausar.

| Entrada | Ação no player Spotify |
|---|---|
| L1/R1 | Faixa anterior/próxima |
| L2/R2 | Recuar/avançar 10 s, se a faixa permitir |
| □ | Mute/restaurar o volume local anterior |
| △ | Abrir fila local; no modo Web, playlists |
| Options / botão Mais | Abrir ajustes e ações complementares |
| ○ | Cancelar espera e voltar; na fila/ajustes retorna ao player |
| PS | Voltar ao início e cancelar espera |
| Espaço | Reproduzir/pausar |
| Mouse | Clicar em botões; clicar no progresso busca o instante, se permitido |

Os botões de volume alteram 5%. Aleatório e repetição só ficam disponíveis
quando o conteúdo anunciar suporte; repetição percorre desligado/contexto/faixa.
O estado é consultado depois de cada comando local. Confirmação de comando
não comprova som na TV. ○/PS cancelam a espera, mas não desfazem ações já
aceitas pelo Spotify nem pausam música automaticamente.

Fila e ajustes têm navegação paginada própria. × em uma faixa inicia e volta
ao player. Em Mais é possível alternar ao Web API opcional, consultar a conta
e escolher dispositivos; □ em uma playlist lista suas faixas. Mute, busca,
shuffle e repetição da nova barra são locais ao Soloist. O modo Web preserva
pausa/retomada, faixas e volume, com atualização de estado da conta.

Só a tela principal visível consulta estado a cada 10 s, sem requisições
simultâneas. O progresso usa posição/duração/velocidade reais; fica desconhecido
após falha/cancelamento ou 20 s sem confirmação. Abrir o painel não inicia
música automaticamente. [Configuração e requisitos](SPOTIFY.md).
