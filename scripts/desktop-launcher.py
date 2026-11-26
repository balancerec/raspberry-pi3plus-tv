#!/usr/bin/env python3
"""Manual desktop launcher. No root, services, credentials or autostart."""
import argparse
import fcntl
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

MARKER = "Raspberry TV desktop launcher v1\n"
ENTRY_MARKER = "X-RaspberryTV-Launcher=1"
ICON = '''<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 256 256">
<rect x="12" y="24" width="232" height="176" rx="30" fill="#102638"/>
<rect x="26" y="38" width="204" height="148" rx="20" fill="#54ddba"/>
<path d="M91 69h43c24 0 36 12 36 32 0 15-7 25-20 30l25 31h-29l-22-28h-9v28H91zm24 22v22h18c8 0 12-4 12-11s-4-11-12-11z" fill="#102638"/>
<path d="M112 200h32v18h33v14H79v-14h33z" fill="#102638"/>
</svg>'''


def home_file(relative):
    return Path.home() / relative


def launcher_dir():
    data = Path(os.environ.get("XDG_DATA_HOME") or home_file(".local/share"))
    return data / "raspberry-tv/desktop-launcher"


def write_owned(path, content, mode=0o600):
    if path.is_symlink():
        raise RuntimeError("Arquivo de atalho e um link inesperado; preservado: " + str(path))
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(prefix=".raspberry-tv-", dir=path.parent)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
            os.fchmod(stream.fileno(), mode)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def desktop_directory():
    if shutil.which("xdg-user-dir"):
        try:
            result = subprocess.run(["xdg-user-dir", "DESKTOP"], stdout=subprocess.PIPE,
                                    stderr=subprocess.DEVNULL, text=True, timeout=3)
            candidate = Path(result.stdout.strip())
            if result.returncode == 0 and candidate.is_absolute() and candidate != Path.home():
                return candidate
        except (OSError, subprocess.TimeoutExpired):
            pass
    for name in ("Desktop", "Área de trabalho", "Área de Trabalho"):
        candidate = home_file(name)
        if candidate.is_dir():
            return candidate
    return home_file("Desktop")


def desktop_argument(value):
    # Desktop Entry escaping has both string-value and Exec quoting layers.
    value = str(value).replace("%", "%%")
    for character in ("\\", '"', "`", "$"):
        value = value.replace(character, "\\" + character)
    return '"' + value.replace("\\", "\\\\") + '"'


def install():
    directory = launcher_dir()
    if directory.is_symlink():
        raise RuntimeError("Diretorio de atalho inesperado; nenhuma alteracao realizada.")
    marker = directory / "managed.txt"
    if directory.exists() and any(directory.iterdir()):
        if not marker.is_file() or marker.is_symlink() or marker.read_text() != MARKER:
            raise RuntimeError("Diretorio existente nao pertence a este instalador; preservado.")
    directory.mkdir(parents=True, mode=0o700, exist_ok=True)
    source = globals().get("SOURCE_BYTES")
    if source is None:
        source = Path(__file__).read_bytes()
    write_owned(marker, MARKER.encode())
    script = directory / "open-tv.py"
    icon = directory / "raspberry-tv.svg"
    write_owned(script, source, 0o700)
    write_owned(icon, ICON.encode(), 0o644)
    entry = ("[Desktop Entry]\nType=Application\nName=Raspberry TV\n"
             "Comment=TV ao vivo e aplicativos na televisao\n"
             "Exec=python3 " + desktop_argument(script) + " --launch\n"
             "Icon=" + str(icon) + "\nTerminal=false\nStartupNotify=false\n"
             "Categories=AudioVideo;Player;\n" + ENTRY_MARKER + "\n")
    applications = directory.parent.parent / "applications"
    for parent in (desktop_directory(), applications):
        target = parent / "raspberry-tv-native.desktop"
        if target.exists() and (target.is_symlink() or ENTRY_MARKER not in target.read_text(encoding="utf-8")):
            raise RuntimeError("Atalho existente de outra origem foi preservado: " + str(target))
        write_owned(target, entry.encode(), 0o755)
        print("Icone instalado:", target, flush=True)
        if shutil.which("gio"):
            try:
                subprocess.run(["gio", "set", str(target), "metadata::trusted", "true"],
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=3, check=False)
            except (OSError, subprocess.TimeoutExpired):
                pass


