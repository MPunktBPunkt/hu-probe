#!/usr/bin/env python3
"""Rebuild session-summaries.json from local Collector sessions/.

Usage (repo root):
  python3 audits/hu-probe-bmw-s2-abend-offline-2026-10-18/summarize_sessions.py
"""
from __future__ import annotations

import json
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent / "session-summaries.json"
SESSIONS = [
    "20261018-175203_bmw_s2_prefetch",
    "20261018-212136_bmw_s2_prefetch",
    "20261018-212635_bmw_s2_prefetch",
]


def load_events(sid: str) -> list[dict]:
    path = ROOT / "sessions" / sid / "events.jsonl"
    if not path.exists():
        raise FileNotFoundError(path)
    out = []
    with path.open(encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line:
                out.append(json.loads(line))
    return out


def summarize(sid: str) -> dict:
    evs = load_events(sid)
    session_meta = next((e for e in evs if e.get("ev") == "session"), {})
    reads = [e for e in evs if e.get("ev") == "read"]
    cbws = [e for e in evs if e.get("ev") == "bot_cbw"]
    read10 = [e for e in cbws if (e.get("scsi") or {}).get("opcode") == "read10"]

    kind_counts = Counter(e.get("kind") for e in reads)
    start_list = [e.get("lba") for e in reads]
    n_list = [e.get("n") for e in reads]
    bytes_total = sum(n for n in n_list if isinstance(n, int))

    covered_lo = covered_hi = None
    ends = []
    for e in reads:
        lba, n = e.get("lba"), e.get("n")
        if isinstance(lba, int) and isinstance(n, int):
            blocks = (n + 511) // 512
            ends.append((lba, lba + blocks - 1))
    if ends:
        covered_lo = min(x[0] for x in ends)
        covered_hi = max(x[1] for x in ends)

    cbw_pairs = []
    for e in read10:
        scsi = e.get("scsi") or {}
        cbw_pairs.append(
            {
                "lba": scsi.get("lba"),
                "blocks": scsi.get("blocks"),
                "bot_tag": e.get("bot_tag"),
                "seq": e.get("seq"),
            }
        )

    atoms = []
    i = 0
    while i < len(cbw_pairs):
        a = cbw_pairs[i]
        if i + 1 < len(cbw_pairs):
            b = cbw_pairs[i + 1]
            if (
                a["blocks"] == 240
                and b["blocks"] == 69
                and b["lba"] == (a["lba"] or 0) + 240
            ):
                atoms.append(
                    {"form": "head_then_tail", "cbws": [a, b], "sum_lba": 309}
                )
                i += 2
                continue
            if (
                a["blocks"] == 69
                and b["blocks"] == 240
                and a["lba"] == (b["lba"] or 0) + 240
            ):
                atoms.append(
                    {"form": "tail_then_head", "cbws": [a, b], "sum_lba": 309}
                )
                i += 2
                continue
        atoms.append({"form": "other", "cbws": [a], "sum_lba": a.get("blocks")})
        i += 1

    start_deltas = []
    for a, b in zip(start_list, start_list[1:]):
        if isinstance(a, int) and isinstance(b, int):
            start_deltas.append(b - a)

    mono = all(
        isinstance(a, int) and isinstance(b, int) and b >= a
        for a, b in zip(start_list, start_list[1:])
    )

    session_json: dict = {}
    sj = ROOT / "sessions" / sid / "session.json"
    if sj.exists():
        session_json = json.loads(sj.read_text(encoding="utf-8"))

    short = sid.split("_", 1)[0].split("-")[-1]
    return {
        "session_id": sid,
        "short_id": short,
        "source": f"sessions/{sid}/events.jsonl (local; not tracked)",
        "fw_version": session_meta.get("fw_version") or session_json.get("fw_version"),
        "transport": session_meta.get("transport") or session_json.get("transport"),
        "vehicle": session_meta.get("vehicle"),
        "scenario": session_meta.get("scenario"),
        "n_events": len(evs),
        "n_reads": len(reads),
        "n_bot_cbw": len(cbws),
        "n_read10_cbw": len(read10),
        "kind_counts": dict(kind_counts),
        "read_bytes_sum": bytes_total,
        "read_n_hist": dict(Counter(n_list)),
        "read_lba_starts": start_list,
        "read_lba_start_deltas": start_deltas,
        "read_starts_monotonic": mono,
        "covered_lba": {
            "lo": covered_lo,
            "hi": covered_hi,
            "span": (covered_hi - covered_lo + 1) if covered_lo is not None else None,
        },
        "read10_cbws": cbw_pairs,
        "track_atoms": atoms,
        "capture_health_count": sum(1 for e in evs if e.get("ev") == "capture_health"),
        "session_gates": session_json.get("gates"),
    }


def main() -> int:
    missing = [
        s for s in SESSIONS if not (ROOT / "sessions" / s / "events.jsonl").exists()
    ]
    if missing:
        print("missing local events.jsonl for:", *missing, file=sys.stderr)
        print("committed session-summaries.json remains the replication source.", file=sys.stderr)
        return 1

    out = {
        "audit_id": "hu-probe-bmw-s2-abend-offline-2026-10-18",
        "purpose": (
            "Replicable meta-summaries for offline PASS sessions "
            "without committing full events.jsonl"
        ),
        "generated_from": "local Collector sessions under sessions/",
        "correction": {
            "track_atom": "(head,240)+(head+240,69)=309 LBA=158208 B",
            "rejected": [
                "312 LBA",
                "3 meta LBA",
                "39×4096 as byte formula",
                "T3 covered 627…938",
            ],
            "t3_covered": "627…935",
        },
        "sessions": [summarize(s) for s in SESSIONS],
    }
    OUT.write_text(json.dumps(out, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {OUT.relative_to(ROOT)}")
    for s in out["sessions"]:
        print(
            f"  {s['short_id']}: kinds={s['kind_counts']} "
            f"bytes={s['read_bytes_sum']} covered={s['covered_lba']['lo']}…{s['covered_lba']['hi']} "
            f"atoms={[a['form'] for a in s['track_atoms']]}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
