# Instalação e rollback — RC1

## Preservar o checkout em uso

O desenvolvimento desta entrega ocorreu no Windows, com importação verificada
da base GitHub `35d8502`. Não foi possível inspecionar alterações locais no Pi.
Não rode reset/clean/pull sobre alterações desconhecidas. A instalação v0.5
em `~/raspberry-tv-iptv` deve continuar intacta.

Para aplicar o patch entregue, primeiro copie-o para o Pi. No checkout existente:

A publicação autorizada está na branch remota `feature/native-tv-release-v10`.
Para obter a versão publicada preservando o diretório extraído e a v0.5,
use outro worktree a partir do checkout Git original:

```bash
cd "$HOME/raspberry-tv-iptv"
git status --short --branch
git fetch origin feature/native-tv-release-v10
git worktree add -b feature/native-tv-release-v10 "$HOME/raspberry-tv-release-v10-git" origin/feature/native-tv-release-v10
```

Se essa branch local já existir, inspecione-a antes de continuar. Não substitua
o diretório extraído `~/raspberry-tv-release-v10/raspberry-tv-v10`, que contém
o build realizado pelo usuário. A playlist privada permanece fora do Git.
Os comandos abaixo são a alternativa histórica de importação pelo patch:

```bash
cd "$HOME/raspberry-tv-iptv"
git status --short --branch
git log -5 --oneline
git fetch origin feature/native-iptv-channels-v05
git worktree add -b feature/native-tv-release-v10 "$HOME/raspberry-tv-release-v10" 35d85026a3c1a32b6ce2124a2ce2f91b7851d0b1
cd "$HOME/raspberry-tv-release-v10"
git am "$HOME/raspberry-tv-v10.patch"
```

Se a branch/worktree já existir, inspecione-a antes; não sobrescreva. `git am
--abort` desfaz apenas uma aplicação em andamento no worktree isolado. Depois
de aplicada, use a candidata sem alterar a v0.5. Se usar o arquivo de fontes
em vez do patch, extraia em diretório novo e não copie sobre a instalação atual.

## Dependências

Consulte o diagnóstico antes de instalar qualquer pacote:

```bash
bash scripts/diagnose-native.sh
```

No Debian Trixie, referências de pacotes (instalação **manual e opcional**):

```bash
sudo apt-get install build-essential cmake pkg-config libsdl2-dev libsdl2-ttf-dev \
  libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
  gstreamer1.0-tools gstreamer1.0-gl gstreamer1.0-alsa \
  gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad gstreamer1.0-libav fonts-dejavu-core
```

Não reinstale o que já existe. O script não executa apt/sudo. O player requer
playbin e glimagesink; plugins de fontes/demux/codec variam por stream.
V4L2 só recebe prioridade se o plugin existir; sua presença não garante
suporte a todos os perfis H.264. PulseAudio/PipeWire são opções conforme os
plugins já disponíveis. Não modifique o áudio global do DietPi.

## Build e execução

Use a sessão X11 do próprio usuário, sem root e sem `ssh -X` para testes HDMI:

```bash
cmake -S native -B build-native -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON &&
cmake --build build-native --parallel 1 &&
ctest --test-dir build-native --output-on-failure &&
bash scripts/run-native.sh --fullscreen --playlist "$HOME/iptv/playlist.m3u"
```

Pelo SSH, somente diagnóstico/build/tests headless são apropriados sem a
sessão gráfica real. Não configure DISPLAY/XAUTHORITY de outro usuário por
tentativa. Execute o launcher no terminal XFCE já aberto na TV.

Áudio específico, se necessário e já configurado no sistema:

```bash
bash scripts/run-native.sh --audio-sink alsasink --audio-device 'hw:0,0'
```

`hw:0,0` é um exemplo, não a confirmação de HDMI. Consulte `aplay -l` e valide
ouvindo um canal com trilha. Remova a preferência por `--audio-sink autoaudiosink
--audio-device ''`. O app salva somente a preferência, sem alterar arquivos ALSA.

