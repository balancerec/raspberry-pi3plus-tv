"""Testes sem dependências externas para o servidor TV independente."""
import json
import sys
import threading
import unittest
from http.client import HTTPConnection
from http.server import ThreadingHTTPServer
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from server import TVHandler, system_status


class ServerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.http = ThreadingHTTPServer(("127.0.0.1", 0), TVHandler)
        cls.http.daemon_threads = True
        cls.thread = threading.Thread(target=cls.http.serve_forever, daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.http.shutdown()
        cls.http.server_close()
        cls.thread.join(timeout=2)

    def fetch(self, method, path):
        con = HTTPConnection("127.0.0.1", self.http.server_port, timeout=3)
        try:
            con.request(method, path)
            result = con.getresponse()
            return result.status, dict(result.getheaders()), result.read()
        finally:
            con.close()

    def test_tv_works(self):
        for path in ("/", "/tv", "/tv/"):
            status, head, body = self.fetch("GET", path)
            self.assertEqual(status, 200)
            self.assertIn(b"RASPBERRY", body)
            self.assertIn("text/html", head["Content-Type"])

    def test_static_assets_are_available(self):
        for path in ("/tv.css", "/tv.js", "/info.js", "/info.css", "/info.html"):
            self.assertEqual(self.fetch("GET", path)[0], 200)

    def test_info(self):
        status, _, body = self.fetch("GET", "/info")
        self.assertEqual(status, 200)
        self.assertIn(b"PAINEL", body)

    def test_status_shape(self):
        status, head, body = self.fetch("GET", "/api/status")
        payload = json.loads(body)
        self.assertEqual(status, 200)
        self.assertEqual(head["Cache-Control"], "no-store")
        self.assertIn("hostname", payload["system"])
        self.assertIn("ip_address", payload["system"])
        self.assertIn("temperature_c", payload["system"])
        self.assertTrue(payload["application"]["standalone"])
        self.assertFalse(payload["application"]["hardware_backend"])

    def test_security_headers(self):
        _, headers, _ = self.fetch("GET", "/tv")
        self.assertEqual(headers["X-Frame-Options"], "DENY")
        self.assertEqual(headers["X-Content-Type-Options"], "nosniff")
        self.assertIn("default-src 'self'", headers["Content-Security-Policy"])

    def test_no_api_writes(self):
        self.assertEqual(self.fetch("POST", "/api/status")[0], 405)

    def test_no_admin_backend_routes(self):
        for path in ("/api/diagnostics", "/api/input", "/api/nrf/send", "/api/controller/pair"):
            self.assertEqual(self.fetch("GET", path)[0], 404)

    def test_cannot_browse_file_tree(self):
        for path in ("/server.py", "/README.md", "/..%2fserver.py", "/static/tv.js"):
            self.assertEqual(self.fetch("GET", path)[0], 404)

    def test_system_is_readable(self):
        status = system_status()
        self.assertIsInstance(status["system"]["hostname"], str)
        self.assertIsInstance(status["system"]["ip_address"], str)


if __name__ == "__main__":
    unittest.main()
