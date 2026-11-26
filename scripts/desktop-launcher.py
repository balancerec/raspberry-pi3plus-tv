#!/usr/bin/env python3
"""Desktop launcher with a private Soloist supervisor; no boot service."""
import argparse
import contextlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import signal
import socket
import stat
import subprocess
import sys
import tempfile
import time
try:
    import fcntl
except ImportError:
    fcntl = None  # The Windows bridge transports this file; runtime is Linux.

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
             "Categories=AudioVideo;Player;\nActions=StopSpotify;\n" + ENTRY_MARKER + "\n"
             "\n[Desktop Action StopSpotify]\nName=Parar Spotify\n"
             "Exec=python3 " + desktop_argument(script) + " --stop-soloist\n")
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


def session_environment():
    environment = os.environ.copy()
    environment.setdefault("DISPLAY", ":0")
    authority = home_file(".Xauthority")
    if authority.is_file():
        environment.setdefault("XAUTHORITY", str(authority))
    runtime = Path("/run/user") / str(os.getuid())
    if runtime.is_dir() and runtime.stat().st_uid == os.getuid():
        environment.setdefault("XDG_RUNTIME_DIR", str(runtime))
        if (runtime / "bus").is_socket():
            environment.setdefault("DBUS_SESSION_BUS_ADDRESS", "unix:path=" + str(runtime / "bus"))
    return environment


def helper_script():
    return Path(__file__).resolve() if "__file__" in globals() else launcher_dir() / "open-tv.py"


