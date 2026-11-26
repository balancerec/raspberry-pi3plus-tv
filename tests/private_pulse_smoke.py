"""CI-only portable Debian bundle check; no installed packages are changed."""
import importlib.util
import os
from pathlib import Path
import shutil
import tempfile
import uuid

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("portable_audio_ci", ROOT / "integrations/spotify/audio_backend.py")
audio = importlib.util.module_from_spec(spec); spec.loader.exec_module(audio)
base = Path(tempfile.gettempdir()).resolve()
temporary = base / ("rtv-private-bundle-ci-" + uuid.uuid4().hex)
temporary.mkdir(mode=0o755)
if os.name != "posix" or os.geteuid() == 0:
    raise RuntimeError("Portable runtime validation requires non-root Linux")
try:
    os.environ["XDG_DATA_HOME"] = str(temporary / "data")
    os.environ["XDG_CONFIG_HOME"] = str(temporary / "config")
    os.environ["XDG_RUNTIME_DIR"] = str(temporary / "run")
    value = audio.prepare_runtime(force_portable=True)
    with audio.PrivatePulse(value, null_sink=True, directory=temporary / "pulse") as environment:
        audio.command([audio.tools(value)["pactl"], "info"], environment=environment)
        setup_spec = importlib.util.spec_from_file_location("portable_tone_ci", ROOT / "scripts/spotify-audio-setup.py")
        setup = importlib.util.module_from_spec(setup_spec); setup_spec.loader.exec_module(setup)
        setup.tone(audio, value, environment)
    print("PORTABLE_PULSE_PASS: Debian packages downloaded/hashed/extracted per-user; private null-sink PCM test passed.")
finally:
    if temporary.resolve().parent != base or not temporary.name.startswith("rtv-private-bundle-ci-"):
        raise RuntimeError("Unexpected cleanup path")
    shutil.rmtree(temporary)
