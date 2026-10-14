#!/usr/bin/env python3
"""Correlate host plug windows with markers and USB timeline for a session."""

from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path


def load_jsonl(path: Path) -> list[dict]:
    if not path.exists():
        return []
    return [json.loads(l) for l in path.read_text(encoding="utf-8").splitlines() if l.strip()]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("session_dir", type=Path)
    args = ap.parse_args()
    d = args.session_dir
    events = load_jsonl(d / "events.jsonl")
    reads = [e for e in events if e.get("ev") == "read"]
    usb = [e for e in events if e.get("ev") == "usb"]
    markers = [e for e in events if e.get("ev") == "marker"]
    host_att = [e for e in usb if e.get("usb_ev") == "attach" and e.get("note") == "host"]
    host_det = [e for e in usb if e.get("usb_ev") == "detach" and e.get("note") == "host"]

    print(f"session {d.name}")
    print(f"markers ({len(markers)}):")
    for m in markers:
        print(f"  seq={m.get('seq')} t_us={m.get('t_us')} name={m.get('name')} note={m.get('note')}")

    print(f"\nhost plugs ({len(host_att)}):")
    for i, att in enumerate(host_att):
        next_det = next((x for x in host_det if x["t_us"] > att["t_us"]), None)
        end = next_det["t_us"] if next_det else 10**18
        if i + 1 < len(host_att):
            end = min(end, host_att[i + 1]["t_us"])
        # include orphan drain reads after detach (pre-0.2.3)
        wr = [r for r in reads if att["t_us"] <= r["t_us"] < end]
        orphans = []
        if next_det:
            next_att = host_att[i + 1] if i + 1 < len(host_att) else None
            gap_end = next_att["t_us"] if next_att else 10**18
            orphans = [r for r in reads if next_det["t_us"] <= r["t_us"] < gap_end]
        combined = wr + orphans
        kinds = dict(Counter(r.get("kind") for r in combined))
        plug_s = (next_det["t_us"] - att["t_us"]) / 1e6 if next_det else None
        nearby = [
            m
            for m in markers
            if abs(m["t_us"] - att["t_us"]) < 15_000_000
            or (next_det and abs(m["t_us"] - next_det["t_us"]) < 15_000_000)
        ]
        print(
            f"  win {i:2d}: attach={att['t_us']} plugged_s={plug_s} "
            f"reads={len(combined)} (orphan_after_detach={len(orphans)}) kinds={kinds}"
        )
        for m in nearby:
            print(f"       marker@{m['t_us']}: {m.get('name')} / {m.get('note')}")

    # Highlight anomalies
    print("\nanomalies:")
    fp = d / "fingerprint.json"
    if fp.exists():
        data = json.loads(fp.read_text(encoding="utf-8"))
        for w in data.get("per_plug_window") or []:
            n = w.get("reads") or 0
            kinds = w.get("kinds") or {}
            if n == 0:
                print(f"  window {w.get('window')}: empty (no MSC reads emitted)")
            elif n == 14 and "file" not in kinds:
                print(f"  window {w.get('window')}: enum without DATA (14 reads)")
            elif n > 40:
                print(
                    f"  window {w.get('window')}: dense ({n} reads, "
                    f"orphan={w.get('reads_after_detach_orphan')})"
                )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
