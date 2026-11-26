"""Transfer observer uses artificial state only; no device or credentials."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import tempfile
import sys
import unittest
import uuid
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("spotify_observer_test", ROOT / "scripts/observe-spotify.py")
observer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(observer)


class ObserverTests(unittest.TestCase):
    def test_cli_falls_back_to_available_binary_without_reading_configuration(self):
        with mock.patch.object(observer.shutil, "which", return_value=sys.executable):
            self.assertEqual(observer.binary_path({"binary": "fixture-absent-path"}), str(Path(sys.executable).resolve()))

    @unittest.skipUnless(os.name == "posix", "Linux /proc PID check")
    def test_process_check_reads_only_owned_pid_and_executable(self):
        base = Path(tempfile.gettempdir()).resolve()
        directory = base / ("rtv-observer-pid-test-" + uuid.uuid4().hex)
        directory.mkdir(mode=0o755)
        try:
            (directory / "soloist.pid").write_text(str(os.getpid()))
            self.assertEqual(observer.process_fields(directory, sys.executable), {"pid": os.getpid(), "processo": "ativo"})
            self.assertEqual(observer.process_fields(directory, "fixture-different-executable")["processo"], "nao_corresponde")
        finally:
            if directory.resolve().parent != base or not directory.name.startswith("rtv-observer-pid-test-"):
                raise AssertionError("Unexpected cleanup target")
            shutil.rmtree(directory)

    def test_only_allowlisted_state_is_emitted(self):
        secret = "artificial-sensitive-value"
        reply = observer.playback_fields({"status": "buffering", "is_active": True, "volume": 35,
                                         "position": {"position_ms": 0}, "item": {"uri": secret},
                                         "context": {"uri": secret}, "access_token": secret, "error": secret})
        self.assertEqual(reply["estado"], "buffering")
        self.assertTrue(reply["tem_faixa"])
        self.assertNotIn(secret, json.dumps(reply))

    def test_malformed_state_and_nonfinite_values_are_unknown(self):
        self.assertEqual(observer.playback_fields(None), {"api": "sem_resposta"})
        reply = observer.playback_fields({"status": "artificial-private-detail", "volume": float("nan"),
                                         "position": {"position_ms": float("inf")}, "is_active": "secret", "item": []})
        self.assertEqual(reply["estado"], "desconhecido")
        self.assertIsNone(reply["volume"])
        self.assertIsNone(reply["posicao_ms"])
        self.assertIsNone(reply["ativo"])

    def test_buffering_to_paused_does_not_claim_success_or_change_playback(self):
        frames = iter([{"estado": "buffering", "pid": 12, "processo": "ativo", "posicao_ms": 0},
                       {"estado": "paused", "pid": 12, "processo": "ativo", "posicao_ms": 0}])
        clock, output = [0.0], []
        with mock.patch.object(observer, "snapshot", side_effect=lambda core: next(frames)):
            observer.observe({}, 4, clock=lambda: clock[0], sleep=lambda delay: clock.__setitem__(0, clock[0] + delay), emit=output.append)
        summary = json.loads(next(line[8:] for line in output if line.startswith("RESUMO: ")))
        self.assertEqual(summary["transicoes"], ["buffering", "paused"])
        self.assertFalse(summary["playing_reportado"])
        self.assertFalse(summary["posicao_avancou"])
        self.assertFalse(summary["troca_de_pid_observada"])

    def test_observed_restart_and_playback_are_reported_separately(self):
        frames = iter([{"estado": "buffering", "pid": 12, "processo": "ativo", "posicao_ms": 0},
                       {"estado": "playing", "pid": 13, "processo": "ativo", "posicao_ms": 1000}])
        clock, output = [0.0], []
        with mock.patch.object(observer, "snapshot", side_effect=lambda core: next(frames)):
            observer.observe({}, 4, clock=lambda: clock[0], sleep=lambda delay: clock.__setitem__(0, clock[0] + delay), emit=output.append)
        summary = json.loads(next(line[8:] for line in output if line.startswith("RESUMO: ")))
        self.assertTrue(summary["troca_de_pid_observada"])
        self.assertTrue(summary["playing_reportado"])
        self.assertTrue(summary["posicao_avancou"])

    def test_duration_cannot_become_an_unbounded_monitor(self):
        for seconds in (0, -1, 31, True):
            with self.subTest(seconds=seconds), self.assertRaises(ValueError):
                observer.observe({}, seconds)

    def test_endpoint_is_local_and_only_observe_command_is_used(self):
        base = Path(tempfile.gettempdir()).resolve()
        directory = base / ("rtv-observer-test-" + uuid.uuid4().hex)
        directory.mkdir(mode=0o755)
        try:
            (directory / "ws.addr").write_text("127.0.0.1")
            (directory / "ws.port").write_text("9090")
            runner = mock.Mock(return_value={"status": "paused"})
            core = {"binary": "fixture-binary", "data": directory, "json_run": runner}
            with mock.patch.object(observer, "process_fields", return_value={"processo": "desconhecido"}):
                reply = observer.snapshot(core)
                self.assertEqual(reply["estado"], "paused")
                runner.assert_called_once_with(["fixture-binary", "ctl", "--ws", "127.0.0.1:9090", "now", "--json"])
                runner.reset_mock()
                (directory / "ws.addr").write_text("192.0.2.1")
                self.assertEqual(observer.snapshot(core)["api"], "endpoint_indisponivel")
                runner.assert_not_called()
        finally:
            if directory.resolve().parent != base or not directory.name.startswith("rtv-observer-test-"):
                raise AssertionError("Unexpected cleanup target")
            shutil.rmtree(directory)


if __name__ == "__main__":
    unittest.main()
