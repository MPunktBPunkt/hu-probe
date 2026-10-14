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

    def ensure_session(self, sid: str, meta: dict[str, Any] | None = None) -> Path:
        if sid in self._active:
            return self._active[sid]
        # Prefer sid as folder name if it already looks like our convention.
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
                    "complete": None,
                },
                "gates": ["HU-PROBE-GATE-01"],
                "notes": "",
            }
            session_json.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        self._active[sid] = folder
        return folder

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
            }
        folder = self.ensure_session(str(sid), meta)
        if event.get("ev") == "session" and event.get("action") == "start":
            sj = folder / "session.json"
            data = json.loads(sj.read_text(encoding="utf-8"))
            for key in ("vehicle", "scenario", "profile_id", "fw_version", "mac"):
                if event.get(key) is not None:
                    data[key if key != "profile_id" else "profile_id"] = event.get(key)
            if event.get("fw_type"):
                data["fw_type"] = event.get("fw_type")
            sj.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
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
                sj = folder / "session.json"
                data = json.loads(sj.read_text(encoding="utf-8"))
                cap = data.setdefault("capture", {})
                cap["dropped"] = event.get("dropped", cap.get("dropped", 0))
                cap["queue_highwater"] = max(
                    int(cap.get("queue_highwater") or 0),
                    int(event.get("queue_highwater") or 0),
                )
                cap["tcp_disconnects"] = event.get(
                    "tcp_disconnects", cap.get("tcp_disconnects", 0)
                )
                dropped = int(cap.get("dropped") or 0)
                disc = int(cap.get("tcp_disconnects") or 0)
                cap["complete"] = dropped == 0 and disc == 0
                sj.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
        elif ev == "session" and event.get("action") == "stop":
            sj = folder / "session.json"
            data = json.loads(sj.read_text(encoding="utf-8"))
            data["ended_at"] = datetime.now(timezone.utc).astimezone().isoformat()
            sj.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
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
