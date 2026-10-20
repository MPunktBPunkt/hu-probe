from __future__ import annotations

import json
import logging
import socket
import threading
from typing import Callable

from .session_store import SessionStore

log = logging.getLogger("hu-probe.tcp")


class TcpCollector:
    def __init__(
        self,
        store: SessionStore,
        host: str = "0.0.0.0",
        port: int = 9100,
        on_event: Callable[[dict], None] | None = None,
    ) -> None:
        self.store = store
        self.host = host
        self.port = port
        self.on_event = on_event
        self._sock: socket.socket | None = None
        self._thread: threading.Thread | None = None
        self._stop = threading.Event()
        self.connections = 0
        self.events = 0

    def start(self) -> None:
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._sock.bind((self.host, self.port))
        self._sock.listen(8)
        self._sock.settimeout(1.0)
        self._thread = threading.Thread(target=self._accept_loop, name="tcp-collector", daemon=True)
        self._thread.start()
        log.info("listening on %s:%s", self.host, self.port)

    def stop(self) -> None:
        self._stop.set()
        if self._sock:
            try:
                self._sock.close()
            except OSError:
                pass
        if self._thread:
            self._thread.join(timeout=3)

    def _accept_loop(self) -> None:
        assert self._sock is not None
        while not self._stop.is_set():
            try:
                conn, addr = self._sock.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            self.connections += 1
            log.info("client %s", addr)
            threading.Thread(
                target=self._client_loop, args=(conn, addr), daemon=True
            ).start()

    def _client_loop(self, conn: socket.socket, addr: tuple) -> None:
        buf = b""
        conn_sid: str | None = None
        # Bad lines before the first sid on this connection are attributed to that sid.
        pending_errors = 0

        def bad_line() -> None:
            nonlocal pending_errors
            if conn_sid is None:
                pending_errors += 1
            else:
                self.store.note_parse_error(conn_sid)

        with conn:
            conn.settimeout(1.0)
            while not self._stop.is_set():
                try:
                    chunk = conn.recv(65536)
                except socket.timeout:
                    continue
                except OSError:
                    break
                if not chunk:
                    break
                buf += chunk
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        text = line.decode("utf-8")
                    except UnicodeDecodeError as e:
                        log.warning("bad line from %s: %s", addr, e)
                        bad_line()
                        continue
                    # Tolerate glued JSON objects on one TCP line (partial-write retry).
                    decoder = json.JSONDecoder()
                    idx = 0
                    parsed_any = False
                    while idx < len(text):
                        while idx < len(text) and text[idx].isspace():
                            idx += 1
                        if idx >= len(text):
                            break
                        try:
                            event, end = decoder.raw_decode(text, idx)
                        except json.JSONDecodeError as e:
                            if not parsed_any:
                                log.warning("bad line from %s: %s", addr, e)
                                bad_line()
                            break
                        idx = end
                        parsed_any = True
                        if not isinstance(event, dict):
                            continue
                        sid = event.get("sid")
                        if sid:
                            conn_sid = str(sid)
                            if pending_errors:
                                self.store.note_parse_error(conn_sid, pending_errors)
                                pending_errors = 0
                        self.store.append_event(event)
                        self.events += 1
                        if self.on_event:
                            self.on_event(event)
        if pending_errors:
            self.store.note_parse_error(None, pending_errors)
        log.info("client closed %s", addr)
