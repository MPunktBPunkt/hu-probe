#!/usr/bin/env python3
"""Analyse hu-probe sessions: mount signature, reads per file, TUR grid, prefetch lead.

Per session it writes ``<session>/derived/analysis.json`` and ``analysis.md``:

* timeline: mount signature (TUR count + first READ10s), read episodes per file,
  TUR grid (idle period, bursts);
* time base: ``capture_t_us`` (FW >= 0.2.31) > ``t_us`` > TUR-grid reconstruction
  when replayed offline events were clamped to one timestamp (B7, FW <= 0.2.30);
* lead/lag of every next-track read, selection latency (marker -> READ10),
  ``tail_then_head`` (file read end-first);
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

# demo_v1 (FAT12, spc 1): boot 0, FAT 1-6, root 7-8, data 9..; 3 x 309 sectors.
DEMO_V1_FILES = [
    {"node": 0, "path": "/TRACK1.MP3", "size": 158208, "dur_ms": 26500, "extents": [[9, 309]]},
    {"node": 1, "path": "/TRACK2.MP3", "size": 158208, "dur_ms": 26500, "extents": [[318, 309]]},
    {"node": 2, "path": "/TRACK3.MP3", "size": 158208, "dur_ms": 26500, "extents": [[627, 309]]},
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
        m = media_map_from(files, variant or layout[0].get("variant") or "unknown", "media_layout")
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


def marker_times(cmds: list[dict], times: list[float]) -> list[dict]:
    return [{"name": c.get("name"), "note": c.get("note"), "t_s": round(times[i], 3),
             "auto": c.get("name") in AUTO_MARKERS, "origin": c.get("origin")}
            for i, c in enumerate(cmds) if c["kind"] == "marker"]


def media_loss(cmds: list[dict]) -> dict:
    post = [c for c in cmds if c.get("origin") in ("post_finalize", "post_finalize_buffered")]
    return {
        "post_finalize_cmds": sum(1 for c in post if c["kind"] == "cbw"),
        "ops": [c["op"] for c in post if c["kind"] == "cbw"][:24],
    }


# ---------------------------------------------------------------- analysis


def analyze(path: Path, tur_period_s: float) -> dict:
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
        "media_loss": media_loss(cmds),
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
    args = ap.parse_args()

    analyses = []
    for p in args.sessions:
        a = analyze(p, args.tur_period)
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