def projects():
    return [home_file("raspberry-tv-spotify"),
            home_file(".local/opt/raspberry-tv/current"),
            home_file("raspberry-tv-release-v10/raspberry-tv-v10"),
            home_file("raspberry-tv-iptv")]


def launch():
    candidates = [(project, binary) for project in projects()
                  for binary in (project / "bin/raspberry-tv-native", project / "build-native/raspberry-tv-native")
                  if binary.is_file() and os.access(binary, os.X_OK) and (project / "scripts/run-native.sh").is_file()]
    if not candidates:
        raise RuntimeError("Executavel nao encontrado nas instalacoes existentes. Nenhum build foi iniciado.")
    directory = launcher_dir()
    directory.mkdir(parents=True, mode=0o700, exist_ok=True)
    with (directory / "launch.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        targets = {binary.resolve() for _, binary in candidates}
        for process in Path("/proc").iterdir():
            if not process.name.isdigit():
                continue
            try:
                if process.stat().st_uid == os.getuid() and (process / "exe").resolve() in targets:
                    print("Raspberry TV ja esta aberta.", flush=True)
                    return
            except (OSError, RuntimeError):
                continue
        project, binary = candidates[0]
        environment = os.environ.copy()
        environment.setdefault("DISPLAY", ":0")
        authority = home_file(".Xauthority")
        if authority.is_file():
            environment.setdefault("XAUTHORITY", str(authority))
        environment["RASPBERRY_TV_BIN"] = str(binary)
        runtime = Path("/run/user") / str(os.getuid())
        if runtime.is_dir() and runtime.stat().st_uid == os.getuid():
            environment.setdefault("XDG_RUNTIME_DIR", str(runtime))
            if (runtime / "bus").is_socket():
                environment.setdefault("DBUS_SESSION_BUS_ADDRESS", "unix:path=" + str(runtime / "bus"))
        arguments = ["bash", str(project / "scripts/run-native.sh"), "--fullscreen"]
        playlist = home_file("Playlist_VLC.m3u")
        if playlist.is_file():
            arguments += ["--playlist", str(playlist)]
        arguments += ["--audio-sink", "alsasink", "--audio-device", "plughw:CARD=vc4hdmi,DEV=0"]
        log = directory / "last-launch.log"
        descriptor = os.open(log, os.O_WRONLY | os.O_CREAT | os.O_TRUNC | os.O_NOFOLLOW, 0o600)
        with os.fdopen(descriptor, "wb") as output:
            process = subprocess.Popen(arguments, cwd=project, env=environment, stdin=subprocess.DEVNULL,
                                       stdout=output, stderr=subprocess.STDOUT, start_new_session=True)
        for _ in range(5):
            time.sleep(0.2)
            if process.poll() is not None:
                raise RuntimeError("O aplicativo encerrou na abertura. Registro privado: " + str(log))
        print("Raspberry TV aberta na televisao. A conexao SSH pode ser fechada.", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--install", action="store_true")
    parser.add_argument("--launch", action="store_true")
    args = parser.parse_args()
    if os.geteuid() == 0:
        parser.error("Execute como usuario da sessao grafica, sem sudo.")
    try:
        if args.install:
            install()
        if args.launch:
            launch()
        if not args.install and not args.launch:
            parser.print_help()
        return 0
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        message = "Raspberry TV: " + str(error)
        print(message, file=sys.stderr, flush=True)
        if not os.environ.get("SSH_CONNECTION"):
            dialog = ["zenity", "--error", "--title=Raspberry TV", "--text=" + message] if shutil.which("zenity") else ["xmessage", "-center", message]
            if shutil.which(dialog[0]):
                subprocess.Popen(dialog, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return 1


if __name__ == "__main__":
    sys.exit(main())
