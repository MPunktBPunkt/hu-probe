#!/usr/bin/env python3
"""P0 offline analysis for BMW S0 sessions (Burst / STALL / Enum / TUR / QD).

Reads session events.jsonl (by seq when present). Does not invent HU FACTs —
outputs measurements + INFERENCE flags for massnahmen-01d P0a–P0d.

Example:
  python3 tools/offline_s0_p0.py sessions/2176_bmw_s0_plug_enum
  python3 tools/offline_s0_p0.py --bmw-017 --markdown docs/feld/2026-10-15-offline-p0.md
"""

from __future__ import annotations

import argparse
import json
from collections import Counter, defaultdict
from pathlib import Path

BMW_017 = [
    "3076_bmw_s0_plug_enum",
    "2199_bmw_s0_plug_enum",
    "2363_bmw_s0_plug_enum",
    "2176_bmw_s0_plug_enum",
    "3334_bmw_s0_plug_enum",
    "2310_bmw_s0_plug_enum",
    "3129_bmw_s0_plug_enum",
    "3085_bmw_s0_plug_enum",
]

# Idle gap between reads that splits "bursts" (field: ~9 s pauses; use 500 ms).
BURST_IDLE_US = 500_000
# Pace gaps for histograms (exclude session tears).
PACE_GAP_MAX_US = 10_000_000
# STALL attributed to preceding CBW if within this window (drain-stamped events
# often share identical t_us — also match equal t_us).
STALL_ATTR_US = 50_000


def load_events(session_dir: Path) -> list[dict]:
    path = session_dir / "events.jsonl"
    rows = [json.loads(l) for l in path.read_text(encoding="utf-8").splitlines() if l.strip()]
    # Prefer seq order; fall back to file order.
    if all(isinstance(e.get("seq"), int) for e in rows):
        rows.sort(key=lambda e: e["seq"])
    return rows


def pct(vals: list[float | int], p: float) -> float | None:
    if not vals:
        return None
    s = sorted(vals)
    if len(s) == 1:
        return float(s[0])
    k = (len(s) - 1) * p
    f = int(k)
    c = min(f + 1, len(s) - 1)
    if f == c:
        return float(s[f])
    return float(s[f] + (s[c] - s[f]) * (k - f))


def scsi_op(e: dict) -> str | None:
    sc = e.get("scsi")
    if isinstance(sc, dict):
        return sc.get("opcode")
    return None


