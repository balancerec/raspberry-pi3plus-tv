"""Private audio tests use fixtures/null sinks, never a Spotify key or HDMI."""
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import contextlib
import tarfile
import tempfile
import unittest
import uuid
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("private_audio_test", ROOT / "integrations/spotify/audio_backend.py")
audio = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audio)
setup_spec = importlib.util.spec_from_file_location("audio_setup_logic_test", ROOT / "scripts/spotify-audio-setup.py")
setup = importlib.util.module_from_spec(setup_spec); setup_spec.loader.exec_module(setup)


class AudioFixture(unittest.TestCase):
    def setUp(self):
        self.base = Path(tempfile.gettempdir()).resolve()
        self.root = self.base / ("rtv-audio-test-" + uuid.uuid4().hex)
        self.root.mkdir(mode=0o755)
        self.patch = mock.patch.dict(os.environ, XDG_CONFIG_HOME=str(self.root / "config"),
                                     XDG_DATA_HOME=str(self.root / "data"), XDG_RUNTIME_DIR=str(self.root / "run"))
        self.patch.start()
        self.addCleanup(self.cleanup)

    def cleanup(self):
        self.patch.stop()
        if self.root.resolve().parent != self.base or not self.root.name.startswith("rtv-audio-test-"):
            raise AssertionError("Unexpected cleanup target")
        shutil.rmtree(self.root)


class AudioTests(AudioFixture):
    def test_disabled_profile_does_not_spawn_or_change_environment(self):
        before = os.environ.copy()
        with mock.patch.object(audio.subprocess, "Popen") as spawn:
            with audio.PrivatePulse(None) as environment:
                self.assertEqual(environment, {})
        spawn.assert_not_called()
        self.assertEqual(os.environ, before)

    def test_profile_is_separate_from_spotify_credentials_and_reversible(self):
        directory = audio.config_directory()
        directory.mkdir(parents=True)
        key_file = directory / "soloist.private.json"
        key_file.write_text("preserved-artificial-private-fixture")
        audio.set_profile({"bundle": ""})
        self.assertEqual(audio.profile()["owner"], audio.OWNER)
        self.assertEqual((directory / "audio-focus-v1").read_text(), audio.FOCUS_MARKER)
        audio.disable_profile()
        self.assertIsNone(audio.profile())
        self.assertFalse((directory / "audio-focus-v1").exists())
        self.assertEqual(key_file.read_text(), "preserved-artificial-private-fixture")

    def test_foreign_profile_and_runtime_paths_are_preserved(self):
        directory = audio.config_directory()
        directory.mkdir(parents=True)
        path = directory / "audio.private.json"
        path.write_text('{"owner":"another-program","enabled":true}')
        with self.assertRaises(RuntimeError):
            audio.set_profile({"bundle": ""})
        self.assertEqual(json.loads(path.read_text())["owner"], "another-program")
        path.write_text(json.dumps({"owner": audio.OWNER, "enabled": True, "bundle": str(self.root.parent)}))
        with self.assertRaises(RuntimeError):
            audio.profile()

    def test_apt_plan_is_bounded_simulation_output(self):
        text = 'Inst pulseaudio (17.0-1 Debian:13 [arm64])\nInst pulseaudio-utils [16.1] (17.0-1 Debian:13 [arm64])\n'
        self.assertEqual(audio.dependency_plan(text), [("pulseaudio", "17.0-1"), ("pulseaudio-utils", "17.0-1")])
        with self.assertRaises(RuntimeError):
            audio.dependency_plan("no dependency plan")
        with self.assertRaises(RuntimeError):
            audio.dependency_plan(text + '\n'.join('Inst libfixture%d (1 Debian [arm64])' % i for i in range(40)))

    def tar(self, name, content=b"fixture", *, link=None):
        stream = io.BytesIO()
        with tarfile.open(fileobj=stream, mode="w") as archive:
            entry = tarfile.TarInfo(name)
            if link is not None:
                entry.type, entry.linkname = tarfile.SYMTYPE, link
                archive.addfile(entry)
            else:
                entry.size, entry.mode = len(content), 0o755
                archive.addfile(entry, io.BytesIO(content))
        return stream.getvalue()

    def test_deb_extraction_keeps_files_inside_private_tree(self):
        audio.extract_payload(self.tar("usr/bin/pulseaudio"), self.root)
        self.assertEqual((self.root / "usr/bin/pulseaudio").read_bytes(), b"fixture")
        audio.extract_payload(self.tar("etc/pulse/default.pa"), self.root)
        self.assertFalse((self.root / "etc/pulse/default.pa").exists())

    def test_deb_traversal_and_external_links_are_rejected(self):
        for raw in (self.tar("../outside"), self.tar("usr/lib/link.so", link="../../../../outside"),
                    self.tar("usr/lib/link.so", link="/../../outside")):
            with self.assertRaises(RuntimeError):
                audio.extract_payload(raw, self.root)

    def test_existing_runtime_needs_no_package_command(self):
        with mock.patch.object(audio.shutil, "which", return_value="fixture-binary"), mock.patch.object(audio, "command") as command:
            self.assertEqual(audio.prepare_runtime(announce=lambda _: None), {"bundle": ""})
        command.assert_not_called()

    def test_runtime_libraries_apply_only_to_child_environment(self):
        library = self.root / "prefix/usr/lib/fixture/libpulse.so.0"
        library.parent.mkdir(parents=True)
        library.write_bytes(b"fixture")
        before = os.environ.copy()
        result = audio.runtime_environment({"bundle": str(self.root / "prefix")}, {"kept": "yes"})
        self.assertEqual(result["kept"], "yes")
        self.assertIn(str(library.parent), result["LD_LIBRARY_PATH"])
        self.assertEqual(os.environ, before)

    def test_failed_activation_restores_audio_profile_and_keeps_credentials(self):
        directory = audio.config_directory()
        directory.mkdir(parents=True)
        original = json.dumps({"owner": audio.OWNER, "enabled": False}).encode()
        (directory / "audio.private.json").write_bytes(original)
        (directory / "soloist.private.json").write_text("preserved-private-fixture")
        session = mock.Mock(); session.provider_running.return_value = False
        launcher = {"stop_native": mock.Mock(), "SoloistSession": mock.Mock(return_value=session)}
        with mock.patch.object(audio, "PrivatePulse"), mock.patch.object(setup, "tone"), contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaisesRegex(RuntimeError, "fixture-failure"):
                setup.apply(self.root, launcher, audio, {"bundle": ""}, mock.Mock(side_effect=RuntimeError("fixture-failure")))
        self.assertEqual((directory / "audio.private.json").read_bytes(), original)
        self.assertFalse((directory / "audio-focus-v1").exists())
        self.assertEqual((directory / "soloist.private.json").read_text(), "preserved-private-fixture")
        launcher["stop_native"].assert_called_once()

    @unittest.skipUnless(os.name == "posix", "Owned Linux process retirement")
    def test_setup_retires_only_exact_owned_sdk_process(self):
        process = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(20)"])
        try:
            (self.root / "soloist.pid").write_text(str(process.pid))
            session = mock.Mock()
            session.provider_running.side_effect = lambda: process.poll() is None
            client = session.core.return_value.Soloist.return_value
            client.binary.return_value = sys.executable
            client.directories.return_value = (self.root, self.root)
            setup.retire_provider(session)
            self.assertIsNotNone(process.poll())
            session.stop.assert_called_once()
        finally:
            if process.poll() is None:
                process.terminate()
            process.wait(timeout=3)


