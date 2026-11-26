"""App-private PulseAudio bridge to HDMI. Never installs a system package/service."""
from __future__ import annotations
import contextlib
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import tarfile
import tempfile
import threading
import time
import uuid
try:
    import fcntl
except ImportError:
    fcntl = None

OWNER = "raspberry-tv-private-pulse-v1"
FOCUS_MARKER = "raspberry-tv-private-hdmi-v1\n"
SINK = "raspberry_tv_hdmi"
DEVICE = "plughw:CARD=vc4hdmi,DEV=0"
MAX_BYTES = 80 * 1024 * 1024


def config_directory():
    return Path(os.environ.get("XDG_CONFIG_HOME") or Path.home() / ".config") / "raspberry-tv/spotify"


def bundle_directory():
    return Path(os.environ.get("XDG_DATA_HOME") or Path.home() / ".local/share") / "raspberry-tv/audio-runtime"


def private_directory(path):
    path = Path(path)
    path.mkdir(parents=True, exist_ok=True, mode=0o700 if os.name == "posix" else 0o755)
    if path.is_symlink() or not path.is_dir() or (os.name == "posix" and path.stat().st_uid != os.getuid()):
        raise RuntimeError("Diretorio de audio privado inseguro; nenhuma configuracao global alterada.")
    if os.name == "posix":
        path.chmod(0o700)
    return path


def atomic_write(path, data):
    private_directory(path.parent)
    if path.is_symlink():
        raise RuntimeError("Arquivo de audio e um link inesperado; preservado.")
    fd, name = tempfile.mkstemp(prefix=".rtv-audio-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
            if os.name == "posix":
                os.fchmod(stream.fileno(), 0o600)
        os.replace(name, path)
    finally:
        if os.path.exists(name):
            os.unlink(name)


def profile(directory=None):
    path = (Path(directory) if directory is not None else config_directory()) / "audio.private.json"
    if not path.exists():
        return None
    if path.is_symlink() or path.stat().st_size > 4096:
        raise RuntimeError("Perfil de audio privado invalido.")
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict) or value.get("owner") != OWNER:
        raise RuntimeError("Perfil de audio de outra origem preservado.")
    if value.get("enabled") is not True:
        return None
    prefix = value.get("bundle", "")
    if not isinstance(prefix, str):
        raise RuntimeError("Caminho do runtime de audio invalido.")
    if prefix:
        path = Path(prefix)
        if not path.is_absolute() or not path.resolve().is_relative_to(bundle_directory().resolve()) or path.is_symlink():
            raise RuntimeError("Runtime de audio fora do diretorio gerenciado.")
        if not (path / "managed.txt").is_file() or (path / "managed.txt").read_text() != OWNER:
            raise RuntimeError("Runtime de audio sem identificacao valida.")
    return value


def set_profile(value):
    current = config_directory() / "audio.private.json"
    if current.exists():
        profile()  # Refuse a foreign file before replacing it.
    value = dict(value, owner=OWNER, enabled=True)
    atomic_write(current, (json.dumps(value) + "\n").encode())
    atomic_write(config_directory() / "audio-focus-v1", FOCUS_MARKER.encode())


def disable_profile():
    path = config_directory() / "audio.private.json"
    if path.exists():
        profile()
        atomic_write(path, (json.dumps({"owner": OWNER, "enabled": False}) + "\n").encode())
    marker = config_directory() / "audio-focus-v1"
    if marker.is_file() and not marker.is_symlink() and marker.read_text() == FOCUS_MARKER:
        marker.unlink()


def command(arguments, *, environment=None, cwd=None, timeout=20, limit=1024 * 1024):
    """Bounded public-tool output; never returns native stderr or secret argv."""
    with tempfile.TemporaryFile() as output:
        process = subprocess.Popen(arguments, stdin=subprocess.DEVNULL, stdout=output,
                                   stderr=subprocess.DEVNULL, env=environment, cwd=cwd)
        try:
            deadline = time.monotonic() + timeout
            while process.poll() is None:
                if time.monotonic() >= deadline or os.fstat(output.fileno()).st_size > limit:
                    raise RuntimeError("Operacao do runtime de audio excedeu o limite.")
                time.sleep(.03)
            if process.returncode:
                raise RuntimeError("Operacao do runtime de audio falhou; servicos e pacotes do sistema foram preservados.")
            output.seek(0)
            data = output.read(limit + 1)
            if len(data) > limit:
                raise RuntimeError("Resposta do runtime de audio excedeu o limite.")
            return data
        finally:
            if process.poll() is None:
                process.kill()
            process.wait()