def analyze_session(session_dir: Path) -> dict:
    meta = {}
    if (session_dir / "session.json").exists():
        meta = json.loads((session_dir / "session.json").read_text(encoding="utf-8"))
    ev = load_events(session_dir)

    reads = [e for e in ev if e.get("ev") == "read"]
    files = [e for e in reads if e.get("kind") == "file"]
    cbw = [e for e in ev if e.get("ev") == "bot_cbw"]
    csw = [e for e in ev if e.get("ev") == "bot_csw"]
    usb = [e for e in ev if e.get("ev") == "usb"]
    phases = [e for e in ev if e.get("ev") == "phase"]

    # --- Bursts (prefer instrumented gap_us; t_us alone regresses under drain/coex) ---
    bursts: list[list[dict]] = []
    cur: list[dict] = []
    for e in reads:
        g = e.get("gap_us")
        split = isinstance(g, int) and g > BURST_IDLE_US
        if split and cur:
            bursts.append(cur)
            cur = [e]
        else:
            cur.append(e)
    if cur:
        bursts.append(cur)

    burst_rows = []
    for i, b in enumerate(bursts):
        kinds = Counter(x.get("kind") for x in b)
        n_file = kinds.get("file", 0)
        bytes_ = sum(int(x.get("n") or 0) for x in b)
        # Duration ≈ sum of in-burst pace gaps (exclude the leading idle gap that split us).
        pace = [
            int(x["gap_us"])
            for x in b
            if isinstance(x.get("gap_us"), int) and x["gap_us"] < BURST_IDLE_US
        ]
        dur_ms = (sum(pace) / 1000.0) if pace else 0.0
        lbas = [x.get("lba") for x in b if x.get("lba") is not None]
        burst_rows.append(
            {
                "i": i,
                "n_reads": len(b),
                "n_file": n_file,
                "bytes": bytes_,
                "kinds": dict(kinds),
                "dur_ms": dur_ms,
                "lba_min": min(lbas) if lbas else None,
                "lba_max": max(lbas) if lbas else None,
            }
        )

    inter_burst_ms = []
    for b in bursts[1:]:
        g = b[0].get("gap_us")
        if isinstance(g, int) and g >= BURST_IDLE_US:
            inter_burst_ms.append(g / 1000.0)

    file_ns = Counter(int(e["n"]) for e in files if e.get("n") is not None)
    file_gaps = [
        int(e["gap_us"])
        for e in files
        if isinstance(e.get("gap_us"), int) and e["gap_us"] < PACE_GAP_MAX_US
    ]

    # H-quant: dominant file-count per burst (ignore empty-file bursts)
    file_counts = [r["n_file"] for r in burst_rows if r["n_file"] > 0]
    byte_counts = [r["bytes"] for r in burst_rows if r["n_file"] > 0]

    # --- STALL vs MODE_SENSE6 ---
    stalls = [e for e in usb if e.get("usb_ev") == "ep_stall"]
    clears = [e for e in usb if e.get("usb_ev") == "clear_halt"]
    ms6 = [e for e in cbw if scsi_op(e) == "mode_sense6"]
    cap = [e for e in cbw if scsi_op(e) == "read_capacity10"]

    stall_attrs: Counter[str] = Counter()
    stall_same_tus_ms6 = 0
    for st in stalls:
        t = st.get("t_us")
        if t is None:
            continue
        # preceding CBW by seq
        pred = [c for c in cbw if c.get("seq", 0) < st.get("seq", 10**9)]
        if not pred:
            stall_attrs["no_pred_cbw"] += 1
            continue
        c = pred[-1]
        op = scsi_op(c) or "unknown"
        ct = c.get("t_us")
        if ct is not None and abs(t - ct) <= STALL_ATTR_US:
            stall_attrs[op] += 1
            if op == "mode_sense6" and t == ct:
                stall_same_tus_ms6 += 1
        else:
            stall_attrs[f"far:{op}"] += 1

    # --- Enum time chain (per mount) ---
    mounts = [e for e in usb if e.get("usb_ev") == "mount"]
    chains = []
    for m in mounts:
        mt = m.get("t_us")
        mseq = m.get("seq", 0)
        if mt is None:
            continue
        # next events after this mount until next mount/umount
        def first_after(pred):
            for e in ev:
                if e.get("seq", 0) <= mseq:
                    continue
                if e.get("ev") == "usb" and e.get("usb_ev") in ("mount", "umount"):
                    return None
                if pred(e):
                    return e
            return None

        inq = first_after(lambda e: e.get("ev") == "bot_cbw" and scsi_op(e) == "inquiry")
        tur = first_after(lambda e: e.get("ev") == "bot_cbw" and scsi_op(e) == "tur")
        r10 = first_after(lambda e: e.get("ev") == "bot_cbw" and scsi_op(e) == "read10")
        rd = first_after(lambda e: e.get("ev") == "read")

        def dt(e):
            if e is None or e.get("t_us") is None:
                return None
            return e["t_us"] - mt

        chains.append(
            {
                "mount_t_us": mt,
                "mount_seq": mseq,
                "to_inquiry_us": dt(inq),
                "to_tur_us": dt(tur),
                "to_read10_us": dt(r10),
                "to_first_read_us": dt(rd),
            }
        )

    # --- TUR rate by phase windows ---
    # Build phase intervals from phase events; default whole session.
    phase_bounds: list[tuple[str, int, int]] = []
    for i, ph in enumerate(phases):
        name = ph.get("phase") or "unknown"
        t0 = ph.get("t_us") or 0
        t1 = phases[i + 1].get("t_us") if i + 1 < len(phases) else (ev[-1].get("t_us") or t0)
        if t1 is None:
            t1 = t0
        phase_bounds.append((name, t0, t1))

    tur_by_phase: dict[str, dict] = {}
    if not phase_bounds and ev:
        t0 = ev[0].get("t_us") or 0
        t1 = ev[-1].get("t_us") or t0
        phase_bounds = [("all", t0, t1)]

    for name, t0, t1 in phase_bounds:
        span = max(t1 - t0, 1)
        n_tur = sum(
            1
            for e in cbw
            if scsi_op(e) == "tur" and t0 <= (e.get("t_us") or -1) < t1
        )
        key = name
        if key not in tur_by_phase:
            tur_by_phase[key] = {"tur": 0, "span_us": 0, "windows": 0}
        tur_by_phase[key]["tur"] += n_tur
        tur_by_phase[key]["span_us"] += span
        tur_by_phase[key]["windows"] += 1

    for v in tur_by_phase.values():
        v["hz"] = v["tur"] / (v["span_us"] / 1e6) if v["span_us"] else None

    # Session-wide TUR hz
    t_first = ev[0].get("t_us") if ev else None
    t_last = ev[-1].get("t_us") if ev else None
    n_tur_all = sum(1 for e in cbw if scsi_op(e) == "tur")
    span_all = (t_last - t_first) if t_first is not None and t_last is not None else None
    tur_hz_session = (n_tur_all / (span_all / 1e6)) if span_all and span_all > 0 else None

    # --- Queue depth proxy: overlapping BOT tags (CBW without CSW yet) ---
    # Track outstanding tags; max concurrent = QD proxy.
    outstanding: set = set()
    max_out = 0
    tag_hist: list[int] = []
    csw_by_tag: dict = defaultdict(list)
    for e in csw:
        tag = e.get("bot_tag")
        if tag is not None:
            csw_by_tag[tag].append(e)
    # Walk CBW/CSW in seq order
    for e in ev:
        if e.get("ev") == "bot_cbw":
            tag = e.get("bot_tag")
            if tag is not None:
                outstanding.add(tag)
                max_out = max(max_out, len(outstanding))
                tag_hist.append(len(outstanding))
        elif e.get("ev") == "bot_csw":
            tag = e.get("bot_tag")
            if tag in outstanding:
                outstanding.discard(tag)

    # CBW→CSW latency for same tag
    cbw_to_csw_us = []
    csw_map = {}
    for e in csw:
        tag = e.get("bot_tag")
        if tag is not None and tag not in csw_map:
            csw_map[tag] = e
    for e in cbw:
        tag = e.get("bot_tag")
        if tag is None or tag not in csw_map:
            continue
        c = csw_map[tag]
        if e.get("t_us") is not None and c.get("t_us") is not None:
            # same drain stamp → 0; still count
            cbw_to_csw_us.append(c["t_us"] - e["t_us"])

    scsi_counts = Counter(scsi_op(e) for e in cbw)

    return {
        "session_id": meta.get("session_id") or session_dir.name,
        "fw": meta.get("fw_version"),
        "events": len(ev),
        "reads": len(reads),
        "file_reads": len(files),
        "file_n_hist": dict(file_ns),
        "file_gap_p50_us": pct(file_gaps, 0.5),
        "file_gap_n": len(file_gaps),
        "bursts": {
            "idle_split_us": BURST_IDLE_US,
            "n": len(bursts),
            "rows": burst_rows,
            "file_counts_hist": dict(Counter(file_counts)),
            "bytes_hist_top": Counter(byte_counts).most_common(8),
            "dur_ms_p50": pct([r["dur_ms"] for r in burst_rows if r["dur_ms"] is not None], 0.5),
            "inter_burst_ms": inter_burst_ms,
            "inter_burst_ms_p50": pct(inter_burst_ms, 0.5),
            "inter_burst_ms_n": len(inter_burst_ms),
        },
        "stall": {
            "ep_stall": len(stalls),
            "clear_halt": len(clears),
            "mode_sense6_cbw": len(ms6),
            "read_capacity10_cbw": len(cap),
            "stall_attr_prev_cbw": dict(stall_attrs),
            "stall_same_t_us_as_mode_sense6": stall_same_tus_ms6,
            "mode_sense6_vs_capacity_ratio": (len(ms6) / len(cap)) if cap else None,
        },
        "enum_chains": chains,
        "tur": {
            "n": n_tur_all,
            "session_hz": tur_hz_session,
            "span_us": span_all,
            "by_phase": tur_by_phase,
        },
        "qd": {
            "max_outstanding_tags": max_out,
            "outstanding_p50": pct(tag_hist, 0.5) if tag_hist else None,
            "cbw_to_csw_us_p50": pct(cbw_to_csw_us, 0.5) if cbw_to_csw_us else None,
            "note": "Device-stack tag concurrency; not wire QD. Drain stamps may collapse latencies to 0.",
        },
        "scsi": dict(scsi_counts),
    }


