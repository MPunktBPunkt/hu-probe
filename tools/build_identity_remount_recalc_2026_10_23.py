#!/usr/bin/env python3
"""
Build the self-recalculable hu-probe dataset for the BMW NBT Evo
identity/remount field test of 2026-10-23.

Default: copy from local repo artifacts (no network).
Optional: --from-github downloads the same paths from commit REF.

Source commit: 62cb4f88428d2dc8da235e2adf88b0d07e7647f8
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import urllib.request
import zipfile
from pathlib import Path

REPO = "MPunktBPunkt/hu-probe"
REF = "62cb4f88428d2dc8da235e2adf88b0d07e7647f8"
ARTIFACT_REL = "docs/feld/artifacts/20261023-identity-remount"
RAW_BASE = f"https://raw.githubusercontent.com/{REPO}/{REF}/{ARTIFACT_REL}"

SESSIONS = [
    "20261023-074319_bmw_free",  # ident_serial (RST-confounded)
    "20261023-074455_bmw_free",  # ident_label — core
    "20261023-074749_bmw_free",  # ident_b — core
    "20261023-074908_bmw_free",  # ident_vol — core
    "20261023-072929_bmw_seq_ident_1r1",  # offline I0 — contextual
]

SESSION_FILES = [
    "session.json",
    "events.jsonl",
    "derived/analysis.json",
    "derived/analysis.md",
]

REPORTS = [
    "docs/feld/2026-10-23-gesamtbericht-identity-remount-multireview.md",
    "docs/feld/2026-10-23-gesamtbericht-identity-remount.md",
    "docs/feld/2026-10-22-feldplan-cache-key-identity.md",
]


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def get_url(url: str) -> bytes:
    req = urllib.request.Request(url, headers={"User-Agent": "hu-probe-self-recalc/1.1"})
    with urllib.request.urlopen(req, timeout=60) as r:
        return r.read()


def read_jsonl(data: bytes):
    return [json.loads(line) for line in data.decode("utf-8").splitlines() if line.strip()]


def dump_json(path: Path, obj) -> None:
    path.write_text(json.dumps(obj, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def dump_jsonl(path: Path, rows) -> None:
    path.write_text(
        "".join(json.dumps(x, ensure_ascii=False) + "\n" for x in rows),
        encoding="utf-8",
    )


def event_time_us(e: dict) -> int | None:
    """Prefer capture_t_us for offline/post_finalize; else t_us."""
    if e.get("capture_t_us") is not None:
        return e["capture_t_us"]
    return e.get("t_us")


def media_from_session(session: dict, events: list) -> dict:
    media = session.get("media") or {}
    if media.get("sector_count") is not None or media.get("variant"):
        return media
    for e in events:
        if e.get("ev") == "session" and isinstance(e.get("media"), dict):
            return e["media"]
    return {}


def load_session_bytes(sid: str, rel: str, *, local_root: Path | None, from_github: bool) -> bytes:
    if from_github:
        return get_url(f"{RAW_BASE}/{sid}/{rel}")
    assert local_root is not None
    path = local_root / sid / rel
    if not path.is_file():
        raise FileNotFoundError(f"missing local artifact: {path}")
    return path.read_bytes()


def copy_reports(out_reports: Path, root: Path) -> list[dict]:
    out_reports.mkdir(parents=True, exist_ok=True)
    meta = []
    for rel in REPORTS:
        src = root / rel
        if not src.is_file():
            continue
        data = src.read_bytes()
        dst = out_reports / src.name
        dst.write_bytes(data)
        meta.append({"path": rel, "bytes": len(data), "sha256": sha256(data)})
    return meta


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--from-github",
        action="store_true",
        help="Download session files from GitHub raw (default: local artifacts)",
    )
    ap.add_argument(
        "--out-dir",
        type=Path,
        default=None,
        help="Output directory (default: docs/feld/artifacts/20261023-identity-remount-recalc)",
    )
    ap.add_argument(
        "--zip",
        action="store_true",
        default=True,
        help="Also write a zip next to the output directory (default: on)",
    )
    ap.add_argument("--no-zip", action="store_true", help="Skip zip creation")
    args = ap.parse_args()
    make_zip = args.zip and not args.no_zip

    root = repo_root()
    local_art = root / ARTIFACT_REL
    out = args.out_dir or (root / "docs/feld/artifacts/20261023-identity-remount-recalc")
    if out.exists():
        shutil.rmtree(out)
    raw = out / "sessions"
    raw.mkdir(parents=True)

    if not args.from_github and not local_art.is_dir():
        raise SystemExit(f"Local artifacts missing: {local_art} (use --from-github)")

    all_events = []
    read10 = []
    media_layout = []
    lifecycle = []
    summaries = []
    integrity = []
    source_manifest = []

    for sid in SESSIONS:
        dst = raw / sid
        (dst / "derived").mkdir(parents=True)
        downloaded = {}
        for rel in SESSION_FILES:
            data = load_session_bytes(sid, rel, local_root=local_art, from_github=args.from_github)
            out_path = dst / rel
            out_path.parent.mkdir(parents=True, exist_ok=True)
            out_path.write_bytes(data)
            downloaded[rel] = {
                "source": (
                    f"{RAW_BASE}/{sid}/{rel}"
                    if args.from_github
                    else str((local_art / sid / rel).relative_to(root))
                ),
                "bytes": len(data),
                "sha256": sha256(data),
            }

        source_manifest.append(
            {
                "session": sid,
                "role": {
                    "20261023-074319_bmw_free": "ident_serial_RST_confounded",
                    "20261023-074455_bmw_free": "core_ident_label",
                    "20261023-074749_bmw_free": "core_ident_b",
                    "20261023-074908_bmw_free": "core_ident_vol",
                    "20261023-072929_bmw_seq_ident_1r1": "offline_I0_contextual",
                }.get(sid, "unknown"),
                "files": downloaded,
            }
        )

        session = json.loads((dst / "session.json").read_text(encoding="utf-8"))
        events = read_jsonl((dst / "events.jsonl").read_bytes())
        media = media_from_session(session, events)
        sector_count = media.get("sector_count") or (session.get("media") or {}).get("sector_count")

        read10_count = tur_count = bot_count = read_count = 0
        suspicious = 0
        first_read = None
        last_read = None

        for e in events:
            t_us = event_time_us(e)
            wrapper = {
                "session_id": sid,
                "source_seq": e.get("seq"),
                "t_us": t_us,
                "t_us_wall": e.get("t_us"),
                "capture_t_us": e.get("capture_t_us"),
                "origin": e.get("origin"),
                "event": e,
            }
            all_events.append(wrapper)

            if e.get("ev") == "media_layout":
                media_layout.append(wrapper)

            if e.get("ev") in {"session", "phase", "usb", "status", "capture_health", "marker"}:
                lifecycle.append(wrapper)

            if e.get("ev") == "bot_cbw":
                bot_count += 1
                scsi = e.get("scsi") or {}
                op = scsi.get("opcode")
                if op == "read10":
                    read10_count += 1
                    lba = scsi.get("lba", 0) or 0
                    blocks = scsi.get("blocks", 0) or 0
                    if sector_count is not None and lba + blocks > sector_count:
                        suspicious += 1
                    read10.append(
                        {
                            "session_id": sid,
                            "source_seq": e.get("seq"),
                            "t_us": t_us,
                            "bot_tag": e.get("bot_tag"),
                            "lba": lba,
                            "blocks": blocks,
                            "xfer_len": e.get("xfer_len"),
                            "cdb_hex": e.get("cdb_hex"),
                            "origin": e.get("origin"),
                            "source_event": e,
                        }
                    )
                elif op == "tur":
                    tur_count += 1

            if e.get("ev") == "read":
                read_count += 1
                if first_read is None:
                    first_read = t_us
                last_read = t_us

        probe = session.get("probe_identity") or {}
        capture = session.get("capture") or {}
        summaries.append(
            {
                "session_id": sid,
                "fw_version": session.get("fw_version"),
                "fw_sha": session.get("fw_sha"),
                "fw_dirty": session.get("fw_dirty"),
                "transport": session.get("transport"),
                "scenario": session.get("scenario"),
                "vehicle": session.get("vehicle"),
                "media_variant": media.get("variant"),
                "volume_id": media.get("volume_id"),
                "volume_label": media.get("fs_label") or probe.get("volume_label"),
                "usb_serial": media.get("usb_serial_override") or probe.get("usb_serial"),
                "sector_count": sector_count,
                "event_count": len(events),
                "bot_cbw_count": bot_count,
                "read10_cbw_count": read10_count,
                "tur_cbw_count": tur_count,
                "read_event_count": read_count,
                "first_read_t_us": first_read,
                "last_read_t_us": last_read,
                "suspicious_read10_out_of_range": suspicious,
                "integrity_pass": capture.get("integrity_pass"),
                "complete": capture.get("complete"),
                "epistemic": {
                    "20261023-074319_bmw_free": "RST-confounded; not a clean Serial-only proof",
                    "20261023-074455_bmw_free": "core live-wire Label cycle",
                    "20261023-074749_bmw_free": "core live-wire Serial+Label+Vol cycle",
                    "20261023-074908_bmw_free": "core live-wire Volume-ID cycle",
                    "20261023-072929_bmw_seq_ident_1r1": "offline I0; upload/replay issues — contextual only",
                }.get(sid),
            }
        )

        integrity.append(
            {
                "session_id": sid,
                "checks": {
                    "jsonl_parse": True,
                    "event_seq_non_decreasing": all(
                        i == 0 or events[i].get("seq", 0) >= events[i - 1].get("seq", 0)
                        for i in range(len(events))
                    ),
                    "read10_lba_within_sector_count": (
                        None if sector_count is None else suspicious == 0
                    ),
                    "fw_dirty_false": session.get("fw_dirty") is False
                    or (session.get("fw_dirty") is None and media.get("variant") is not None),
                    "dropped": capture.get("dropped"),
                    "stream_lost": capture.get("stream_lost"),
                    "tcp_tx_fail": capture.get("tcp_tx_fail"),
                    "integrity_pass": capture.get("integrity_pass"),
                },
            }
        )

    report_meta = copy_reports(out / "reports", root)

    reference = {
        "schema": "hu-probe-self-recalc/reference/1",
        "atom_definition": {
            "head_blocks": 240,
            "tail_blocks": 70,
            "small_file_blocks": 310,
            "scan_head_pattern_blocks": [32, 128],
        },
        "known_media": {
            "sector_count": 32768,
            "files": [
                {
                    "path": "TRACK1.MP3",
                    "start_lba": 289,
                    "blocks": 310,
                    "size_bytes": 158473,
                    "duration_ms": 26514,
                    "seed": 1,
                },
                {
                    "path": "TRACK2.MP3",
                    "start_lba": 599,
                    "blocks": 310,
                    "size_bytes": 158473,
                    "duration_ms": 26514,
                    "seed": 2,
                },
                {
                    "path": "TRACK3.MP3",
                    "start_lba": 909,
                    "blocks": 310,
                    "size_bytes": 158473,
                    "duration_ms": 26514,
                    "seed": 3,
                },
            ],
        },
        "session_summary": summaries,
        "do_not_claim_as_fact": [
            "Resume keyed on content/seed",
            "074319 proves Serial alone triggers head-scan",
            "Short USB cycle always triggers file head re-scan",
            "Elevator scheduler is proven",
            "158 KiB atom/lead findings generalize to large MP3s",
        ],
    }

    manifest = {
        "schema": "hu-probe-self-recalc/1.1",
        "source_repo": REPO,
        "source_ref": REF,
        "source_commit_url": f"https://github.com/{REPO}/commit/{REF}",
        "built_from": "github_raw" if args.from_github else "local_artifacts",
        "local_artifact_root": ARTIFACT_REL,
        "sessions": SESSIONS,
        "primary_evidence": [
            "sessions/*/events.jsonl",
            "sessions/*/session.json",
            "events_normalized.jsonl",
            "read10.jsonl",
            "media_layout.jsonl",
            "lifecycle.jsonl",
        ],
        "reference_only": [
            "derived_reference.json",
            "sessions/*/derived/analysis.json",
            "sessions/*/derived/analysis.md",
            "reports/*",
        ],
        "epistemic_warnings": [
            "074319 is RST-confounded and cannot isolate Serial causality.",
            "072929 is an offline seq_ident run with upload/replay infrastructure problems; do not treat it as equal-strength live-wire evidence.",
            "Do not extrapolate the small 158473-byte files to large MP3s without a size ladder.",
            "Operator Resume observations are not wire measurements.",
            "Do not write 'Resume keyed on content' as FACT — only Serial/Label/Vol were excluded.",
            "Short cycles in core sessions: FS-enum without file head-scan (074455/074749/074908).",
        ],
        "source_manifest": source_manifest,
        "reports": report_meta,
    }

    dump_json(out / "manifest.json", manifest)
    dump_json(out / "integrity_checks.json", integrity)
    dump_json(out / "derived_reference.json", reference)
    dump_jsonl(out / "session_summary.jsonl", summaries)
    dump_jsonl(out / "events_normalized.jsonl", all_events)
    dump_jsonl(out / "read10.jsonl", read10)
    dump_jsonl(out / "media_layout.jsonl", media_layout)
    dump_jsonl(out / "lifecycle.jsonl", lifecycle)

    readme = f"""# hu-probe self-recalculation dataset — 2026-10-23