def dependency_plan(text):
    result = []
    for line in text.splitlines():
        match = re.match(r"^Inst ([a-z0-9][a-z0-9+.:-]*)(?: \[[^]]+\])? \(([A-Za-z0-9.+:~_-]+)(?: |\))", line)
        if match:
            result.append((match[1], match[2]))
    if not {"pulseaudio", "pulseaudio-utils"}.issubset({name.split(":")[0] for name, _ in result}) or len(result) > 32:
        raise RuntimeError("Nao foi possivel resolver o runtime privado com os indices APT existentes.")
    return result


def extract_payload(raw, destination):
    """Extract only binaries/libraries, without maintainer scripts or services."""
    destination = destination.resolve()
    links, total = [], 0
    with tarfile.open(fileobj=io.BytesIO(raw), mode="r:") as archive:
        for index, entry in enumerate(archive):
            relative = PurePosixPath(entry.name)
            if index > 10000 or relative.is_absolute() or ".." in relative.parts or "\\" in entry.name:
                raise RuntimeError("Caminho inesperado no pacote Debian.")
            parts = relative.parts
            allowed = parts and (parts[0] in ("lib", "lib64", "bin") or len(parts) > 1 and parts[0] == "usr" and parts[1] in ("bin", "lib", "libexec") or
                                 len(parts) > 3 and parts[:3] == ("usr", "share", "doc") and parts[-1] == "copyright")
            if not allowed or parts[:3] == ("usr", "lib", "systemd"):
                continue
            target = destination.joinpath(*parts)
            if not target.resolve().is_relative_to(destination):
                raise RuntimeError("Pacote tentou sair do diretorio privado.")
            if entry.isdir():
                target.mkdir(parents=True, exist_ok=True)
            elif entry.isfile():
                total += entry.size
                if entry.size < 0 or total > MAX_BYTES:
                    raise RuntimeError("Pacote excedeu o limite de extracao.")
                data = archive.extractfile(entry).read(entry.size + 1)
                if len(data) != entry.size:
                    raise RuntimeError("Pacote Debian incompleto.")
                target.parent.mkdir(parents=True, exist_ok=True)
                if target.exists():
                    if target.is_symlink() or target.read_bytes() != data:
                        raise RuntimeError("Colisao entre arquivos do runtime privado.")
                else:
                    with target.open("xb") as stream:
                        stream.write(data)
                    target.chmod(0o755 if entry.mode & 0o111 else 0o644)
            elif entry.issym():
                linked = PurePosixPath(entry.linkname)
                resolved = (destination.joinpath(*linked.parts[1:]) if linked.is_absolute() else target.parent / entry.linkname).resolve()
                if not resolved.is_relative_to(destination):
                    raise RuntimeError("Link Debian externo recusado.")
                links.append((target, resolved))
            else:
                raise RuntimeError("Entrada especial Debian recusada.")
    for target, linked in links:
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.is_symlink():
            if target.resolve() != linked.resolve():
                raise RuntimeError("Link Debian conflitante.")
        elif target.exists():
            raise RuntimeError("Arquivo Debian conflitante.")
        else:
            target.symlink_to(os.path.relpath(linked, target.parent))


def tools(value):
    prefix = value.get("bundle", "")
    result = {}
    for name in ("pulseaudio", "pactl", "paplay"):
        candidate = Path(prefix) / "usr/bin" / name if prefix else Path(shutil.which(name) or "/nonexistent")
        if not candidate.is_file():
            raise RuntimeError("Runtime de audio incompleto; execute a correcao de audio do pacote.")
        result[name] = str(candidate)
    return result


def runtime_environment(value, base=None):
    environment = dict(os.environ if base is None else base)
    if value.get("bundle"):
        prefix = Path(value["bundle"])
        paths = sorted({str(path.parent) for path in prefix.rglob("*.so*") if path.is_file()})
        if not paths or len(paths) > 64:
            raise RuntimeError("Bibliotecas do runtime privado indisponiveis.")
        environment["LD_LIBRARY_PATH"] = ":".join(paths)
    return environment


