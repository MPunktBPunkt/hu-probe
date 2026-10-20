#!/usr/bin/env python3
"""Analyse hu-probe sessions: mount signature, reads per file, TUR grid, prefetch lead.

Per session it writes ``<session>/derived/analysis.json`` and ``analysis.md``:

* timeline: mount signature (TUR count + first READ10s), read episodes per file,
  TUR grid (idle period, bursts);
* time base: ``capture_t_us`` (FW >= 0.2.31) > ``t_us`` > TUR-grid reconstruction
  when replayed offline events were clamped to one timestamp (B7, FW <= 0.2.30);
* lead/lag of every next-track read, selection latency (marker -> READ10),
  ``tail_then_head`` (file read end-first);
* with real timestamps only: TUR phase (free-running grid), media-loss detection
  latency and burst, plug timeline (set_address -> first file read), file atoms
  (CBW start-to-start vs. gap_us, atom end -> next read) and 512-block read windows;
* cross-session comparison with ``--compare`` (JSON + Markdown).

``analysis.json`` is also the exchange format for PiDrive
(``tools/nbt_hu_sim.py --replay-huprobe``).

Labels follow the audit vocabulary: values from real timestamps are MEASURED,
values on the TUR grid are INFERRED (+-1 TUR period).
"""

from __future__ import annotations

import argparse
import json
import statistics
from bisect import bisect_right
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

SCHEMA = "hu-probe-analysis/1"
TUR_PERIOD_S_DEFAULT = 2.239          # BMW NBT Evo, n=382 (2026-10-19)
MOUNT_TURS = 32
MOUNT_REF = "0/8,0/8,24/8,0/8,0/1,1/3,7/2,1/1,2/1,3/5,24/8"
FILE_READ_MIN_BLOCKS = 9               # skips the 24/8 mount probe
EPISODE_GAP_S = 1.0
SELECT_MARKERS = ("play_start", "track_next", "replay")
AUTO_MARKERS = ("mount_done", "first_read", "next_track_read", "media_hide", "ua_sent",
                "read_stall", "stall_abandon", "stall_on", "stall_off")
MUTATE_MARKERS = ("media_mutate", "ua_request", "ua_sent")
REACT_WINDOW_S = 30.0
STALL_MIN_HOLD_US = 500_000
TUR_GRID_TOL_S = 0.03                  # residual allowed for a TUR to count as on-grid
CBW_MAX_BLOCKS = 240
WINDOW_BLOCKS = 512                    # hypothesis: 256 KiB read window (PiDrive A7 + demo_v1)
WINDOW_GAP_S = 1.0
CHUNK_PAUSE_US = 10_000                # gap between 4 KiB chunks of one CBW reported as pause

# demo_v1 (FAT12, spc 1): boot 0, FAT 1-6, root 7-8, data 9..; 3 x 309 sectors.
# Tracks: MPEG-1 L3 48 kbit/s 44.1 kHz, 1013 complete frames (+1 cut by 20 B) = 26.462 s.
DEMO_V1_FILES = [
    {"node": 0, "path": "/TRACK1.MP3", "size": 158208, "dur_ms": 26462, "kbps": 48, "extents": [[9, 309]]},
    {"node": 1, "path": "/TRACK2.MP3", "size": 158208, "dur_ms": 26462, "kbps": 48, "extents": [[318, 309]]},
    {"node": 2, "path": "/TRACK3.MP3", "size": 158208, "dur_ms": 26462, "kbps": 48, "extents": [[627, 309]]},
]
DEMO_V1_REGIONS = [(0, 0, "BOOT"), (1, 6, "FAT"), (7, 8, "ROOT")]


# ---------------------------------------------------------------- loading


def load_jsonl(path: Path) -> list[dict]:
    out = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            out.append(json.loads(line))
        except json.JSONDecodeError:
            continue
    return out