Para testes sem tocar preferências pessoais, acrescente
`--state-dir "$HOME/.config/raspberry-tv-rc1-test"`. Não há autoplay.

## Pacote ARM64

Depois de aplicar todos os commits, no Raspberry:

```bash
JOBS=1 bash scripts/package-arm64.sh
```

O script exige aarch64 e Git limpo para arquivos rastreados; usa mktemp para um
build totalmente novo, roda CTest e só empacota se passar. Não executado neste
host Windows. O pacote terá `bin/`, `scripts/`, `config/`, `docs/`, logs de build,
RELEASE_ID e SHA256SUMS; também é gerado SHA-256 do tarball. Diretório temporário
de build/logs é preservado para auditoria. Nenhuma instalação automática.

## Instalação/atualização opcional

Feche a aplicação. Extraia o pacote em diretório novo e confira o checksum:

```bash
sha256sum -c raspberry-tv-1.0.0-rc1-COMMIT-arm64.tar.gz.sha256
tar -xzf raspberry-tv-1.0.0-rc1-COMMIT-arm64.tar.gz
cd raspberry-tv-1.0.0-rc1-COMMIT-arm64
bash scripts/install-native.sh
bash "$HOME/.local/opt/raspberry-tv/current/scripts/run-native.sh" --fullscreen
```

Troque COMMIT pelo identificador real de 12 caracteres. `install-native.sh`
verifica o manifesto interno e cria uma release versionada em
`~/.local/opt/raspberry-tv/releases/`. `current` é substituído atomicamente;
`previous` guarda a versão anterior gerenciada. Não sobrescreve releases nem
diretórios sem marcador de propriedade. Não cria .desktop/systemd/autostart.
O mesmo procedimento instala uma atualização com novo identificador.

## Rollback

Antes da primeira instalação gerenciada, rollback significa fechar a candidata
e executar o binário v0.5 que permanece no checkout original:

```bash
cd "$HOME/raspberry-tv-iptv"
./build/raspberry-tv-native --fullscreen --playlist "$HOME/iptv/playlist.m3u"
```

Use o caminho real do binário antigo caso seja diferente. Depois de duas
instalações gerenciadas:

```bash
bash "$HOME/.local/opt/raspberry-tv/current/scripts/rollback-native.sh"
bash "$HOME/.local/opt/raspberry-tv/current/scripts/run-native.sh" --fullscreen
```

Feche a aplicação antes. Dados XDG permanecem; para reverter preferências,
guarde uma cópia de `state.conf` antes da atualização. A v0.5 não usa esse
arquivo. Não faça rollback de sistema operacional nem de serviços.

## Desinstalação

```bash
bash "$HOME/.local/opt/raspberry-tv/current/scripts/uninstall-native.sh" --all
```

Remove somente a árvore gerenciada, validada dentro de HOME, e preserva
favoritos/configurações XDG e o checkout v0.5. Não executado nesta entrega.
Qualquer autostart futuro requer autorização específica e instruções próprias;
nenhum foi criado para a RC1.

## Publicação Git

O usuário autorizou a publicação de código/documentação na branch
`feature/native-tv-release-v10`. A publicação usa o conector GitHub porque
a rede Git direta do agente está bloqueada. Main e v0.5 são preservados;
nenhuma playlist privada ou configuração de usuário faz parte dos commits.

Para atualizações futuras publicadas pelo próprio usuário a partir de um
checkout Git dessa branch:

```bash
git push -u origin feature/native-tv-release-v10
```

## Integração Spotify opcional

Esta branch adicional requer Python 3.9 ou superior para o helper e os novos
testes. Use `python3 --version` para conferir. Compile/teste em um diretório
novo, com a mesma resolução e sem alterar a instalação RC1 já validada:

```bash
cmake -S native -B build-native-spotify -DCMAKE_BUILD_TYPE=Release &&
cmake --build build-native-spotify --parallel 1 &&
ctest --test-dir build-native-spotify --output-on-failure
```