def prepare_runtime(announce=print, *, force_portable=False):
    if not force_portable and all(shutil.which(name) for name in ("pulseaudio", "pactl", "paplay")):
        announce("PulseAudio existente sera usado em uma instancia privada, sem mudar servicos.")
        return {"bundle": ""}
    root = private_directory(bundle_directory())
    marker = root / "managed.txt"
    if marker.exists() and (marker.is_symlink() or marker.read_text() != OWNER):
        raise RuntimeError("Diretorio de runtime de outra origem preservado.")
    if not marker.exists() and any(root.iterdir()):
        raise RuntimeError("Diretorio de runtime desconhecido preservado.")
    atomic_write(marker, OWNER.encode())
    environment = dict(os.environ, LC_ALL="C")
    announce("Resolvendo dependencias; APT sera usado somente para simular e baixar, sem instalar.")
    plan = dependency_plan(command(["apt-get", "--simulate", "--no-install-recommends", "--reinstall", "install",
                                    "pulseaudio", "pulseaudio-utils"], environment=environment, timeout=45).decode())
    architecture = command(["dpkg", "--print-architecture"], environment=environment).decode().strip()
    if not re.fullmatch(r"[a-z0-9-]{1,32}", architecture):
        raise RuntimeError("Arquitetura do runtime invalida.")
    identity = hashlib.sha256(json.dumps({"architecture": architecture, "packages": plan}).encode()).hexdigest()[:16]
    target = root / ("pulse-" + identity)
    if target.is_dir() and not target.is_symlink() and (target / "managed.txt").is_file() and (target / "managed.txt").read_text() == OWNER:
        value = {"bundle": str(target)}
        command([tools(value)["pulseaudio"], "--version"], environment=runtime_environment(value))
        return value
    stage = root / (".staging-" + uuid.uuid4().hex)
    private_directory(stage)
    downloads, prefix = stage / "downloads", stage / "prefix"
    downloads.mkdir(); prefix.mkdir()
    try:
        total = 0
        for index, (name, version) in enumerate(plan):
            announce("Baixando runtime privado " + str(index + 1) + "/" + str(len(plan)) + ": " + name)
            metadata = command(["apt-cache", "show", name + "=" + version], environment=environment).decode()
            digest = re.search(r"^SHA256: ([0-9a-f]{64})$", metadata, re.M)
            if not digest:
                raise RuntimeError("Metadados APT sem hash; nenhum pacote foi instalado.")
            before = set(downloads.glob("*.deb"))
            command(["apt-get", "-o", "Acquire::Retries=0", "-o", "Acquire::http::Timeout=12", "-o",
                     "Acquire::https::Timeout=12", "download", name + "=" + version], cwd=downloads,
                    environment=environment, timeout=60)
            added = set(downloads.glob("*.deb")) - before
            if len(added) != 1:
                raise RuntimeError("Download APT incompleto.")
            package = added.pop(); total += package.stat().st_size
            if total > MAX_BYTES or hashlib.sha256(package.read_bytes()).hexdigest() != digest[1]:
                raise RuntimeError("Hash ou limite do pacote Debian invalido.")
            raw = command(["dpkg-deb", "--fsys-tarfile", str(package)], timeout=45, limit=MAX_BYTES)
            extract_payload(raw, prefix)
            if sum(path.stat().st_size for path in prefix.rglob("*") if path.is_file() and not path.is_symlink()) > MAX_BYTES:
                raise RuntimeError("Runtime extraido excedeu o limite de tamanho.")
        atomic_write(prefix / "managed.txt", OWNER.encode())
        value = {"bundle": str(prefix)}
        command([tools(value)["pulseaudio"], "--version"], environment=runtime_environment(value))
        prefix.rename(target)
        return {"bundle": str(target)}
    finally:
        if stage.resolve().parent != root.resolve() or not stage.name.startswith(".staging-"):
            raise RuntimeError("Diretorio de staging inesperado; preservado.")
        shutil.rmtree(stage)


def runtime_directory():
    base = Path(os.environ.get("XDG_RUNTIME_DIR") or os.environ.get("XDG_STATE_HOME") or Path.home() / ".local/state")
    path = base / "raspberry-tv-audio"
    if len(str(path / "native").encode()) >= 100:
        path = Path("/tmp") / ("raspberry-tv-audio-" + str(os.getuid()))
    return private_directory(path)


_DEFAULT = object()


