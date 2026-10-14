import json
from pathlib import Path

from hu_probe_collector.session_store import SessionStore


def test_append_session_events(tmp_path: Path) -> None:
    store = SessionStore(tmp_path)
    store.append_event(
        {
            "v": 1,
            "sid": "20261014-120000_bmw_s0_plug_enum",
            "seq": 1,
            "t_ms": 0,
            "ev": "session",
            "action": "start",
            "vehicle": "bmw",
            "scenario": "s0_plug_enum",
            "profile_id": "bmw_nbt_evo",
        }
    )
    store.append_event(
        {
            "v": 1,
            "sid": "20261014-120000_bmw_s0_plug_enum",
            "seq": 2,
            "t_ms": 10,
            "ev": "read",
            "lba": 1,
            "n": 4096,
            "dur_us": 3000,
            "gap_us": 4000,
            "tag": "sim",
            "kind": "file",
        }
    )
    folder = tmp_path / "20261014-120000_bmw_s0_plug_enum"
    assert folder.exists()
    meta = json.loads((folder / "session.json").read_text(encoding="utf-8"))
    assert meta["vehicle"] == "bmw"
    assert meta["schema"] == 2
    lines = (folder / "events.jsonl").read_text(encoding="utf-8").strip().splitlines()
    assert len(lines) == 2

    store.append_event(
        {
            "v": 2,
            "sid": "20261014-120000_bmw_s0_plug_enum",
            "seq": 3,
            "t_us": 1000,
            "ev": "capture_health",
            "dropped": 0,
            "queue_highwater": 4,
            "tcp_disconnects": 0,
        }
    )
    meta = json.loads((folder / "session.json").read_text(encoding="utf-8"))
    assert meta["capture"]["complete"] is True
    assert meta["capture"]["queue_highwater"] == 4
