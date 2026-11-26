#!/usr/bin/env python3
"""Small Spotify Web API client. No client secret, audio decoding or third-party deps.

OAuth tokens never cross the native UI protocol. They live only in private XDG
storage. Network exceptions and Spotify error bodies are deliberately not logged.
"""
from __future__ import annotations

import argparse
import base64
import contextlib
import hashlib
import getpass
import http.server
import json
import math
import os
from pathlib import Path
import re
import secrets
import shutil
import stat
import subprocess
import sys
import tempfile
import time
from urllib import error, parse, request
import webbrowser
import warnings

API = "https://api.spotify.com/v1"
TOKEN_URL = "https://accounts.spotify.com/api/token"
REDIRECT = "http://127.0.0.1:8765/spotify/callback"
SCOPES = "playlist-read-private playlist-read-collaborative user-read-playback-state user-modify-playback-state"
MAX_RESPONSE = 2 * 1024 * 1024
PAGE_SIZE = 20


class SpotifyError(Exception):
    def __init__(self, code: str, message: str):
        super().__init__(message)
        self.code = code


def private_directory(path: Path) -> Path:
    # Python 3.13+ interprets 0700 as a special Windows DACL; on Windows keep
    # inherited user-profile ACLs. The target DietPi uses explicit POSIX modes.
    path.mkdir(parents=True, exist_ok=True, mode=0o700 if os.name == "posix" else 0o755)
    if path.is_symlink() or not path.is_dir():
        raise SpotifyError("storage", "Diretorio Spotify inseguro ou indisponivel.")
    if os.name == "posix":
        if path.stat().st_uid != os.getuid():
            raise SpotifyError("storage", "Diretorio Spotify pertence a outro usuario.")
        path.chmod(0o700)
    return path


class Store:
    def __init__(self, directory: Path | None = None):
        base = Path(os.environ.get("XDG_CONFIG_HOME") or Path.home() / ".config")
        self.directory = directory or base / "raspberry-tv" / "spotify"

    def read(self, name: str) -> dict:
        path = self.directory / name
        try:
            flags = os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_BINARY", 0)
            fd = os.open(path, flags)
        except FileNotFoundError:
            return {}
        except OSError:
            raise SpotifyError("storage", "Nao foi possivel ler a configuracao privada.") from None
        try:
            info = os.fstat(fd)
            if not stat.S_ISREG(info.st_mode) or info.st_size > 65536:
                raise SpotifyError("storage", "Arquivo Spotify invalido; reconfigure a conta.")
            if os.name == "posix" and (info.st_uid != os.getuid() or info.st_mode & 0o077):
                raise SpotifyError("storage", "Arquivo Spotify precisa pertencer ao usuario e ter permissao 600.")
            with os.fdopen(fd, "r", encoding="utf-8") as stream:
                fd = -1
                value = json.load(stream)
            if not isinstance(value, dict):
                raise ValueError
            return value
        except (ValueError, UnicodeError):
            raise SpotifyError("storage", "Arquivo Spotify corrompido; reconfigure a conta.") from None
        finally:
            if fd >= 0:
                os.close(fd)

    def write(self, name: str, value: dict) -> None:
        directory = private_directory(self.directory)
        fd, temporary = tempfile.mkstemp(prefix=".spotify-", dir=directory)
        try:
            if os.name == "posix":
                os.fchmod(fd, 0o600)
            with os.fdopen(fd, "w", encoding="utf-8") as stream:
                json.dump(value, stream, ensure_ascii=True)
                stream.write("\n")
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary, directory / name)
            if os.name == "posix":
                directory_fd = os.open(directory, os.O_RDONLY | getattr(os, "O_DIRECTORY", 0))
                try:
                    os.fsync(directory_fd)
                finally:
                    os.close(directory_fd)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)

    @contextlib.contextmanager
    def locked(self):
        private_directory(self.directory)
        fd = os.open(self.directory / ".lock", os.O_CREAT | os.O_RDWR | getattr(os, "O_NOFOLLOW", 0), 0o600)
        try:
            if os.name == "posix":
                import fcntl
                try:
                    fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                except BlockingIOError:
                    raise SpotifyError("busy", "Outra operacao Spotify esta em andamento.") from None
            else:
                import msvcrt
                try:
                    if os.fstat(fd).st_size == 0:
                        os.write(fd, b"0")
                    os.lseek(fd, 0, os.SEEK_SET)
                    msvcrt.locking(fd, msvcrt.LK_NBLCK, 1)
                except OSError:
                    raise SpotifyError("busy", "Outra operacao Spotify esta em andamento.") from None
            yield
        finally:
            os.close(fd)

    def logout(self) -> None:
        for name in ("tokens.private.json", "cooldown.private.json"):
            (self.directory / name).unlink(missing_ok=True)