class PrivatePulse:
    def __init__(self, value=_DEFAULT, *, null_sink=False, directory=None):
        self.value = profile() if value is _DEFAULT else value
        self.null_sink = null_sink
        self.directory = directory
        self.process = None
        self.thread = None
        self.lock = None
        self.failure = "servidor_indisponivel"
        self.environment = {}

    def _errors(self):
        for line in iter(lambda: self.process.stderr.readline(1024), b""):
            lower = line.lower()
            if b"busy" in lower or b"resource temporarily unavailable" in lower:
                self.failure = "hdmi_ocupado"
            elif b"failed to load module" in lower or b"cannot open shared object" in lower:
                self.failure = "modulo_ou_biblioteca_ausente"
            elif b"permission denied" in lower:
                self.failure = "permissao_de_audio"

    def __enter__(self):
        if self.value is None:
            return {}
        if os.name != "posix" or os.geteuid() == 0:
            raise RuntimeError("Audio privado exige usuario Linux, sem sudo.")
        directory = private_directory(self.directory) if self.directory else runtime_directory()
        self.directory = directory
        marker = directory / "managed.txt"
        if marker.exists() and (marker.is_symlink() or marker.read_text() != OWNER):
            raise RuntimeError("Diretorio de audio de outra origem preservado.")
        if not marker.exists() and any(directory.iterdir()):
            raise RuntimeError("Diretorio de audio desconhecido preservado.")
        atomic_write(marker, OWNER.encode())
        descriptor = os.open(directory / "audio.lock", os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
        self.lock = os.fdopen(descriptor, "a")
        try:
            fcntl.flock(self.lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            cookie = directory / "cookie"
            if cookie.is_symlink():
                raise RuntimeError("Cookie de audio e um link inesperado.")
            if not cookie.exists():
                atomic_write(cookie, os.urandom(256))
            elif not cookie.is_file() or cookie.stat().st_size != 256:
                raise RuntimeError("Cookie de audio privado invalido.")
            environment = runtime_environment(self.value)
            environment.update(PULSE_SERVER="unix:" + str(directory / "native"), PULSE_COOKIE=str(cookie),
                               PULSE_SINK=SINK, PULSE_RUNTIME_PATH=str(directory), PULSE_STATE_PATH=str(directory / "state"),
                               PULSE_CONFIG_PATH=str(directory), PIPEWIRE_REMOTE="raspberry-tv-private-pulse")
            private_directory(directory / "state")
            atomic_write(directory / "client.conf", b"autospawn = no\n")
            backend = "module-null-sink" if self.null_sink else "module-alsa-sink"
            options = "" if self.null_sink else " device=" + DEVICE
            script = ('load-module module-native-protocol-unix socket="' + str(directory / "native") +
                      '" auth-cookie="' + str(cookie) + '" auth-anonymous=false\nload-module ' + backend +
                      ' sink_name=' + SINK + options + ' rate=48000 channels=2\n'
                      'load-module module-suspend-on-idle timeout=1\nset-default-sink ' + SINK + '\n')
            atomic_write(directory / "private.pa", script.encode())
            executables = tools(self.value)
            arguments = [executables["pulseaudio"], "-n", "--daemonize=no", "--use-pid-file=no", "--exit-idle-time=-1",
                         "--high-priority=no", "--realtime=no", "--log-level=error", "--log-target=stderr", "--fail=yes",
                         "--file=" + str(directory / "private.pa")]
            if self.value.get("bundle"):
                modules = list(Path(self.value["bundle"]).rglob("module-native-protocol-unix.so"))
                if len(modules) != 1:
                    raise RuntimeError("Diretorio de modulos PulseAudio invalido.")
                arguments += ["-p", str(modules[0].parent)]
            self.process = subprocess.Popen(arguments, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                            stderr=subprocess.PIPE, env=environment)
            self.thread = threading.Thread(target=self._errors, daemon=True); self.thread.start()
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                if self.process.poll() is not None:
                    break
                if (directory / "native").is_socket():
                    try:
                        command([executables["pactl"], "info"], environment=environment, timeout=1)
                        self.environment = environment
                        return environment
                    except RuntimeError:
                        pass
                time.sleep(.05)
            raise RuntimeError("Audio HDMI privado nao iniciou (" + self.failure + "). Pare a TV e tente novamente.")
        except BaseException:
            self.close()
            raise

    def close(self):
        if self.process is not None:
            if self.process.poll() is None:
                self.process.terminate()
                try:
                    self.process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    self.process.kill()
            self.process.wait()
            if self.thread is not None:
                self.thread.join(timeout=1)
            self.process.stderr.close()
            self.process = None
        if self.lock is not None:
            self.lock.close(); self.lock = None

    def __exit__(self, *_):
        self.close()
