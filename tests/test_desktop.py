"""Desktop/Soloist lifecycle tests. No real account, key or Spotify binary."""
import base64
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import types
import unittest
import uuid
from unittest import mock
import zlib

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/desktop-launcher.py"


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


desktop = module("desktop_launcher_test", SCRIPT)


class DesktopLogicTests(unittest.TestCase):
    def test_healthy_client_is_reused_without_key_read_or_spawn(self):
        session = desktop.SoloistSession(ROOT)
        with mock.patch.object(session, "locked", return_value=contextlib.nullcontext()), mock.patch.object(session, "ready", return_value=True), mock.patch.object(session, "core") as core, mock.patch.object(desktop.subprocess, "Popen") as spawn:
            session.ensure()
        core.assert_not_called()
        spawn.assert_not_called()

    def test_unconfigured_client_is_not_started(self):
        session = desktop.SoloistSession(ROOT)
        core = mock.Mock()
        core.Store.return_value.read.return_value = {}
        with mock.patch.object(session, "locked", return_value=contextlib.nullcontext()), mock.patch.object(session, "ready", return_value=False), mock.patch.object(session, "core", return_value=core), mock.patch.object(desktop.subprocess, "Popen") as spawn:
            with self.assertRaisesRegex(RuntimeError, "configurado"):
                session.ensure()
        spawn.assert_not_called()

    def test_start_failure_is_redacted_in_native_protocol(self):
        session = mock.Mock()
        session.ensure.side_effect = RuntimeError("artificial-sensitive-value")
        core = session.core.return_value
        core.emit.side_effect = lambda rows: print(json.dumps(rows))
        with mock.patch.object(desktop, "SoloistSession", return_value=session), contextlib.redirect_stdout(io.StringIO()) as output:
            result = desktop.spotify_request(["soloist-status"])
        self.assertEqual(result, 1)
        self.assertIn("unavailable", output.getvalue())
        self.assertNotIn("artificial-sensitive-value", output.getvalue())
        core.main.assert_not_called()

    def test_windows_transport_roundtrip_and_command_limit(self):
        bridge = module("desktop_windows_test", ROOT / "scripts/desktop-windows.py")
        with mock.patch.object(sys, "argv", ["desktop-windows.py"]), mock.patch.object(bridge.shutil, "which", return_value="ssh"), mock.patch.object(bridge.subprocess, "CREATE_NEW_CONSOLE", 0, create=True), mock.patch.object(bridge.subprocess, "run", return_value=types.SimpleNamespace(returncode=0)) as run:
            self.assertEqual(bridge.main(), 0)
        command = run.call_args.args[0][-1]
        payload = re.search(r"b64decode\('([^']+)'\)", command).group(1)
        self.assertEqual(zlib.decompress(base64.b64decode(payload)), SCRIPT.read_bytes())
        self.assertLess(len(command), 30000)


FAKE_CORE = r'''
import json, os, socket, subprocess, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
class SpotifyError(Exception):
    def __init__(self, code, message):
        super().__init__(message)
        self.code = code
class Store:
    def read(self, name):
        return {} if (ROOT / 'unconfigured').exists() else {'api_key': 'configured-fixture'}
class Soloist:
    def __init__(self, store): self.runner = None
    def binary(self): return sys.executable
    def directories(self):
        path = Path(os.environ['XDG_DATA_HOME']) / 'raspberry-tv/soloist'
        path.mkdir(parents=True, exist_ok=True)
        return path, path
    def ctl(self, arguments):
        data, _ = self.directories()
        port = int((data / 'ws.port').read_text())
        with socket.create_connection(('127.0.0.1', port), timeout=.3) as connection:
            connection.sendall(b'ping')
            if connection.recv(8) != b'ok': raise SpotifyError('network', 'fixture unavailable')
        return {}
    def run_foreground(self):
        with (ROOT / 'starts').open('a') as stream: stream.write('start\n')
        if (ROOT / 'fail').exists(): raise SpotifyError('unavailable', 'artificial-sensitive-value')
        process = subprocess.Popen([sys.executable, str(ROOT / 'provider.py')], stdin=subprocess.DEVNULL,
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try: process.wait()
        except KeyboardInterrupt:
            if process.poll() is None: process.terminate()
            try: process.wait(timeout=2)
            except subprocess.TimeoutExpired: process.kill(); process.wait()
def emit(rows):
    print('RTVSPOTIFY\t1')
    for row in rows: print('\t'.join(str(value) for value in row))
def main(arguments):
    emit([('status', 'ready', 'fixture-ready')])
    return 0
'''

PROVIDER = r'''
import os, socket
from pathlib import Path
data = Path(os.environ['XDG_DATA_HOME']) / 'raspberry-tv/soloist'
data.mkdir(parents=True, exist_ok=True)
server = socket.socket()
server.bind(('127.0.0.1', 0))
server.listen(8)
(data / 'ws.addr').write_text('127.0.0.1')
(data / 'ws.port').write_text(str(server.getsockname()[1]))
(data / 'provider.pid').write_text(str(os.getpid()))
while True:
    connection, _ = server.accept()
    with connection:
        connection.settimeout(.3)
        try:
            if connection.recv(4): connection.sendall(b'ok')
        except OSError: pass
'''

compile(FAKE_CORE, "fake_spotify_client.py", "exec")
compile(PROVIDER, "fake_soloist_provider.py", "exec")


@unittest.skipUnless(os.name == "posix" and hasattr(os, "geteuid") and os.geteuid() != 0,
                     "Non-root Linux process/session integration")
