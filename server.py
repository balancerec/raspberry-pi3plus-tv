#!/usr/bin/env python3
"""Raspberry TV independente: somente servidor web e status de leitura.

Extraído do módulo /tv de raspberry-spi-lcd-nrf-system.
Não altera serviços, rede, hardware ou configurações do DietPi.
"""
from __future__ import annotations

import argparse
import json
import mimetypes
import socket
import threading
from datetime import datetime, timezone
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

HERE = Path(__file__).resolve().parent
STATIC = HERE / "static"
ALLOWED_STATIC = frozenset({"tv.html", "tv.css", "tv.js", "info.html", "info.js", "info.css"})


class CpuSampler:
    def __init__(self):
        self._previous = None
        self._lock = threading.Lock()

    def sample(self) -> float | None:
        # Os counters do /proc/stat são cumulativos. Calculamos a variação.
        try:
            line = Path("/proc/stat").read_text(encoding="ascii").splitlines()[0]
            fields = [int(x) for x in line.split()[1:]]
            if len(fields) < 4:
                return None
            idle = fields[3] + (fields[4] if len(fields) > 4 else 0)
            total = sum(fields)
            with self._lock:
                old = self._previous
                self._previous = (total, idle)
            if old is None or total <= old[0]:
                return None
            return round(100 * (1 - (idle - old[1]) / (total - old[0])), 1)
        except (OSError, ValueError, IndexError):
            return None


CPU = CpuSampler()


def temperature_c() -> float | None:
    for path in ("/sys/class/thermal/thermal_zone0/temp", "/sys/devices/virtual/thermal/thermal_zone0/temp"):
        try:
            return round(int(Path(path).read_text(encoding="ascii").strip()) / 1000, 1)
        except (OSError, ValueError):
            continue
    return None


def primary_ip() -> str:
    # Não transmite dados: o connect de UDP apenas seleciona a interface local.
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.connect(("192.0.2.1", 80))
            return sock.getsockname()[0]
    except OSError:
        try:
            return socket.gethostbyname(socket.gethostname())
        except OSError:
            return "127.0.0.1"


def system_status() -> dict:
    return {
        "system": {
            "hostname": socket.gethostname(),
            "ip_address": primary_ip(),
            "cpu_percent": CPU.sample(),
            "temperature_c": temperature_c(),
        },
        # Controle via Gamepad API é detectado no navegador, não no servidor.
        "gamepad": {"connected": False, "device_name": ""},
        "connectivity": {"wifi": {"connected": False, "ssid": ""}},
        "application": {"name": "Raspberry TV", "standalone": True, "hardware_backend": False},
    }


class TVHandler(BaseHTTPRequestHandler):
    server_version = "RaspberryTV/1.0"
    sys_version = ""

    def log_message(self, fmt: str, *args) -> None:
        print(f"[{datetime.now(timezone.utc).isoformat(timespec='seconds')}] {self.client_address[0]} - {fmt % args}")

    def _write(self, data: bytes, mime: str, status: int = 200, cache: str = "no-store") -> None:
        self.send_response(status)
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", cache)
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("X-Frame-Options", "DENY")
        self.send_header("Referrer-Policy", "no-referrer")
        self.send_header("Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'")
        self.end_headers()
        try:
            self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def do_GET(self):
        path = urlsplit(self.path).path
        if path == "/api/status":
            payload = json.dumps(system_status(), ensure_ascii=False).encode("utf-8")
            self._write(payload, "application/json; charset=utf-8")
            return
        if path in ("/", "/tv", "/tv/"):
            filename = "tv.html"
        elif path in ("/info", "/info/"):
            filename = "info.html"
        else:
            filename = path.removeprefix("/")
        if filename not in ALLOWED_STATIC:
            self.send_error(HTTPStatus.NOT_FOUND, "Recurso não encontrado")
            return
        file_path = STATIC / filename
        try:
            data = file_path.read_bytes()
        except OSError:
            self.send_error(HTTPStatus.NOT_FOUND, "Recurso indisponível")
            return
        mime = mimetypes.guess_type(filename)[0] or "application/octet-stream"
        if mime.startswith("text/") or mime in ("application/javascript", "text/javascript"):
            mime += "; charset=utf-8"
        self._write(data, mime)

    def do_POST(self):
        self.send_error(HTTPStatus.METHOD_NOT_ALLOWED, "API somente leitura")


def main() -> None:
    parser = argparse.ArgumentParser(description="Interface Raspberry TV isolada do sistema SPI/LCD/NRF")
    parser.add_argument("--host", default="127.0.0.1", help="Host; use 0.0.0.0 apenas para acesso na LAN")
    parser.add_argument("--port", type=int, default=8090, help="Porta (padrão: 8090; 8080 permanece intacta)")
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("a porta deve estar entre 1 e 65535")
    server = ThreadingHTTPServer((args.host, args.port), TVHandler)
    server.daemon_threads = True
    print(f"Raspberry TV isolada: http://127.0.0.1:{args.port}/tv", flush=True)
    if args.host == "0.0.0.0":
        print(f"Acesso LAN (se permitido pelo firewall): http://{primary_ip()}:{args.port}/tv", flush=True)
    try:
        server.serve_forever(poll_interval=0.25)
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
