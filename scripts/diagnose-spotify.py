#!/usr/bin/env python3
"""Read-only audio/session diagnosis. Never reads keys or process command lines."""
import json
import os
from pathlib import Path
import shutil
import subprocess

PRIVATE_ENVIRONMENT = None
PRIVATE_PACTL = None


def label(value):
    return ''.join(c for c in str(value or '') if c.isprintable())[:180]


def run(arguments, timeout=4):
    arguments = list(arguments)
    if arguments[0] == "pactl" and PRIVATE_PACTL:
        arguments[0] = PRIVATE_PACTL
    if not shutil.which(arguments[0]) and not Path(arguments[0]).is_file():
        return None
    try:
        result = subprocess.run(arguments, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                                stdin=subprocess.DEVNULL, timeout=timeout, check=False, env=PRIVATE_ENVIRONMENT)
        if result.returncode or len(result.stdout) > 1024 * 1024:
            return None
        return result.stdout.decode('utf-8', errors='replace')
    except (OSError, subprocess.TimeoutExpired):
        return None


def json_run(arguments):
    result = run(arguments)
    try:
        return json.loads(result) if result is not None else None
    except (TypeError, ValueError):
        return None


def volumes(item):
    value = item.get('volume') or {}
    return ','.join(label(channel.get('value_percent')) for channel in value.values()
                    if isinstance(channel, dict)) if isinstance(value, dict) else 'desconhecido'


runtime = Path('/run/user') / str(os.getuid())
if not os.environ.get('XDG_RUNTIME_DIR') and runtime.is_dir():
    os.environ['XDG_RUNTIME_DIR'] = str(runtime)
if not os.environ.get('DBUS_SESSION_BUS_ADDRESS') and (runtime / 'bus').exists():
    os.environ['DBUS_SESSION_BUS_ADDRESS'] = 'unix:path=' + str(runtime / 'bus')
print('DIAGNOSTICO DE AUDIO SPOTIFY - SOMENTE LEITURA', flush=True)
print('Runtime:', os.environ.get('XDG_RUNTIME_DIR', 'nao definido'))
print('Socket PipeWire:', (runtime / 'pipewire-0').is_socket())
print('Socket PulseAudio:', (runtime / 'pulse/native').is_socket())

# This separate profile contains paths/settings only, never Spotify credentials.
profile_path = Path(os.environ.get('XDG_CONFIG_HOME') or Path.home() / '.config') / 'raspberry-tv/spotify/audio.private.json'
try:
    if profile_path.is_file() and not profile_path.is_symlink() and profile_path.stat().st_size <= 4096:
        profile = json.loads(profile_path.read_text(encoding='utf-8'))
        if isinstance(profile, dict) and profile.get('owner') == 'raspberry-tv-private-pulse-v1' and profile.get('enabled') is True:
            private_runtime = Path(os.environ.get('XDG_RUNTIME_DIR') or os.environ.get('XDG_STATE_HOME') or Path.home() / '.local/state') / 'raspberry-tv-audio'
            if len(str(private_runtime / 'native').encode()) >= 100:
                private_runtime = Path('/tmp') / ('raspberry-tv-audio-' + str(os.getuid()))
            PRIVATE_ENVIRONMENT = dict(os.environ, PULSE_SERVER='unix:' + str(private_runtime / 'native'),
                                       PULSE_COOKIE=str(private_runtime / 'cookie'))
            prefix = profile.get('bundle')
            if prefix:
                prefix = Path(prefix)
                managed = Path(os.environ.get('XDG_DATA_HOME') or Path.home() / '.local/share') / 'raspberry-tv/audio-runtime'
                if not prefix.resolve().is_relative_to(managed.resolve()) or not (prefix / 'managed.txt').is_file():
                    raise ValueError
                PRIVATE_PACTL = str(prefix / 'usr/bin/pactl')
                PRIVATE_ENVIRONMENT['LD_LIBRARY_PATH'] = ':'.join(sorted({str(path.parent) for path in prefix.rglob('*.so*') if path.is_file()}))
            print('Socket PulseAudio privado Raspberry TV:', (private_runtime / 'native').is_socket())
