#!/usr/bin/env python3
"""Explicit, per-user native update. Existing projects and private data survive."""
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import tarfile
import time
from urllib import request

MARKER = "Raspberry TV native update v1"
MAX_ARCHIVE = 4 * 1024 * 1024
MAX_SOURCE = 16 * 1024 * 1024


def release_spec(value):
    if not isinstance(value, dict) or not re.fullmatch(r"[0-9a-f]{40}", str(value.get("commit", ""))) or not re.fullmatch(r"[0-9a-f]{64}", str(value.get("manifest_sha256", ""))):
        raise RuntimeError("Identificacao da release invalida; instalacao anterior preservada.")
    return {key: value[key] for key in ("commit", "manifest_sha256")}


class GitHubRedirect(request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        if not newurl.startswith("https://codeload.github.com/balancerec/raspberry-pi3plus-tv/"):
            raise RuntimeError("Redirecionamento inesperado na atualizacao.")
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def download(spec):
    url = "https://codeload.github.com/balancerec/raspberry-pi3plus-tv/tar.gz/" + spec["commit"]
    opener = request.build_opener(GitHubRedirect())
    deadline = time.monotonic() + 40
    with opener.open(request.Request(url, headers={"User-Agent": "Raspberry-TV-Updater/1.0"}), timeout=8) as response:
        data = bytearray()
        while True:
            if time.monotonic() > deadline:
                raise RuntimeError("Download excedeu 40 segundos. Tente novamente.")
            chunk = response.read(65536)
            if not chunk:
                return bytes(data)
            data.extend(chunk)
            if len(data) > MAX_ARCHIVE:
                raise RuntimeError("Pacote de atualizacao excedeu o limite de tamanho.")


def source_files(raw, spec):
    if len(raw) > MAX_ARCHIVE:
        raise RuntimeError("Pacote excedeu o limite de tamanho.")
    files, roots, size = {}, set(), 0
    with tarfile.open(fileobj=io.BytesIO(raw), mode="r:gz") as archive:
        for index, entry in enumerate(archive):
            path = PurePosixPath(entry.name)
            if index >= 300 or path.is_absolute() or ".." in path.parts or "\\" in entry.name or not path.parts:
                raise RuntimeError("Caminho inesperado no pacote.")
            roots.add(path.parts[0])
            if len(roots) != 1 or not (entry.isdir() or entry.isfile()):
                raise RuntimeError("Links e entradas especiais nao sao permitidos no pacote.")
            if entry.isdir():
                continue
            relative = str(PurePosixPath(*path.parts[1:]))
            size += entry.size
            if len(path.parts) < 2 or relative in files or entry.size < 0 or size > MAX_SOURCE:
                raise RuntimeError("Conteudo inesperado no pacote.")
            files[relative] = archive.extractfile(entry).read(entry.size + 1)
            if len(files[relative]) != entry.size:
                raise RuntimeError("Pacote incompleto.")
    manifest = files.get("SHA256SUMS.txt", b"")
    if hashlib.sha256(manifest).hexdigest() != spec["manifest_sha256"]:
        raise RuntimeError("Assinatura de integridade da release nao corresponde ao pacote.")
    verified = set()
    for row in manifest.decode("utf-8").splitlines():
        digest, name = row.split("  ", 1)
        if name in verified or not re.fullmatch(r"[0-9a-f]{64}", digest) or hashlib.sha256(files.get(name, b"")).hexdigest() != digest:
            raise RuntimeError("Arquivo da release falhou na verificacao de integridade.")
        verified.add(name)
    if verified != set(files) - {"SHA256SUMS.txt"}:
        raise RuntimeError("O pacote contem arquivos fora do manifesto.")
    required = {"native/CMakeLists.txt", "scripts/desktop-launcher.py", "integrations/spotify/spotify_client.py"}
    if not required.issubset(files):
        raise RuntimeError("Pacote nativo incompleto.")
    return files


def load_launcher(source):
    namespace = {"__name__": "raspberry_tv_update_launcher", "SOURCE_BYTES": source}
    exec(compile(source, "raspberry-tv-desktop-launcher", "exec"), namespace)
    return namespace


def install_source(files, spec, root):
    marker = root / "managed-release.json"
    expected = dict(spec, owner=MARKER)
    if root.is_symlink():
        raise RuntimeError("Destino da release e um link inesperado.")
    if root.exists() and any(root.iterdir()):
        if marker.is_symlink() or not marker.is_file() or json.loads(marker.read_text()) != expected:
            raise RuntimeError("Diretorio de outra origem preservado: " + str(root))
        for name, content in files.items():
            target = root / name
            if target.is_symlink() or not target.is_file() or target.read_bytes() != content:
                raise RuntimeError("Release local modificada; arquivos preservados: " + str(root))
        return
    root.mkdir(parents=True, mode=0o700 if os.name == "posix" else 0o755, exist_ok=True)
    # A failed first extraction stays visibly owned and can be diagnosed without
    # deleting or replacing any existing user project.
    marker.write_text(json.dumps(expected) + "\n", encoding="utf-8")
    for name, content in files.items():
        target = root / name
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open("xb") as stream:
            stream.write(content)


def build(root, run=subprocess.run):
    for label, arguments, timeout in (
        ("Configurando a interface nativa", ["cmake", "-S", str(root / "native"), "-B", str(root / "build-native"), "-DCMAKE_BUILD_TYPE=Release"], 90),
        ("Compilando com um processo; aguarde", ["cmake", "--build", str(root / "build-native"), "--parallel", "1"], 1200),
        ("Validando os testes", ["ctest", "--test-dir", str(root / "build-native"), "--output-on-failure", "--timeout", "45"], 150)):
        print(label + "...", flush=True)
        result = run(arguments, cwd=root, timeout=timeout, check=False)
        if result.returncode:
            raise RuntimeError("Atualizacao interrompida; build/testes falharam. Aplicacao anterior preservada.")


def activate(root, launcher):
    directory = launcher["managed_launcher_directory"]()
    pointer = directory / "project.private.json"
    current = next((project for project in launcher["projects"]()
                    if (project / "build-native/raspberry-tv-native").is_file() or (project / "bin/raspberry-tv-native").is_file()), None)
    if current is not None and current.resolve() == root.resolve():
        launcher["install"]()
        launcher["launch"]()
        return
    # Nothing stops the old UI until a complete build and CTest have succeeded.
    launcher["stop_native"]()
    previous = pointer.read_bytes() if pointer.is_file() and not pointer.is_symlink() else None
    launcher["write_owned"](pointer, json.dumps({"current": str(root), "previous": str(current) if current else ""}).encode())
    try:
        launcher["install"]()
        launcher["launch"]()
    except Exception:
        if previous is None:
            pointer.unlink(missing_ok=True)
        else:
            launcher["write_owned"](pointer, previous)
        # The old executable remains intact and is available through the icon.
        raise


def main(spec=None, launcher_source=None):
    spec = release_spec(spec or globals().get("RELEASE_SPEC"))
    if os.name != "posix" or os.geteuid() == 0:
        raise RuntimeError("Execute a atualizacao no Raspberry como usuario, sem sudo.")
    launcher_source = launcher_source or globals().get("LAUNCHER_BYTES")
    if not isinstance(launcher_source, bytes):
        raise RuntimeError("Launcher da atualizacao ausente.")
    launcher = load_launcher(launcher_source)
    directory = launcher["managed_launcher_directory"]()
    with (directory / "update.lock").open("a") as lock:
        launcher["fcntl"].flock(lock, launcher["fcntl"].LOCK_EX | launcher["fcntl"].LOCK_NB)
        print("Raspberry TV: atualizando os controles de midia. Configuracoes e playlists serao preservadas.", flush=True)
        root = Path.home() / ".local/opt/raspberry-tv-media/releases" / spec["commit"]
        home = Path.home().resolve()
        if not root.resolve().is_relative_to(home) or any(parent.is_symlink() for parent in (root, *root.parents) if parent != home and parent.is_relative_to(home)):
            raise RuntimeError("Destino inesperado; instalacao anterior preservada.")
        files = source_files(download(spec), spec)
        install_source(files, spec, root)
        build(root)
        if globals().get("AUDIO_FIX") is True:
            source = root / "scripts/spotify-audio-setup.py"
            specifier = importlib.util.spec_from_file_location("raspberry_tv_audio_setup", source)
            setup = importlib.util.module_from_spec(specifier)
            specifier.loader.exec_module(setup)
            backend, value = setup.prepare(root)
            setup.apply(root, launcher, backend, value, activate)
        else:
            activate(root, launcher)
        print("Controles de midia atualizados. Nas proximas vezes, abra pelo icone Raspberry TV.", flush=True)
        print("Versao instalada:", root, flush=True)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError, tarfile.TarError) as error:
        print("Raspberry TV: " + str(error), file=sys.stderr, flush=True)
        print("A versao anterior continua disponivel pelo icone. Nenhuma chave foi transferida.", flush=True)
        input("Pressione Enter para fechar.")
        sys.exit(1)
