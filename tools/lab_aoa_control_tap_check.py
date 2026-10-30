#!/usr/bin/env python3
"""Compat wrapper → lab_aoa_iface.py tap-check (C1b-P0.5-A).

Prefer: ``.venv-lab/bin/python tools/lab_aoa_iface.py doctor|arm|host-handshake``
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from lab_aoa_iface import main

if __name__ == "__main__":
    # Preserve --ip; map to tap-check
    argv = sys.argv[1:]
    ip = "192.168.178.88"
    skip = False
    out = []
    i = 0
    while i < len(argv):
        if argv[i] == "--ip" and i + 1 < len(argv):
            ip = argv[i + 1]
            i += 2
            continue
        if argv[i] == "--skip-usb":
            skip = True
            i += 1
            continue
        out.append(argv[i])
        i += 1
    if skip:
        from lab_aoa_iface import probe_status, print_status

        print_status(probe_status(ip))
        raise SystemExit(0)
    raise SystemExit(main(["--ip", ip, "tap-check", *out]))
