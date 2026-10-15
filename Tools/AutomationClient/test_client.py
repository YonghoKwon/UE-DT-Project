import contextlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import io
import json
from pathlib import Path
import tempfile
import threading
import unittest
from unittest.mock import patch

from client import Client, ControlError, check_result, make_job, main


class ClientTests(unittest.TestCase):
    def test_fixed_loopback_and_validation(self):
        self.assertEqual(Client(18099, token="").base, "http://127.0.0.1:18099")
        for port in (0, 65536, "8099", True):
            with self.assertRaises(ValueError): Client(port)
        with self.assertRaises(ValueError): Client(token="bad\r\nheader")
        with self.assertRaises(ValueError): Client().request("http://example.com/")
        with self.assertRaises(ValueError): Client().request("/v1/sensor", {"x": float("nan")})

    def test_http_auth_and_error_redaction(self):
        observed = []
        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                observed.append(self.headers.get("Authorization"))
                self.send_response(200 if self.path == "/v1/status" else 401)
                self.end_headers(); self.wfile.write(b'{"ok":true}')
            def log_message(self, *_): pass
        server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True); thread.start()
        try:
            c = Client(server.server_port, token="test-only-secret")
            self.assertTrue(c.status()["ok"])
            self.assertEqual(observed, ["Bearer test-only-secret"])
            with self.assertRaises(ControlError) as error: c.request("/v1/rejected")
            self.assertNotIn("test-only-secret", str(error.exception))
        finally: server.shutdown(); server.server_close(); thread.join()

    def test_completion_and_supersession(self):
        c = Client(token="")
        with patch.object(c, "status", return_value={"operation_id": "id", "terminal": True, "success": True}):
            self.assertTrue(c.wait("id")["success"])
        with patch.object(c, "status", return_value={"operation_id": "other"}):
            with self.assertRaises(ControlError): c.wait("id")

    def test_job_and_result(self):
        job = make_job("scenario.json", "Slab", "lidar", "camera")
        self.assertEqual(job["steps"][5]["scenes"], 20)
        self.assertEqual(job["steps"][-1]["seconds"], 30)
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "result.json"
            path.write_text(json.dumps({"schema": "ma0t10.job.result.v1", "ok": True, "steps": [{"op": "wait", "ok": True}]}))
            self.assertTrue(check_result(path)["ok"])
            path.write_text('{"schema":"ma0t10.job.result.v1","ok":false}')
            with self.assertRaises(ControlError): check_result(path)

    def test_cli_never_prints_config_secrets(self):
        with tempfile.TemporaryDirectory() as tmp:
            cfg = Path(tmp) / "control.json"; cfg.write_text('{"port":0,"token":"private-test-value"}')
            out, err = io.StringIO(), io.StringIO()
            with patch("sys.argv", ["client.py", "--config", str(cfg), "status"]), contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
                self.assertEqual(main(), 1)
            self.assertNotIn("private-test-value", out.getvalue() + err.getvalue())


if __name__ == "__main__": unittest.main()