except (OSError, ValueError, TypeError):
    PRIVATE_ENVIRONMENT = None; PRIVATE_PACTL = None
    print('Perfil privado de audio indisponivel ou invalido.')

default = run(['pactl', 'get-default-sink'])
print('\nSaida padrao Pulse/PipeWire-Pulse:', label(default) if default else 'nao acessivel')
sinks = json_run(['pactl', '-f', 'json', 'list', 'sinks'])
print('Saidas de audio:')
if isinstance(sinks, list):
    for sink in sinks:
        if isinstance(sink, dict):
            print(json.dumps({key: value for key, value in {
                'id': sink.get('index'), 'nome': label(sink.get('name')),
                'descricao': label(sink.get('description')), 'mudo': sink.get('mute'),
                'volume': volumes(sink), 'estado': label(sink.get('state')),
                'porta_ativa': label(sink.get('active_port'))}.items()}, ensure_ascii=True))
else:
    print('  pactl ausente, servidor inacessivel ou JSON indisponivel.')

streams = json_run(['pactl', '-f', 'json', 'list', 'sink-inputs'])
print('\nFluxos de audio (aplicativos):')
if isinstance(streams, list):
    for stream in streams:
        if not isinstance(stream, dict):
            continue
        properties = stream.get('properties') or {}
        print(json.dumps({'id': stream.get('index'), 'saida_id': stream.get('sink'),
                          'app': label(properties.get('application.name')),
                          'binario': label(properties.get('application.process.binary')),
                          'mudo': stream.get('mute'), 'suspenso': stream.get('corked'),
                          'volume': volumes(stream)}, ensure_ascii=True))
    if not streams:
        print('  nenhum fluxo registrado.')
else:
    print('  nao acessiveis pelo pactl.')

wp = run(['wpctl', 'status', '--name'])
print('\nWirePlumber / PipeWire:')
if wp:
    for line in wp.splitlines()[:85]:
        print(label(line))
else:
    print('  wpctl ausente ou sessao PipeWire inacessivel.')

print('\nPlacas ALSA (somente inventario):')
alsa = run(['aplay', '-l'])
print(alsa[:6000] if alsa else '  aplay ausente ou inventario indisponivel.')

binary = os.environ.get('SOLOIST_BIN') or str(Path.home() / '.local/opt/spotify-soloist/soloist')
version = run([binary, '--version'])
print('\nVersao Soloist:', label(version) if version else 'nao obtida')
data = Path(os.environ.get('XDG_DATA_HOME') or Path.home() / '.local/share') / 'raspberry-tv/soloist'
try:
    address = (data / 'ws.addr').read_text(encoding='ascii').strip()
    port = int((data / 'ws.port').read_text(encoding='ascii').strip())
    if address != '127.0.0.1' or not 1 <= port <= 65535:
        raise ValueError
    now = json_run([binary, 'ctl', '--ws', f'127.0.0.1:{port}', 'now', '--json'])
    if not isinstance(now, dict):
        raise ValueError
    actions = now.get('available_actions') or {}
    known = ('play', 'pause', 'seek', 'skip_next', 'skip_prev', 'shuffle', 'set_repeat')
    print('Estado Soloist:', json.dumps({
        'estado': label(now.get('status')), 'ativo': now.get('is_active'), 'volume': now.get('volume'),
        'tem_faixa': bool((now.get('item') or {}).get('uri')),
        'tem_contexto': bool((now.get('context') or {}).get('uri')),
        'acoes': [name for name in known if name in actions]}, ensure_ascii=True))
except (OSError, ValueError, TypeError, AttributeError):
    print('Estado Soloist: cliente ausente, sem sessao ou endpoint local indisponivel.')
print('\nFim. Credenciais Spotify e argv dos processos nao foram lidos; nenhum token/cookie foi exibido.')
