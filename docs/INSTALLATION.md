# Instalação e rollback — RC1

## Preservar o checkout em uso

O desenvolvimento desta entrega ocorreu no Windows, com importação verificada
da base GitHub `35d8502`. Não foi possível inspecionar alterações locais no Pi.
Não rode reset/clean/pull sobre alterações desconhecidas. A instalação v0.5
em `~/raspberry-tv-iptv` deve continuar intacta.

Para aplicar o patch entregue, primeiro copie-o para o Pi. No checkout existente:

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

Nenhum push/merge foi feito. Após importar/revisar os commits no Pi, o comando
para publicar **apenas se você decidir autorizar a publicação** é:

```bash
git push -u origin feature/native-tv-release-v10
```