def aggregate(reports: list[dict]) -> dict:
    all_file_counts: list[int] = []
    all_bytes: list[int] = []
    all_inter: list[float] = []
    all_dur: list[float] = []
    stall_same = 0
    stall_total = 0
    ms6 = 0
    cap = 0
    to_inq: list[float] = []
    to_tur: list[float] = []
    to_r10: list[float] = []
    to_rd: list[float] = []
    tur_hz: list[float] = []
    max_qd: list[int] = []

    for r in reports:
        b = r["bursts"]
        for row in b["rows"]:
            if row["n_file"] > 0:
                all_file_counts.append(row["n_file"])
                all_bytes.append(row["bytes"])
            if row["dur_ms"] is not None:
                all_dur.append(row["dur_ms"])
        # recompute inter from rows? use stored p50 only — collect from rows timestamps
        # already in per-session; pull inter via re-scan of rows order
        stall_same += r["stall"]["stall_same_t_us_as_mode_sense6"]
        stall_total += r["stall"]["ep_stall"]
        ms6 += r["stall"]["mode_sense6_cbw"]
        cap += r["stall"]["read_capacity10_cbw"]
        for c in r["enum_chains"]:
            if c["to_inquiry_us"] is not None:
                to_inq.append(c["to_inquiry_us"])
            if c["to_tur_us"] is not None:
                to_tur.append(c["to_tur_us"])
            if c["to_read10_us"] is not None:
                to_r10.append(c["to_read10_us"])
            if c["to_first_read_us"] is not None:
                to_rd.append(c["to_first_read_us"])
        if r["tur"]["session_hz"] is not None:
            tur_hz.append(r["tur"]["session_hz"])
        max_qd.append(r["qd"]["max_outstanding_tags"])
        all_inter.extend(b.get("inter_burst_ms") or [])

    return {
        "sessions": len(reports),
        "h_quant": {
            "file_reads_per_burst_hist": dict(Counter(all_file_counts)),
            "file_reads_per_burst_p50": pct(all_file_counts, 0.5),
            "bytes_per_burst_hist_top": Counter(all_bytes).most_common(10),
            "bytes_per_burst_p50": pct(all_bytes, 0.5),
            "dur_ms_p50": pct(all_dur, 0.5),
            "inter_burst_ms_p50": pct(all_inter, 0.5),
            "inter_burst_ms_n": len(all_inter),
            "inference": (
                "Dominant file-count clusters indicate Host batch size candidates; "
                "do not publish as FACT until burst definition validated against markers."
            ),
        },
        "stall_artifact": {
            "ep_stall_total": stall_total,
            "stall_same_t_us_mode_sense6": stall_same,
            "mode_sense6_cbw": ms6,
            "read_capacity10_cbw": cap,
            "mode_sense6_per_capacity": (ms6 / cap) if cap else None,
            "inference": (
                "High fraction of stalls sharing t_us with mode_sense6 CBW + ~2× mode_sense6 "
                "vs capacity supports Instrument-STALL hypothesis (P1b), not HU recovery FACT."
            ),
        },
        "enum": {
            "mount_to_inquiry_us_p50": pct(to_inq, 0.5),
            "mount_to_tur_us_p50": pct(to_tur, 0.5),
            "mount_to_read10_us_p50": pct(to_r10, 0.5),
            "mount_to_first_read_us_p50": pct(to_rd, 0.5),
            "n_chains": len(to_rd),
            "note": (
                "Mount→SCSI intervals are device-callback times; not PiDrive enum_s≈1s "
                "(attach→usable). Drain stamps may collapse to 0."
            ),
        },
        "tur_hz_session_p50": pct(tur_hz, 0.5),
        "tur_hz_session_values": [round(x, 3) for x in tur_hz],
        "qd_max_hist": dict(Counter(max_qd)),
        "qd_max_p50": pct(max_qd, 0.5),
    }