Source: `{REPO}` @ `{REF}`  
Built from: `{"GitHub raw" if args.from_github else "local " + ARTIFACT_REL}`

This package contains the raw archived sessions plus normalized JSONL
views so another AI/analyst can independently recalculate the findings.

## Start here

1. Read `reports/2026-10-23-gesamtbericht-identity-remount-multireview.md`
2. Use `manifest.json` epistemic_warnings
3. Recalculate from primary event streams (not derived/)

## Primary evidence

- `sessions/*/events.jsonl` — original event streams, preserved unchanged
- `sessions/*/session.json` — original session metadata
- `events_normalized.jsonl` — all events from all five sessions
- `read10.jsonl` — every READ10 CBW (`scsi.opcode == read10`)
- `media_layout.jsonl` — media/file layout events
- `lifecycle.jsonl` — session/phase/USB/status/capture-health/marker
- `integrity_checks.json` — packaging-time structural checks

## Reference layer

`derived_reference.json` and archived `derived/analysis.*` are reference
calculations only. They must not replace recalculation from primary events.

## Session warnings

| Session | Role |
|---------|------|
| `074319` | RST-confounded Serial attempt — **not** Serial causality proof |
| `074455` | **core** Label cycle |
| `074749` | **core** Serial+Label+Vol cycle |
| `074908` | **core** Volume-ID cycle |
| `072929` | offline I0 — contextual only |