Para executar esse build isolado, defina explicitamente o binário no launcher:

```bash
DISPLAY=:0 RASPBERRY_TV_BIN="$PWD/build-native-spotify/raspberry-tv-native" \
  bash scripts/run-native.sh --fullscreen --playlist "$HOME/Playlist_VLC.m3u"
```

O script informa ao binário onde está o helper Python. A instalação CMake
inclui apenas o helper da nossa aplicação; `package-arm64.sh` inclui seu
launcher e documentação, sem estado XDG, chave ou binário Soloist. Download,
pareamento e saída de áudio local exigem a preparação de [SPOTIFY.md](SPOTIFY.md).
O Client ID Web API é opcional para consulta de playlists; Soloist usa sua
chave própria. Nenhum autostart é instalado. Para rollback, encerre a UI e o
launcher Soloist, depois execute novamente o checkout RC1 anterior.

## Ícone para abertura manual

No Raspberry, uma única instalação do atalho, como usuário da sessão XFCE:

```bash
python3 scripts/desktop-launcher.py --install --launch
```

Depois use **Raspberry TV** no desktop ou no menu de aplicativos. O atalho
tem `Terminal=false`; abre o executável existente em tela cheia, com
`~/Playlist_VLC.m3u` quando presente e a saída IPTV ALSA HDMI já usada nos
testes (`plughw:CARD=vc4hdmi,DEV=0`). Prioriza `~/raspberry-tv-spotify`, seguido
da instalação gerenciada, RC1 isolada e checkout original. Não compila ou
substitui o app. Quando Spotify está configurado, inicia ou reutiliza o
Soloist em segundo plano, com supervisor e tentativas limitadas de recuperação.
Não exige recompilar o binário existente para essa integração pelo ícone.

O launcher instalado fica em
`~/.local/share/raspberry-tv/desktop-launcher/open-tv.py` (ou XDG_DATA_HOME).
Consulta a pasta localizada com `xdg-user-dir DESKTOP` e cria
`raspberry-tv-native.desktop` nela e em `~/.local/share/applications`.
Arquivos existentes de outra origem e links inesperados são preservados.
Uma instância já aberta do executável do usuário evita nova abertura.

No Windows, dê duplo clique em `scripts/install-desktop-windows.cmd`, com
Python instalado em `C:\Python314`, como nesta máquina de desenvolvimento.
No pacote central extraído, `ABRIR_NO_WINDOWS.cmd` chama esse instalador.
O instalador copia os arquivos para `%LOCALAPPDATA%\Programs\RaspberryTV` e
cria **Raspberry TV.lnk** na área de trabalho localizada do Windows. Esse
atalho abre o app na TV por SSH e instala o ícone local do Pi no primeiro uso.
O SSH pode pedir a senha normal em sua própria janela; a senha não é guardada
pelo launcher. No VNC, o ícone local dispensa terminal e SSH.

Nenhum autostart é criado. Para remover, exclua o atalho Windows e a pasta
Programs\RaspberryTV acima. No Pi, remova somente os dois `.desktop` gerados,
identificados por `X-RaspberryTV-Launcher=1`, e o diretório desktop-launcher.
Playlists, favoritos, configurações, chave Soloist e binários são preservados.

Antes de remover o launcher com o Spotify gerenciado ativo, use **Parar
Spotify** no menu contextual do ícone do Raspberry. A alternativa manual é
`python3 ~/.local/share/raspberry-tv/desktop-launcher/open-tv.py --stop-soloist`.
Pode então remover também `~/.local/state/raspberry-tv/soloist-runtime`.
Esse diretório é apenas do supervisor; preserve `~/.local/share/raspberry-tv/soloist`,
que contém a identidade e a sessão Connect. Nenhum serviço do sistema exige
instalação, desinstalação ou rollback por essa mudança.

## Pacote central de fontes