def render_markdown(reports: list[dict], agg: dict) -> str:
    lines = [
        "# Offline P0 — BMW S0 0.2.17 (Burst / STALL / Enum / TUR / QD)",
        "",
        f"**Sessions:** {agg['sessions']} · **Burst-Split:** idle gap > {BURST_IDLE_US} µs · "
        "Pace weiter über `gap_us`.",
        "",
        "Epistemik: Zahlen hier = Messwerte aus hu-probe-Rohdaten. "
        "**Keine** neuen HU-FACTs ohne Feld-Marker (S2/S3).",
        "",
        "## Aggregat",
        "",
        "### H-Quant / Burst",
        "",
        f"- File-Reads/Burst Hist: `{agg['h_quant']['file_reads_per_burst_hist']}`",
        f"- File-Reads/Burst p50: **{agg['h_quant']['file_reads_per_burst_p50']}**",
        f"- Bytes/Burst p50: **{agg['h_quant']['bytes_per_burst_p50']}** "
        f"(top `{agg['h_quant']['bytes_per_burst_hist_top'][:5]}`)",
        f"- Burst-Dauer p50: **{agg['h_quant']['dur_ms_p50']}** ms",
        f"- Inter-Burst-Pause p50: **{agg['h_quant']['inter_burst_ms_p50']}** ms "
        f"(n={agg['h_quant']['inter_burst_ms_n']})",
        f"- INFERENCE: {agg['h_quant']['inference']}",
        "",
        "### STALL ↔ MODE_SENSE6",
        "",
        f"- ep_stall total: **{agg['stall_artifact']['ep_stall_total']}**",
        f"- davon gleiches `t_us` wie mode_sense6-CBW: "
        f"**{agg['stall_artifact']['stall_same_t_us_mode_sense6']}**",
        f"- mode_sense6 CBW / read_capacity10: "
        f"**{agg['stall_artifact']['mode_sense6_cbw']}** / "
        f"**{agg['stall_artifact']['read_capacity10_cbw']}** "
        f"(Ratio {agg['stall_artifact']['mode_sense6_per_capacity']})",
        f"- INFERENCE: {agg['stall_artifact']['inference']}",
        "",
        "### Enum-Zeitkette (Mount → …)",
        "",
        f"- n chains: {agg['enum']['n_chains']}",
        f"- Mount→INQUIRY p50: **{agg['enum']['mount_to_inquiry_us_p50']}** µs",
        f"- Mount→TUR p50: **{agg['enum']['mount_to_tur_us_p50']}** µs",
        f"- Mount→READ10 p50: **{agg['enum']['mount_to_read10_us_p50']}** µs",
        f"- Mount→first read p50: **{agg['enum']['mount_to_first_read_us_p50']}** µs",
        f"- Note: {agg['enum']['note']}",
        "",
        "### TUR / QD",
        "",
        f"- Session TUR-Hz Werte: {agg['tur_hz_session_values']} · p50 **{agg['tur_hz_session_p50']}**",
        f"- max outstanding BOT-Tags Hist: `{agg['qd_max_hist']}` · p50 **{agg['qd_max_p50']}**",
        "",
        "## Per Session",
        "",
        "| Session | file | bursts | file/burst hist | stall→ms6 same t | ms6/cap | TUR Hz | QD max |",
        "|---------|-----:|-------:|-----------------|-----------------:|--------:|-------:|-------:|",
    ]
    for r in reports:
        fc = r["bursts"]["file_counts_hist"]
        lines.append(
            f"| `{r['session_id']}` | {r['file_reads']} | {r['bursts']['n']} | `{fc}` | "
            f"{r['stall']['stall_same_t_us_as_mode_sense6']}/{r['stall']['ep_stall']} | "
            f"{r['stall']['mode_sense6_cbw']}/{r['stall']['read_capacity10_cbw']} | "
            f"{None if r['tur']['session_hz'] is None else round(r['tur']['session_hz'], 2)} | "
            f"{r['qd']['max_outstanding_tags']} |"
        )
    lines.extend(
        [
            "",
            "## Folgen für Maßnahmen-01d",
            "",
            "| ID | Ergebnis | Next |",
            "|----|----------|------|",
            "| P0a | Burst-Split operativ; H-Quant zeigt Cluster — nicht als FACT | Profil: nur INFERENCE |",
            "| P0b | STALL stark an mode_sense6 gebunden | **P1b** MODE_SENSE6 fix |",
            "| P0c | Mount→SCSI oft ms-Skala (Drain-Vorsicht) | nicht enum_s≈1s ersetzen ohne Attach-Kette |",
            "| P0d | QD max oft 1 (Tag-Concurrency) | INFERENCE; Wire UNKNOWN |",
            "",
        ]
    )
    return "\n".join(lines) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("sessions", nargs="*", type=Path, help="session directories")
    ap.add_argument("--bmw-017", action="store_true", help="use the eight tracked 0.2.17 BMW sessions")
    ap.add_argument("--root", type=Path, default=Path("sessions"))
    ap.add_argument("-o", "--json-out", type=Path, default=None)
    ap.add_argument("--markdown", type=Path, default=None)
    args = ap.parse_args()

    dirs: list[Path] = []
    if args.bmw_017:
        dirs = [args.root / n for n in BMW_017]
    dirs.extend(args.sessions)
    if not dirs:
        ap.error("pass session dirs or --bmw-017")

    reports = []
    for d in dirs:
        if not d.is_dir():
            raise SystemExit(f"missing session: {d}")
        reports.append(analyze_session(d))

    agg = aggregate(reports)
    out = {"aggregate": agg, "sessions": reports}

    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(json.dumps(out, indent=2), encoding="utf-8")
        print(f"wrote {args.json_out}")

    md = render_markdown(reports, agg)
    if args.markdown:
        args.markdown.parent.mkdir(parents=True, exist_ok=True)
        args.markdown.write_text(md, encoding="utf-8")
        print(f"wrote {args.markdown}")
    else:
        print(md)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