## Known synthetic medium (cache_a family)

- sector_count: 32768
- TRACK1: LBA 289, 310 blocks, 158473 bytes
- TRACK2: LBA 599, 310 blocks, 158473 bytes
- TRACK3: LBA 909, 310 blocks, 158473 bytes
- expected small-file atom: 240-block head + 70-block tail
- common cold-start scan pattern: 32 + 128 blocks per file

## Recommended recalculations

1. Parse every READ10 from the primary events
2. Map every READ10 range to file extents
3. Detect 32+128 head scans
4. Detect complete 310-block atoms
5. Classify head-first versus tail-first order
6. Recalculate gaps from `t_us` / `capture_t_us`
7. Recalculate TUR intervals from TUR CBWs
8. Compare behavior before/after USB cycle (`ev=usb` / `phase`)
9. Keep wire facts separate from operator Resume observations

## Rebuild

```bash
# from repo root (local artifacts, recommended)
python3 tools/build_identity_remount_recalc_2026_10_23.py

# or fetch from GitHub raw at pinned commit
python3 tools/build_identity_remount_recalc_2026_10_23.py --from-github
```
"""
    (out / "README.md").write_text(readme, encoding="utf-8")

    zip_path = None
    if make_zip:
        zip_path = out.with_suffix(".zip")
        if zip_path.exists():
            zip_path.unlink()
        with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
            for p in out.rglob("*"):
                if p.is_file():
                    z.write(p, p.relative_to(out.parent).as_posix())
        print(f"Created: {zip_path.resolve()}")

    print(f"Directory: {out.resolve()}")
    print(f"Sessions: {len(SESSIONS)}")
    print(f"READ10 rows: {len(read10)}")
    print(f"Normalized events: {len(all_events)}")
    print(f"Source: {'github' if args.from_github else local_art}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