class NoRedirect(request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None  # Never forward Bearer tokens to a redirect destination.


def https_json(method: str, url: str, headers: dict, body: bytes | None, deadline: float):
    parts = parse.urlsplit(url)
    if parts.scheme != "https" or parts.netloc not in ("api.spotify.com", "accounts.spotify.com"):
        raise SpotifyError("request", "Destino da API recusado.")
    remaining = deadline - time.monotonic()
    if remaining <= 0:
        raise SpotifyError("network", "Tempo limite da operacao Spotify.")
    req = request.Request(url, data=body, headers=headers, method=method)
    try:
        try:
            response = request.build_opener(NoRedirect()).open(req, timeout=min(5, remaining))
        except error.HTTPError as exc:
            response = exc
        with response:
            code = response.code
            response_headers = dict(response.headers.items())
            if not 200 <= code < 300:
                return code, response_headers, {}  # Server error text may contain secrets.
            data = bytearray()
            while True:
                if time.monotonic() >= deadline:
                    raise SpotifyError("network", "Tempo limite da operacao Spotify.")
                chunk = response.read(min(16384, MAX_RESPONSE + 1 - len(data)))
                if not chunk:
                    break
                data.extend(chunk)
                if len(data) > MAX_RESPONSE:
                    raise SpotifyError("response", "Resposta Spotify excedeu o limite de tamanho.")
            value = json.loads(data) if data else {}
            if not isinstance(value, dict):
                raise ValueError
            return code, response_headers, value
    except (error.URLError, TimeoutError, OSError):
        raise SpotifyError("network", "Spotify indisponivel: verifique a rede e tente novamente.") from None
    except (ValueError, UnicodeError):
        raise SpotifyError("response", "Resposta Spotify invalida.") from None


def pkce_challenge(verifier: str) -> str:
    return base64.urlsafe_b64encode(hashlib.sha256(verifier.encode("ascii")).digest()).decode("ascii").rstrip("=")


def authorization_url(client_id: str, verifier: str, state: str) -> str:
    return "https://accounts.spotify.com/authorize?" + parse.urlencode({
        "client_id": client_id, "response_type": "code", "redirect_uri": REDIRECT,
        "scope": SCOPES, "state": state, "code_challenge_method": "S256",
        "code_challenge": pkce_challenge(verifier),
    })


def callback_code(target: str, expected_state: str) -> str:
    parts = parse.urlsplit(target)
    if parts.scheme or parts.netloc or parts.fragment or parts.path != "/spotify/callback":
        raise SpotifyError("callback", "Callback OAuth recusado.")
    try:
        query = parse.parse_qs(parts.query, max_num_fields=8)
    except ValueError:
        raise SpotifyError("callback", "Callback OAuth invalido.") from None
    state = query.get("state", [])
    if len(state) != 1 or not state[0].isascii() or not secrets.compare_digest(state[0], expected_state):
        raise SpotifyError("callback", "Estado OAuth invalido; login cancelado.")
    if "error" in query:
        raise SpotifyError("auth", "Autorizacao Spotify recusada pelo usuario.")
    code = query.get("code", [])
    if len(code) != 1 or not code[0] or len(code[0]) > 4096:
        raise SpotifyError("callback", "Callback OAuth sem codigo valido.")
    return code[0]


class Client:
    def __init__(self, store: Store, transport=https_json, clock=time.time):
        self.store, self.transport, self.clock = store, transport, clock
        self.deadline = time.monotonic() + 15

    def client_id(self) -> str:
        value = self.store.read("config.private.json").get("client_id", "")
        if not isinstance(value, str) or not re.fullmatch(r"[0-9a-fA-F]{32}", value):
            raise SpotifyError("setup", "Configure o Client ID publico: bash scripts/spotify.sh configure; depois login.")
        return value

    def cooldown(self):
        try:
            remaining = math.ceil(float(self.store.read("cooldown.private.json").get("until", 0)) - self.clock())
        except (ValueError, TypeError, OverflowError):
            remaining = 0
        if remaining > 0:
            raise SpotifyError("rate_limit", f"Limite Spotify: aguarde {remaining} s antes de tentar novamente.")

    def check_status(self, code: int, headers: dict, *, token=False):
        if 200 <= code < 300:
            return
        if code == 429:
            raw = next((v for k, v in headers.items() if k.lower() == "retry-after"), "30")
            try:
                delay = max(1, min(86400, int(raw)))
            except (ValueError, TypeError):
                delay = 30
            self.store.write("cooldown.private.json", {"until": self.clock() + delay})
            raise SpotifyError("rate_limit", f"Limite Spotify: aguarde {delay} s antes de tentar novamente.")
        if code == 401 or (token and code in (400, 403)):
            raise SpotifyError("auth", "Sessao Spotify expirada ou revogada; execute login novamente.")
        if code == 403:
            raise SpotifyError("access", "Spotify recusou a acao: confira Premium, permissoes e usuarios autorizados no painel.")
        if code == 404:
            raise SpotifyError("unavailable", "Conteudo ou dispositivo indisponivel; abra o Spotify e escolha um dispositivo.")
        if 500 <= code <= 599:
            raise SpotifyError("network", "Servidor Spotify indisponivel. Tente novamente mais tarde.")
        raise SpotifyError("request", "Spotify recusou a solicitacao; confira o contexto e o dispositivo.")

    def save_tokens(self, payload: dict, previous: dict | None = None):
        token = payload.get("access_token")
        refresh = payload.get("refresh_token") or (previous or {}).get("refresh_token")
        try:
            expires = max(1, min(86400, int(payload["expires_in"])))
        except (KeyError, TypeError, ValueError):
            raise SpotifyError("auth", "Resposta de autenticacao Spotify invalida.") from None
        if not isinstance(token, str) or not token or len(token) > 8192 or not isinstance(refresh, str) or not refresh:
            raise SpotifyError("auth", "Resposta de autenticacao Spotify incompleta.")
        saved = {"access_token": token, "refresh_token": refresh, "expires_at": self.clock() + expires,
                 "scope": payload.get("scope", (previous or {}).get("scope", "")), "client_id": self.client_id()}
        self.store.write("tokens.private.json", saved)
        return saved

    def token_request(self, fields: dict, previous=None):
        self.cooldown()
        code, headers, payload = self.transport("POST", TOKEN_URL,
            {"Content-Type": "application/x-www-form-urlencoded", "Accept": "application/json"},
            parse.urlencode(fields).encode("ascii"), self.deadline)
        self.check_status(code, headers, token=True)
        return self.save_tokens(payload, previous)

    def access_token(self, *, refresh=False) -> str:
        client_id = self.client_id()
        saved = self.store.read("tokens.private.json")
        if not saved or saved.get("client_id") != client_id:
            raise SpotifyError("auth", "Conta Spotify desconectada; execute bash scripts/spotify.sh login.")
        try:
            expired = float(saved.get("expires_at", 0)) <= self.clock() + 30
        except (ValueError, TypeError):
            expired = True
        if refresh or expired:
            refresh_token = saved.get("refresh_token")
            if not isinstance(refresh_token, str) or not refresh_token:
                raise SpotifyError("auth", "Sessao sem renovacao; execute login novamente.")
            saved = self.token_request({"grant_type": "refresh_token", "refresh_token": refresh_token,
                                        "client_id": client_id}, saved)
        value = saved.get("access_token")
        if not isinstance(value, str) or not value or "\n" in value or "\r" in value:
            raise SpotifyError("auth", "Sessao invalida; execute login novamente.")
        return value

    def api(self, method: str, path: str, query=None, body=None):
        if not path.startswith("/") or path.startswith("//") or "?" in path or "#" in path:
            raise SpotifyError("request", "Caminho da API recusado.")
        self.cooldown()
        uri = API + path + ("?" + parse.urlencode(query) if query else "")
        payload = json.dumps(body).encode("utf-8") if body is not None else None
        for attempt in range(2):
            token = self.access_token(refresh=attempt == 1)
            code, headers, value = self.transport(method, uri,
                {"Authorization": "Bearer " + token, "Content-Type": "application/json", "Accept": "application/json"},
                payload, self.deadline)
            if code == 401 and attempt == 0:
                continue
            self.check_status(code, headers)
            return value
        raise SpotifyError("auth", "Sessao Spotify invalida; execute login novamente.")

    def profile(self):
        return self.api("GET", "/me")

    def playlists(self, offset=0):
        return self.api("GET", "/me/playlists", {"limit": PAGE_SIZE, "offset": bounded_offset(offset)})

    def playlist_items(self, playlist_id: str, offset=0):
        require_id(playlist_id)
        return self.api("GET", f"/playlists/{playlist_id}/items", {"limit": PAGE_SIZE, "offset": bounded_offset(offset)})

    def search(self, text: str, offset=0):
        if not text.strip() or len(text) > 200:
            raise SpotifyError("request", "Busca precisa conter entre 1 e 200 caracteres.")
        return self.api("GET", "/search", {"q": text, "type": "track", "limit": 10, "offset": bounded_offset(offset)})

    def devices(self):
        return self.api("GET", "/me/player/devices")

    def playback(self):
        return self.api("GET", "/me/player")

    def play(self, uri="", device=""):
        body = None
        if uri:
            kind, _ = require_uri(uri)
            body = {"context_uri": uri} if kind in ("playlist", "album", "artist") else {"uris": [uri]}
        return self.api("PUT", "/me/player/play", device_query(device), body)

    def pause(self, device=""):
        return self.api("PUT", "/me/player/pause", device_query(device))

    def skip(self, direction: str, device=""):
        if direction not in ("next", "previous"):
            raise SpotifyError("request", "Direcao invalida.")
        return self.api("POST", "/me/player/" + direction, device_query(device))

    def volume(self, percent: int, device=""):
        if type(percent) is not int or not 0 <= percent <= 100:
            raise SpotifyError("request", "Volume precisa estar entre 0 e 100.")
        return self.api("PUT", "/me/player/volume", {"volume_percent": percent, **device_query(device)})

    def change_volume(self, delta: int):
        state = self.playback()
        device = state.get("device") or {}
        current = device.get("volume_percent")
        if not device.get("id") or device.get("is_restricted") or not device.get("supports_volume") or type(current) is not int:
            raise SpotifyError("unavailable", "O dispositivo atual nao permite controlar volume pela API.")
        return self.volume(max(0, min(100, current + delta)), device["id"])

    def transfer(self, device: str):
        device_query(device, required=True)
        return self.api("PUT", "/me/player", body={"device_ids": [device], "play": False})


def bounded_offset(value):
    if type(value) is not int or not 0 <= value <= 100000:
        raise SpotifyError("request", "Pagina invalida.")
    return value


def require_id(value):
    if not isinstance(value, str) or not re.fullmatch(r"[A-Za-z0-9]{22}", value):
        raise SpotifyError("request", "Identificador Spotify invalido.")


def require_uri(value):
    match = re.fullmatch(r"spotify:(playlist|album|artist|track|episode):([A-Za-z0-9]{22})", value)
    if not match:
        raise SpotifyError("request", "Use uma URI oficial spotify:playlist/album/artist/track/episode.")
    return match.group(1), match.group(2)


def device_query(value, required=False):
    if not value and not required:
        return {}
    if not isinstance(value, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,128}", value):
        raise SpotifyError("request", "Identificador do dispositivo invalido.")
    return {"device_id": value}


def login(client: Client, no_browser=False, timeout=120):
    client_id = client.client_id()
    verifier, state = secrets.token_urlsafe(64), secrets.token_urlsafe(32)
    result = {}

    class Callback(http.server.BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass  # The request URL includes an authorization code.

        def do_GET(self):
            if len(self.path) > 8192 or self.headers.get("Host") != "127.0.0.1:8765":
                self.send_error(400)
                return
            if parse.urlsplit(self.path).path != "/spotify/callback":
                self.send_error(404)
                return
            try:
                result["code"] = callback_code(self.path, state)
                message = b"Autorizacao recebida. Volte ao terminal Raspberry TV."
                status = 200
            except SpotifyError as exc:
                result["error"] = exc
                message = b"Autorizacao recusada. Volte ao terminal e tente novamente."
                status = 400
            self.send_response(status)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Referrer-Policy", "no-referrer")
            self.send_header("Content-Length", str(len(message)))
            self.end_headers()
            self.wfile.write(message)

    with http.server.HTTPServer(("127.0.0.1", 8765), Callback) as server:
        server.timeout = 0.5
        url = authorization_url(client_id, verifier, state)
        print("Abra no navegador do mesmo computador (ou use o tunel SSH descrito em docs/SPOTIFY.md):", flush=True)
        print(url, flush=True)  # Public authorization request: no token or verifier.
        if not no_browser:
            webbrowser.open(url)
        until = time.monotonic() + timeout
        while not result and time.monotonic() < until:
            server.handle_request()
    if "error" in result:
        raise result["error"]
    if "code" not in result:
        raise SpotifyError("auth", "Login expirou; nenhuma credencial foi alterada.")
    client.deadline = time.monotonic() + 15
    client.token_request({"grant_type": "authorization_code", "code": result["code"], "redirect_uri": REDIRECT,
                          "client_id": client_id, "code_verifier": verifier})
    print("Spotify conectado. Tokens salvos no diretorio privado do usuario; nenhum segredo exibido.")


def clean_label(value, limit=240):
    return "".join(c for c in str(value or "") if c.isprintable())[:limit]


class Soloist:
    """Adapter for the official, separately installed Soloist executable.

    No download, service install or automatic start. Controls use the official
    ctl command and a loopback endpoint, so we do not implement a second player.
    """
    def __init__(self, store: Store, runner=None):
        self.store = store
        self.runner = runner or self._bounded_run

    def binary(self):
        configured = os.environ.get("SOLOIST_BIN")
        default = Path.home() / ".local/opt/spotify-soloist/soloist"
        binary = configured or (str(default) if default.is_file() else shutil.which("soloist"))
        if not binary or not Path(binary).is_file():
            raise SpotifyError("setup", "Instale o Soloist oficial para sua arquitetura; consulte docs/SPOTIFY.md.")
        return str(Path(binary).resolve())

    def directories(self):
        data = Path(os.environ.get("XDG_DATA_HOME") or Path.home() / ".local/share") / "raspberry-tv/soloist"
        cache = Path(os.environ.get("XDG_CACHE_HOME") or Path.home() / ".cache") / "raspberry-tv/soloist"
        return private_directory(data), private_directory(cache)

    @staticmethod
    def _bounded_run(arguments):
        # No shell and no secret in ctl argv. Bound time, disk output and memory.
        with tempfile.TemporaryFile() as output:
            process = subprocess.Popen(arguments, stdin=subprocess.DEVNULL, stdout=output, stderr=subprocess.DEVNULL)
            try:
                deadline = time.monotonic() + 8
                while process.poll() is None:
                    if time.monotonic() >= deadline or os.fstat(output.fileno()).st_size > MAX_RESPONSE:
                        raise SpotifyError("network", "Soloist nao respondeu dentro do limite.")
                    time.sleep(0.02)
                output.seek(0)
                data = output.read(MAX_RESPONSE + 1)
                if len(data) > MAX_RESPONSE:
                    raise SpotifyError("response", "Resposta Soloist excedeu o limite.")
                return process.returncode, data
            finally:
                if process.poll() is None:
                    process.kill()
                process.wait()

    def ctl(self, arguments, json_output=False):
        binary = self.binary()
        data, _ = self.directories()
        try:
            with (data / "ws.addr").open(encoding="ascii") as stream:
                address = stream.read(65)
            with (data / "ws.port").open(encoding="ascii") as stream:
                raw_port = stream.read(9)
        except (OSError, UnicodeError):
            raise SpotifyError("setup", "Inicie em outro terminal: bash scripts/spotify.sh soloist-run; depois selecione Raspberry TV no Spotify.") from None
        if len(address) > 64 or len(raw_port) > 8 or address.strip() != "127.0.0.1":
            raise SpotifyError("request", "Endpoint Soloist recusado: somente loopback 127.0.0.1 e permitido.")
        try:
            port = int(raw_port.strip())
        except ValueError:
            raise SpotifyError("request", "Endpoint Soloist invalido.") from None
        if not 1 <= port <= 65535:
            raise SpotifyError("request", "Endpoint Soloist invalido.")
        argv = [binary, "ctl", "--ws", f"127.0.0.1:{port}", *arguments]
        if json_output:
            argv.append("--json")
        code, output = self.runner(argv)
        if code == 2:
            raise SpotifyError("network", "Soloist indisponivel; inicie o cliente local novamente.")
        if code != 0:
            raise SpotifyError("auth", "Soloist recusou a acao; selecione Raspberry TV no aplicativo Spotify e confira a sessao.")
        if not json_output:
            return {}
        try:
            value = json.loads(output)
            if not isinstance(value, dict):
                raise ValueError
            return value
        except (ValueError, UnicodeError):
            raise SpotifyError("response", "Resposta Soloist invalida; atualize o cliente oficial.") from None

    def run_foreground(self):
        binary = self.binary()
        key = self.store.read("soloist.private.json").get("api_key", "")
        if not isinstance(key, str) or not re.fullmatch(r"spak_[A-Za-z0-9_-]{15,507}", key):
            raise SpotifyError("setup", "Configure a chave sem exibi-la: bash scripts/spotify.sh soloist-configure.")
        data, cache = self.directories()
        arguments = [binary, "--device-name", "Raspberry TV", "--api-key", key,
                     "--data-dir", str(data), "--cache-dir", str(cache), "--cache-size", "100",
                     "--initial-volume", "35", "--ws", "127.0.0.1:0"]
        device = self.store.read("soloist.private.json").get("pipewire_device", "")
        if isinstance(device, str) and device:
            arguments += ["--pipewire-device", device]
        print("Soloist em primeiro plano. Selecione Raspberry TV no Spotify da mesma rede. Ctrl+C encerra.", flush=True)
        # Official Soloist currently accepts the key as an argv option. Never
        # print argv or exceptions containing it. See the documented limitation.
        process = subprocess.Popen(arguments, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            code = process.wait()
        except KeyboardInterrupt:
            if process.poll() is None:
                try:
                    process.terminate()
                except ProcessLookupError:
                    pass
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            print("Soloist encerrado.")
            return
        if code == 10:
            raise SpotifyError("setup", "Build Soloist expirou; baixe uma versao atual no site oficial.")
        if code != 0:
            raise SpotifyError("unavailable", "Soloist encerrou com falha. Confira chave, build e disponibilidade de PipeWire/PulseAudio.")

    @staticmethod
    def entity(entity):
        if not isinstance(entity, dict):
            return "", ""
        decorations = entity.get("decorations") or {}
        name = clean_label((decorations.get("identity") or {}).get("name"))
        artists = []
        for creator in decorations.get("creators") or []:
            if isinstance(creator, dict):
                value = ((creator.get("entity") or {}).get("decorations") or {}).get("identity") or {}
                artists.append(clean_label(value.get("name")))
        return name, clean_label(", ".join(artists))

    def records(self, command, value=""):
        command = command.removeprefix("soloist-")
        if command == "status":
            data = self.ctl(["now"], True)
            name, artists = self.entity(data.get("item"))
            state = data.get("status", "idle")
            if state not in ("idle", "playing", "paused", "buffering"):
                state = "idle"
            volume = data.get("volume")
            volume = int(volume) if isinstance(volume, (int, float)) and 0 <= volume <= 100 else -1
            return [("status", "ready", "Soloist local: estado recebido. Confirme o audio no Raspberry."),
                    ("playback", state, name, artists, "Raspberry TV / Soloist", str(volume))]
        if command == "queue":
            data = self.ctl(["queue", "20"], True)
            entries = []
            for entry in (data.get("upcoming") or [])[:20]:
                if not isinstance(entry, dict):
                    continue
                item = entry.get("item") or {}
                uri = item.get("uri", "")
                try:
                    kind, _ = require_uri(uri)
                except (SpotifyError, TypeError):
                    continue
                if kind not in ("track", "episode"):
                    continue
                name, artists = self.entity(item)
                entries.append(("row", kind, uri, name, artists, "1"))
            return [("status", "ready", "Fila local; X inicia a faixa selecionada no Soloist."),
                    ("page", "queue", "0", "-1", str(len(entries)), ""), *entries]
        if command in ("volume-up", "volume-down"):
            data = self.ctl(["now"], True)
            current = data.get("volume")
            if not isinstance(current, (int, float)) or not 0 <= current <= 100:
                raise SpotifyError("unavailable", "Volume Soloist indisponivel.")
            self.ctl(["volume", str(max(0, min(100, int(current) + (5 if command == "volume-up" else -5))))])
        elif command in ("play", "resume"):
            if value:
                require_uri(value)
            self.ctl(["play", *([value] if value else [])])
        elif command in ("pause", "next", "previous", "activate", "deactivate"):
            self.ctl(["prev" if command == "previous" else command])
        else:
            raise SpotifyError("request", "Comando Soloist invalido.")
        return [("status", "ready", "Comando local aceito. Atualize o estado e confirme o audio no Raspberry.")]


def page_rows(kind, data, offset, parent=""):
    rows = []
    for entry in (data.get("items") or [])[:PAGE_SIZE]:
        if not isinstance(entry, dict):
            continue
        if kind == "playlists":
            item = entry
            detail = (item.get("owner") or {}).get("display_name", "")
            item_kind = "playlist"
        else:
            item = entry.get("item", entry.get("track")) if kind == "items" else entry
            if not isinstance(item, dict):
                continue
            detail = ", ".join(clean_label(a.get("name")) for a in (item.get("artists") or []) if isinstance(a, dict))
            item_kind = item.get("type", "track")
        uri = item.get("uri", "")
        try:
            require_uri(uri)
        except (SpotifyError, TypeError):
            continue
        enabled = not item.get("is_local", False) and item.get("is_playable", True) is not False
        rows.append(("row", item_kind, uri, clean_label(item.get("name")), clean_label(detail), "1" if enabled else "0"))
    total = data.get("total")
    total = total if type(total) is int and 0 <= total <= 100000 else offset + len(rows)
    size = 10 if kind == "search" else PAGE_SIZE
    next_offset = offset + size if data.get("next") and offset + size <= 100000 else -1
    return [("page", kind, str(offset), str(next_offset), str(total), parent), *rows]


def command_records(client: Client, command: str, value="", offset=0):
    if command == "logout":
        client.store.logout()
        return [("status", "auth", "Conta desconectada neste aparelho. A revogacao global pode ser feita na conta Spotify.")]
    if command == "profile":
        data = client.profile()
        return [("status", "ready", "Perfil atualizado."), ("profile", clean_label(data.get("display_name") or data.get("id")))]
    if command == "playlists":
        return [("status", "ready", "X: reproduzir playlist. Quadrado: listar faixas."), *page_rows("playlists", client.playlists(offset), offset)]
    if command == "items":
        kind, identity = require_uri(value)
        if kind != "playlist":
            raise SpotifyError("request", "Selecione uma playlist.")
        return [("status", "ready", "Faixas da playlist; X reproduz no dispositivo Spotify."), *page_rows("items", client.playlist_items(identity, offset), offset, value)]
    if command == "search":
        data = client.search(value, offset).get("tracks") or {}
        return [("status", "ready", "Resultados da busca."), *page_rows("search", data, offset, value)]
    if command == "devices":
        rows = []
        for device in (client.devices().get("devices") or [])[:50]:
            if not isinstance(device, dict) or not device.get("id"):
                continue
            try:
                device_query(device["id"], required=True)
            except SpotifyError:
                continue
            rows.append(("row", "device", device["id"], clean_label(device.get("name")),
                         "ATIVO" if device.get("is_active") else clean_label(device.get("type")), "0" if device.get("is_restricted") else "1"))
        return [("status", "ready", "X seleciona sem iniciar musica." if rows else "Abra o Spotify em um dispositivo para ele aparecer aqui."),
                ("page", "devices", "0", "-1", str(len(rows)), ""), *rows]
    if command == "status":
        data = client.playback()
        item, device = data.get("item") or {}, data.get("device") or {}
        artists = ", ".join(clean_label(a.get("name")) for a in (item.get("artists") or []) if isinstance(a, dict))
        volume = device.get("volume_percent")
        volume = volume if type(volume) is int and 0 <= volume <= 100 else -1
        state = "playing" if data.get("is_playing") else "paused" if data else "idle"
        return [("status", "ready", "Estado atualizado; audio sai no dispositivo Spotify, conforme a selecao da conta."),
                ("playback", state, clean_label(item.get("name")), clean_label(artists), clean_label(device.get("name")), str(volume))]
    if command in ("play", "resume"):
        client.play(value if command == "play" else "")
    elif command == "pause":
        client.pause()
    elif command in ("next", "previous"):
        client.skip(command)
    elif command == "volume-up":
        client.change_volume(5)
    elif command == "volume-down":
        client.change_volume(-5)
    elif command == "volume":
        try:
            percent = int(value)
        except ValueError:
            raise SpotifyError("request", "Volume precisa estar entre 0 e 100.") from None
        client.volume(percent)
    elif command == "transfer":
        client.transfer(value)
    else:
        raise SpotifyError("request", "Comando Spotify desconhecido.")
    return [("status", "ready", "Comando aceito pelo Spotify. Atualize o estado para confirmar a reproducao.")]


def emit(records):
    print("RTVSPOTIFY\t1")
    for record in records:
        print("\t".join(parse.quote(str(field), safe="") for field in record))


def main(argv=None):
    parser = argparse.ArgumentParser(description="Raspberry TV: Spotify Connect com OAuth PKCE; nunca recebe segredos por argumento.")
    parser.add_argument("command", choices=("configure", "login", "logout", "profile", "playlists", "items", "search", "devices", "status", "play", "resume", "pause", "next", "previous", "volume", "volume-up", "volume-down", "transfer", "soloist-configure", "soloist-run", "soloist-status", "soloist-queue", "soloist-play", "soloist-resume", "soloist-pause", "soloist-next", "soloist-previous", "soloist-volume-up", "soloist-volume-down", "soloist-activate", "soloist-deactivate"))
    parser.add_argument("value", nargs="?", default="")
    parser.add_argument("--offset", type=int, default=0)
    parser.add_argument("--no-browser", action="store_true")
    args = parser.parse_args(argv)
    store = Store()
    interactive = args.command in ("configure", "login", "soloist-configure", "soloist-run")
    try:
        if args.command == "soloist-run":
            # Its lifetime is intentionally separate from short API operations.
            # The official client protects the data directory against duplicates.
            Soloist(store).run_foreground()
            return 0
        with store.locked():
            if args.command == "soloist-configure":
                with warnings.catch_warnings():
                    warnings.simplefilter("error", getpass.GetPassWarning)
                    try:
                        key = getpass.getpass("Chave Soloist (entrada oculta, nunca use argumentos de linha de comando): ").strip()
                    except getpass.GetPassWarning:
                        raise SpotifyError("setup", "Use um terminal interativo com entrada oculta; a chave nao sera lida com eco.") from None
                if not re.fullmatch(r"spak_[A-Za-z0-9_-]{15,507}", key):
                    raise SpotifyError("setup", "Formato de chave Soloist invalido.")
                device = input("No PipeWire de saida (vazio: padrao do usuario): ").strip()
                store.write("soloist.private.json", {"api_key": key, "pipewire_device": device})
                print("Chave Soloist salva somente em arquivo privado fora do projeto.")
                return 0
            if args.command == "configure":
                value = input("Client ID publico do painel Spotify (32 caracteres hexadecimais): ").strip()
                if not re.fullmatch(r"[0-9a-fA-F]{32}", value):
                    raise SpotifyError("setup", "Client ID invalido. Nao informe Client Secret, Access Token ou outra chave.")
                old = store.read("config.private.json")
                if old.get("client_id") != value:
                    store.logout()
                store.write("config.private.json", {"client_id": value, "redirect_uri": REDIRECT})
                print("Client ID salvo em arquivo privado. Cadastre esta Redirect URI no painel:")
                print(REDIRECT)
                return 0
            client = Client(store)
            if args.command == "login":
                login(client, args.no_browser)
                return 0
            records = Soloist(store).records(args.command, args.value) if args.command.startswith("soloist-") else command_records(client, args.command, args.value, bounded_offset(args.offset))
        emit(records)
        return 0
    except SpotifyError as exc:
        if interactive:
            print(str(exc), file=sys.stderr)
        else:
            emit([("status", exc.code, str(exc))])
    except (OSError, ValueError, TypeError, KeyError, AttributeError, OverflowError):
        if interactive:
            print("Spotify: falha local ou resposta invalida; confira a configuracao e tente novamente.", file=sys.stderr)
        else:
            emit([("status", "error", "Spotify: falha local ou resposta invalida. Nenhum detalhe privado registrado.")])
    except KeyboardInterrupt:
        if interactive:
            print("Operacao cancelada.", file=sys.stderr)
        return 130
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