class DesktopProcessTests(unittest.TestCase):
    def setUp(self):
        base = Path(tempfile.gettempdir()).resolve()
        self.root = base / ("rtv-desktop-test-" + uuid.uuid4().hex)
        self.root.mkdir(mode=0o755)
        self.project = self.root / "project"
        helper = self.project / "integrations/spotify/spotify_client.py"
        helper.parent.mkdir(parents=True)
        helper.write_text(FAKE_CORE, encoding="utf-8")
        (self.project / "provider.py").write_text(PROVIDER, encoding="utf-8")
        self.environment = dict(os.environ, XDG_DATA_HOME=str(self.root / "data"),
                                XDG_STATE_HOME=str(self.root / "state"),
                                RASPBERRY_TV_SPOTIFY_PROJECT=str(self.project))
        self.patch = mock.patch.dict(os.environ, self.environment, clear=True)
        self.patch.start()
        self.session = desktop.SoloistSession(self.project)
        self.addCleanup(self.cleanup)

    def cleanup(self):
        try:
            self.session.stop()
        finally:
            self.patch.stop()
            if self.root.resolve().parent != Path(tempfile.gettempdir()).resolve() or not self.root.name.startswith("rtv-desktop-test-"):
                raise AssertionError("Unexpected cleanup target")
            shutil.rmtree(self.root)

    def request(self):
        return subprocess.run([sys.executable, "-I", str(SCRIPT), "soloist-status"], env=self.environment,
                              stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=12)

    def wait_for(self, predicate, timeout=5):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if predicate(): return
            time.sleep(.05)
        self.fail("Process condition timed out")

    def starts(self):
        path = self.project / "starts"
        return len(path.read_text().splitlines()) if path.exists() else 0

    def test_detached_start_reuse_and_private_permissions(self):
        config = self.project / "fixture-config.private.json"
        config.write_text("preserved-private-fixture", encoding="utf-8")
        result = self.request()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(self.session.ready())  # Initiating helper has exited.
        owner = self.session.read("owner.private.json")
        self.assertTrue(self.session.alive(owner))
        self.assertEqual(os.getpgid(owner["pid"]), owner["pid"])
        self.assertEqual(self.session.directory.stat().st_mode & 0o777, 0o700)
        self.assertEqual((self.session.directory / "owner.private.json").stat().st_mode & 0o777, 0o600)
        self.assertEqual(self.request().returncode, 0)
        self.assertEqual(self.starts(), 1)
        self.assertEqual(config.read_text(), "preserved-private-fixture")

    def test_concurrent_requests_start_only_one_provider(self):
        arguments = [sys.executable, "-I", str(SCRIPT), "soloist-status"]
        children = [subprocess.Popen(arguments, env=self.environment, stdout=subprocess.PIPE, stderr=subprocess.PIPE) for _ in range(2)]
        for child in children:
            output, error = child.communicate(timeout=12)
            self.assertEqual(child.returncode, 0, output + error)
        self.assertEqual(self.starts(), 1)

    def test_provider_exit_recovers_without_terminal(self):
        self.assertEqual(self.request().returncode, 0)
        data, _ = self.session.core().Soloist(self.session.core().Store()).directories()
        old_pid = int((data / "provider.pid").read_text())
        os.kill(old_pid, signal.SIGTERM)
        self.wait_for(lambda: self.starts() == 2 and self.session.ready())
        self.assertNotEqual(int((data / "provider.pid").read_text()), old_pid)

    def test_retry_limit_cooldown_and_error_redaction(self):
        (self.project / "fail").touch()
        result = self.request()
        self.assertEqual(result.returncode, 1)
        self.assertNotIn("artificial-sensitive-value", result.stdout + result.stderr)
        self.wait_for(lambda: self.starts() == 3 and bool(self.session.read("failure.private.json")))
        self.assertEqual(self.request().returncode, 1)
        self.assertEqual(self.starts(), 3)

    def test_external_client_is_reused_and_never_stopped(self):
        process = subprocess.Popen([sys.executable, str(self.project / "provider.py")], env=self.environment,
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
        try:
            self.wait_for(self.session.ready)
            self.session.ensure()
            self.assertFalse(self.session.read("owner.private.json"))
            self.session.stop()
            self.assertIsNone(process.poll())
            self.assertEqual(self.starts(), 0)
        finally:
            process.terminate()
            process.wait(timeout=3)

    def test_reopening_existing_tv_starts_provider_and_installs_stop_action(self):
        binary = self.project / "bin/raspberry-tv-native"
        binary.parent.mkdir()
        binary.symlink_to(sys.executable)  # This test process is an existing TV executable.
        wrapper = self.project / "scripts/run-native.sh"
        wrapper.parent.mkdir()
        wrapper.touch()
        with mock.patch.object(desktop, "projects", return_value=[self.project]), contextlib.redirect_stdout(io.StringIO()) as output:
            desktop.launch()
        self.assertIn("ja esta aberta", output.getvalue())
        self.assertTrue(self.session.ready())
        self.assertEqual(self.starts(), 1)
        with mock.patch.object(desktop, "desktop_directory", return_value=self.root / "Desktop"), mock.patch.object(desktop.shutil, "which", return_value=None), contextlib.redirect_stdout(io.StringIO()):
            desktop.install()
        entry = (self.root / "Desktop/raspberry-tv-native.desktop").read_text()
        self.assertIn("Terminal=false", entry)
        self.assertIn("Actions=StopSpotify;", entry)
        self.assertIn("--stop-soloist", entry)


if __name__ == "__main__":
    unittest.main()
