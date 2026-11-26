#!/usr/bin/env python3
"""Explicit private HDMI audio setup; no root, package install or service edits."""
import importlib.util
import io
import os
from pathlib import Path
import signal
import struct
import math
import tempfile
import time
import wave


def module(path, name="raspberry_tv_audio_setup_backend"):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def retire_provider(session):
    """Setup may replace the user's exact Soloist process, never a service."""
    session.stop()
    if not session.provider_running():
        return
    client = session.core().Soloist(session.core().Store())
    data, _ = client.directories()
    path = data / "soloist.pid"
    if path.is_symlink() or path.stat().st_size > 24:
        raise RuntimeError("PID Soloist invalido; cliente externo preservado.")
    pid = int(path.read_text(encoding="ascii").strip())
    process = Path("/proc") / str(pid)
    if pid <= 1 or process.stat().st_uid != os.getuid() or (process / "exe").resolve() != Path(client.binary()).resolve():
        raise RuntimeError("Cliente externo nao foi identificado com seguranca; preservado.")
    identity = (process / "stat").read_text().rsplit(") ", 1)[1].split()[19]
    # Only this confirmed SDK process is signalled. Its parent/terminal and
    # unrelated audio servers are left untouched.
    if (process / "stat").read_text().rsplit(") ", 1)[1].split()[19] != identity:
        raise RuntimeError("PID mudou; cliente externo preservado.")
    os.kill(pid, signal.SIGTERM)
    deadline = time.monotonic() + 5
    while session.provider_running() and time.monotonic() < deadline:
        time.sleep(.1)
    if session.provider_running():
        raise RuntimeError("Soloist externo ainda esta aberto; feche-o antes da correcao. Nenhum encerramento forcado foi aplicado.")


def tone(backend, value, environment):
    # 0.25 s, 440 Hz, amplitude 3%, sink volume 35%. Never a full-volume tone.
    frames = b"".join(struct.pack("<hh", sample, sample) for sample in
                      (int(983 * math.sin(index * 2 * math.pi * 440 / 48000)) for index in range(12000)))
    with tempfile.NamedTemporaryFile(prefix="rtv-audio-tone-", suffix=".wav") as output:
        with wave.open(output, "wb") as wav:
            wav.setnchannels(2); wav.setsampwidth(2); wav.setframerate(48000); wav.writeframes(frames)
        output.flush()
        executables = backend.tools(value)
        backend.command([executables["pactl"], "set-sink-volume", backend.SINK, "35%"], environment=environment, timeout=2)
        backend.command([executables["paplay"], "--device=" + backend.SINK, output.name], environment=environment, timeout=5)


def prepare(root):
    backend = module(root / "integrations/spotify/audio_backend.py")
    backend.profile()  # Validate any old profile before touching the application.
    value = backend.prepare_runtime()
    return backend, value


def apply(root, launcher, backend, value, activate):
    paths = [backend.config_directory() / "audio.private.json", backend.config_directory() / "audio-focus-v1"]
    previous = {}
    for path in paths:
        if path.is_symlink():
            raise RuntimeError("Perfil de audio e um link inesperado; preservado.")
        previous[path] = path.read_bytes() if path.is_file() else None
    # Source build, tests and dependency downloads have already succeeded.
    launcher["stop_native"]()
    session = launcher["SoloistSession"](root)
    retire_provider(session)
    try:
        print("Testando a saida HDMI com um tom curto e baixo...", flush=True)
        with backend.PrivatePulse(value) as environment:
            tone(backend, value, environment)
        print("Teste de envio ao HDMI concluiu; confirme o som na TV apos a abertura.", flush=True)
        backend.set_profile(value)
        activate(root, launcher)
    except BaseException:
        session.stop()
        for path, data in previous.items():
            if data is None:
                path.unlink(missing_ok=True)
            else:
                backend.atomic_write(path, data)
        raise
    print("Audio privado configurado. Nenhum pacote ou servico global foi instalado/alterado.", flush=True)
