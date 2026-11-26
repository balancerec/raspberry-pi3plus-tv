# Raspberry TV Native — 1.0.0-rc1

TV Box nativa C++17/SDL2/GStreamer para Raspberry Pi, DietPi/Debian Trixie
ARM64, XFCE/LightDM/X11 e controle DualShock 4. Interface interna **1920×1080**.

**Status: candidata à validação física, não aprovada para uso diário ainda.**
Após o hotfix X11, o usuário compilou a aplicação completa no Raspberry e
executou **12/12 testes com sucesso (4,46 s)**. O log da UI informa saída e
renderização 1920×1080 e reconhecimento do PS4. Vídeo visível, áudio audível,
navegação e reconexão física ainda exigem confirmação. A playlist real do
usuário carregou **398 canais, 0 inválidos, 0 duplicados, sem truncamento**.
Foram relatados canais indisponíveis e travamentos durante o uso. A alimentação
inadequada é uma hipótese relevante, não uma causa exclusiva comprovada.
Consulte [o relatório real](docs/TEST_REPORT.md) e [o checklist](docs/RELEASE_CHECKLIST.md).

## Spotify com controles de mídia

APLICATIVOS abre um player nativo com faixa, artista, progresso, anterior,
reproduzir/pausar, próxima, mute e volume. Fila, dispositivo e ajustes ficam
separados. Funciona com DualShock, teclado e mouse. Shuffle, repetição e busca
no tempo dependem das capacidades anunciadas pelo Soloist; tempo desconhecido
aparece como `--:--`. [Controles](docs/CONTROLS.md#aplicativos--spotify-nesta-branch).

No ZIP central, `ABRIR_NO_WINDOWS.cmd` aplica esta atualização explicitamente:
baixa a revisão pública fixada pelo pacote, verifica o manifesto, compila com
um processo e executa CTest em um diretório novo do usuário no Pi. Só então
altera a versão aberta pelo ícone. A versão anterior fica disponível pelo
menu **Voltar a versão anterior** do ícone no Raspberry. Nas próximas vezes,
abra pelo ícone normal; ele não recompila. [Detalhes](docs/INSTALLATION.md#atualizar-os-controles-de-mídia).

## Implementado

- Catálogos locais M3U/M3U8: UTF-8, EXTINF/tvg metadata, grupos, filtro textual
  opcional, paginação de oito linhas, deduplicação e limites de leitura.
- Carregamento/recarregamento em background; falha preserva a lista anterior.
- Favoritos e seleção por identidade estável; volume, mute e preferências
  em arquivo XDG privado, com substituição atômica e fsync fora do loop SDL.
- Player com worker exclusivo, estados explícitos, deadlines, até duas
  reconexões, cancelamento e substituição de solicitações obsoletas.
- V4L2 H.264 preferido quando disponível; glimagesink/VideoOverlay preservado.
- OSD em janela X11 separada, sem SDL desenhar no drawable do vídeo.
- Navegação por controle, reconexão por eventos SDL e teclado simultâneo.
- Seis módulos preservados. Biblioteca continua fora do escopo. Na branch
  Spotify, APLICATIVOS oferece controle Soloist local e Web API opcional.

## Pacote central: TV, Spotify e ícone de abertura

O pacote central inclui a interface TV, integração Spotify, diagnóstico e
ícones de abertura manual. No Windows, extraia o ZIP completo e dê duplo
clique em **ABRIR_NO_WINDOWS.cmd**; ele instala o atalho **Raspberry TV**.
O primeiro acesso conectado também instala o ícone no desktop do Raspberry.
Pelo VNC, esse ícone abre o executável existente sem terminal. O SSH pode
solicitar sua senha quando a abertura parte do Windows; ela não é salva.
O ícone também inicia o Soloist configurado em segundo plano, reutiliza
instâncias existentes e tenta recuperar quedas com limite. O cliente continua
disponível após fechar SSH; o menu do ícone permite parar o Spotify gerenciado.
Nenhum autostart de boot é criado. [Instalação e remoção](docs/INSTALLATION.md#ícone-para-abertura-manual).

O ZIP/TAR de fontes contém código compilável, sem executável ARM64 ou cliente
Soloist oficial. Para gerar uma cópia auditada a partir de um checkout limpo:
`python scripts/package-source.py`. O pacote nativo ARM64, gerado no Pi por
`scripts/package-arm64.sh`, também inclui os helpers e arquivos dos atalhos.

## Spotify opcional

A integração `feature/native-spotify-integration`, incluída na branch central
`feature/native-tv-release-v10`, adiciona funções nativas em APLICATIVOS:
controle Soloist local (estado, fila, pausa/retomada, faixas e volume) e Web API
opcional (playlists, faixas e dispositivos da conta, OAuth PKCE). O Soloist
oficial é instalado separadamente pelo usuário; nenhuma chave ou binário
Spotify é distribuído. [Configuração, comandos e limites](docs/SPOTIFY.md).

Esta integração passou por testes offline. Em 25/11/2026, o usuário configurou
a chave pelo prompt oculto e enviou captura com **Raspberry TV selecionado no
Spotify Connect**. Posteriormente, forneceu o log de compilação completa ARM64
da nova UI e **15/15 CTest aprovados em 12,26 s**, com renderização e saída
1920×1080 reportadas pelo SDL. Ainda faltam áudio audível, controles físicos
da integração e login OAuth opcional. Os 12/12 testes anteriores referem-se
à RC1 antes da integração. Nenhum serviço foi modificado por esta integração.

A foto posterior confirma o painel APLICATIVOS na TV e o indicador PS4
conectado. O usuário relatou pausa funcionando e **ausência de som**. A
correção de pausa/retomada e estado foi testada no host; seu novo build no Pi
e a identificação da saída de áudio continuam pendentes. Consulte o
[diagnóstico Spotify](docs/SPOTIFY.md#diagnosticar-ausência-de-som).

## Compilar e iniciar manualmente no Raspberry

Na sessão XFCE do usuário, em um **checkout isolado** desta branch:

```bash
bash scripts/diagnose-native.sh
cmake -S native -B build-native -DCMAKE_BUILD_TYPE=Release &&
cmake --build build-native --parallel 1 &&
ctest --test-dir build-native --output-on-failure &&
bash scripts/run-native.sh --fullscreen --playlist "$HOME/iptv/playlist.m3u"
```

Não é necessário instalar `codex` no Raspberry para compilar/executar.
Nunca use sudo para executar a aplicação. Não habilitamos autostart nem
modificamos serviços, vídeo global, Bluetooth ou áudio do DietPi.

X confirma/reproduz/pausa; O para/cancela; PS retorna ao início; L1/R1 filtram
grupos; L2/R2 trocam canais da lista de origem; △ favorita; □ mostra detalhes
na lista ou alterna mute no vídeo; Options abre ajuda. [Controles completos](docs/CONTROLS.md).

O arquivo da última playlist é lembrado, mas **nenhum canal inicia sozinho**.
Recarregue com F5 ou Configurações → Recarregar playlist local.

## Dependências e entrega

[Instalação, atualização, desinstalação e rollback](docs/INSTALLATION.md).
`scripts/package-arm64.sh` faz build limpo/CTest no aarch64 e gera pacote
com binário, documentação, defaults, logs e SHA-256. Não instala nem inicia.
Instalação opcional em `~/.local/opt/raspberry-tv`, sem afetar a v0.5 existente.

O servidor Python/arquivos web e a unit systemd legados permanecem no
repositório por compatibilidade histórica. Não são usados pelo launcher nativo
e nenhuma unit deve ser habilitada durante esta validação.

## Limites conhecidos

- Fonte apresentou `get_throttled=0x50005`: subtensão e throttling ativos e
  históricos. Não há medição nova de FPS/temperatura/memória nesta entrega.
- Não prometemos 1080p30 contínuo. Escolha um canal compatível/menos exigente
  quando necessário; a interface continua Full HD. Sem overclock/kmssink.
- Codecs/protocolos dependem dos plugins. Não presumimos HEVC/AV1.
- A apresentação do OSD depende do WM X11 e precisa ser validada no XFCE.
  `--no-osd` desativa; Options então para o canal e exibe ajuda no menu.
- Chamadas nativas de plugins rodam fora do SDL, mas um driver que bloqueie
  indefinidamente pode atrasar teardown/encerramento. Não destruímos a janela
  enquanto o pipeline ainda pode usá-la.
- Áudio disponível/volume configurado não prova áudio audível: validação na TV pendente.

Mais detalhes: [arquitetura](docs/ARCHITECTURE.md), [playlists](docs/PLAYLISTS.md),
[solução de problemas](docs/TROUBLESHOOTING.md), [changelog](CHANGELOG.md).
