#!/usr/bin/env python3
"""Validate a hu-probe session directory (GATE-01 hygiene checks)."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def load_jsonl(path: Path) -> list[dict]:
    rows = []
    if not path.exists():
        return rows
    for i, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = line.strip()
        if not line:
            continue
        try:
            rows.append(json.loads(line))
        except json.JSONDecodeError as e:
            raise SystemExit(f"{path}:{i}: invalid JSON: {e}") from e
    return rows


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("session_dir", type=Path)
    ap.add_argument("--strict", action="store_true", help="exit 1 on any FAIL")
    args = ap.parse_args()
    d = args.session_dir
    if not d.is_dir():
        print(f"FAIL not a directory: {d}", file=sys.stderr)
        return 2

    session_path = d / "session.json"
    events_path = d / "events.jsonl"
    meta = json.loads(session_path.read_text(encoding="utf-8")) if session_path.exists() else {}
    events = load_jsonl(events_path)

    fails: list[str] = []
    warns: list[str] = []
    oks: list[str] = []

    if not events:
        fails.append("events.jsonl empty or missing")
    else:
        oks.append(f"events={len(events)}")

    sid = meta.get("session_id")
    sids = {e.get("sid") for e in events}
    if sid and sids - {sid, None}:
        fails.append(f"sid mismatch vs session.json: {sorted(sids)}")
    elif len(sids - {None}) == 1:
        oks.append(f"sid consistent ({next(iter(sids - {None}))})")
    elif len(sids - {None}) > 1:
        fails.append(f"multiple sids in events: {sorted(sids)}")

    seqs = [e["seq"] for e in events if "seq" in e]
    if seqs:
        missing = sorted(set(range(seqs[0], seqs[-1] + 1)) - set(seqs))
        if missing:
            fails.append(f"seq gaps: missing {missing[:20]}{'...' if len(missing) > 20 else ''}")
        else:
            oks.append(f"seq contiguous {seqs[0]}..{seqs[-1]}")
        nonmono = sum(1 for i in range(1, len(seqs)) if seqs[i] <= seqs[i - 1])
        if nonmono:
            fails.append(f"seq non-monotonic steps={nonmono}")

    tus = [e["t_us"] for e in events if "t_us" in e]
    if tus:
        back = sum(1 for i in range(1, len(tus)) if tus[i] < tus[i - 1])
        if back:
            # Allowed after 0.2.3 fix for drain vs capture; still flag for audit.
            warns.append(f"t_us regressions={back} (check capture-vs-drain stamping)")
        else:
            oks.append("t_us non-decreasing")

    ev_types = {}
    for e in events:
        k = e.get("ev", "?")
        ev_types[k] = ev_types.get(k, 0) + 1
    oks.append("ev_counts=" + ",".join(f"{k}:{v}" for k, v in sorted(ev_types.items())))

    if not any(e.get("ev") == "session" and e.get("action") == "start" for e in events):
        fails.append("missing session start")
    if not any(e.get("ev") == "session" and e.get("action") == "stop" for e in events):
        warns.append("missing session stop event")

    cap = meta.get("capture") or {}
    if meta:
        oks.append(
            f"capture dropped={cap.get('dropped')} tcp_disconnects={cap.get('tcp_disconnects')} "
            f"complete={cap.get('complete')} highwater={cap.get('queue_highwater')}"
        )
        if cap.get("dropped"):
            warns.append(f"session.json reports dropped={cap.get('dropped')}")
        if meta.get("fw_sha") in (None, "", "null"):
            warns.append("fw_sha is null (GATE-01)")
    else:
        warns.append("session.json missing")

    # Heuristic: large gap_us vs tiny Δt_us on consecutive reads → drain-time stamp
    reads = [e for e in events if e.get("ev") == "read" and "t_us" in e]
    drain_suspect = 0
    for i in range(1, len(reads)):
        dt = reads[i]["t_us"] - reads[i - 1]["t_us"]
        gap = reads[i].get("gap_us") or 0
        if gap > 50_000 and 0 <= dt < 2_000:
            drain_suspect += 1
    if drain_suspect:
        fails.append(
            f"read t_us looks like drain-time ({drain_suspect} pairs with gap_us>50ms but Δt_us<2ms); "
            "need FW ≥0.2.3 capture-time stamp"
        )

    print(f"session: {d}")
    for x in oks:
        print(f"  OK   {x}")
    for x in warns:
        print(f"  WARN {x}")
    for x in fails:
        print(f"  FAIL {x}")

    if fails and args.strict:
        return 1
    return 0 if not fails else 1


if __name__ == "__main__":
    raise SystemExit(main())