def load_session(path: Path) -> tuple[Path, dict, list[dict]]:
    """Accept a session dir or an events.jsonl path."""
    if path.is_file():
        folder = path.parent
        events = load_jsonl(path)
    else:
        folder = path
        events = load_jsonl(path / "events.jsonl")
    meta: dict = {}
    if (folder / "session.json").exists():
        try:
            meta = json.loads((folder / "session.json").read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            meta = {}
    events.sort(key=lambda e: e.get("seq", 0) if isinstance(e.get("seq"), int) else 0)
    return folder, meta, events


# ---------------------------------------------------------------- media map


@dataclass
class MediaMap:
    variant: str
    files: list[dict]
    regions: list[tuple[int, int, str]] = field(default_factory=list)
    source: str = ""
    _starts: list[int] = field(default_factory=list)
    _ext: list[tuple[int, int, int, int]] = field(default_factory=list)  # lba, n, node, file_blk0

    def __post_init__(self) -> None:
        ext = []
        for f in self.files:
            if f.get("dir"):
                continue
            blk = 0
            for lba, n in f.get("extents") or []:
                ext.append((int(lba), int(n), int(f["node"]), blk))
                blk += int(n)
        ext.sort()
        self._ext = ext
        self._starts = [e[0] for e in ext]

    def locate(self, lba: int) -> tuple[int | None, int | None]:
        i = bisect_right(self._starts, lba) - 1
        if i >= 0:
            s, n, node, blk0 = self._ext[i]
            if s <= lba < s + n:
                return node, blk0 + (lba - s)
        return None, None

    def region(self, lba: int) -> str:
        for a, b, tag in self.regions:
            if a <= lba <= b:
                return tag
        node, _ = self.locate(lba)
        return "DATA" if node is not None else "OTHER"

    def file(self, node: int | None) -> dict | None:
        if node is None:
            return None
        for f in self.files:
            if f.get("node") == node:
                return f
        return None

    def file_blocks(self, node: int) -> int:
        f = self.file(node) or {}
        return sum(int(n) for _, n in f.get("extents") or [])


def media_map(meta: dict, events: list[dict]) -> MediaMap:
    layout = [e for e in events if e.get("ev") == "media_layout"]
    variant = (meta.get("media") or {}).get("variant")
    if not variant:
        start = next((e for e in events if e.get("ev") == "session" and e.get("action") == "start"), {})
        variant = (start.get("media") or {}).get("variant")
    if layout:
        files = [
            {k: e.get(k) for k in ("node", "path", "dir", "size", "dur_ms", "kbps", "seed", "extents",
                                   "title", "cover", "apic_bytes")}
            for e in layout
        ]
        variant = variant or layout[0].get("variant") or "unknown"
        if variant == "demo_v1":
            ref = {f["path"].lstrip("/"): f for f in DEMO_V1_FILES}
            for f in files:
                r = ref.get(str(f.get("path") or "").lstrip("/"))
                if r:
                    f["dur_ms"] = f.get("dur_ms") or r["dur_ms"]
                    f["kbps"] = f.get("kbps") or r["kbps"]
        m = media_map_from(files, variant, "media_layout")
        media = (meta.get("media") or {})
        if media.get("data_lba"):
            m.regions = [(0, 0, "BOOT"), (1, int(media["root_lba"]) - 1, "FAT"),
                         (int(media["root_lba"]), int(media["data_lba"]) - 1, "ROOT")]
        return m
    if meta.get("media_layout"):
        return media_map_from(meta["media_layout"], variant or "unknown", "session.json")
    return MediaMap("demo_v1", DEMO_V1_FILES, DEMO_V1_REGIONS, "demo_v1_builtin")


def media_map_from(files: list[dict], variant: str, source: str) -> MediaMap:
    return MediaMap(variant, files, [], source)


# ---------------------------------------------------------------- time base


def _raw_time_us(e: dict) -> int | None:
    v = e.get("capture_t_us")
    if isinstance(v, (int, float)):
        return int(v)
    v = e.get("t_us")
    if isinstance(v, (int, float)):
        return int(v)
    v = e.get("t_ms")
    if isinstance(v, (int, float)):
        return int(v) * 1000
    return None


def build_time_base(cmds: list[dict], tur_period_s: float) -> tuple[list[float], str]:
    """Seconds per CBW. Falls back to the TUR grid when timestamps collapsed."""
    raw = [_raw_time_us(c) for c in cmds]
    have_capture = any(isinstance(c.get("capture_t_us"), (int, float)) for c in cmds)
    turs = [i for i, c in enumerate(cmds) if c["op"] == "tur"]
    distinct_tur_t = len({raw[i] for i in turs if raw[i] is not None})
    collapsed = len(turs) >= 4 and distinct_tur_t <= max(2, len(turs) // 4)
    if not collapsed and all(r is not None for r in raw):
        t0 = raw[0] if raw else 0
        return [(r - t0) / 1e6 for r in raw], ("capture_t_us" if have_capture else "t_us")

    # TUR grid: the ~15-TUR burst after a mount (INQUIRY / usb mount) takes ~0.3 s and
    # is treated as zero; every other TUR advances one period. Commands between two
    # TURs are spread evenly in the interval (order kept, absolute error <= 1 period).
    times = [0.0] * len(cmds)
    tur_times: dict[int, float] = {}
    t = 0.0
    burst_left = 0
    first = True
    for idx, c in enumerate(cmds):
        if (c["kind"] == "usb" and c["op"] == "mount") or c["op"] == "inquiry":
            burst_left = 15
            continue
        if c["op"] != "tur":
            continue
        if burst_left > 0:
            burst_left -= 1
        elif not first:
            t += tur_period_s
        first = False
        tur_times[idx] = t
    anchors = sorted(tur_times.items())
    if not anchors:
        return [0.0] * len(cmds), "order_only"
    a = 0
    for i in range(len(cmds)):
        while a + 1 < len(anchors) and anchors[a + 1][0] <= i:
            a += 1
        prev = anchors[a] if anchors[a][0] <= i else None
        nxt = anchors[a + 1] if prev and a + 1 < len(anchors) else None
        if prev and prev[0] == i:
            times[i] = prev[1]
        elif prev and nxt:
            frac = (i - prev[0]) / (nxt[0] - prev[0])
            times[i] = prev[1] + frac * (nxt[1] - prev[1])
        elif prev:
            times[i] = prev[1]
        else:
            times[i] = 0.0
    return times, "tur_grid"


# ---------------------------------------------------------------- extraction


def extract_cmds(events: list[dict]) -> list[dict]:
    """CBWs in seq order with op/lba/blocks, plus usb mount and marker positions."""
    out = []
    for e in events:
        ev = e.get("ev")
        if ev == "bot_cbw":
            scsi = e.get("scsi") or {}
            out.append({
                "kind": "cbw", "op": "tur" if scsi.get("opcode") == "tur" else scsi.get("opcode"),
                "lba": scsi.get("lba"), "blocks": scsi.get("blocks"),
                "bot_tag": e.get("bot_tag"), "seq": e.get("seq"),
                "t_us": e.get("t_us"), "capture_t_us": e.get("capture_t_us"),
                "origin": e.get("origin"),
            })
        elif ev == "usb" and e.get("usb_ev") in ("mount", "umount", "msc_bot_reset", "bot_need_reset",
                                                 "set_address"):
            out.append({"kind": "usb", "op": e.get("usb_ev"), "seq": e.get("seq"),
                        "t_us": e.get("t_us"), "capture_t_us": e.get("capture_t_us"),
                        "origin": e.get("origin")})
        elif ev == "marker":
            out.append({"kind": "marker", "op": "marker", "name": e.get("name"),
                        "note": e.get("note"), "seq": e.get("seq"), "t_us": e.get("t_us"),
                        "capture_t_us": e.get("capture_t_us"), "origin": e.get("origin")})
    return out


def pctl(vals: list[float], p: float) -> float | None:
    if not vals:
        return None
    s = sorted(vals)
    k = (len(s) - 1) * p
    f = int(k)
    c = min(f + 1, len(s) - 1)
    return s[f] + (s[c] - s[f]) * (k - f)


def tur_grid(cmds: list[dict], times: list[float], time_source: str) -> dict:
    tt = [times[i] for i, c in enumerate(cmds) if c["op"] == "tur" and c.get("origin") != "post_finalize"]
    iv = [b - a for a, b in zip(tt, tt[1:])]
    idle = [x for x in iv if 0.5 <= x <= 5.0]
    burst = [x for x in iv if x < 0.5]
    res = {
        "tur_count": len(tt),
        "burst_intervals": len(burst),
        "idle_n": len(idle),
        "label": "MEASURED" if time_source in ("t_us", "capture_t_us") else "ASSUMED",
    }
    if idle and time_source != "tur_grid":
        res.update({
            "idle_median_s": round(statistics.median(idle), 4),
            "idle_p10_s": round(pctl(idle, 0.1), 4),
            "idle_p90_s": round(pctl(idle, 0.9), 4),
        })
    return res


def mount_signatures(cmds: list[dict], times: list[float]) -> list[dict]:
    """Per mount: TUR count until the first file-sized read and the first 11 READ10s."""
    sigs = []
    starts = [i for i, c in enumerate(cmds)
              if (c["kind"] == "usb" and c["op"] == "mount") or (c["op"] == "inquiry")]
    # collapse inquiry right after a usb mount into one start
    merged: list[int] = []
    for i in starts:
        if merged and i - merged[-1] < 6:
            continue
        merged.append(i)
    for n, s in enumerate(merged):
        end = merged[n + 1] if n + 1 < len(merged) else len(cmds)
        reads: list[str] = []
        turs = 0
        for i in range(s, end):
            c = cmds[i]
            if c["op"] == "tur":
                turs += 1
            if c["op"] == "read10" and len(reads) < 11:
                reads.append(f"{c['lba']}/{c['blocks']}")
            if len(reads) >= 11 and turs >= MOUNT_TURS:
                break
        sig = ",".join(reads)
        sigs.append({
            "start_seq": cmds[s].get("seq"),
            "t_s": round(times[s], 3),
            "turs_until_signature_end": turs,
            "reads": sig,
            "matches_reference": sig == MOUNT_REF,
            "variant_3_1_4_4": sig.replace("3/1,4/4", "3/5") == MOUNT_REF and "3/1,4/4" in sig,
        })
    return sigs


def read_episodes(cmds: list[dict], times: list[float], mm: MediaMap) -> list[dict]:
    eps: list[dict] = []
    cur: dict | None = None
    for i, c in enumerate(cmds):
        if c["op"] != "read10" or c.get("lba") is None:
            continue
        lba, blocks = int(c["lba"]), int(c.get("blocks") or 0)
        node, blk = mm.locate(lba)
        if node is None:
            continue
        t = times[i]
        if blocks < FILE_READ_MIN_BLOCKS:
            # Mount probe reads (24/8) stay separate so they cannot fake a tail-first order.
            f = mm.file(node) or {}
            eps.append({"node": node, "file": f.get("path"), "t_start_s": t, "t_end_s": t,
                        "start_seq": c.get("seq"), "chunks": [[blk, blocks]], "blocks": blocks,
                        "origin": c.get("origin")})
            cur = None
            continue
        if cur and cur["node"] == node and t - cur["t_end_s"] <= EPISODE_GAP_S:
            cur["chunks"].append([blk, blocks])
            cur["t_end_s"] = t
            cur["blocks"] += blocks
            continue
        f = mm.file(node) or {}
        cur = {"node": node, "file": f.get("path"), "t_start_s": t, "t_end_s": t,
               "start_seq": c.get("seq"), "chunks": [[blk, blocks]], "blocks": blocks,
               "origin": c.get("origin")}
        eps.append(cur)
    for ep in eps:
        total = mm.file_blocks(ep["node"]) or 1
        first_blk = ep["chunks"][0][0]
        ep["first_blk"] = first_blk
        ep["coverage"] = round(min(1.0, ep["blocks"] / total), 3)
        has_head = any(b == 0 for b, _ in ep["chunks"])
        ep["order"] = ("tail_then_head" if first_blk > 0 and has_head
                       else "head_first" if first_blk == 0 else "partial_offset")
        ep["probe_only"] = ep["blocks"] < FILE_READ_MIN_BLOCKS
        ep["t_start_s"] = round(ep["t_start_s"], 3)
        ep["t_end_s"] = round(ep["t_end_s"], 3)
    return eps


def prefetch_leads(eps: list[dict], mm: MediaMap, time_label: str) -> list[dict]:
    """Next-track read relative to the start of the track playing before it."""
    real = [e for e in eps if not e["probe_only"]]
    out = []
    for prev, nxt in zip(real, real[1:]):
        if prev["node"] == nxt["node"]:
            continue
        f = mm.file(prev["node"]) or {}
        dur = (f.get("dur_ms") or 0) / 1000.0
        lag = nxt["t_start_s"] - prev["t_start_s"]
        row = {
            "playing": prev["file"], "next": nxt["file"],
            "lag_after_start_s": round(lag, 3),
            "playing_dur_s": dur or None,
            "lead_before_end_s": round(dur - lag, 3) if dur else None,
            "next_order": nxt["order"],
            "label": time_label,
        }
        out.append(row)
    return out


def selection_latency(cmds: list[dict], times: list[float], eps: list[dict]) -> list[dict]:
    out = []
    real = [e for e in eps if not e["probe_only"]]
    for i, c in enumerate(cmds):
        if c["kind"] != "marker" or c.get("name") not in SELECT_MARKERS:
            continue
        t = times[i]
        nxt = next((e for e in real if e["t_start_s"] >= t - 1e-6), None)
        if not nxt:
            continue
        out.append({"marker": c["name"], "note": c.get("note"), "t_s": round(t, 3),
                    "read_file": nxt["file"], "latency_s": round(nxt["t_start_s"] - t, 3)})
    return out


def mutation_reactions(cmds: list[dict], times: list[float], eps: list[dict],
                       mm: MediaMap) -> list[dict]:
    """Host reaction to live content changes / UNIT ATTENTION (firmware markers)."""
    out = []
    for i, c in enumerate(cmds):
        if c["kind"] != "marker" or c.get("name") not in MUTATE_MARKERS:
            continue
        t = times[i]
        after = [(times[j], cmds[j]) for j in range(i + 1, len(cmds))
                 if cmds[j]["kind"] == "cbw" and times[j] - t <= REACT_WINDOW_S]
        row = {"marker": c["name"], "note": c.get("note"), "t_s": round(t, 3),
               "cbw_in_window": len(after), "ops_after": [x["op"] for _, x in after][:16]}
        first_read = next(((tj, x) for tj, x in after
                           if x["op"] == "read10" and x.get("lba") is not None), None)
        if first_read:
            row["first_read_after_s"] = round(first_read[0] - t, 3)
            row["first_read_region"] = mm.region(int(first_read[1]["lba"]))
        row["head_reads"] = [{"file": e["file"], "after_s": round(e["t_start_s"] - t, 3)}
                             for e in eps
                             if e["first_blk"] == 0 and t - 1e-6 <= e["t_start_s"] <= t + REACT_WINDOW_S][:8]
        out.append(row)
    return out


def first_data_us(events: list[dict]) -> dict:
    """bot_tag -> capture time (µs) of its first bot_data (end of a BUSY hold)."""
    out: dict = {}
    for e in events:
        if e.get("ev") != "bot_data" or e.get("bot_tag") is None:
            continue
        t = _raw_time_us(e)
        if t is not None and e["bot_tag"] not in out:
            out[e["bot_tag"]] = t
    return out


def read_stalls(cmds: list[dict], times: list[float], events: list[dict], mm: MediaMap,
                time_source: str) -> dict:
    """Held READ10 commands (SEQ-STALL) and what the host did around them.

    hold_s = CBW -> first DATA of the same bot_tag; needs capture/emit time stamps
    (not the TUR grid). A held command without DATA before the next CBW counts as
    abandoned: the host gave up (timeout) or reset.
    """
    if time_source not in ("t_us", "capture_t_us"):
        return {"available": False, "reason": f"time_base={time_source}"}
    data_us = first_data_us(events)
    raw = [_raw_time_us(c) for c in cmds]
    rows = []
    for i, c in enumerate(cmds):
        if c["kind"] != "cbw" or c["op"] != "read10" or raw[i] is None:
            continue
        nxt = next((j for j in range(i + 1, len(cmds)) if cmds[j]["kind"] in ("cbw", "usb")), None)
        t_next = raw[nxt] if nxt is not None else None
        d = data_us.get(c.get("bot_tag"))
        served = d is not None and d >= raw[i] and (t_next is None or d <= t_next)
        hold_us = (d - raw[i]) if served else ((t_next - raw[i]) if t_next is not None else None)
        if hold_us is None or hold_us < STALL_MIN_HOLD_US:
            continue
        lba = c.get("lba")
        node, _ = mm.locate(int(lba)) if lba is not None else (None, None)
        after = [cmds[j] for j in range(i + 1, len(cmds))
                 if cmds[j]["kind"] in ("cbw", "usb") and times[j] - times[i] <= REACT_WINDOW_S + hold_us / 1e6]
        first_tur = next((times[j] - times[i] for j in range(i + 1, len(cmds))
                          if cmds[j]["op"] == "tur"), None)
        rows.append({
            "t_s": round(times[i], 3), "lba": lba, "blocks": c.get("blocks"),
            "region": mm.region(int(lba)) if lba is not None else None,
            "file": (mm.file(node) or {}).get("path"),
            "hold_s": round(hold_us / 1e6, 3),
            "outcome": "served" if served else "abandoned",
            "next_ops": [x["op"] for x in after][:12],
            "reset_or_remount": any(x["op"] in ("mount", "umount", "msc_bot_reset", "set_address")
                                    for x in after),
            "first_tur_after_s": round(first_tur, 3) if first_tur is not None else None,
        })
    served = [r["hold_s"] for r in rows if r["outcome"] == "served"]
    abandoned = [r["hold_s"] for r in rows if r["outcome"] == "abandoned"]
    return {
        "available": True,
        "held": rows,
        # Per-command timeout lies between the longest tolerated and the shortest abandoned hold.
        "timeout_lower_s": max(served) if served else None,
        "timeout_upper_s": min(abandoned) if abandoned else None,
        "menu_latency_s": operator_latency(cmds, times, "menu_open", "menu_reaction"),
    }


def operator_latency(cmds: list[dict], times: list[float], start: str, end: str) -> list[float]:
    out, t0 = [], None
    for i, c in enumerate(cmds):
        if c["kind"] != "marker":
            continue
        if c.get("name") == start:
            t0 = times[i]
        elif c.get("name") == end and t0 is not None:
            out.append(round(times[i] - t0, 3))
            t0 = None
    return out


def is_checkpoint(name: Any) -> bool:
    return isinstance(name, str) and name.startswith("ckpt:")


def marker_times(cmds: list[dict], times: list[float]) -> list[dict]:
    return [{"name": c.get("name"), "note": c.get("note"), "t_s": round(times[i], 3),
             "auto": c.get("name") in AUTO_MARKERS or is_checkpoint(c.get("name")),
             "origin": c.get("origin")}
            for i, c in enumerate(cmds) if c["kind"] == "marker"]


def flash_checkpoints(cmds: list[dict], times: list[float], health: dict) -> dict:
    """Drive mode: `ckpt:<ms>:<KiB>` markers = flash journal commits (FW 0.3.0)."""
    rows = []
    for i, c in enumerate(cmds):
        if c["kind"] != "marker" or not is_checkpoint(c.get("name")):
            continue
        parts = c["name"].split(":")
        try:
            ms, kib = int(parts[1]), int(parts[2])
        except (IndexError, ValueError):
            ms, kib = None, None
        rows.append({"t_s": round(times[i], 3), "write_ms": ms, "persisted_kib": kib})
    trip = {k: health.get(k) for k in
            ("trip_id", "persisted", "checkpoints", "persist_full", "stored_replay", "capture_fw",
             "media_variant", "end_reason") if k in health}
    if not rows and not trip.get("persisted"):
        return {"available": False}
    gaps = [round(b["t_s"] - a["t_s"], 3) for a, b in zip(rows, rows[1:])]
    writes = [r["write_ms"] for r in rows if r["write_ms"] is not None]
    return {"available": True, "trip": trip, "count": len(rows), "rows": rows,
            "gap_s": {"max": max(gaps) if gaps else None,
                      "p50": pctl(gaps, 0.5) if gaps else None},
            "write_ms_max": max(writes) if writes else None,
            # Power loss keeps everything up to the last checkpoint; the tail after it is lost.
            "power_loss": health.get("end_reason") == "power_loss"}


def media_loss(cmds: list[dict], times: list[float], time_source: str,
               period_s: float | None) -> dict:
    """Host reaction after `media_hide`: detection latency (next TUR tick) and burst shape."""
    post = [c for c in cmds if c.get("origin") in ("post_finalize", "post_finalize_buffered")]
    res: dict[str, Any] = {
        "post_finalize_cmds": sum(1 for c in post if c["kind"] == "cbw"),
        "ops": [c["op"] for c in post if c["kind"] == "cbw"][:24],
    }
    if time_source not in ("t_us", "capture_t_us"):
        return res
    hide = next((i for i, c in enumerate(cmds) if c["kind"] == "marker" and c.get("name") == "media_hide"), None)
    if hide is None:
        return res
    t_hide = times[hide]
    after = [(times[j], cmds[j]) for j in range(hide + 1, len(cmds)) if cmds[j]["kind"] == "cbw"]
    turs = [t for t, c in after if c["op"] == "tur"]
    if not turs:
        return res
    last_before = max((times[j] for j in range(hide) if cmds[j]["op"] == "tur"), default=None)
    burst = [turs[0]]
    for t in turs[1:]:
        if t - burst[-1] > 0.5:
            break
        burst.append(t)
    t_end = burst[-1]
    in_burst = [c["op"] for t, c in after if t <= t_end + 0.01]
    res.update({
        "detect_latency_s": round(turs[0] - t_hide, 4),
        "first_tur_on_grid": (period_s is not None and last_before is not None
                              and abs(turs[0] - last_before - period_s) <= TUR_GRID_TOL_S),
        "burst_tur": len(burst),
        "burst_request_sense": in_burst.count("request_sense"),
        "burst_prevent_allow": in_burst.count("prevent_allow"),
        "burst_duration_ms": round((t_end - burst[0]) * 1000, 1),
        "burst_tur_intervals_ms": [round((b - a) * 1000, 1) for a, b in zip(burst, burst[1:])],
    })
    nxt = next((t for t in turs if t > t_end + 0.5), None)
    if nxt is not None and period_s:
        res["next_tur_after_first_burst_tur_s"] = round(nxt - burst[0], 4)
        res["grid_survives_burst"] = abs(nxt - burst[0] - period_s) <= TUR_GRID_TOL_S
    return res


def tur_phase(cmds: list[dict], times: list[float], time_source: str, period_hint_s: float) -> dict:
    """Free-running TUR timer: precise period and whether reads/bursts shift its phase.

    A TUR is on-grid when it lands k periods (k >= 1) after the previous on-grid TUR within
    TUR_GRID_TOL_S. Spans containing other commands report their residual: ~0 means the
    timer kept running independently of those commands.
    """
    if time_source not in ("t_us", "capture_t_us"):
        return {"available": False, "reason": f"time_base={time_source}"}
    tt = [(times[i], i) for i, c in enumerate(cmds) if c["op"] == "tur"]
    single = [b[0] - a[0] for a, b in zip(tt, tt[1:])
              if abs((b[0] - a[0]) - period_hint_s) <= 0.05]
    if len(single) < 3:
        return {"available": False, "reason": "too_few_idle_turs"}
    period = statistics.median(single)
    grid: list[tuple[float, int]] = []
    for t, i in tt:
        if not grid:
            grid.append((t, i))
            continue
        dt = t - grid[-1][0]
        k = round(dt / period)
        if k >= 1 and abs(dt - k * period) <= TUR_GRID_TOL_S:
            grid.append((t, i))
        elif dt > period * 1.5:
            grid.append((t, i))  # new grid segment (e.g. after re-enumeration)
    chains, spans = [], []
    for (ta, ia), (tb, ib) in zip(grid, grid[1:]):
        dt = tb - ta
        k = round(dt / period)
        resid = dt - k * period
        if k < 1 or abs(resid) > TUR_GRID_TOL_S:
            continue
        chains.append((dt, k))
        between = [cmds[j]["op"] for j in range(ia + 1, ib) if cmds[j]["kind"] == "cbw"]
        if any(op != "tur" for op in between):
            spans.append({"t_s": round(ta, 3), "k": k, "residual_ms": round(resid * 1000, 2),
                          "ops": sorted(set(op for op in between if op != "tur"))})
    if chains:
        period = sum(dt for dt, _ in chains) / sum(k for _, k in chains)
    resid_all = [abs(s["residual_ms"]) for s in spans]
    return {
        "available": True,
        "period_ms": round(period * 1000, 2),
        "grid_turs": len(grid),
        "spans_with_other_cmds": spans,
        "max_abs_residual_ms_across_cmds": round(max(resid_all), 2) if resid_all else None,
        "label": "MEASURED",
    }


def _usb_events(events: list[dict]) -> list[tuple[int, dict]]:
    out = []
    for e in events:
        if e.get("ev") not in ("usb", "marker", "bot_cbw"):
            continue
        t = e.get("capture_t_us")
        if isinstance(t, (int, float)):
            out.append((int(t), e))
    out.sort(key=lambda x: x[0])
    return out


def plug_timeline(events: list[dict], mm: MediaMap) -> list[dict]:
    """Per enumeration: set_address -> SET_CONFIG -> GET_MAX_LUN -> mount_done -> first file read."""
    ev = _usb_events(events)
    rows = []
    for idx, (t_mount, e) in enumerate(ev):
        if not (e.get("ev") == "usb" and e.get("usb_ev") == "mount"):
            continue
        t_addr = next((t for t, x in reversed(ev[:idx])
                       if x.get("usb_ev") == "set_address" and t_mount - t <= 1_000_000), None)
        later = ev[idx + 1:]
        nxt_mount = next((t for t, x in later if x.get("ev") == "usb" and x.get("usb_ev") == "mount"), None)
        later = [(t, x) for t, x in later if nxt_mount is None or t < nxt_mount]
        t_lun = next((t for t, x in later if x.get("ev") == "usb" and x.get("usb_ev") == "control"
                      and (x.get("bRequest") == 254 or x.get("note") == "msc_get_max_lun")), None)
        t_done = next((t for t, x in later if x.get("ev") == "marker" and x.get("name") == "mount_done"), None)
        t_read = None
        for t, x in later:
            scsi = x.get("scsi") or {}
            if x.get("ev") == "bot_cbw" and scsi.get("opcode") == "read10" \
                    and int(scsi.get("blocks") or 0) >= FILE_READ_MIN_BLOCKS \
                    and mm.locate(int(scsi.get("lba") or 0))[0] is not None:
                t_read = t
                break

        def ms(a: int | None, b: int | None) -> float | None:
            return round((b - a) / 1000, 1) if a is not None and b is not None else None

        rows.append({
            "t_mount_s": round(t_mount / 1e6, 3),
            "set_address_to_config_ms": ms(t_addr, t_mount),
            "config_to_max_lun_ms": ms(t_mount, t_lun),
            "max_lun_to_mount_done_ms": ms(t_lun, t_done),
            "mount_done_to_first_read_ms": ms(t_done, t_read),
            "set_address_to_first_read_s": round((t_read - t_addr) / 1e6, 4)
            if t_addr is not None and t_read is not None else None,
        })
    return rows


def cbw_data(events: list[dict]) -> list[tuple[dict, list[tuple[int, dict]]]]:
    """Each timed bot_cbw with all bot_data records of its tag up to the next CBW of that tag.

    One record per CBW (coalesced) or one per 4 KiB chunk (fine_data) both land here.
    """
    # bot_tag restarts after every enumeration, so the next CBW with the same tag bounds the window.
    data: dict[Any, list[tuple[int, dict]]] = {}
    cbw_t: dict[Any, list[int]] = {}
    for e in events:
        t = e.get("capture_t_us")
        if e.get("bot_tag") is None or not isinstance(t, (int, float)):
            continue
        if e.get("ev") == "bot_data":
            data.setdefault(e["bot_tag"], []).append((int(t), e))
        elif e.get("ev") == "bot_cbw":
            cbw_t.setdefault(e["bot_tag"], []).append(int(t))
    for lst in data.values():
        lst.sort(key=lambda x: x[0])
    for lst in cbw_t.values():
        lst.sort()
    out = []
    for e in events:
        t = e.get("capture_t_us")
        if e.get("ev") != "bot_cbw" or e.get("bot_tag") is None or not isinstance(t, (int, float)):
            continue
        tag, t = e["bot_tag"], int(t)
        starts = cbw_t[tag]
        i = bisect_right(starts, t)
        until = starts[i] if i < len(starts) else None
        out.append((e, [(td, d) for td, d in data.get(tag, [])
                        if td >= t and (until is None or td < until)]))
    out.sort(key=lambda x: int(x[0]["capture_t_us"]))
    return out


def _chunk_end(td: int, d: dict) -> int | None:
    dur = d.get("dur_us")
    return td + int(dur) if isinstance(dur, (int, float)) else None


def file_cbws(cd: list[tuple[dict, list[tuple[int, dict]]]], mm: MediaMap) -> list[dict]:
    """File READ10 CBWs (>= FILE_READ_MIN_BLOCKS): start = first DATA, end = last DATA + dur_us."""
    out = []
    for e, chunks in cd:
        scsi = e.get("scsi") or {}
        blocks = int(scsi.get("blocks") or 0)
        if scsi.get("opcode") != "read10" or blocks < FILE_READ_MIN_BLOCKS:
            continue
        lba = int(scsi.get("lba") or 0)
        node, blk = mm.locate(lba)
        if node is None:
            continue
        first = chunks[0] if chunks else None
        end = _chunk_end(*chunks[-1]) if chunks else None
        out.append({"t_us": int(e["capture_t_us"]), "lba": lba, "blocks": blocks, "node": node,
                    "file_blk": blk, "gap_us": first[1].get("gap_us") if first else None,
                    "dur_us": end - first[0] if first and end is not None else None,
                    "end_us": end, "data_records": len(chunks)})
    return out


def chunk_pacing(cd: list[tuple[dict, list[tuple[int, dict]]]], mm: MediaMap,
                 fine: bool | None) -> dict:
    """Per-chunk timing inside READ10 data phases; needs one record per chunk (fine_data)."""
    multi = [(e, ch) for e, ch in cd
             if len(ch) >= 2 and (e.get("scsi") or {}).get("opcode") == "read10"]
    if not multi:
        return {"available": False, "fine_data": fine,
                "reason": "no CBW with more than one DATA record (coalesced capture)"}
    per_cbw, starts, pauses = [], [], []
    for e, ch in multi:
        per_cbw.append(len(ch))
        for (ta, da), (tb, db) in zip(ch, ch[1:]):
            starts.append((tb - ta) / 1000)
            end_a = _chunk_end(ta, da)
            if end_a is not None and tb - end_a > CHUNK_PAUSE_US:
                scsi = e.get("scsi") or {}
                lba = db.get("lba") if db.get("lba") is not None else scsi.get("lba")
                pauses.append({"t_s": round(tb / 1e6, 6), "pause_ms": round((tb - end_a) / 1000, 2),
                               "cbw_lba": scsi.get("lba"), "cbw_blocks": scsi.get("blocks"),
                               "region": mm.region(int(lba)) if lba is not None else None})
    pauses.sort(key=lambda p: -p["pause_ms"])

    def r(v: float | None) -> float | None:
        return round(v, 3) if v is not None else None

    return {"available": True, "fine_data": fine, "cbws": len(multi),
            "chunks_per_cbw": {"p50": r(pctl(per_cbw, 0.5)), "max": max(per_cbw)},
            "interval_ms": {"n": len(starts), "p50": r(pctl(starts, 0.5)),
                            "p95": r(pctl(starts, 0.95)), "max": r(max(starts))},
            "pause_threshold_ms": CHUNK_PAUSE_US / 1000, "pauses": len(pauses),
            "longest_pauses": pauses[:20]}


def atoms(fc: list[dict], mm: MediaMap, track_dur_s: float | None) -> dict:
    """Group file CBWs into per-file atoms; timing between CBWs and between atoms."""
    groups: list[list[dict]] = []
    for r in fc:
        g = groups[-1] if groups else None
        if g and g[-1]["node"] == r["node"] and g[-1]["end_us"] is not None \
                and r["t_us"] - g[-1]["end_us"] <= EPISODE_GAP_S * 1e6:
            g.append(r)
        else:
            groups.append([r])
    rows = []
    for g in groups:
        f = mm.file(g[0]["node"]) or {}
        blocks = sum(r["blocks"] for r in g)
        ends = [r["end_us"] for r in g if r["end_us"] is not None]
        rows.append({
            "file": f.get("path"),
            "t_start_s": round(g[0]["t_us"] / 1e6, 6),
            "order": "tail_then_head" if g[0]["file_blk"] > 0 and any(r["file_blk"] == 0 for r in g)
            else "head_first" if g[0]["file_blk"] == 0 else "partial_offset",
            "cbws": [f"{r['file_blk']}/{r['blocks']}" for r in g],
            "complete": blocks >= (mm.file_blocks(g[0]["node"]) or blocks + 1),
            "start_to_start_ms": [round((b["t_us"] - a["t_us"]) / 1000, 2) for a, b in zip(g, g[1:])],
            "gap_us": [r["gap_us"] for r in g[1:]],
            "dur_ms": [round(r["dur_us"] / 1000, 2) for r in g if r["dur_us"] is not None],
            "total_ms": round((max(ends) - g[0]["t_us"]) / 1000, 2) if ends else None,
            "_start_us": g[0]["t_us"], "_end_us": max(ends) if ends else None,
        })
    pairs = []
    for a, b in zip(rows, rows[1:]):
        if a["file"] == b["file"]:
            continue
        s2s = (b["_start_us"] - a["_start_us"]) / 1e6
        p = {"from": a["file"], "to": b["file"], "to_order": b["order"],
             "start_to_start_s": round(s2s, 6),
             "atom_end_to_next_read_s": round((b["_start_us"] - a["_end_us"]) / 1e6, 6)
             if a["_end_us"] is not None else None}
        if track_dur_s:
            p["start_to_start_minus_dur_ms"] = round((s2s - track_dur_s) * 1000, 1)
        pairs.append(p)
    for r in rows:
        r.pop("_start_us")
        r.pop("_end_us")
    return {"atoms": rows, "transitions": pairs, "track_dur_s": track_dur_s}


def greedy_split(total: int) -> list[int]:
    """CBW sequence predicted by the 512-block window model for a read of `total` blocks."""
    seq = []
    while total > 0:
        win = min(WINDOW_BLOCKS, total)
        total -= win
        while win > 0:
            seq.append(min(CBW_MAX_BLOCKS, win))
            win -= seq[-1]
    return seq


def read_windows(fc: list[dict], mm: MediaMap, kbps: float | None) -> list[dict]:
    """Per file: contiguous read runs, their CBW sequence vs. the window model, refill cadence."""
    out = []
    for node in sorted({r["node"] for r in fc}):
        rs = [r for r in fc if r["node"] == node]
        runs: list[list[dict]] = []
        for r in rs:
            g = runs[-1] if runs else None
            if g and g[-1]["end_us"] is not None and r["t_us"] - g[-1]["end_us"] <= WINDOW_GAP_S * 1e6:
                g.append(r)
            else:
                runs.append([r])
        f = mm.file(node) or {}
        file_kbps = f.get("kbps") or kbps
        windows = []
        for g in runs:
            seq = [r["blocks"] for r in sorted(g, key=lambda r: r["file_blk"])]
            first = min(r["file_blk"] for r in g)
            w = {"t_start_s": round(g[0]["t_us"] / 1e6, 3), "first_blk": first, "cbw_blocks": seq,
                 "sum_blocks": sum(seq), "matches_window_model": seq == greedy_split(sum(seq))}
            windows.append(w)
        if file_kbps and windows:
            t0 = windows[0]["t_start_s"]
            bytes_per_s = float(file_kbps) * 125.0
            for w in windows:
                read_to = (w["first_blk"] + w["sum_blocks"]) * 512
                w["read_ahead_s_if_playing_from_first"] = round(read_to / bytes_per_s - (w["t_start_s"] - t0), 2)
        out.append({
            "file": f.get("path"), "file_blocks": mm.file_blocks(node), "kbps": file_kbps,
            "windows": windows,
            "refill_intervals_s": [round(b["t_start_s"] - a["t_start_s"], 3) for a, b in zip(windows, windows[1:])],
        })
    return out


# ---------------------------------------------------------------- analysis


def analyze(path: Path, tur_period_s: float, track_dur_s: float | None = None,
            kbps: float | None = None) -> dict:
    folder, meta, events = load_session(path)
    mm = media_map(meta, events)
    cmds = extract_cmds(events)
    if cmds and all(isinstance(c.get("capture_t_us"), (int, float)) for c in cmds):
        # Offline replay and post-finalize live events interleave in seq; capture time is truth.
        cmds.sort(key=lambda c: (c["capture_t_us"], c.get("seq") or 0))
    times, time_source = build_time_base(cmds, tur_period_s)
    label = "MEASURED" if time_source in ("t_us", "capture_t_us") else "INFERRED"
    eps = read_episodes(cmds, times, mm)
    start = next((e for e in events if e.get("ev") == "session" and e.get("action") == "start"), {})
    health = [e for e in events if e.get("ev") == "capture_health"]
    last_health = health[-1] if health else {}
    tail_cases = [e for e in eps if not e["probe_only"] and e["order"] == "tail_then_head"]
    real_eps = [e for e in eps if not e["probe_only"]]
    real_time = time_source in ("t_us", "capture_t_us") and any(
        isinstance(e.get("capture_t_us"), (int, float)) for e in events)
    phase = tur_phase(cmds, times, time_source, tur_period_s)
    period_s = phase["period_ms"] / 1000 if phase.get("available") else None
    if track_dur_s is None and mm.variant == "demo_v1":
        track_dur_s = DEMO_V1_FILES[0]["dur_ms"] / 1000
    cd = cbw_data(events) if real_time else []
    fc = file_cbws(cd, mm)
    fine = last_health.get("fine_data", meta.get("fine_data"))
    return {
        "schema": SCHEMA,
        "session": folder.name,
        "vehicle": meta.get("vehicle") or start.get("vehicle"),
        "scenario": meta.get("scenario") or start.get("scenario"),
        "fw_version": meta.get("fw_version") or start.get("fw_version"),
        "fw_sha": meta.get("fw_sha") or start.get("fw_sha"),
        "fw_dirty": meta.get("fw_dirty", start.get("fw_dirty")),
        "transport": start.get("transport"),
        "media": {"variant": mm.variant, "source": mm.source,
                  "files": [{"node": f.get("node"), "path": f.get("path"),
                             "dur_ms": f.get("dur_ms"), "size": f.get("size"),
                             "extents": f.get("extents")}
                            for f in mm.files if not f.get("dir")]},
        "time_base": {"source": time_source, "label": label,
                      "tur_period_s": tur_period_s if time_source == "tur_grid" else None},
        "counts": {"cbw": sum(1 for c in cmds if c["kind"] == "cbw"),
                   "read10": sum(1 for c in cmds if c["op"] == "read10"),
                   "markers": sum(1 for c in cmds if c["kind"] == "marker")},
        "tur": tur_grid(cmds, times, time_source),
        "probe_tur_stats": {k: last_health[k] for k in last_health if k.startswith("tur_")},
        "mount_signatures": mount_signatures(cmds, times),
        "episodes": eps,
        "prefetch": prefetch_leads(eps, mm, label),
        "selection_latency": selection_latency(cmds, times, eps),
        "mutation_reactions": mutation_reactions(cmds, times, eps, mm),
        "read_stalls": read_stalls(cmds, times, events, mm, time_source),
        "tail_then_head": {"cases": len(tail_cases), "of": len(real_eps),
                           "files": [e["file"] for e in tail_cases]},
        "markers": marker_times(cmds, times),
        "flash_checkpoints": flash_checkpoints(cmds, times, last_health),
        "media_loss": media_loss(cmds, times, time_source, period_s),
        "tur_phase": phase,
        "plug_timeline": plug_timeline(events, mm) if real_time else [],
        "atoms": atoms(fc, mm, track_dur_s) if real_time else {"available": False},
        "read_windows": read_windows(fc, mm, kbps) if real_time else [],
        "chunk_pacing": chunk_pacing(cd, mm, fine) if real_time
        else {"available": False, "fine_data": fine, "reason": "no capture time base"},
        "integrity": {k: last_health.get(k) for k in
                      ("complete", "integrity_pass", "offline_overflow", "dropped",
                       "offline_post_finalize_records", "replay_clock") if k in last_health},
        # Replay format for PiDrive: one row per SCSI command on the chosen time base.
        "replay": [
            {"t_s": round(times[i], 4), "op": c["op"], "lba": c.get("lba"), "blocks": c.get("blocks")}
            for i, c in enumerate(cmds)
            if c["kind"] == "cbw" and c.get("origin") not in ("post_finalize",)
        ],
    }


def to_markdown(a: dict) -> str:
    L = [f"# Analyse {a['session']}", ""]
    L.append(f"- Fahrzeug/Szenario: {a['vehicle']} / {a['scenario']} · FW {a['fw_version']}"
             f" ({a['fw_sha']}{', dirty' if a.get('fw_dirty') else ''})")
    L.append(f"- Medium: `{a['media']['variant']}` (Quelle {a['media']['source']})")
    tb = a["time_base"]
    L.append(f"- Zeitbasis: {tb['source']} → Werte {tb['label']}"
             + (f" (TUR-Raster {tb['tur_period_s']} s, Fehler ≤ 1 Periode)" if tb["tur_period_s"] else ""))
    t = a["tur"]
    L.append(f"- TUR: {t['tur_count']} gesamt, {t['burst_intervals']} Burst-Intervalle"
             + (f", Leerlauf Median {t['idle_median_s']} s (p10 {t['idle_p10_s']}, p90 {t['idle_p90_s']}, n={t['idle_n']})"
                if "idle_median_s" in t else ""))
    L.append("")
    L.append("## Mount-Signatur")
    for s in a["mount_signatures"]:
        flag = "= Referenz" if s["matches_reference"] else ("Variante 3/1+4/4" if s["variant_3_1_4_4"] else "abweichend")
        L.append(f"- t={s['t_s']} s: `{s['reads']}` ({flag}), TUR bis Signaturende: {s['turs_until_signature_end']}")
    L.append("")
    L.append("## Read-Episoden")
    L.append("")
    L.append("| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |")
    L.append("|---:|---|---:|---:|---|")
    for e in a["episodes"]:
        if e["probe_only"]:
            continue
        L.append(f"| {e['t_start_s']} | {e['file']} | {e['blocks']} | {e['coverage']} | {e['order']} |")
    L.append("")
    L.append("## Folgetrack-Reads")
    for p in a["prefetch"]:
        lead = f", Vorlauf vor Ende {p['lead_before_end_s']} s" if p["lead_before_end_s"] is not None else ""
        L.append(f"- {p['playing']} → {p['next']}: {p['lag_after_start_s']} s nach Start{lead} ({p['label']}, {p['next_order']})")
    if not a["prefetch"]:
        L.append("- keine")
    L.append("")
    L.append("## Auswahllatenz")
    for s in a["selection_latency"]:
        L.append(f"- {s['marker']} @ {s['t_s']} s → {s['read_file']} nach {s['latency_s']} s")
    if not a["selection_latency"]:
        L.append("- keine Marker mit folgendem Read")
    L.append("")
    if a.get("mutation_reactions"):
        L.append(f"## Live-Änderung / UNIT ATTENTION (Fenster {REACT_WINDOW_S:.0f} s)")
        for r in a["mutation_reactions"]:
            fr = (f", erster READ10 nach {r['first_read_after_s']} s ({r['first_read_region']})"
                  if "first_read_after_s" in r else ", kein READ10")
            heads = ", ".join(f"{h['file']}+{h['after_s']}s" for h in r["head_reads"]) or "keine"
            L.append(f"- {r['marker']} @ {r['t_s']} s ({r.get('note') or ''}): {r['cbw_in_window']} CBW"
                     f"{fr}; Kopf-Reads: {heads}; Ops: {r['ops_after']}")
        L.append("")
    rs = a.get("read_stalls") or {}
    if rs.get("held") or rs.get("menu_latency_s"):
        L.append("## Gebremste READ10 (BUSY)")
        L.append("")
        L.append("| t (s) | Datei/Region | Blöcke | gehalten (s) | Ergebnis | erste TUR danach (s) | Reset/Remount | Ops danach |")
        L.append("|---:|---|---:|---:|---|---:|---|---|")
        for r in rs.get("held", []):
            L.append(f"| {r['t_s']} | {r['file'] or r['region']} | {r['blocks']} | {r['hold_s']} | {r['outcome']} | "
                     f"{r['first_tur_after_s']} | {'ja' if r['reset_or_remount'] else 'nein'} | {' '.join(r['next_ops'])} |")
        L.append("")
        L.append(f"Timeout pro READ10: > {rs.get('timeout_lower_s')} s (längste tolerierte Haltezeit), "
                 f"≤ {rs.get('timeout_upper_s')} s (kürzeste abgebrochene). `None` = in dieser Session nicht erreicht.")
        if rs.get("menu_latency_s"):
            L.append(f"Menülatenz (menu_open → menu_reaction, Bediener-Marker): {rs['menu_latency_s']} s")
        L.append("")
    elif rs.get("available") is False:
        L.append(f"Gebremste READ10: nicht auswertbar ({rs.get('reason')}).")
        L.append("")
    tth = a["tail_then_head"]
    L.append(f"tail_then_head: {tth['cases']} von {tth['of']} Episoden {tth['files']}")
    ml = a["media_loss"]
    if ml["post_finalize_cmds"]:
        L.append(f"Media-Loss-Reaktion (post_finalize): {ml['post_finalize_cmds']} Kommandos, {ml['ops']}")
    if "detect_latency_s" in ml:
        L.append(f"Media-Loss: erkannt {ml['detect_latency_s']} s nach media_hide"
                 f" (erste TUR auf Raster: {ml['first_tur_on_grid']}); Burst {ml['burst_tur']} TUR /"
                 f" {ml['burst_request_sense']} REQUEST_SENSE / {ml['burst_prevent_allow']} PREVENT_ALLOW"
                 f" in {ml['burst_duration_ms']} ms, Abstände {ml['burst_tur_intervals_ms']} ms"
                 + (f"; Raster läuft weiter: {ml['grid_survives_burst']}" if "grid_survives_burst" in ml else ""))
    L.append("")
    ph = a.get("tur_phase") or {}
    if ph.get("available"):
        L.append("## TUR-Phase")
        L.append(f"- Periode {ph['period_ms']} ms aus {ph['grid_turs']} Raster-TURs; max. Abweichung über"
                 f" Spannen mit anderen Kommandos: {ph['max_abs_residual_ms_across_cmds']} ms")
        for s in ph["spans_with_other_cmds"]:
            L.append(f"  - t={s['t_s']} s, k={s['k']}, Rest {s['residual_ms']} ms, Ops {s['ops']}")
        L.append("")
    if a.get("plug_timeline"):
        L.append("## Plug-Zeitkette")
        L.append("")
        L.append("| Mount (s) | set_address→config (ms) | config→GET_MAX_LUN (ms) | MAX_LUN→mount_done (ms) | mount_done→1. Read (ms) | set_address→1. Read (s) |")
        L.append("|---:|---:|---:|---:|---:|---:|")
        for p in a["plug_timeline"]:
            L.append(f"| {p['t_mount_s']} | {p['set_address_to_config_ms']} | {p['config_to_max_lun_ms']} | "
                     f"{p['max_lun_to_mount_done_ms']} | {p['mount_done_to_first_read_ms']} | {p['set_address_to_first_read_s']} |")
        L.append("")
    at = a.get("atoms") or {}
    if at.get("atoms"):
        L.append("## Datei-Atome")
        L.append("")
        L.append("| t Start (s) | Datei | Reihenfolge | CBWs (Block/Anzahl) | Start→Start (ms) | gap_us | Gesamt (ms) | vollständig |")
        L.append("|---:|---|---|---|---|---|---:|---|")
        for r in at["atoms"]:
            L.append(f"| {r['t_start_s']} | {r['file']} | {r['order']} | {' '.join(r['cbws'])} | "
                     f"{r['start_to_start_ms']} | {r['gap_us']} | {r['total_ms']} | {r['complete']} |")
        L.append("")
        for p in at["transitions"]:
            extra = (f", minus Dauer {at['track_dur_s']} s = {p['start_to_start_minus_dur_ms']} ms"
                     if "start_to_start_minus_dur_ms" in p else "")
            L.append(f"- {p['from']} → {p['to']} ({p['to_order']}): Start→Start {p['start_to_start_s']} s,"
                     f" Atom-Ende→nächster Read {p['atom_end_to_next_read_s']} s{extra}")
        L.append("")
    if a.get("read_windows"):
        L.append(f"## Lesefenster (Modell: {WINDOW_BLOCKS} Blöcke, CBW ≤ {CBW_MAX_BLOCKS})")
        for w in a["read_windows"]:
            seqs = "; ".join(f"t={x['t_start_s']} ab Block {x['first_blk']}: {x['cbw_blocks']}"
                             f"{'' if x['matches_window_model'] else ' (weicht ab)'}"
                             + (f", Vorlauf {x['read_ahead_s_if_playing_from_first']} s"
                                if "read_ahead_s_if_playing_from_first" in x else "")
                             for x in w["windows"])
            L.append(f"- {w['file']} ({w['file_blocks']} Blöcke, {w['kbps']} kbit/s): {seqs};"
                     f" Abstände {w['refill_intervals_s']} s")
        L.append("")
    cp = a.get("chunk_pacing") or {}
    if cp.get("available"):
        iv, pc = cp["interval_ms"], cp["chunks_per_cbw"]
        L.append(f"## Chunk-Takt (4 KiB, fine_data={cp['fine_data']})")
        L.append(f"- {cp['cbws']} READ10 mit ≥ 2 DATA-Records; Chunks pro CBW p50 {pc['p50']}, max {pc['max']}")
        L.append(f"- Start→Start zwischen Chunks: p50 {iv['p50']} ms, p95 {iv['p95']} ms, max {iv['max']} ms (n={iv['n']})")
        L.append(f"- Pausen > {cp['pause_threshold_ms']:.0f} ms: {cp['pauses']}")
        for p in cp["longest_pauses"]:
            L.append(f"  - t={p['t_s']} s: {p['pause_ms']} ms (CBW LBA {p['cbw_lba']}/{p['cbw_blocks']}, {p['region']})")
        L.append("")
    elif cp.get("fine_data"):
        L.append(f"Chunk-Takt: nicht auswertbar ({cp.get('reason')}).")
        L.append("")
    fk = a.get("flash_checkpoints") or {}
    if fk.get("available"):
        tr = fk["trip"]
        L.append("## Flash-Checkpoints (Heimfahrt)")
        L.append(f"- Fahrt #{tr.get('trip_id')} · Ende {tr.get('end_reason')} · aus Flash nachgeladen: "
                 f"{'ja' if tr.get('stored_replay') else 'nein'} · FW bei Aufnahme {tr.get('capture_fw')}"
                 f" · Medium {tr.get('media_variant')}"
                 + (" · Partition VOLL" if tr.get("persist_full") else ""))
        g = fk["gap_s"]
        L.append(f"- {fk['count']} Checkpoints, Abstand p50 {g['p50']} s, max {g['max']} s; "
                 f"längster Schreibvorgang {fk['write_ms_max']} ms")
        if fk["power_loss"]:
            L.append("- Stromverlust: Daten nach dem letzten Checkpoint fehlen (Ende der Aufnahme ≠ Ende der Fahrt).")
        L.append("")
    L.append("Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.")
    return "\n".join(L) + "\n"


def compare(analyses: list[dict]) -> dict:
    rows = []
    for a in analyses:
        leads = [p["lead_before_end_s"] for p in a["prefetch"] if p["lead_before_end_s"] is not None]
        lags = [p["lag_after_start_s"] for p in a["prefetch"]]
        lat = [s["latency_s"] for s in a["selection_latency"]]
        rows.append({
            "session": a["session"], "vehicle": a["vehicle"], "media": a["media"]["variant"],
            "time_base": a["time_base"]["source"],
            "tur_idle_median_s": a["tur"].get("idle_median_s"),
            "mount_ref_matches": sum(1 for s in a["mount_signatures"] if s["matches_reference"]),
            "mounts": len(a["mount_signatures"]),
            "next_track_reads": len(a["prefetch"]),
            "lag_after_start_s": [round(x, 2) for x in lags],
            "lead_before_end_s": [round(x, 2) for x in leads],
            "selection_latency_s": [round(x, 2) for x in lat],
            "tail_then_head": f"{a['tail_then_head']['cases']}/{a['tail_then_head']['of']}",
            "tur_period_ms": (a.get("tur_phase") or {}).get("period_ms"),
            "plug_to_first_read_s": [p["set_address_to_first_read_s"] for p in a.get("plug_timeline") or []
                                     if p["set_address_to_first_read_s"] is not None],
            "atom_end_to_next_read_s": [p["atom_end_to_next_read_s"]
                                        for p in (a.get("atoms") or {}).get("transitions", [])],
            "media_loss_detect_s": (a.get("media_loss") or {}).get("detect_latency_s"),
        })
    all_idle = [r["tur_idle_median_s"] for r in rows if r["tur_idle_median_s"]]
    all_lead = [x for r in rows for x in r["lead_before_end_s"]]
    all_lag = [x for r in rows for x in r["lag_after_start_s"]]
    return {
        "schema": SCHEMA + "+compare",
        "sessions": rows,
        "summary": {
            "tur_idle_median_s": round(statistics.median(all_idle), 4) if all_idle else None,
            "lead_before_end_s": {"min": min(all_lead), "max": max(all_lead), "n": len(all_lead)} if all_lead else None,
            "lag_after_start_s": {"min": min(all_lag), "max": max(all_lag), "n": len(all_lag)} if all_lag else None,
            # Constant lag across different durations -> "delay after start"; constant lead -> "lead before end".
            "lead_spread_s": round(max(all_lead) - min(all_lead), 2) if all_lead else None,
            "lag_spread_s": round(max(all_lag) - min(all_lag), 2) if all_lag else None,
        },
    }


def compare_markdown(c: dict) -> str:
    L = ["# Session-Vergleich", "",
         "| Session | Medium | Zeitbasis | TUR Median (s) | Mount=Ref | Lag nach Start (s) | Vorlauf vor Ende (s) | Auswahl (s) | tail_then_head |",
         "|---|---|---|---:|---|---|---|---|---|"]
    for r in c["sessions"]:
        L.append(f"| {r['session']} | {r['media']} | {r['time_base']} | {r['tur_idle_median_s']} | "
                 f"{r['mount_ref_matches']}/{r['mounts']} | {r['lag_after_start_s']} | "
                 f"{r['lead_before_end_s']} | {r['selection_latency_s']} | {r['tail_then_head']} |")
    L += ["", "| Session | TUR-Periode (ms) | Stecken→1. Read (s) | Atom-Ende→nächster Read (s) | Media-Loss erkannt (s) |",
          "|---|---:|---|---|---:|"]
    for r in c["sessions"]:
        L.append(f"| {r['session']} | {r['tur_period_ms']} | {r['plug_to_first_read_s']} | "
                 f"{r['atom_end_to_next_read_s']} | {r['media_loss_detect_s']} |")
    s = c["summary"]
    L += ["", f"- Streuung Lag nach Start: {s['lag_spread_s']} s, Streuung Vorlauf vor Ende: {s['lead_spread_s']} s.",
          "  Bei unterschiedlichen Trackdauern zeigt die kleinere Streuung die wahrscheinlichere Prefetch-Regel (INFERRED)."]
    return "\n".join(L) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sessions", nargs="+", type=Path, help="session dirs or events.jsonl files")
    ap.add_argument("--tur-period", type=float, default=TUR_PERIOD_S_DEFAULT)
    ap.add_argument("--compare", type=Path, default=None,
                    help="write cross-session comparison (JSON; .md next to it)")
    ap.add_argument("--no-write", action="store_true", help="print summary only")
    ap.add_argument("--track-dur-s", type=float, default=None,
                    help="audio duration of the tracks (start-to-start minus duration); demo_v1 default 26.462")
    ap.add_argument("--kbps", type=float, default=None,
                    help="bitrate for read-ahead estimate when media_layout has none")
    args = ap.parse_args()

    analyses = []
    for p in args.sessions:
        a = analyze(p, args.tur_period, args.track_dur_s, args.kbps)
        analyses.append(a)
        folder = p.parent if p.is_file() else p
        if not args.no_write:
            out = folder / "derived"
            out.mkdir(parents=True, exist_ok=True)
            (out / "analysis.json").write_text(json.dumps(a, indent=2, ensure_ascii=False), encoding="utf-8")
            (out / "analysis.md").write_text(to_markdown(a), encoding="utf-8")
        print(f"{a['session']}: time={a['time_base']['source']} tur_median={a['tur'].get('idle_median_s')} "
              f"episodes={sum(1 for e in a['episodes'] if not e['probe_only'])} "
              f"next_track={len(a['prefetch'])} tail_then_head={a['tail_then_head']['cases']}")
    if args.compare:
        c = compare(analyses)
        args.compare.parent.mkdir(parents=True, exist_ok=True)
        args.compare.write_text(json.dumps(c, indent=2, ensure_ascii=False), encoding="utf-8")
        args.compare.with_suffix(".md").write_text(compare_markdown(c), encoding="utf-8")
        print(f"compare -> {args.compare}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
