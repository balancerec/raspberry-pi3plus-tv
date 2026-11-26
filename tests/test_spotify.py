"""Offline Spotify tests. All credentials and responses below are artificial."""
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import stat
import shutil
import sys
import tempfile
import unittest
import uuid
from unittest import mock
from urllib import parse

MODULE = Path(__file__).resolve().parents[1] / "integrations/spotify/spotify_client.py"
spec = importlib.util.spec_from_file_location("raspberry_spotify", MODULE)
spotify = importlib.util.module_from_spec(spec)
spec.loader.exec_module(spotify)

FAKE_CLIENT_ID = "0" * 32
FAKE_TOKEN = "test-access-value-not-a-credential"
FAKE_REFRESH = "test-refresh-value-not-a-credential"
ITEM_ID = "A" * 22
URI = "spotify:playlist:" + ITEM_ID


class Transport:
    def __init__(self, *responses):
        self.responses = list(responses)
        self.calls = []

    def __call__(self, method, url, headers, body, deadline):
        self.calls.append((method, url, headers, body))
        if not self.responses:
            raise AssertionError("Unexpected HTTP request")
        return self.responses.pop(0)


class SpotifyTests(unittest.TestCase):
    def setUp(self):
        base = Path(tempfile.gettempdir()).resolve()
        temporary = base / ("rtv-spotify-test-" + uuid.uuid4().hex)
        temporary.mkdir(mode=0o755)
        def cleanup():
            if temporary.resolve().parent != base or not temporary.name.startswith("rtv-spotify-test-"):
                raise AssertionError("Unexpected cleanup target")
            shutil.rmtree(temporary)
        self.addCleanup(cleanup)
        self.store = spotify.Store(temporary / "spotify")
        self.store.write("config.private.json", {"client_id": FAKE_CLIENT_ID})
        self.store.write("tokens.private.json", {
            "client_id": FAKE_CLIENT_ID, "access_token": FAKE_TOKEN, "refresh_token": FAKE_REFRESH,
            "expires_at": 5000,
        })

    def client(self, *responses):
        transport = Transport(*responses)
        return spotify.Client(self.store, transport, clock=lambda: 1000), transport

    def assertSafe(self, value):
        for secret in (FAKE_TOKEN, FAKE_REFRESH, "Bearer "):
            self.assertNotIn(secret, str(value))

    def test_pkce_rfc7636_vector(self):
        verifier = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"
        self.assertEqual(spotify.pkce_challenge(verifier), "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM")
        query = parse.parse_qs(parse.urlsplit(spotify.authorization_url(FAKE_CLIENT_ID, verifier, "state")).query)
        self.assertEqual(query["code_challenge_method"], ["S256"])
        self.assertEqual(query["redirect_uri"], [spotify.REDIRECT])
        self.assertEqual(query["state"], ["state"])
        self.assertNotIn(verifier, str(query))
        self.assertNotIn("client_secret", query)

    def test_callback_state_path_denial_and_duplicate_parameters(self):
        self.assertEqual(spotify.callback_code("/spotify/callback?state=s&code=c", "s"), "c")
        for target in ("/other?state=s&code=c", "/spotify/callback?state=x&code=c",
                       "/spotify/callback?state=s&state=s&code=c", "/spotify/callback?state=s",
                       "/spotify/callback?state=%C3%A9&code=c", "https://outside.invalid/spotify/callback?state=s&code=c",
                       "/spotify/callback?state=s&code=c#fragment",
                       "/spotify/callback?state=s&code=c&code=d", "/spotify/callback?state=s&error=access_denied"):
            with self.subTest(target=target), self.assertRaises(spotify.SpotifyError):
                spotify.callback_code(target, "s")

    def test_store_atomic_replacement_and_no_temporary_remnants(self):
        self.store.write("tokens.private.json", {"revision": 1})
        self.store.write("tokens.private.json", {"revision": 2})
        self.assertEqual(self.store.read("tokens.private.json"), {"revision": 2})
        self.assertEqual(list(self.store.directory.glob(".spotify-*")), [])

    @unittest.skipUnless(os.name == "posix", "POSIX permission semantics")
    def test_private_modes_and_insecure_file_rejected(self):
        self.assertEqual(stat.S_IMODE(self.store.directory.stat().st_mode), 0o700)
        path = self.store.directory / "tokens.private.json"
        self.assertEqual(stat.S_IMODE(path.stat().st_mode), 0o600)
        path.chmod(0o644)
        with self.assertRaises(spotify.SpotifyError):
            self.store.read("tokens.private.json")

    def test_corrupt_or_oversized_storage_has_safe_error(self):
        path = self.store.directory / "tokens.private.json"
        for value in ("not-json", "[]", '"' + "x" * 66000 + '"'):
            path.write_text(value, encoding="utf-8")
            with self.subTest(size=len(value)), self.assertRaises(spotify.SpotifyError) as raised:
                self.store.read("tokens.private.json")
            self.assertSafe(raised.exception)

    def test_lock_rejects_concurrent_operation_and_releases(self):
        with self.store.locked():
            with self.assertRaises(spotify.SpotifyError) as raised:
                with self.store.locked():
                    self.fail("Concurrent lock acquired")
            self.assertEqual(raised.exception.code, "busy")
        with self.store.locked():
            pass

    def test_missing_configuration_and_token_do_not_make_network_requests(self):
        self.store.logout()
        client, transport = self.client()
        with self.assertRaises(spotify.SpotifyError) as raised:
            client.playlists()
        self.assertEqual(raised.exception.code, "auth")
        self.store.write("config.private.json", {})
        with self.assertRaises(spotify.SpotifyError) as raised:
            client.playlists()
        self.assertEqual(raised.exception.code, "setup")
        self.assertEqual(transport.calls, [])

    def test_changed_client_id_cannot_reuse_other_app_token(self):
        self.store.write("config.private.json", {"client_id": "1" * 32})
        client, transport = self.client()
        with self.assertRaises(spotify.SpotifyError):
            client.profile()
        self.assertEqual(transport.calls, [])

    def test_expired_token_refresh_rotates_and_preserves_refresh_if_omitted(self):
        for refresh in (None, "rotated-test-refresh"):
            saved = self.store.read("tokens.private.json")
            saved["expires_at"] = 900
            self.store.write("tokens.private.json", saved)
            payload = {"access_token": "renewed-test-token", "expires_in": 3600}
            if refresh:
                payload["refresh_token"] = refresh
            client, transport = self.client((200, {}, payload), (200, {}, {"items": []}))
            client.playlists(20)
            self.assertEqual(len(transport.calls), 2)
            fields = parse.parse_qs(transport.calls[0][3].decode("ascii"))
            self.assertEqual(fields["grant_type"], ["refresh_token"])
            self.assertEqual(fields["client_id"], [FAKE_CLIENT_ID])
            self.assertNotIn("client_secret", fields)
            self.assertEqual(self.store.read("tokens.private.json")["refresh_token"], refresh or saved["refresh_token"])
            self.assertEqual(self.store.read("tokens.private.json")["expires_at"], 4600)
            self.assertIn("offset=20", transport.calls[1][1])

    def test_401_refresh_only_once_then_fail(self):
        client, transport = self.client((401, {}, {}), (200, {}, {"access_token": "new-test-token", "expires_in": 3600}), (401, {}, {}))
        with self.assertRaises(spotify.SpotifyError) as raised:
            client.profile()
        self.assertEqual(raised.exception.code, "auth")
        self.assertEqual(len(transport.calls), 3)
        self.assertSafe(raised.exception)

    def test_revoked_refresh_has_no_token_body_in_error(self):
        client, _ = self.client((401, {}, {}), (400, {}, {"error_description": FAKE_REFRESH}))
        with self.assertRaises(spotify.SpotifyError) as raised:
            client.profile()
        self.assertEqual(raised.exception.code, "auth")
        self.assertSafe(raised.exception)

    def test_429_persists_cooldown_and_never_retries_immediately(self):
        client, transport = self.client((429, {"Retry-After": "60"}, {"message": FAKE_TOKEN}))
        for _ in range(2):
            with self.assertRaises(spotify.SpotifyError) as raised:
                client.playlists()
            self.assertEqual(raised.exception.code, "rate_limit")
            self.assertSafe(raised.exception)
        self.assertEqual(len(transport.calls), 1)
        self.assertEqual(self.store.read("cooldown.private.json")["until"], 1060)

    def test_bad_retry_after_uses_bounded_default(self):
        client, _ = self.client((429, {"retry-after": "invalid"}, {}))
        with self.assertRaises(spotify.SpotifyError):
            client.devices()
        self.assertEqual(self.store.read("cooldown.private.json")["until"], 1030)

    def test_403_404_500_errors_are_classified_without_private_bodies(self):
        for code, category in ((403, "access"), (404, "unavailable"), (503, "network")):
            client, transport = self.client((code, {}, {"error": FAKE_TOKEN}))
            with self.subTest(code=code), self.assertRaises(spotify.SpotifyError) as raised:
                client.pause()
            self.assertEqual(raised.exception.code, category)
            self.assertSafe(raised.exception)
            self.assertEqual(len(transport.calls), 1)

    def test_play_context_and_single_track_use_correct_payloads(self):
        client, transport = self.client((204, {}, {}), (204, {}, {}), (204, {}, {}))
        client.play(URI)
        client.play("spotify:track:" + ITEM_ID, "test-device")
        client.play()
        self.assertEqual(json.loads(transport.calls[0][3]), {"context_uri": URI})
        self.assertEqual(json.loads(transport.calls[1][3]), {"uris": ["spotify:track:" + ITEM_ID]})
        self.assertTrue(transport.calls[1][1].endswith("?device_id=test-device"))
        self.assertIsNone(transport.calls[2][3])

    def test_pause_skip_volume_transfer_payloads_and_no_automatic_play(self):
        client, transport = self.client(*[(204, {}, {})] * 5)
        client.pause()
        client.skip("next")
        client.skip("previous")
        client.volume(65, "test-device")
        client.transfer("test-device")
        self.assertEqual([c[0] for c in transport.calls], ["PUT", "POST", "POST", "PUT", "PUT"])
        self.assertIn("volume_percent=65", transport.calls[3][1])
        self.assertEqual(json.loads(transport.calls[4][3]), {"device_ids": ["test-device"], "play": False})

    def test_relative_volume_clamped_and_device_capabilities_checked(self):
        client, transport = self.client((200, {}, {"device": {"id": "test-device", "supports_volume": True, "volume_percent": 98}}), (204, {}, {}))
        client.change_volume(5)
        self.assertIn("volume_percent=100", transport.calls[1][1])
        for device in ({}, {"id": "test-device", "supports_volume": False}, {"id": "test-device", "supports_volume": True, "is_restricted": True, "volume_percent": 40}):
            client, transport = self.client((200, {}, {"device": device}))
            with self.assertRaises(spotify.SpotifyError):
                client.change_volume(-5)
            self.assertEqual(len(transport.calls), 1)

    def test_uri_device_offset_validation_blocks_before_network(self):
        client, transport = self.client()
        for method, args in ((client.play, ("https://example.invalid/private",)), (client.transfer, ("",)),
                             (client.volume, (101,)), (client.volume, (True,)), (client.playlist_items, ("../escape",)),
                             (client.playlists, (-1,)), (client.skip, ("invalid",)), (client.api, ("GET", "//outside"))):
            with self.subTest(method=method.__name__), self.assertRaises(spotify.SpotifyError):
                method(*args)
        self.assertEqual(transport.calls, [])

    def test_current_items_endpoint_and_bounded_search(self):
        client, transport = self.client((200, {}, {}), (200, {}, {}))
        client.playlist_items(ITEM_ID, 40)
        client.search("música brasileira")
        self.assertIn(f"/playlists/{ITEM_ID}/items?limit=20&offset=40", transport.calls[0][1])
        self.assertIn("limit=10", transport.calls[1][1])
        self.assertIn("m%C3%BAsica", transport.calls[1][1])

    def test_metadata_pagination_new_and_legacy_fields_and_null_items(self):
        data = {"total": 61, "next": "https://outside.invalid/never-followed", "items": [
            {"item": {"uri": "spotify:track:" + ITEM_ID, "name": "Canção\nBrasileira", "artists": [{"name": "Artista"}], "type": "track"}},
            {"track": {"uri": "spotify:track:" + ITEM_ID, "name": "Antiga", "is_playable": False}},
            {"item": None}, None]}
        rows = spotify.page_rows("items", data, 20, URI)
        self.assertEqual(rows[0], ("page", "items", "20", "40", "61", URI))
        self.assertEqual(rows[1][3], "CançãoBrasileira")
        self.assertEqual(rows[2][-1], "0")
        self.assertEqual(len(rows), 3)

    def test_device_null_id_restricted_and_empty_list(self):
        client, _ = self.client((200, {}, {"devices": [None, {"id": None}, {"id": "test-device", "name": "Sala", "is_restricted": True}]}))
        rows = spotify.command_records(client, "devices")
        self.assertEqual(rows[-1], ("row", "device", "test-device", "Sala", "", "0"))
        client, _ = self.client((200, {}, {"devices": []}))
        self.assertEqual(len(spotify.command_records(client, "devices")), 2)

    def test_empty_playback_204_and_unknown_volume(self):
        client, _ = self.client((204, {}, {}), (200, {}, {"is_playing": True, "item": {"name": "Faixa"}, "device": {"name": "Sala", "volume_percent": None}}))
        rows = spotify.command_records(client, "status")
        self.assertEqual(rows[-1], ("playback", "idle", "", "", "", "-1"))
        rows = spotify.command_records(client, "status")
        self.assertEqual(rows[-1], ("playback", "playing", "Faixa", "", "Sala", "-1"))

    def test_wire_output_utf8_is_encoded_and_never_contains_tokens(self):
        client, _ = self.client((200, {}, {"display_name": "João\tBrasil"}))
        with contextlib.redirect_stdout(io.StringIO()) as output:
            spotify.emit(spotify.command_records(client, "profile"))
        text = output.getvalue()
        self.assertTrue(text.startswith("RTVSPOTIFY\t1\n"))
        self.assertIn("Jo%C3%A3oBrasil", text)
        self.assertSafe(text)

    def test_logout_removes_tokens_preserving_client_configuration(self):
        client, _ = self.client()
        spotify.command_records(client, "logout")
        self.assertEqual(self.store.read("tokens.private.json"), {})
        self.assertEqual(self.store.read("config.private.json")["client_id"], FAKE_CLIENT_ID)

    def test_https_allowlist_and_no_bearer_redirect(self):
        for url in ("http://api.spotify.com/v1/me", "https://api.spotify.com.evil.invalid/v1/me", "https://user@api.spotify.com/v1/me"):
            with self.assertRaises(spotify.SpotifyError):
                spotify.https_json("GET", url, {}, None, 0)
        self.assertIsNone(spotify.NoRedirect().redirect_request(None, None, 302, "", {}, "https://outside.invalid"))

    def test_http_transport_response_bounds_and_error_redaction(self):
        class Response(io.BytesIO):
            code = 200
            headers = {}
        response = Response(b'{"ok":true}')
        opener = mock.Mock()
        opener.open.return_value = response
        with mock.patch.object(spotify.request, "build_opener", return_value=opener):
            result = spotify.https_json("GET", spotify.API + "/me", {}, None, spotify.time.monotonic() + 10)
        self.assertEqual(result, (200, {}, {"ok": True}))
        for content in (b"[]", b"not-json", b"x" * (spotify.MAX_RESPONSE + 1)):
            opener.open.return_value = Response(content)
            with mock.patch.object(spotify.request, "build_opener", return_value=opener), self.assertRaises(spotify.SpotifyError):
                spotify.https_json("GET", spotify.API + "/me", {}, None, spotify.time.monotonic() + 10)
        response = Response(FAKE_TOKEN.encode())
        response.code = 403
        opener.open.return_value = response
        with mock.patch.object(spotify.request, "build_opener", return_value=opener):
            result = spotify.https_json("GET", spotify.API + "/me", {}, None, spotify.time.monotonic() + 10)
        self.assertEqual(result, (403, {}, {}))
        self.assertSafe(result)

    def test_cli_setup_failure_uses_protocol_and_no_traceback(self):
        self.store.write("config.private.json", {})
        with mock.patch.object(spotify, "Store", return_value=self.store), contextlib.redirect_stdout(io.StringIO()) as output, contextlib.redirect_stderr(io.StringIO()) as errors:
            result = spotify.main(["status"])
        self.assertEqual(result, 1)
        self.assertIn("status\tsetup\t", output.getvalue())
        self.assertEqual(errors.getvalue(), "")
        self.assertSafe(output.getvalue())

    def soloist(self, *responses):
        calls = []
        responses = list(responses)
        def runner(arguments):
            calls.append(arguments)
            if not responses:
                raise AssertionError("Unexpected Soloist invocation")
            return responses.pop(0)
        client = spotify.Soloist(self.store, runner)
        client.binary = lambda: "test-soloist-binary"
        data = self.store.directory / "runtime"
        data.mkdir(exist_ok=True)
        (data / "ws.addr").write_text("127.0.0.1\n", encoding="ascii")
        (data / "ws.port").write_text("9090\n", encoding="ascii")
        client.directories = lambda: (data, data)
        return client, calls, data

    def test_soloist_local_controls_never_receive_api_key(self):
        client, calls, _ = self.soloist(*[(0, b"")] * 6)
        for command in ("pause", "resume", "next", "previous", "activate", "deactivate"):
            client.records("soloist-" + command)
        self.assertEqual([c[-1] for c in calls], ["pause", "play", "next", "prev", "activate", "deactivate"])
        for call in calls:
            self.assertEqual(call[:4], ["test-soloist-binary", "ctl", "--ws", "127.0.0.1:9090"])
            self.assertNotIn("--api-key", call)
            self.assertSafe(call)

    def test_soloist_refuses_external_or_invalid_endpoint(self):
        client, calls, data = self.soloist()
        for address, port in (("0.0.0.0", "9090"), ("192.0.2.1", "9090"), ("127.0.0.1", "65536"), ("127.0.0.1", "0"), ("127.0.0.1", "invalid"), ("127.0.0.1", "1" * 10000)):
            (data / "ws.addr").write_text(address, encoding="ascii")
            (data / "ws.port").write_text(port, encoding="ascii")
            with self.subTest(address=address, port=port[:10]), self.assertRaises(spotify.SpotifyError):
                client.records("soloist-status")
        self.assertEqual(calls, [])

    def test_soloist_no_runtime_files_requests_manual_start(self):
        client, calls, data = self.soloist()
        (data / "ws.port").unlink()
        with self.assertRaises(spotify.SpotifyError) as raised:
            client.records("soloist-status")
        self.assertEqual(raised.exception.code, "setup")
        self.assertEqual(calls, [])

    def test_soloist_playback_current_entity_and_buffering(self):
        entity = {"uri": "spotify:track:" + ITEM_ID, "entity_type": "track", "decorations": {
            "identity": {"name": "Canção"}, "creators": [{"entity": {"decorations": {"identity": {"name": "Artista"}}}}]}}
        client, calls, _ = self.soloist((0, json.dumps({"type": "playback_state", "status": "buffering", "volume": 35, "item": entity}).encode()))
        records = client.records("soloist-status")
        self.assertEqual(records[-1], ("playback", "buffering", "Canção", "Artista", "Raspberry TV / Soloist", "35"))
        self.assertEqual(calls[0][-2:], ["now", "--json"])

    def test_soloist_queue_uses_upcoming_items_only(self):
        data = {"upcoming": [None, {"item": {"uri": "spotify:track:" + ITEM_ID, "decorations": {"identity": {"name": "Faixa"}}}},
                              {"item": {"uri": "spotify:ad:unsupported"}}], "previous": [{"item": {"name": "Ignored"}}]}
        client, calls, _ = self.soloist((0, json.dumps(data).encode()))
        records = client.records("soloist-queue")
        self.assertEqual(records[1], ("page", "queue", "0", "-1", "1", ""))
        self.assertEqual(records[-1][3], "Faixa")
        self.assertEqual(calls[0][-3:], ["queue", "20", "--json"])

    def test_soloist_volume_relative_and_state_checked(self):
        client, calls, _ = self.soloist((0, b'{"volume":2}'), (0, b""))
        client.records("soloist-volume-down")
        self.assertEqual(calls[-1][-2:], ["volume", "0"])
        client, calls, _ = self.soloist((0, b'{}'))
        with self.assertRaises(spotify.SpotifyError):
            client.records("soloist-volume-up")
        self.assertEqual(len(calls), 1)

    def test_soloist_error_bodies_and_json_corruption_are_not_exposed(self):
        for code, text, category in ((2, FAKE_TOKEN.encode(), "network"), (3, FAKE_REFRESH.encode(), "auth"), (0, b"not-json", "response")):
            client, _, _ = self.soloist((code, text))
            with self.assertRaises(spotify.SpotifyError) as raised:
                client.records("soloist-status")
            self.assertEqual(raised.exception.code, category)
            self.assertSafe(raised.exception)

    def test_soloist_play_requires_official_uri(self):
        client, calls, _ = self.soloist((0, b""))
        client.records("soloist-play", URI)
        self.assertEqual(calls[0][-2:], ["play", URI])
        with self.assertRaises(spotify.SpotifyError):
            client.records("soloist-play", "https://outside.invalid/track")
        self.assertEqual(len(calls), 1)

    def test_soloist_configuration_hides_fake_key_and_never_reads_it_as_argument(self):
        synthetic_key = "spak_" + "synthetic_test_value_0000"
        with mock.patch.object(spotify, "Store", return_value=self.store), mock.patch.object(spotify.getpass, "getpass", return_value=synthetic_key), mock.patch("builtins.input", return_value=""), contextlib.redirect_stdout(io.StringIO()) as output:
            result = spotify.main(["soloist-configure"])
        self.assertEqual(result, 0)
        self.assertNotIn(synthetic_key, output.getvalue())
        self.assertEqual(self.store.read("soloist.private.json")["api_key"], synthetic_key)

    def test_soloist_foreground_uses_private_key_loopback_bounded_cache_and_suppressed_logs(self):
        synthetic_key = "spak_" + "synthetic_test_value_0000"
        self.store.write("soloist.private.json", {"api_key": synthetic_key})
        client, _, data = self.soloist()
        process = mock.Mock()
        process.wait.return_value = 10
        with mock.patch.object(spotify.subprocess, "Popen", return_value=process) as spawn, contextlib.redirect_stdout(io.StringIO()) as output:
            with self.assertRaises(spotify.SpotifyError) as raised:
                client.run_foreground()
        self.assertEqual(raised.exception.code, "setup")
        self.assertIn("expirou", str(raised.exception))
        self.assertNotIn(synthetic_key, output.getvalue() + str(raised.exception))
        arguments = spawn.call_args.args[0]
        self.assertEqual(arguments[arguments.index("--ws") + 1], "127.0.0.1:0")
        self.assertEqual(arguments[arguments.index("--cache-size") + 1], "100")
        self.assertEqual(spawn.call_args.kwargs["stderr"], spotify.subprocess.DEVNULL)

    def test_soloist_foreground_interrupt_terminates_owned_process(self):
        self.store.write("soloist.private.json", {"api_key": "spak_" + "synthetic_test_value_0000"})
        client, _, _ = self.soloist()
        process = mock.Mock()
        process.wait.side_effect = [KeyboardInterrupt(), 0]
        process.poll.return_value = None
        with mock.patch.object(spotify.subprocess, "Popen", return_value=process), contextlib.redirect_stdout(io.StringIO()):
            client.run_foreground()
        process.terminate.assert_called_once()

    def test_soloist_refuses_key_entry_when_hidden_input_unavailable(self):
        with mock.patch.object(spotify, "Store", return_value=self.store), mock.patch.object(spotify.getpass, "getpass", side_effect=spotify.getpass.GetPassWarning()), contextlib.redirect_stderr(io.StringIO()) as errors:
            result = spotify.main(["soloist-configure"])
        self.assertEqual(result, 1)
        self.assertEqual(self.store.read("soloist.private.json"), {})
        self.assertIn("entrada oculta", errors.getvalue())


if __name__ == "__main__":
    unittest.main()