`python scripts/package-source.py --output-dir dist` gera ZIP e TAR versionados
pelo commit, seus SHA-256 e um JSON de procedência. Exporta arquivos do
Git e gera o identificador público `scripts/release.json`; exige checkout limpo, confere SHA256SUMS.txt e recusa configurações
privadas, chaves Soloist e playlists fora das fixtures de teste. Entregas
existentes não são sobrescritas. Não inclui binários nativos ou Soloist.
TV, Spotify, atalhos, diagnóstico, testes e documentação ficam no mesmo pacote.

O empacotamento nativo ARM64 inclui também o launcher Windows, o instalador
do atalho, o ícone e o launcher XFCE; copiar/extrair esse pacote no Windows
permite usar o instalador em `scripts`. A instalação do atalho permanece uma
ação manual, sem hook de instalação do CMake ou autostart.

## Atualizar os controles de mídia

No ZIP central completo, extraia a pasta e execute `ABRIR_NO_WINDOWS.cmd`.
O instalador detecta `scripts/release.json`, gerado pelo empacotador com o
commit e o SHA-256 do manifesto. A janela SSH pode pedir a senha do Pi;
aguarde nela a compilação e os testes. Nenhum comando precisa ser digitado.
O instalador anterior sem esse arquivo continua sendo apenas um launcher.

A atualização baixa somente o código público daquela revisão, por HTTPS,
com limite de 4 MiB/40 s. Valida cada arquivo e recusa links/caminhos externos.
O destino no Pi é
`/home/balancerec/.local/opt/raspberry-tv-media/releases/<commit>/`.
O build usa `--parallel 1`, sem instalar dependências, usar sudo ou alterar
serviços. Falha de download, compilação ou testes preserva a seleção anterior.
Mantenha a janela de atualização aberta até terminar; não há benchmark de carga.

Depois de CTest aprovado, fecha apenas a UI nativa identificada do mesmo
usuário com SIGINT, espera até 8 s e ativa o novo build pelo ícone. Se a UI
não encerrar, a atualização pede que ela seja fechada pelo menu; não usa kill
forçado. Playlists, favoritos, chave, identidade Connect e cache não são copiados
nem apagados. A seleção fica em
`/home/balancerec/.local/share/raspberry-tv/desktop-launcher/project.private.json`.
Essa seleção tem prioridade sobre os diretórios anteriores na abertura normal.

**Rollback:** no desktop do Raspberry, clique com o botão direito em Raspberry
TV e escolha **Voltar a versão anterior**. A ação fecha a UI atual de modo
normal e troca a seleção para o build anterior, preservando ambas as pastas.
Se a nova UI falhar imediatamente na abertura, a seleção anterior é restaurada.
Para remover uma release antiga depois de testá-la, feche a aplicação e remova
somente o diretório do commit identificado por `managed-release.json`; nunca
remova dados XDG do Spotify ou a instalação aprovada original.

A aplicação da atualização e seu áudio no Pi continuam pendentes até execução
real. O agente não teve uma conexão SSH utilizável nesta etapa.

## Corrigir a saída Spotify sem servidor de áudio

Após extrair o ZIP central completo, abra
`CORRIGIR_AUDIO_SPOTIFY_NO_WINDOWS.cmd`. Além de atualizar os helpers dos
ícones, aplica explicitamente a saída privada HDMI. O build e os testes
ocorrem antes de fechar a aplicação existente. Dependências de áudio ausentes
são somente baixadas, verificadas e extraídas no diretório do usuário; não
são instaladas no sistema. É necessário acesso aos repositórios APT já
configurados e ao GitHub. Não solicita a chave Spotify novamente.

O processo pode emitir um tom curto e baixo, abre o aplicativo e preserva
a versão anterior. Confirme música audível e a alternância com IPTV. A ação
**Voltar a versão anterior** do ícone também desabilita o perfil de áudio
privado após parar seus processos gerenciados. Arquivos, limites, remoção e
diagnóstico estão em [SPOTIFY.md](SPOTIFY.md#saída-hdmi-privada).
