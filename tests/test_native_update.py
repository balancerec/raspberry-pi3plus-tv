"""Offline tests for explicit native updates; no SSH, download or user data."""
import hashlib
import base64
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import shutil
import tarfile
import tempfile
import types
import unittest
import uuid
import sys
import zlib
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


update = module("native_update_test", ROOT / "scripts/desktop-native-update.py")
desktop = module("update_desktop_test", ROOT / "scripts/desktop-launcher.py")


class NativeUpdateTests(unittest.TestCase):
    def setUp(self):
        self.base = Path(tempfile.gettempdir()).resolve()
        self.root = self.base / ("rtv-native-update-test-" + uuid.uuid4().hex)
        self.root.mkdir(mode=0o755)
        self.addCleanup(self.cleanup)
        self.files = {"native/CMakeLists.txt": b"fixture-cmake",
                      "scripts/desktop-launcher.py": b"fixture-launcher",
                      "integrations/spotify/spotify_client.py": b"fixture-helper"}
        manifest = "".join(hashlib.sha256(data).hexdigest() + "  " + name + "\n" for name, data in self.files.items()).encode()
        self.files["SHA256SUMS.txt"] = manifest
        self.spec = {"commit": "0" * 40, "manifest_sha256": hashlib.sha256(manifest).hexdigest()}

    def cleanup(self):
        if self.root.resolve().parent != self.base or not self.root.name.startswith("rtv-native-update-test-"):
            raise AssertionError("Unexpected cleanup target")
        shutil.rmtree(self.root)

    def archive(self, files=None, extra=None):
        buffer = io.BytesIO()
        with tarfile.open(fileobj=buffer, mode="w:gz") as archive:
            for name, data in (files or self.files).items():
                entry = tarfile.TarInfo("fixture-root/" + name)
                entry.size = len(data)
                archive.addfile(entry, io.BytesIO(data))
            if extra is not None:
                archive.addfile(extra)
        return buffer.getvalue()

    def test_exact_source_manifest_and_commit_identity(self):
        self.assertEqual(update.release_spec(self.spec), self.spec)
        self.assertEqual(update.source_files(self.archive(), self.spec), self.files)
        for value in ({}, {**self.spec, "commit": "main"}, {**self.spec, "manifest_sha256": "invalid"}):
            with self.assertRaises(RuntimeError):
                update.release_spec(value)
        with self.assertRaises(RuntimeError):
            update.source_files(self.archive(), {**self.spec, "manifest_sha256": "1" * 64})

    def test_modified_or_extra_files_are_rejected_before_install(self):
        for files in ({**self.files, "native/CMakeLists.txt": b"modified"},
                      {**self.files, "unexpected.txt": b"extra"}):
            with self.assertRaises(RuntimeError):
                update.source_files(self.archive(files), self.spec)

    def test_unsafe_archive_paths_and_links_are_rejected(self):
        for name, kind in (("fixture-root/../../outside", tarfile.REGTYPE),
                           ("/absolute", tarfile.REGTYPE), ("fixture-root/link", tarfile.SYMTYPE),
                           ("fixture-root/hardlink", tarfile.LNKTYPE)):
            entry = tarfile.TarInfo(name)
            entry.type, entry.linkname = kind, "outside"
            with self.subTest(name=name), self.assertRaises(RuntimeError):
                update.source_files(self.archive(extra=entry), self.spec)

    def test_new_release_does_not_replace_unknown_or_modified_projects(self):
        destination = self.root / "release"
        update.install_source(self.files, self.spec, destination)
        update.install_source(self.files, self.spec, destination)
        file = destination / "native/CMakeLists.txt"
        file.write_bytes(b"user-edit")
        with self.assertRaises(RuntimeError):
            update.install_source(self.files, self.spec, destination)
        self.assertEqual(file.read_bytes(), b"user-edit")
        unknown = self.root / "unknown"
        unknown.mkdir()
        (unknown / "keep").write_bytes(b"keep")
        with self.assertRaises(RuntimeError):
            update.install_source(self.files, self.spec, unknown)
        self.assertEqual((unknown / "keep").read_bytes(), b"keep")

    def test_build_failure_cannot_activate_or_stop_existing_app(self):
        run = mock.Mock(side_effect=[types.SimpleNamespace(returncode=0), types.SimpleNamespace(returncode=2)])
        with self.assertRaisesRegex(RuntimeError, "preservada"):
            update.build(self.root, run)
        self.assertEqual(run.call_count, 2)
        self.assertEqual(run.call_args.args[0][-2:], ["--parallel", "1"])
        self.assertNotIn("sudo", str(run.call_args_list))

    def test_failed_launch_restores_original_selection(self):
        directory = self.root / "launcher"
        directory.mkdir()
        pointer = directory / "project.private.json"
        original = b'{"current":"old","previous":"older"}'
        pointer.write_bytes(original)
        old = self.root / "old"
        (old / "build-native").mkdir(parents=True)
        (old / "build-native/raspberry-tv-native").write_bytes(b"old-executable")
        launcher = {"managed_launcher_directory": lambda: directory, "projects": lambda: [old],
                    "stop_native": mock.Mock(), "install": mock.Mock(),
                    "write_owned": lambda path, data: path.write_bytes(data),
                    "launch": mock.Mock(side_effect=RuntimeError("fixture-failure"))}
        with self.assertRaisesRegex(RuntimeError, "fixture-failure"):
            update.activate(self.root / "new", launcher)
        self.assertEqual(pointer.read_bytes(), original)
        self.assertEqual((old / "build-native/raspberry-tv-native").read_bytes(), b"old-executable")
        launcher["stop_native"].assert_called_once()

    def test_preferred_release_and_corrupt_selection_fallback(self):
        directory = self.root / "launcher"
        directory.mkdir()
        project = self.root / "media-release"
        (project / "build-native").mkdir(parents=True)
        (project / "build-native/raspberry-tv-native").write_bytes(b"fixture")
        (project / "managed-release.json").write_text(json.dumps({"owner": update.MARKER}))
        pointer = directory / "project.private.json"
        with mock.patch.object(desktop, "launcher_dir", return_value=directory), mock.patch.object(desktop.Path, "home", return_value=self.root):
            pointer.write_text(json.dumps({"current": str(project)}))
            self.assertEqual(desktop.projects()[0], project)
            pointer.write_text("invalid")
            self.assertEqual(desktop.projects()[0], self.root / "raspberry-tv-spotify")
            pointer.write_text(json.dumps({"current": str(self.root.parent)}))
            self.assertEqual(desktop.preferred_projects(), [])

    def test_rollback_switches_selection_without_deleting_releases(self):
        directory = self.root / "launcher"
        directory.mkdir()
        pointer = directory / "project.private.json"
        pointer.write_text(json.dumps({"current": "new", "previous": "old"}))
        with mock.patch.object(desktop, "managed_launcher_directory", return_value=directory), \
                mock.patch.object(desktop, "preferred_projects", return_value=[Path("old")]), \
                mock.patch.object(desktop, "write_owned", side_effect=lambda path, data: path.write_bytes(data)), \
                mock.patch.object(desktop, "stop_native") as stop, mock.patch.object(desktop, "launch") as launch:
            desktop.rollback_native()
        self.assertEqual(json.loads(pointer.read_text()), {"current": "old", "previous": "new"})
        stop.assert_called_once()
        launch.assert_called_once()

    def test_windows_update_payload_is_pinned_and_bounded_without_ssh(self):
        bridge = module("native_update_windows_test", ROOT / "scripts/desktop-windows.py")
        for name in ("desktop-launcher.py", "desktop-native-update.py"):
            (self.root / name).write_bytes((ROOT / "scripts" / name).read_bytes())
        (self.root / "release.json").write_text(json.dumps(self.spec))
        with mock.patch.object(bridge, "__file__", str(self.root / "desktop-windows.py")), \
                mock.patch.object(sys, "argv", ["desktop-windows.py", "--update-native"]), \
                mock.patch.object(bridge.shutil, "which", return_value="ssh"), \
                mock.patch.object(bridge.subprocess, "CREATE_NEW_CONSOLE", 0, create=True), \
                mock.patch.object(bridge.subprocess, "run", return_value=types.SimpleNamespace(returncode=0)) as run:
            self.assertEqual(bridge.main(), 0)
        command = run.call_args.args[0][-1]
        encoded = command.split("b64decode('", 1)[1].split("')", 1)[0]
        bundle = json.loads(zlib.decompress(base64.b64decode(encoded)))
        self.assertEqual(bundle["release"], self.spec)
        self.assertEqual(bundle["launcher"].encode(), (ROOT / "scripts/desktop-launcher.py").read_bytes())
        self.assertLess(len(command), 30000)


if __name__ == "__main__":
    unittest.main()
