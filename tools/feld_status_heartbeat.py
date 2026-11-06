#!/usr/bin/env python3
"""Poll hu-probe /api/status for the full field window (I-303A ops).

Lab OTG can freeze LocalWeb under load; on BMW there is no .88 fallback.
This script keeps a JSONL trail so a dead UI mid-run does not erase facts.

  python3 tools/feld_status_heartbeat.py --dut 192.168.x.y --out docs/feld/artifacts/i303a-hb.jsonl
  python3 tools/feld_status_heartbeat.py --dut 192.168.x.y --interval 5 --duration 600
"""
from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path


def fetch_status(url: str, timeout: float) -> tuple[int, dict | str]:
    req = urllib.request.Request(url, method="GET")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            raw = r.read().decode(errors="replace")
            try:
                return r.status, json.loads(raw)
            except json.JSONDecodeError:
                return r.status, raw[:500]
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode(errors="replace")[:500]
    except Exception as e:
        return 0, str(e)


def slim(st: dict) -> dict:
    ca = st.get("connected_app") or {}
    sm = ca.get("session_metrics") or {}
    tr = ca.get("transport") or {}
    return {
        "fw": st.get("fw"),
        "usb_gadget": st.get("usb_gadget"),
        "ip": st.get("ip"),
        "tcp_ok": st.get("tcp_ok"),
        "wifi_rssi": (st.get("wifi") or {}).get("rssi"),
        "msc_soft_attached": st.get("msc_soft_attached"),
        "ca_state": ca.get("state"),
        "ca_running": ca.get("running"),
        "gate": ca.get("gate"),
        "visibility_scope": sm.get("visibility_scope"),
        "identity_vid": sm.get("identity_vid"),
        "identity_pid": sm.get("identity_pid"),
        "aoa_51": sm.get("aoa_51"),
        "aoa_52": sm.get("aoa_52"),
        "aoa_53": sm.get("aoa_53"),
        "aoa51_respond_ok": sm.get("aoa51_respond_ok"),
        "vendor_setup_count": sm.get("vendor_setup_count"),
        "bulk_rx_count": sm.get("bulk_rx_count"),
        "timeline_size": sm.get("timeline_size"),
        "attach_count": sm.get("attach_count"),
        "detach_count": sm.get("detach_count"),
        "c1b_stage": tr.get("c1b_stage"),
        "usb_stack_bound": tr.get("usb_stack_bound"),
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dut", required=True, help="Probe IP or host")
    ap.add_argument("--interval", type=float, default=5.0, help="Seconds between polls")
    ap.add_argument("--duration", type=float, default=0.0, help="Stop after N seconds (0=until Ctrl-C)")
    ap.add_argument("--timeout", type=float, default=8.0, help="HTTP timeout per poll")
    ap.add_argument("--out", type=Path, help="JSONL output path")
    args = ap.parse_args()

    url = f"http://{args.dut}/api/status"
    out = args.out
    if out:
        out.parent.mkdir(parents=True, exist_ok=True)

    t0 = time.time()
    n_ok = n_fail = 0
    print(f"# heartbeat dut={args.dut} interval={args.interval}s out={out or '-'}", flush=True)

    while True:
        code, body = fetch_status(url, args.timeout)
        now = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        rec: dict = {"t": now, "elapsed_s": round(time.time() - t0, 1), "http": code}
        if code == 200 and isinstance(body, dict):
            n_ok += 1
            rec["ok"] = True
            rec["snap"] = slim(body)
            s = rec["snap"]
            print(
                f"{now} ok gadget={s.get('usb_gadget')} aoa51={s.get('aoa_51')} "
                f"aoa52={s.get('aoa_52')} aoa53={s.get('aoa_53')} "
                f"vend={s.get('vendor_setup_count')} tl={s.get('timeline_size')} "
                f"attach={s.get('attach_count')}",
                flush=True,
            )
        else:
            n_fail += 1
            rec["ok"] = False
            rec["err"] = body if isinstance(body, str) else json.dumps(body)[:300]
            print(f"{now} FAIL http={code} {rec['err'][:120]}", flush=True)

        if out:
            with out.open("a", encoding="utf-8") as f:
                f.write(json.dumps(rec, ensure_ascii=False) + "\n")

        if args.duration > 0 and (time.time() - t0) >= args.duration:
            break
        time.sleep(args.interval)

    print(f"# done ok={n_ok} fail={n_fail}", flush=True)
    return 0 if n_fail == 0 or n_ok > 0 else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n# interrupted", flush=True)
        sys.exit(130)
