#!/usr/bin/env python3
"""Export the central source bundle from a clean Git commit, without user data."""
import argparse
import gzip
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import tarfile
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent

    def git(*arguments):
        return subprocess.check_output(["git", *arguments], cwd=root)

    if git("status", "--porcelain").strip():
        parser.error("Commit all source changes before packaging; no working files are exported.")
    revision = git("rev-parse", "HEAD").decode().strip()
    prefix = "raspberry-tv-central/"
    raw = git("archive", "--format=tar", "--prefix=" + prefix, "HEAD")
    with tarfile.open(fileobj=io.BytesIO(raw)) as archive:
        files = {}
        for member in archive.getmembers():
            relative = PurePosixPath(member.name).relative_to(prefix.rstrip("/"))
            if relative.is_absolute() or ".." in relative.parts or member.issym() or member.islnk():
                raise RuntimeError("Unexpected archive member: " + member.name)
            if member.isfile():
                data = archive.extractfile(member).read()
                name = str(relative)
                if relative.name.startswith(".env") or name.endswith((".private.json", ".secret")):
                    raise RuntimeError("Private configuration refused: " + name)
                if name.endswith((".m3u", ".m3u8")) and not name.startswith("tests/fixtures/"):
                    raise RuntimeError("Non-fixture playlist refused: " + name)
                if re.search(rb"spak_[A-Za-z0-9_-]{15,}", data):
                    raise RuntimeError("Credential pattern refused: " + name)
                files[name] = data
    required = ("ABRIR_NO_WINDOWS.cmd", "CORRIGIR_AUDIO_SPOTIFY_NO_WINDOWS.cmd",
                "integrations/spotify/spotify_client.py", "integrations/spotify/audio_backend.py",
                "scripts/spotify.sh", "scripts/desktop-launcher.py", "scripts/desktop-windows.py",
                "scripts/install-desktop-windows.cmd", "scripts/desktop-native-update.py",
                "scripts/spotify-audio-setup.py", "native/raspberry-tv.ico", "docs/SPOTIFY.md")
    for name in required:
        if name not in files:
            raise RuntimeError("Incomplete central bundle: " + name)
    checksums = files["SHA256SUMS.txt"].decode("utf-8").splitlines()
    for row in checksums:
        digest, name = row.split("  ", 1)
        if hashlib.sha256(files[name]).hexdigest() != digest:
            raise RuntimeError("Source checksum mismatch: " + name)
    release = {"commit": revision, "manifest_sha256": hashlib.sha256(files["SHA256SUMS.txt"]).hexdigest()}
    files["scripts/release.json"] = (json.dumps(release, indent=2) + "\n").encode("utf-8")
    # Derived metadata pins the public source used by the explicit one-click
    # update. It is generated after commit creation to avoid a circular hash.
    expanded = io.BytesIO(raw)
    with tarfile.open(fileobj=expanded, mode="a") as archive:
        entry = tarfile.TarInfo(prefix + "scripts/release.json")
        entry.size = len(files["scripts/release.json"])
        entry.mode = 0o644
        archive.addfile(entry, io.BytesIO(files["scripts/release.json"]))
    raw = expanded.getvalue()
    output = (args.output_dir or root / "dist").resolve()
    output.mkdir(parents=True, exist_ok=True)
    stem = "raspberry-tv-central-1.0.0-rc1-" + revision[:12] + "-source"
    tar_path, zip_path = output / (stem + ".tar.gz"), output / (stem + ".zip")
    metadata_path = output / (stem + ".json")
    expected = (tar_path, zip_path, metadata_path,
                tar_path.with_name(tar_path.name + ".sha256"), zip_path.with_name(zip_path.name + ".sha256"))
    if any(path.exists() for path in expected):
        parser.error("Output already exists; previous deliveries are preserved.")
    with tar_path.open("xb") as stream:
        with gzip.GzipFile(filename="", mode="wb", fileobj=stream, mtime=0) as compressed:
            compressed.write(raw)
    with zipfile.ZipFile(zip_path, "x", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in files.items():
            archive.writestr(prefix + name, data)
    with zipfile.ZipFile(zip_path) as archive:
        for name, data in files.items():
            if archive.read(prefix + name) != data:
                raise RuntimeError("ZIP verification failed: " + name)
    with tarfile.open(tar_path, "r:gz") as archive:
        for name, data in files.items():
            if archive.extractfile(prefix + name).read() != data:
                raise RuntimeError("TAR verification failed: " + name)
    for path in (tar_path, zip_path):
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        path.with_name(path.name + ".sha256").write_text(digest + "  " + path.name + "\n", encoding="ascii")
    metadata = {"commit": revision, "status": "release-candidate", "source_files": len(files) - 1,
                "generated_release_metadata": True, "includes_explicit_native_updater": True,
                "verified_source_checksums": len(checksums), "includes_spotify": True,
                "includes_desktop_launchers": True, "includes_native_binary": False,
                "includes_official_soloist_binary": False, "includes_credentials": False,
                "spotify_physical_audio_validated": False}
    metadata_path.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    print("Central bundle verified:", len(files) - 1, "Git files + release metadata;", len(checksums), "source checksums.")
    print("Commit:", revision)
    print("ZIP:", zip_path)
    print("TAR:", tar_path)
    print("Metadata:", metadata_path)


if __name__ == "__main__":
    main()
