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


def version_at_least(value: object, expected: tuple[int, int, int]) -> bool:
    try:
        parts = tuple(int(x) for x in str(value).split(".")[:3])
        return len(parts) == 3 and parts >= expected
    except ValueError:
        return False


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

    # TCP may deliver packets out of order — evaluate timeline by seq, not file order.
    ordered = sorted((e for e in events if "seq" in e and "t_us" in e), key=lambda e: e["seq"])
    seqs = [e["seq"] for e in ordered]
    if seqs:
        missing = sorted(set(range(seqs[0], seqs[-1] + 1)) - set(seqs))
        if missing:
            fails.append(f"seq gaps: missing {missing[:20]}{'...' if len(missing) > 20 else ''}")
        else:
            oks.append(f"seq contiguous {seqs[0]}..{seqs[-1]}")
        nonmono = sum(1 for i in range(1, len(seqs)) if seqs[i] <= seqs[i - 1])
        if nonmono:
            fails.append(f"seq non-monotonic steps={nonmono}")

    tus = [e["t_us"] for e in ordered]
    if tus:
        back = sum(1 for i in range(1, len(tus)) if tus[i] < tus[i - 1])
        if back:
            # 0.2.12+: Capture-tUs on deferred BOT; regressions are a hard FAIL.
            fails.append(
                f"t_us regressions={back} (need FW ≥0.2.12 capture stamp on bot_data/read)"
            )
        else:
            oks.append("t_us non-decreasing")
        file_back = sum(
            1
            for i in range(1, len(events))
            if "t_us" in events[i]
            and "t_us" in events[i - 1]
            and events[i]["t_us"] < events[i - 1]["t_us"]
        )
        if file_back and not back:
            warns.append(
                f"file-order t_us wobble={file_back} (TCP reorder; seq-ordered timeline OK)"
            )

    ev_types = {}
    for e in events:
        k = e.get("ev", "?")
        ev_types[k] = ev_types.get(k, 0) + 1
    oks.append("ev_counts=" + ",".join(f"{k}:{v}" for k, v in sorted(ev_types.items())))

    # Device-stack evidence must remain explicit; this is not a wire capture claim.
    bot_events = [e for e in events if e.get("ev") in {"bot_cbw", "bot_data", "bot_csw"}]
    bad_evidence = [
        e
        for e in bot_events
        if e.get("bot_source") is None or e.get("synthesized") is None
    ]
    if bad_evidence:
        fails.append(f"BOT device-stack evidence fields missing={len(bad_evidence)}")
    elif bot_events:
        oks.append("BOT evidence source/synthesized explicit")

    starts = [
        e for e in events if e.get("ev") == "session" and e.get("action") == "start"
    ]
    fw_version = starts[0].get("fw_version") if starts else meta.get("fw_version")
    offline = any(e.get("transport") == "ram_offline" for e in starts)
    health = [e for e in events if e.get("ev") == "capture_health"]
    if offline and version_at_least(fw_version, (0, 2, 24)):
        required = {
            "reset_reason",
            "transport_epoch_start",
            "transport_epoch_end",
            "end_reason",
            "end_strategy",
            "upload_status",
        }
        if not health:
            fails.append("offline session missing capture_health")
        else:
            h = health[-1]
            missing_health = sorted(required - set(h))
            if missing_health:
                fails.append(f"offline capture_health missing {missing_health}")
            else:
                oks.append("offline reset/epoch/end/upload health present")
            # Hard integrity gate: quantitative use only when clean.
            overflow = h.get("offline_overflow")
            dropped = h.get("dropped")
            complete = h.get("complete")
            if overflow not in (0, None) and overflow != 0:
                fails.append(f"offline integrity FAIL offline_overflow={overflow}")
            elif dropped not in (0, None) and dropped != 0:
                fails.append(f"offline integrity FAIL dropped={dropped}")
            elif complete is False:
                fails.append("offline integrity FAIL complete=false")
            elif overflow == 0 and dropped == 0 and complete is True:
                oks.append("offline integrity PASS (overflow=0 dropped=0 complete)")
            if version_at_least(fw_version, (0, 2, 25)):
                if h.get("integrity_pass") is True:
                    oks.append("capture_health.integrity_pass=true")
                elif h.get("integrity_pass") is False:
                    fails.append("capture_health.integrity_pass=false")

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
