from __future__ import annotations

import argparse
import logging
import signal
import sys
import threading
from pathlib import Path

from .session_store import SessionStore
from .tcp_server import TcpCollector
from .web import start_web


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="hu-probe-collector")
    p.add_argument("--host", default="0.0.0.0")
    p.add_argument("--port", type=int, default=9100)
    p.add_argument("--web-host", default="0.0.0.0")
    p.add_argument("--web-port", type=int, default=8088)
    p.add_argument(
        "--sessions",
        type=Path,
        default=Path(__file__).resolve().parents[2] / "sessions",
    )
    p.add_argument("-v", "--verbose", action="store_true")
    args = p.parse_args(argv)

    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    )

    store = SessionStore(args.sessions.resolve())
    collector = TcpCollector(store, host=args.host, port=args.port)
    collector.start()
    httpd = start_web(store, collector, args.web_host, args.web_port)

    stop = threading.Event()

    def _shutdown(*_args) -> None:
        stop.set()

    signal.signal(signal.SIGINT, _shutdown)
    signal.signal(signal.SIGTERM, _shutdown)

    t = threading.Thread(target=httpd.serve_forever, daemon=True)
    t.start()
    logging.getLogger("hu-probe").info(
        "ready — tcp :%s web :%s sessions %s", args.port, args.web_port, store.root
    )
    stop.wait()
    httpd.shutdown()
    collector.stop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
