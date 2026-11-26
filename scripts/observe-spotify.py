#!/usr/bin/env python3
"""Observe a failing Connect transfer without playback commands or credentials."""
import argparse
import json
import os
from pathlib import Path
import shutil
import time


def numeric(value, minimum, maximum):
    return value if type(value) in (int, float) and minimum <= value <= maximum else None


def playback_fields(data):
    if not isinstance(data, dict):
        return {"api": "sem_resposta"}
    position = data.get("position")
    position = position if isinstance(position, dict) else {}
    item, context = data.get("item"), data.get("context")
    return {"api": "disponivel", "estado": data.get("status") if data.get("status") in
            ("idle", "buffering", "playing", "paused") else "desconhecido",
            "ativo": data.get("is_active") if type(data.get("is_active")) is bool else None,
            "volume": numeric(data.get("volume"), 0, 100),
            "posicao_ms": numeric(position.get("position_ms"), 0, 604800000),
            "tem_faixa": bool(item.get("uri")) if isinstance(item, dict) else False,
            "tem_contexto": bool(context.get("uri")) if isinstance(context, dict) else False}


def process_fields(directory, binary):
    """Check the runtime PID and executable, never /proc/*/cmdline."""
    result = {"pid": None, "processo": "desconhecido"}
    try:
        path = directory / "soloist.pid"
        if path.is_symlink() or path.stat().st_size > 24:
            return result
        text = path.read_text(encoding="ascii").strip()
        if not text.isdigit() or not 1 <= int(text) <= 1073741824:
            return result
        pid = int(text)
        result["pid"] = pid
        process = Path("/proc") / str(pid)
        if not process.exists():
            result["processo"] = "ausente"
        elif process.stat().st_uid != os.getuid() or (process / "exe").resolve() != Path(binary).resolve():
            result["processo"] = "nao_corresponde"
        else:
            state = (process / "stat").read_text().rsplit(") ", 1)[1].split()[0]
            result["processo"] = "ausente" if state == "Z" else "ativo"
    except (OSError, ValueError, IndexError):
        pass
    return result


def binary_path(core):
    for candidate in (core.get("binary"), shutil.which("soloist")):
        if candidate and Path(candidate).is_file():
            return str(Path(candidate).resolve())
    # A manually started client may not be in the SSH PATH. Its owned runtime
    # PID reveals only the executable path, without reading secret-bearing argv.
    try:
        path = core["data"] / "soloist.pid"
        if path.is_symlink() or path.stat().st_size > 24:
            return core["binary"]
        text = path.read_text(encoding="ascii").strip()
        if not text.isdigit() or not 1 <= int(text) <= 1073741824:
            return core["binary"]
        process = Path("/proc") / text
        executable = (process / "exe").resolve()
        if process.stat().st_uid == os.getuid() and executable.name == "soloist" and executable.is_file():
            return str(executable)
    except (OSError, ValueError):
        pass
    return core["binary"]


def snapshot(core):
    result = process_fields(core["data"], core["binary"])
    try:
        address_file, port_file = core["data"] / "ws.addr", core["data"] / "ws.port"
        if address_file.is_symlink() or port_file.is_symlink() or address_file.stat().st_size > 64 or port_file.stat().st_size > 8:
            raise ValueError
        address = address_file.read_text(encoding="ascii").strip()
        text = port_file.read_text(encoding="ascii").strip()
        if address != "127.0.0.1" or not text.isdigit() or not 1 <= int(text) <= 65535:
            raise ValueError
        response = core["json_run"]([core["binary"], "ctl", "--ws", "127.0.0.1:" + text, "now", "--json"])
        result.update(playback_fields(response))
    except (OSError, ValueError):
        result["api"] = "endpoint_indisponivel"
    return result


def observe(core, seconds, clock=time.monotonic, sleep=time.sleep, emit=print):
    if type(seconds) is not int or not 1 <= seconds <= 30:
        raise ValueError("A coleta deve durar de 1 a 30 segundos.")
    start = clock()
    transitions, identities = [], set()
    playing, position_advanced, last_position = False, False, None
    emit("AGORA: escolha Raspberry TV nos dispositivos do Spotify e tente reproduzir.")
    emit("Coleta somente leitura; nao envia play, pause, activate ou configuracoes.")
    while clock() - start < seconds:
        data = snapshot(core)
        data["tempo_s"] = round(clock() - start, 1)
        emit(json.dumps(data, ensure_ascii=True, allow_nan=False))
        state = data.get("estado")
        if state and (not transitions or transitions[-1] != state):
            transitions.append(state)
        if data.get("processo") == "ativo":
            identities.add(data["pid"])
        playing = playing or state == "playing"
        position = data.get("posicao_ms")
        if position is not None:
            position_advanced = position_advanced or (last_position is not None and position > last_position)
            last_position = position
        remaining = seconds - (clock() - start)
        if remaining <= 0:
            break
        sleep(min(2, remaining))
    emit("RESUMO: " + json.dumps({"transicoes": transitions, "troca_de_pid_observada": len(identities) > 1,
                                  "playing_reportado": playing, "posicao_avancou": position_advanced}, ensure_ascii=True))
    emit("Estado informado pelo cliente nao comprova audio audivel. A causa exige interpretar a coleta.")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seconds", type=int, default=30, choices=range(1, 31))
    args = parser.parse_args(argv)
    if os.name != "posix" or os.geteuid() == 0:
        parser.error("Execute no Raspberry como usuario da sessao, sem sudo.")
    source = globals().get("DIAGNOSTIC_SOURCE")
    if source is None:
        source = Path(__file__).with_name("diagnose-spotify.py").read_bytes()
    core = {"__name__": "raspberry_tv_audio_inventory"}
    exec(compile(source, "raspberry-tv-audio-inventory", "exec"), core)
    resolved_binary = binary_path(core)
    if resolved_binary != core["binary"]:
        version = core["run"]([resolved_binary, "--version"])
        print("Versao do cliente localizado:", core["label"](version) if version else "nao obtida")
        core["binary"] = resolved_binary
    observe(core, args.seconds)
    print("\nFluxos Pulse apos a tentativa (nao cobre clientes PipeWire nativos):")
    streams = core["json_run"](["pactl", "-f", "json", "list", "sink-inputs"])
    if isinstance(streams, list):
        for stream in streams:
            if not isinstance(stream, dict):
                continue
            props = stream.get("properties") or {}
            if not isinstance(props, dict):
                continue
            name = str(props.get("application.name", "")) + " " + str(props.get("application.process.binary", ""))
            if "soloist" in name.lower() or "spotify" in name.lower():
                print(json.dumps({"app": "Spotify/Soloist", "saida_id": numeric(stream.get("sink"), 0, 2147483647),
                                  "mudo": stream.get("mute") if type(stream.get("mute")) is bool else None,
                                  "suspenso": stream.get("corked") if type(stream.get("corked")) is bool else None}, ensure_ascii=True))
        print("Fluxos Pulse consultados:", len(streams))
    else:
        print("API Pulse nao acessivel nesta sessao.")
    print("Fim da coleta. Nenhuma chave foi lida e nenhuma preferencia ou servico foi alterado.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
