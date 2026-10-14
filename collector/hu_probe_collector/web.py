from __future__ import annotations

import json
import logging
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import TYPE_CHECKING
from urllib.parse import urlparse

if TYPE_CHECKING:
    from .session_store import SessionStore
    from .tcp_server import TcpCollector

log = logging.getLogger("hu-probe.web")

PAGE = """<!DOCTYPE html>
<html><head><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<title>hu-probe collector</title>
<style>
body{font-family:system-ui,sans-serif;margin:1.5rem;background:#0f1419;color:#e7ecf1}
h1{font-size:1.4rem} .muted{color:#8b9aab}
pre{background:#1a222c;padding:.8rem;border-radius:8px;overflow:auto;font-size:.8rem}
a{color:#6cf}
</style></head><body>
<h1>hu-probe collector</h1>
<p class=muted>TCP ingest · Session browser</p>
<pre id=st>loading…</pre>
<script>
async function refresh(){
  const r=await fetch('/api/status');
  st.textContent=JSON.stringify(await r.json(),null,2);
}
setInterval(refresh,1500);refresh();
</script></body></html>
"""


def make_handler(store: "SessionStore", collector: "TcpCollector"):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, fmt: str, *args) -> None:
            log.debug("%s - " + fmt, self.address_string(), *args)

        def _json(self, code: int, payload: object) -> None:
            body = json.dumps(payload, indent=2).encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self) -> None:  # noqa: N802
            path = urlparse(self.path).path
            if path == "/":
                body = PAGE.encode("utf-8")
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
                return
            if path == "/api/status":
                self._json(
                    200,
                    {
                        "tcp_port": collector.port,
                        "connections": collector.connections,
                        "events": collector.events,
                        "sessions_root": str(store.root),
                        "sessions": store.list_sessions()[:50],
                    },
                )
                return
            if path == "/api/sessions":
                self._json(200, store.list_sessions())
                return
            self._json(404, {"error": "not found"})

    return Handler


def start_web(store: "SessionStore", collector: "TcpCollector", host: str, port: int) -> ThreadingHTTPServer:
    handler = make_handler(store, collector)
    httpd = ThreadingHTTPServer((host, port), handler)
    log.info("web on %s:%s", host, port)
    return httpd
