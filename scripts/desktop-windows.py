#!/usr/bin/env python3
"""Windows shortcut bridge to the existing native app. SSH handles authentication."""
import ast
import base64
import ctypes
import json
from pathlib import Path
import shutil
import subprocess
import sys
import zlib


def main():
    directory = Path(__file__).resolve().parent
    source = (directory / "desktop-launcher.py").read_bytes()
    ast.parse(source.decode("utf-8"))
    payload = base64.b64encode(zlib.compress(source, 9)).decode("ascii")
    remote = ('python3 -c "import base64,zlib;code=zlib.decompress(base64.b64decode(\'' + payload +
              '\'));exec(compile(code,\'raspberry-tv-launcher\',\'exec\'),'
              '{\'__name__\':\'__main__\',\'SOURCE_BYTES\':code})" --install --launch')
    if "--update-native" in sys.argv:
        updater = (directory / "desktop-native-update.py").read_text(encoding="utf-8")
        ast.parse(updater)
        release = json.loads((directory / "release.json").read_text(encoding="utf-8"))
        bundle = json.dumps({"launcher": source.decode("utf-8"), "updater": updater, "release": release}).encode()
        payload = base64.b64encode(zlib.compress(bundle, 9)).decode("ascii")
        remote = ('python3 -c "import base64,zlib,json;v=json.loads(zlib.decompress(base64.b64decode(\'' + payload +
                  '\')));exec(compile(v[\'updater\'],\'raspberry-tv-update\',\'exec\'),'
                  '{\'__name__\':\'__main__\',\'LAUNCHER_BYTES\':v[\'launcher\'].encode(),\'RELEASE_SPEC\':v[\'release\']})"')
    if len(remote) >= 30000:
        raise RuntimeError("Atualizacao excedeu o tamanho permitido; pacote preservado.")
    if "--check" in sys.argv:
        print("Launcher valido; destino balancerec@192.168.100.17; nenhuma conexao executada.")
        print("Tamanho do comando:", len(remote))
        return 0
    ssh = shutil.which("ssh")
    if not ssh:
        raise RuntimeError("Cliente SSH do Windows nao encontrado.")
    result = subprocess.run([ssh, "-tt", "-o", "ConnectTimeout=8", "-o", "ServerAliveInterval=15",
                             "balancerec@192.168.100.17", remote],
                            creationflags=subprocess.CREATE_NEW_CONSOLE)
    if result.returncode:
        raise RuntimeError("Nao foi possivel abrir a Raspberry TV. Confira a conexao e a senha SSH na janela de autenticacao.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, ValueError) as error:
        ctypes.windll.user32.MessageBoxW(None, str(error), "Raspberry TV", 0x10)
        sys.exit(1)
