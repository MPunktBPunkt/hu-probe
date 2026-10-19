from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


class SessionStore:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.root.mkdir(parents=True, exist_ok=True)
        self._active: dict[str, Path] = {}
        self._last_seq: dict[str, int] = {}
        self._recv_events: dict[str, int] = {}
        self._parse_errors = 0

    @property
    def parse_errors(self) -> int:
        return self._parse_errors

    def note_parse_error(self) -> None:
        self._parse_errors += 1

    def ensure_session(self, sid: str, meta: dict[str, Any] | None = None) -> Path:
        if sid in self._active:
            return self._active[sid]
        folder = self.root / sid
        folder.mkdir(parents=True, exist_ok=True)
        (folder / "derived").mkdir(exist_ok=True)
        session_json = folder / "session.json"
        if not session_json.exists():
            payload = {
                "schema": 2,
                "session_id": sid,
                "started_at": datetime.now(timezone.utc).astimezone().isoformat(),
                "ended_at": None,
                "vehicle": (meta or {}).get("vehicle"),
                "profile_id": (meta or {}).get("profile_id"),
                "scenario": (meta or {}).get("scenario"),
                "fw_version": (meta or {}).get("fw_version"),
                "transport": (meta or {}).get("transport") or "tcp_live",
                "fw_type": "hu-probe",
                "fw_sha": None,
                "mac": (meta or {}).get("mac"),
                "clock": {
                    "domain": "probe_monotonic_us",
                    "source": "esp_timer",
                },
                "capture": {
                    "dropped": 0,
                    "queue_highwater": 0,
                    "tcp_disconnects": 0,
                    "published": 0,
                    "stream_lost": 0,
                    "tcp_tx_fail": 0,
                    "msc_sample_overflow": 0,
                    "recv_seq_gaps": [],
                    "recv_events": 0,
                    "parse_errors": 0,
                    "complete": None,
                },
                "gates": ["HU-PROBE-GATE-01"],
                "notes": "",
            }
            session_json.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        self._active[sid] = folder
        return folder

    def _write_session(self, folder: Path, data: dict[str, Any]) -> None:
        (folder / "session.json").write_text(
            json.dumps(data, indent=2) + "\n", encoding="utf-8"
        )

    def append_event(self, event: dict[str, Any]) -> Path | None:
        sid = event.get("sid")
        if not sid:
            return None
        meta = None
        if event.get("ev") == "session" and event.get("action") == "start":
            meta = {
                "vehicle": event.get("vehicle"),
                "scenario": event.get("scenario"),
                "profile_id": event.get("profile_id"),
                "fw_version": event.get("fw_version"),
                "mac": event.get("mac"),
                "transport": event.get("transport"),
            }
        folder = self.ensure_session(str(sid), meta)
        sid_s = str(sid)
        self._recv_events[sid_s] = self._recv_events.get(sid_s, 0) + 1

        dirty_meta = False
        data: dict[str, Any] | None = None

        def load() -> dict[str, Any]:
            nonlocal data
            if data is None:
                data = json.loads((folder / "session.json").read_text(encoding="utf-8"))
            return data

        if event.get("ev") == "session" and event.get("action") == "start":
            data = load()
            for key in (
                "vehicle",
                "scenario",
                "profile_id",
                "fw_version",
                "fw_sha",
                "fw_build_id",
                "fw_dirty",
                "mac",
                "usb_host_kind",
                "usb_host_role",
            ):
                if event.get(key) is not None:
                    data[key] = event.get(key)
            if event.get("fw_type"):
                data["fw_type"] = event.get("fw_type")
            if isinstance(event.get("probe_identity"), dict):
                data["probe_identity"] = event.get("probe_identity")
            if isinstance(event.get("media"), dict):
                data["media"] = event.get("media")
                data.pop("media_layout", None)
            dirty_meta = True
            self._last_seq.pop(sid_s, None)

        if event.get("ev") == "media_layout":
            data = load()
            layout = list(data.get("media_layout") or [])
            layout.append(
                {
                    k: event.get(k)
                    for k in (
                        "node", "path", "dir", "size", "kbps", "dur_ms", "seed",
                        "flags", "apic_bytes", "extents", "extents_truncated",
                    )
                    if event.get(k) is not None
                }
            )
            data["media_layout"] = layout
            dirty_meta = True

        seq = event.get("seq")
        if isinstance(seq, int):
            prev = self._last_seq.get(sid_s)
            if prev is not None and seq != prev + 1:
                data = load()
                cap = data.setdefault("capture", {})
                gaps = list(cap.get("recv_seq_gaps") or [])
                gaps.append(
                    {
                        "after": prev,
                        "got": seq,
                        "missing": list(range(prev + 1, seq)),
                    }
                )
                if len(gaps) > 64:
                    gaps = gaps[-64:]
                cap["recv_seq_gaps"] = gaps
                cap["complete"] = False
                dirty_meta = True
            self._last_seq[sid_s] = seq

        line = json.dumps(event, separators=(",", ":")) + "\n"
        with (folder / "events.jsonl").open("a", encoding="utf-8") as f:
            f.write(line)

        ev = event.get("ev")
        if ev == "marker":
            with (folder / "markers.jsonl").open("a", encoding="utf-8") as f:
                f.write(line)
        elif ev in ("status", "capture_health"):
            with (folder / "status.jsonl").open("a", encoding="utf-8") as f:
                f.write(line)
            if ev == "capture_health":
                data = load()
                cap = data.setdefault("capture", {})
                for key in (
                    "dropped",
                    "tcp_disconnects",
                    "published",
                    "stream_lost",
                    "tcp_tx_fail",
                    "msc_sample_overflow",
                    "bot_pending_overflow",
                    "reset_reason",
                    "transport",
                    "transport_epoch",
                    "transport_epoch_start",
                    "transport_epoch_end",
                    "end_reason",
                    "end_strategy",
                    "upload_status",
                    "offline_records",
                    "offline_bytes",
                    "offline_overflow",
                    "offline_post_finalize_records",
                    "offline_post_finalize_dropped",
                    "replay_clock",
                    "record_size",
                    "record_format",
                    "replay_records",
                    "replay_complete",
                    "replay_error",
                    "offline_time_range_dropped",
                    "offline_anchor_unix_us",
                    "offline_anchor_capture_t_us",
                    "capacity_bytes",
                    "arm_heap_free",
                    "arm_heap_max_alloc",
                    "heap_free",
                    "heap_min_free",
                    "psram_bytes",
                    "hw_module",
                    "tur_count",
                    "tur_burst_n",
                    "tur_idle_n",
                    "tur_idle_p10_ms",
                    "tur_idle_median_ms",
                    "tur_idle_p90_ms",
                    "fw_dirty",
                ):
                    if key in event:
                        cap[key] = event.get(key)
                cap["queue_highwater"] = max(
                    int(cap.get("queue_highwater") or 0),
                    int(event.get("queue_highwater") or 0),
                )
                cap["recv_events"] = self._recv_events.get(sid_s, 0)
                cap["parse_errors"] = self._parse_errors
                dropped = int(cap.get("dropped") or 0)
                disc = int(cap.get("tcp_disconnects") or 0)
                stream_lost = int(cap.get("stream_lost") or 0)
                gaps = cap.get("recv_seq_gaps") or []
                probe_complete = event.get("complete")
                offline_overflow = int(cap.get("offline_overflow") or 0)
                cap["complete"] = (
                    dropped == 0 and disc == 0 and stream_lost == 0 and not gaps
                    and offline_overflow == 0 and probe_complete is not False
                )
                # Quantitative integrity: collector gaps + probe offline gate.
                probe_integrity = event.get("integrity_pass")
                cap["integrity_pass"] = bool(cap["complete"]) and probe_integrity is not False
                if isinstance(event.get("panic_tail"), dict):
                    cap["panic_tail"] = event.get("panic_tail")
                dirty_meta = True
        elif ev == "session" and event.get("action") == "stop":
            data = load()
            data["ended_at"] = datetime.now(timezone.utc).astimezone().isoformat()
            cap = data.setdefault("capture", {})
            cap["recv_events"] = self._recv_events.get(sid_s, 0)
            cap["parse_errors"] = self._parse_errors
            dirty_meta = True

        if dirty_meta and data is not None:
            self._write_session(folder, data)
        return folder

    def list_sessions(self) -> list[dict[str, Any]]:
        out: list[dict[str, Any]] = []
        for p in sorted(self.root.iterdir(), reverse=True):
            if not p.is_dir() or p.name.startswith("."):
                continue
            meta_path = p / "session.json"
            meta: dict[str, Any] = {"session_id": p.name}
            if meta_path.exists():
                try:
                    meta = json.loads(meta_path.read_text(encoding="utf-8"))
                except json.JSONDecodeError:
                    pass
            events = p / "events.jsonl"
            meta["path"] = str(p)
            meta["events_bytes"] = events.stat().st_size if events.exists() else 0
            out.append(meta)
        return out
