#!/usr/bin/env python3
"""Windows shortcut bridge to the existing native app. SSH handles authentication."""
import ast
import base64
import ctypes
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    directory = Path(__file__).resolve().parent
    source = (directory / "desktop-launcher.py").read_bytes()
    ast.parse(source.decode("utf-8"))
    payload = base64.b64encode(source).decode("ascii")
    remote = ('python3 -c "import base64;code=base64.b64decode(\'' + payload +
              '\');exec(compile(code,\'raspberry-tv-launcher\',\'exec\'),'
              '{\'__name__\':\'__main__\',\'SOURCE_BYTES\':code})" --install --launch')
    if "--check" in sys.argv:
        assert len(remote) < 30000
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