@unittest.skipUnless(os.name == "posix" and hasattr(os, "geteuid") and os.geteuid() != 0 and
                     all(shutil.which(name) for name in ("pulseaudio", "pactl", "paplay")),
                     "Non-root Linux PulseAudio integration (null sink, no hardware)")
class PulseIntegration(AudioFixture):
    def test_real_private_server_cookie_pcm_and_cleanup(self):
        before = os.environ.copy()
        server = audio.PrivatePulse({"bundle": ""}, null_sink=True, directory=self.root / "pulse")
        with server as environment:
            self.assertEqual(os.environ, before)
            self.assertTrue((self.root / "pulse/native").is_socket())
            sinks = json.loads(audio.command([shutil.which("pactl"), "-f", "json", "list", "sinks"], environment=environment))
            self.assertEqual([sink["name"] for sink in sinks], [audio.SINK])
            self.assertIn("module-null-sink", sinks[0]["driver"])
            tone_spec = importlib.util.spec_from_file_location("audio_tone_test", ROOT / "scripts/spotify-audio-setup.py")
            setup = importlib.util.module_from_spec(tone_spec); tone_spec.loader.exec_module(setup)
            setup.tone(audio, {"bundle": ""}, environment)
            self.assertEqual((self.root / "pulse/cookie").stat().st_size, 256)
            self.assertEqual((self.root / "pulse").stat().st_mode & 0o777, 0o700)
            self.assertEqual((self.root / "pulse/cookie").stat().st_mode & 0o777, 0o600)
            process = server.process
        self.assertIsNotNone(process.poll())
        self.assertIsNone(server.process)

    def test_private_server_rejects_duplicate_without_stopping_first(self):
        with audio.PrivatePulse({"bundle": ""}, null_sink=True, directory=self.root / "pulse") as environment:
            with self.assertRaises(BlockingIOError):
                with audio.PrivatePulse({"bundle": ""}, null_sink=True, directory=self.root / "pulse"):
                    pass
            audio.command([shutil.which("pactl"), "info"], environment=environment)


if __name__ == "__main__":
    unittest.main()
