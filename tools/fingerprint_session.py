#!/usr/bin/env python3
"""Recompute fingerprint.json for a hu-probe session (reproducible)."""

from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path


def load_jsonl(path: Path) -> list[dict]:
    return [json.loads(l) for l in path.read_text(encoding="utf-8").splitlines() if l.strip()]


def percentile(sorted_vals: list[int], p: float) -> int | None:
    if not sorted_vals:
        return None
    if len(sorted_vals) == 1:
        return sorted_vals[0]
    k = (len(sorted_vals) - 1) * p
    f = int(k)
    c = min(f + 1, len(sorted_vals) - 1)
    if f == c:
        return sorted_vals[f]
    return int(sorted_vals[f] + (sorted_vals[c] - sorted_vals[f]) * (k - f))


def gap_stats(gaps: list[int]) -> dict:
    if not gaps:
        return {}
    s = sorted(gaps)
    return {
        "n": len(s),
        "min": s[0],
        "p50": percentile(s, 0.50),
        "p95": percentile(s, 0.95),
        "max": s[-1],
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("session_dir", type=Path)
    ap.add_argument("-o", "--output", type=Path, default=None)
    ap.add_argument("--include-empty-windows", action="store_true")
    args = ap.parse_args()
    d = args.session_dir
    meta = json.loads((d / "session.json").read_text(encoding="utf-8"))
    events = load_jsonl(d / "events.jsonl")

    reads = [e for e in events if e.get("ev") == "read"]
    usb = [e for e in events if e.get("ev") == "usb"]
    phase = [e for e in events if e.get("ev") == "phase"]
    host_att = [e for e in usb if e.get("usb_ev") == "attach" and e.get("note") == "host"]
    host_det = [e for e in usb if e.get("usb_ev") == "detach" and e.get("note") == "host"]

    n_hist = Counter(str(e.get("n")) for e in reads)
    kind_hist = Counter(e.get("kind") for e in reads)
    tag_hist = Counter(e.get("tag") for e in reads)
    durs = [int(e["dur_us"]) for e in reads if "dur_us" in e]
    gaps_all = [int(e["gap_us"]) for e in reads if e.get("gap_us") is not None]
    gaps_pace = [g for g in gaps_all if g < 10_000_000]
    files = [e for e in reads if e.get("kind") == "file" or e.get("tag") == "DATA"]

    # Pre-0.2.3: read t_us may be drain-time after detach. Attribute orphan reads
    # (between host detach and next attach) to the preceding plug window.
    orphans_by_prev: dict[int, list] = {i: [] for i in range(len(host_att))}
    for r in reads:
        t = r["t_us"]
        inside = False
        for i, att in enumerate(host_att):
            next_det = next((x for x in host_det if x["t_us"] > att["t_us"]), None)
            end = next_det["t_us"] if next_det else 10**18
            if i + 1 < len(host_att):
                end = min(end, host_att[i + 1]["t_us"])
            if att["t_us"] <= t < end:
                inside = True
                break
        if inside:
            continue
        pred = [i for i, a in enumerate(host_att) if a["t_us"] <= t]
        if pred:
            orphans_by_prev[pred[-1]].append(r)

    windows = []
    for i, att in enumerate(host_att):
        next_det = next((x for x in host_det if x["t_us"] > att["t_us"]), None)
        end = next_det["t_us"] if next_det else 10**18
        if i + 1 < len(host_att):
            end = min(end, host_att[i + 1]["t_us"])
        wr = [r for r in reads if att["t_us"] <= r["t_us"] < end]
        orphan = orphans_by_prev.get(i) or []
        combined = wr + orphan
        combined.sort(key=lambda e: e["t_us"])
        if not combined and not args.include_empty_windows:
            continue
        wg = [
            int(r["gap_us"])
            for r in combined
            if r.get("gap_us") is not None and r["gap_us"] < 10_000_000
        ]
        windows.append(
            {
                "window": i,
                "reads": len(combined),
                "bytes": sum(int(r.get("n") or 0) for r in combined),
                "kinds": dict(Counter(r.get("kind") for r in combined)),
                "gap_p50_us": percentile(sorted(wg), 0.50) if wg else None,
                "t_us_first": combined[0]["t_us"] if combined else None,
                "t_us_last": combined[-1]["t_us"] if combined else None,
                "attach_t_us": att["t_us"],
                "detach_t_us": next_det["t_us"] if next_det else None,
                "reads_after_detach_orphan": len(orphan),
            }
        )

    # Host detach→attach gaps (physical)
    detach_attach = []
    last_det = None
    for e in usb:
        if e.get("usb_ev") == "detach" and e.get("note") == "host":
            last_det = e
        elif e.get("usb_ev") == "attach" and e.get("note") == "host" and last_det:
            detach_attach.append(e["t_us"] - last_det["t_us"])
            last_det = None

    mode_n = None
    if n_hist:
        mode_n = int(max(n_hist.items(), key=lambda kv: kv[1])[0])

    fp = {
        "session_id": meta.get("session_id"),
        "vehicle": meta.get("vehicle"),
        "scenario": meta.get("scenario"),
        "profile_id": meta.get("profile_id"),
        "fw_version": meta.get("fw_version"),
        "mac": meta.get("mac"),
        "started_at": meta.get("started_at"),
        "ended_at": meta.get("ended_at"),
        "capture": meta.get("capture"),
        "counts": {
            "events": len(events),
            "reads": len(reads),
            "usb": len(usb),
            "phase": len(phase),
            "markers": sum(1 for e in events if e.get("ev") == "marker"),
            "capture_health": sum(1 for e in events if e.get("ev") == "capture_health"),
            "physical_plug_events": len(host_att),
            "physical_unplug_events": len(host_det),
        },
        "read": {
            "bytes_total": sum(int(e.get("n") or 0) for e in reads),
            "n_hist": {k: n_hist[k] for k in sorted(n_hist, key=lambda x: int(x))},
            "n_mode": mode_n,
            "kind_hist": dict(kind_hist),
            "tag_hist": dict(tag_hist),
            "dur_us": gap_stats(durs),
            "gap_us_all": gap_stats(gaps_all),
            "gap_us_pace_lt_10s": gap_stats(gaps_pace),
            "file": {
                "count": len(files),
                "bytes": sum(int(e.get("n") or 0) for e in files),
                "lba_min": min((e.get("lba") for e in files), default=None),
                "lba_max": max((e.get("lba") for e in files), default=None),
            },
        },
        "host_detach_attach_us": {
            "n": len(detach_attach),
            "min": min(detach_attach) if detach_attach else None,
            "max": max(detach_attach) if detach_attach else None,
            "values": detach_attach,
        },
        "per_plug_window": windows,
        "notes": [
            "Windows indexed by host attach order; empty windows omitted unless --include-empty-windows.",
            "Pre-0.2.3 sessions may stamp read t_us at drain time — prefer gap_us for pacing.",
        ],
        "vs_bmw_nbt_evo_expectations": {
            "read_size_4096_dominant": mode_n == 4096,
            "gap_ms_p50_near_4": (gap_stats(gaps_pace).get("p50") or 0) / 1000.0,
            "eager_large_body_stream": False,
            "epistemic": "FACT from this hu-probe session only; medium geometry limits streaming claims.",
        },
        "tool": "tools/fingerprint_session.py",
    }

    out = args.output or (d / "fingerprint.json")
    out.write_text(json.dumps(fp, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {out}")
    print(
        f"reads={len(reads)} mode={mode_n} gap_p50={gap_stats(gaps_pace).get('p50')} "
        f"windows={len(windows)} host_plugs={len(host_att)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
