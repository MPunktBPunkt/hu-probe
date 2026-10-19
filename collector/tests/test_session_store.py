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
            "published": 3,
            "stream_lost": 0,
            "msc_sample_overflow": 0,
            "reset_reason": "power_on",
            "transport_epoch_start": 2,
            "transport_epoch_end": 2,
            "end_reason": "auto_media_hide",
            "end_strategy": "media_hide",
            "upload_status": "replay_enqueued",
            "offline_overflow": 0,
        }
    )
    meta = json.loads((folder / "session.json").read_text(encoding="utf-8"))
    assert meta["capture"]["complete"] is True
    assert meta["capture"]["queue_highwater"] == 4
    assert meta["capture"]["recv_events"] == 3
    assert meta["capture"]["reset_reason"] == "power_on"
    assert meta["capture"]["transport_epoch_start"] == 2
    assert meta["capture"]["end_reason"] == "auto_media_hide"
    assert meta["capture"]["end_strategy"] == "media_hide"
    assert meta["capture"]["upload_status"] == "replay_enqueued"

    # Seq gap must mark incomplete
    store.append_event(
        {
            "v": 2,
            "sid": "20261014-120000_bmw_s0_plug_enum",
            "seq": 6,
            "t_us": 2000,
            "ev": "read",
            "n": 512,
        }
    )
    meta = json.loads((folder / "session.json").read_text(encoding="utf-8"))
    assert meta["capture"]["complete"] is False
    assert meta["capture"]["recv_seq_gaps"][0]["missing"] == [4, 5]


def test_offline_health_overflow_is_incomplete(tmp_path: Path) -> None:
    store = SessionStore(tmp_path)
    sid = "20261016-120000_bmw_s0_plug_enum"
    store.append_event(
        {
            "v": 2,
            "sid": sid,
            "seq": 1,
            "t_us": 1,
            "ev": "session",
            "action": "start",
            "vehicle": "bmw",
            "scenario": "s0_plug_enum",
            "profile_id": "bmw_nbt_evo",
            "transport": "ram_offline",
        }
    )
    store.append_event(
        {
            "v": 2,
            "sid": sid,
            "seq": 2,
            "t_us": 2,
            "ev": "capture_health",
            "dropped": 0,
            "tcp_disconnects": 0,
            "stream_lost": 0,
            "offline_overflow": 1,
            "complete": False,
            "upload_status": "delivered",
        }
    )

    meta = json.loads((tmp_path / sid / "session.json").read_text(encoding="utf-8"))
    assert meta["capture"]["offline_overflow"] == 1
    assert meta["capture"]["upload_status"] == "delivered"
    assert meta["capture"]["complete"] is False
    assert meta["capture"]["integrity_pass"] is False


def test_offline_health_integrity_pass(tmp_path: Path) -> None:
    store = SessionStore(tmp_path)
    sid = "20261018-120000_lab_debian_s0_plug_enum"
    store.append_event(
        {
            "v": 2,
            "sid": sid,
            "seq": 1,
            "t_us": 1,
            "ev": "session",
            "action": "start",
            "vehicle": "lab_debian",
            "scenario": "s0_plug_enum",
            "profile_id": "lab_debian",
            "transport": "ram_offline",
        }
    )
    store.append_event(
        {
            "v": 2,
            "sid": sid,
            "seq": 2,
            "t_us": 2,
            "ev": "capture_health",
            "dropped": 0,
            "tcp_disconnects": 0,
            "stream_lost": 0,
            "offline_overflow": 0,
            "complete": True,
            "integrity_pass": True,
            "record_size": 99,
            "capacity_bytes": 65536,
            "upload_status": "delivered",
        }
    )

    meta = json.loads((tmp_path / sid / "session.json").read_text(encoding="utf-8"))
    assert meta["capture"]["complete"] is True
    assert meta["capture"]["integrity_pass"] is True
    assert meta["capture"]["record_size"] == 99
    assert meta["capture"]["capacity_bytes"] == 65536

def test_media_metadata_and_tur_stats(tmp_path: Path) -> None:
    store = SessionStore(tmp_path)
    sid = "20261019-120000_bmw_seq_dur"
    store.append_event(
        {
            "v": 2,
            "sid": sid,
            "seq": 1,
            "t_us": 0,
            "ev": "session",
            "action": "start",
            "vehicle": "bmw",
            "scenario": "seq_dur",
            "fw_dirty": False,
            "media": {"variant": "dur_mix", "hash": "0x1234abcd", "synthetic": True},
        }
    )
    store.append_event(
        {
            "v": 2,
            "sid": sid,
            "seq": 2,
            "t_us": 10,
            "ev": "media_layout",
            "variant": "dur_mix",
            "node": 0,
            "path": "/D010.MP3",
            "size": 160000,
            "extents": [[600, 313]],
        }
    )
    store.append_event(
        {
            "v": 2,
            "sid": sid,
            "seq": 3,
            "t_us": 20,
            "ev": "capture_health",
            "dropped": 0,
            "offline_overflow": 0,
            "offline_post_finalize_records": 7,
            "replay_clock": "capture_t_us",
            "tur_idle_median_ms": 2239,
        }
    )
    meta = json.loads((tmp_path / sid / "session.json").read_text(encoding="utf-8"))
    assert meta["fw_dirty"] is False
    assert meta["media"]["variant"] == "dur_mix"
    assert meta["media_layout"][0]["path"] == "/D010.MP3"
    assert meta["media_layout"][0]["extents"] == [[600, 313]]
    assert meta["capture"]["offline_post_finalize_records"] == 7
    assert meta["capture"]["replay_clock"] == "capture_t_us"
    assert meta["capture"]["tur_idle_median_ms"] == 2239