class SoloistSession:
    """Own one detached supervisor; reuse manual clients and retain Connect data."""
    def __init__(self, project, directory=None):
        self.project = Path(project).resolve()
        self.directory = directory or Path(os.environ.get("XDG_STATE_HOME") or home_file(".local/state")) / "raspberry-tv/soloist-runtime"
        self.module = None

    def core(self):
        if self.module is None:
            source = self.project / "integrations/spotify/spotify_client.py"
            spec = importlib.util.spec_from_file_location("raspberry_tv_spotify_client", source)
            if not spec or not spec.loader:
                raise RuntimeError("Integracao Spotify ausente nesta instalacao.")
            self.module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(self.module)
        return self.module

    def prepare(self):
        self.directory.mkdir(parents=True, mode=0o700, exist_ok=True)
        if self.directory.is_symlink() or self.directory.stat().st_uid != os.getuid():
            raise RuntimeError("Diretorio de controle Spotify inseguro.")
        self.directory.chmod(0o700)

    @contextlib.contextmanager
    def locked(self, name, deadline):
        self.prepare()
        descriptor = os.open(self.directory / name, os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
        try:
            while True:
                try:
                    fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    break
                except BlockingIOError:
                    if time.monotonic() >= deadline:
                        raise RuntimeError("Spotify esta iniciando; tente atualizar o estado em instantes.") from None
                    time.sleep(0.05)
            yield
        finally:
            os.close(descriptor)

    def read(self, name):
        try:
            descriptor = os.open(self.directory / name, os.O_RDONLY | os.O_NOFOLLOW)
            with os.fdopen(descriptor, "r", encoding="utf-8") as stream:
                info = os.fstat(stream.fileno())
                if not stat.S_ISREG(info.st_mode) or info.st_uid != os.getuid() or info.st_size > 4096:
                    return {}
                value = json.load(stream)
                return value if isinstance(value, dict) else {}
        except (OSError, ValueError):
            return {}

    def write(self, name, value):
        write_owned(self.directory / name, json.dumps(value).encode())

    @staticmethod
    def identity(pid):
        try:
            process = Path("/proc") / str(pid)
            fields = (process / "stat").read_text().rsplit(") ", 1)[1].split()
            if process.stat().st_uid == os.getuid() and fields[0] != "Z" and os.getpgid(pid) == pid:
                return fields[19]
        except (OSError, IndexError, ValueError):
            pass
        return None

    def alive(self, record):
        pid = record.get("pid")
        return type(pid) is int and pid > 1 and self.identity(pid) == record.get("start") and record.get("start") is not None

    @staticmethod
    def probe_runner(arguments):
        with tempfile.TemporaryFile() as output:
            process = subprocess.Popen(arguments, stdin=subprocess.DEVNULL, stdout=output, stderr=subprocess.DEVNULL)
            try:
                deadline = time.monotonic() + 1
                while process.poll() is None:
                    if time.monotonic() >= deadline or os.fstat(output.fileno()).st_size > 65536:
                        raise RuntimeError("Spotify ainda nao respondeu.")
                    time.sleep(0.02)
                output.seek(0)
                return process.returncode, output.read(65536)
            finally:
                if process.poll() is None:
                    process.kill()
                process.wait()

    def ready(self):
        try:
            core = self.core()
            client = core.Soloist(core.Store())
            data, _ = client.directories()
            with (data / "ws.addr").open(encoding="ascii") as stream:
                address = stream.read(65).strip()
            with (data / "ws.port").open(encoding="ascii") as stream:
                port = int(stream.read(9).strip())
            if address != "127.0.0.1" or not 1 <= port <= 65535:
                return False
            with socket.create_connection((address, port), timeout=0.2):
                pass
            client.runner = self.probe_runner
            client.ctl(["status"])  # Works even before Connect login; never plays.
            return True
        except Exception:
            # Provider exceptions can contain private data; none cross this API.
            return False

    def ensure(self, timeout=6):
        deadline = time.monotonic() + timeout
        with self.locked("startup.lock", deadline):
            if self.ready():
                return
            core = self.core()
            configuration = core.Store().read("soloist.private.json")
            if not configuration.get("api_key"):
                raise RuntimeError("Spotify ainda nao foi configurado neste usuario.")
            core.Soloist(core.Store()).binary()
            if self.read("failure.private.json").get("retry_after", 0) > time.time():
                raise RuntimeError("Spotify encerrou com falha. Aguarde 30 segundos e abra o aplicativo novamente.")
            record = self.read("owner.private.json")
            process = None
            if not self.alive(record):
                arguments = [sys.executable, "-I", str(helper_script()), "--soloist-worker", "--project", str(self.project)]
                process = subprocess.Popen(arguments, env=session_environment(), stdin=subprocess.DEVNULL,
                                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
            while time.monotonic() < deadline:
                if self.ready():
                    return
                if process is not None and process.poll() is not None:
                    break
                time.sleep(0.15)
        raise RuntimeError("Spotify ainda nao ficou disponivel. Atualize o estado em instantes; confira a saida de audio se a falha persistir.")

    def supervise(self):
        with self.locked("worker.lock", time.monotonic()):
            stopped = False

            def stop(signum, frame):
                nonlocal stopped
                stopped = True
                raise KeyboardInterrupt

            previous = signal.signal(signal.SIGTERM, stop)
            owner = {"pid": os.getpid(), "start": self.identity(os.getpid())}
            self.write("owner.private.json", owner)
            try:
                core = self.core()
                for attempt in range(3):
                    try:
                        core.Soloist(core.Store()).run_foreground()
                        if stopped:
                            return
                    except core.SpotifyError as error:
                        if error.code in ("setup", "storage"):
                            break
                    if attempt < 2:
                        time.sleep((1, 3)[attempt])
                self.write("failure.private.json", {"retry_after": time.time() + 30})
            except KeyboardInterrupt:
                pass
            except Exception:
                self.write("failure.private.json", {"retry_after": time.time() + 30})
            finally:
                signal.signal(signal.SIGTERM, previous)
                if self.read("owner.private.json") == owner:
                    (self.directory / "owner.private.json").unlink(missing_ok=True)

    def stop(self):
        with self.locked("startup.lock", time.monotonic() + 4):
            record = self.read("owner.private.json")
            if self.alive(record):
                os.killpg(record["pid"], signal.SIGTERM)
                deadline = time.monotonic() + 3.5
                while self.alive(record) and time.monotonic() < deadline:
                    time.sleep(0.05)
                if self.alive(record):
                    os.killpg(record["pid"], signal.SIGKILL)
            (self.directory / "failure.private.json").unlink(missing_ok=True)


def spotify_request(arguments):
    project = Path(os.environ.get("RASPBERRY_TV_SPOTIFY_PROJECT", ""))
    if not (project / "integrations/spotify/spotify_client.py").is_file():
        project = next((p for p in projects() if (p / "integrations/spotify/spotify_client.py").is_file()), project)
    session = SoloistSession(project)
    try:
        core = session.core()
    except Exception:
        print("RTVSPOTIFY\t1\nstatus\tsetup\tIntegracao%20Spotify%20ausente%20ou%20invalida.")
        return 1
    if arguments[0].startswith("soloist-") and arguments[0] not in ("soloist-configure", "soloist-audio", "soloist-run"):
        try:
            session.ensure()
        except Exception:
            core.emit([("status", "unavailable", "Spotify nao iniciou. Aguarde e tente atualizar; confira configuracao, cliente instalado e saida de audio.")])
            return 1
    return core.main(arguments)


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
        project, binary = candidates[0]
        if (project / "integrations/spotify/spotify_client.py").is_file():
            try:
                SoloistSession(project).ensure()
                print("Spotify local disponivel.", flush=True)
            except Exception:
                print("Spotify ainda indisponivel; a TV sera aberta e podera tentar novamente no painel.", flush=True)
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
        environment = session_environment()
        environment["RASPBERRY_TV_BIN"] = str(binary)
        environment["GST_DEBUG"] = "0"
        environment["RASPBERRY_TV_SPOTIFY_HELPER"] = str(helper_script())
        environment["RASPBERRY_TV_SPOTIFY_PROJECT"] = str(project)
        arguments = [str(binary), "--fullscreen"]
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
    if len(sys.argv) > 1 and not sys.argv[1].startswith("-"):
        return spotify_request(sys.argv[1:])
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--install", action="store_true")
    parser.add_argument("--launch", action="store_true")
    parser.add_argument("--soloist-worker", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--stop-soloist", action="store_true")
    parser.add_argument("--project", type=Path)
    args = parser.parse_args()
    if os.geteuid() == 0:
        parser.error("Execute como usuario da sessao grafica, sem sudo.")
    if args.soloist_worker and args.project is None:
        parser.error("Worker Spotify exige --project.")
    try:
        if args.install:
            install()
        if args.soloist_worker:
            SoloistSession(args.project).supervise()
            return 0
        if args.stop_soloist:
            project = args.project or next((p for p in projects() if (p / "integrations/spotify/spotify_client.py").is_file()), Path.home())
            SoloistSession(project).stop()
            return 0
        if args.launch:
            launch()
        if not args.install and not args.launch:
            parser.print_help()
        return 0
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        message = "Raspberry TV: " + str(error)
        print(message, file=sys.stderr, flush=True)
        if not args.soloist_worker and not os.environ.get("SSH_CONNECTION"):
            dialog = ["zenity", "--error", "--title=Raspberry TV", "--text=" + message] if shutil.which("zenity") else ["xmessage", "-center", message]
            if shutil.which(dialog[0]):
                subprocess.Popen(dialog, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return 1


if __name__ == "__main__":
    sys.exit(main())
