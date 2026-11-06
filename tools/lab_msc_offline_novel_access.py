#!/usr/bin/env python3
"""Lab: MSC offline + sequence smoke + media change + novel host file access.

Topology: Analyzer Host .88 × hu-probe .89 (must be usb_gadget=msc).

Novel vs BMW NBT Feld-FACTS:
  HU: P-MOUNT + READ10 sizes 8/32/128/240, no WRITE, no reverse LBA dump.
  Here: reverse LBA staircase + large sequential probe + WRITE attempt
        (DUT is read-only → expect fail) while offline capture runs.

Not claimed: real HU wire.
"""
from __future__ import annotations

import json
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

DUT = "192.168.178.89"
HOST = "192.168.178.88"
OUT = Path(__file__).resolve().parents[1] / "docs/feld/artifacts/20261106-lab-msc-novel.json"


def http(method: str, url: str, body: dict | None = None, timeout: float = 60):
    data = None
    headers = {}
    if body is not None:
        data = json.dumps(body).encode()
        headers["Content-Type"] = "application/json"
    req = urllib.request.Request(url, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            raw = r.read().decode(errors="replace")
            try:
                return r.status, json.loads(raw)
            except Exception:
                return r.status, {"_raw": raw[:500]}
    except urllib.error.HTTPError as e:
        raw = e.read().decode(errors="replace")
        try:
            return e.code, json.loads(raw)
        except Exception:
            return e.code, {"_raw": raw[:500]}
    except Exception as e:
        return 0, {"err": str(e)}


def wait_dut(want_gadget: str | None = None, timeout_s: float = 120.0):
    t0 = time.time()
    last = None
    while time.time() - t0 < timeout_s:
        c, st = http("GET", f"http://{DUT}/api/status", timeout=12)
        last = (c, st)
        if c == 200 and isinstance(st, dict) and st.get("fw"):
            g = st.get("usb_gadget")
            if want_gadget is None or g == want_gadget:
                return st
        time.sleep(2)
    raise RuntimeError(f"DUT not ready: {last}")


def snap(st: dict | None = None) -> dict:
    if st is None:
        _, st = http("GET", f"http://{DUT}/api/status", timeout=12)
    sm = (st.get("connected_app") or {}).get("session_metrics") or {}
    media = st.get("media") or {}
    if isinstance(media, dict) and "active" in media:
        media = media.get("active") or media
    return {
        "fw": st.get("fw"),
        "usb_gadget": st.get("usb_gadget"),
        "msc_reads": st.get("msc_reads"),
        "read_count": st.get("read_count"),
        "plug_count": st.get("plug_count"),
        "msc_soft_attached": st.get("msc_soft_attached"),
        "tur_hz": st.get("tur_hz"),
        "media_variant": media.get("variant"),
        "media_files": media.get("files"),
        "scsi_hist": sm.get("scsi_opcode_histogram"),
        "read_bytes": sm.get("read_bytes"),
        "write_count": sm.get("write_count"),
        "offline": st.get("offline"),
        "sequence": st.get("sequence"),
    }


def main() -> int:
    report: dict = {"fw": "0.3.36", "steps": []}

    print("=== 1) Ensure MSC gadget")
    st = wait_dut()
    if st.get("usb_gadget") != "msc":
        http("POST", f"http://{DUT}/api/config", {"usbGadget": "msc", "enableHub": True})
        http("POST", f"http://{DUT}/api/connected/gadget/reboot", timeout=15)
        time.sleep(8)
        st = wait_dut("msc")
    report["steps"].append({"ensure_msc": snap(st)})
    print("gadget", st.get("usb_gadget"), "soft", st.get("msc_soft_attached"))

    print("=== 2) Soft-attach / remount so Host sees MSC")
    c, rem = http("POST", f"http://{DUT}/api/usb/remount", {"bumpSerial": False})
    print("remount", c, rem)
    time.sleep(2)
    # also cycle briefly to force re-enum
    http("POST", f"http://{DUT}/api/usb/cycle", {"holdMs": 400})
    time.sleep(3)
    st = wait_dut("msc")
    c, hs = http("GET", f"http://{HOST}/api/status", timeout=20)
    dut = (hs or {}).get("dut") if c == 200 else None
    print("host dut", dut)
    report["steps"].append({"attach": snap(st), "host_dut": dut})
    if not dut or not dut.get("present"):
        # one more remount
        http("POST", f"http://{DUT}/api/usb/remount", {})
        time.sleep(3)
        c, hs = http("GET", f"http://{HOST}/api/status", timeout=25)
        dut = (hs or {}).get("dut")
        print("host dut retry", dut)
        report["host_dut_retry"] = dut

    print("=== 3) Media → size_1_4_8 (synthetic files)")
    c, med = http(
        "POST",
        f"http://{DUT}/api/media/variant",
        {"preset": "size_1_4_8", "method": "remount"},
        timeout=30,
    )
    print("media", c, {k: med.get(k) for k in ("ok", "err", "active") if k in med or True})
    time.sleep(1)
    c, mj = http("GET", f"http://{DUT}/api/media")
    active = (mj or {}).get("active") or {}
    files = active.get("files") or []
    print("files", [(f.get("name"), f.get("lba"), f.get("size")) for f in files[:8]])
    report["steps"].append({"media_size_1_4_8": {"http": c, "files": files, "snap": snap()}})

    print("=== 4) Short offline measurement (20s)")
    # stop any prior
    http("POST", f"http://{DUT}/api/offline/stop")
    http("POST", f"http://{DUT}/api/sequence/abort")
    c, off = http(
        "POST",
        f"http://{DUT}/api/offline/start",
        {
            "vehicle": "lab_debian",
            "scenario": "s0_plug_enum",
            "profile_id": "lab_debian",
            "measure_ms": 20000,
            "soft_usb": True,
            "fine_data": True,
            "end_strategy": "media_hide",
        },
        timeout=30,
    )
    print("offline/start", c, off)
    report["steps"].append({"offline_start": off})
    time.sleep(2)

    print("=== 5) Host: BMW-like P-MOUNT/P-MREAD (baseline)")
    before = snap()
    q = urllib.parse.urlencode(
        {"profile": "bmw_mount_scan", "tur": 16, "scan_blocks": 64, "file_lba0": 9}
    )
    c, stim = http("POST", f"http://{HOST}/api/lab/bmw-hu-stimulus?{q}", timeout=180)
    print("bmw stimulus", c, {k: stim.get(k) for k in list(stim)[:12]})
    time.sleep(0.5)
    after_bmw = snap()
    report["steps"].append(
        {
            "bmw_mount_scan": stim,
            "before": before,
            "after": after_bmw,
            "delta_msc_reads": (after_bmw.get("msc_reads") or 0) - (before.get("msc_reads") or 0),
        }
    )

    print("=== 6) NOVEL host access (not HU Feld grammar)")
    novel = {"reads": [], "write": None, "reverse": []}
    # large sequential from LBA 0
    q = urllib.parse.urlencode({"blocks": 256, "lba": 0, "chunk": 8})
    c, probe = http("POST", f"http://{HOST}/api/lab/usb-host-probe?{q}", timeout=120)
    print("probe sequential", c, {k: probe.get(k) for k in ("ok", "bytes", "kb_s", "err", "msc_ready")})
    novel["sequential_256"] = probe

    # reverse LBA staircase — HU never observed dumping volume backwards
    for lba in (200, 150, 100, 50, 20, 10, 5, 1):
        q = urllib.parse.urlencode({"lba": lba, "blocks": 8})
        c, rd = http("POST", f"http://{HOST}/api/lab/usb-host-read?{q}", timeout=30)
        novel["reverse"].append({"lba": lba, "http": c, "ok": rd.get("ok"), "ms": rd.get("duration_ms")})
        print("reverse read lba", lba, rd.get("ok"), rd.get("duration_ms"))

    # file-ish LBAs if media listed them
    for f in files[:3]:
        lba = int(f.get("lba") or f.get("start_lba") or 0)
        if not lba:
            continue
        q = urllib.parse.urlencode({"lba": lba, "blocks": 16})
        c, rd = http("POST", f"http://{HOST}/api/lab/usb-host-read?{q}", timeout=30)
        novel["reads"].append(
            {"file": f.get("name"), "lba": lba, "ok": rd.get("ok"), "http": c}
        )
        print("file read", f.get("name"), lba, rd.get("ok"))

    # WRITE attempt — HU never writes; DUT RO → expect fail
    q = urllib.parse.urlencode({"dir": "write", "blocks": 8, "lba": 100, "chunk": 1})
    c, wr = http("POST", f"http://{HOST}/api/lab/usb-host-probe?{q}", timeout=60)
    print("write probe", c, {k: wr.get(k) for k in ("ok", "err", "bytes", "msc_ready")})
    novel["write"] = wr
    after_novel = snap()
    report["steps"].append(
        {
            "novel": novel,
            "after": after_novel,
            "delta_msc_reads": (after_novel.get("msc_reads") or 0)
            - (after_bmw.get("msc_reads") or 0),
            "scsi_hist": after_novel.get("scsi_hist"),
        }
    )

    print("=== 7) Media change mid-session → cache_b")
    c, med2 = http(
        "POST",
        f"http://{DUT}/api/media/variant",
        {"preset": "cache_b", "method": "remount"},
        timeout=30,
    )
    print("media cache_b", c, med2.get("ok"), med2.get("err"))
    time.sleep(1.5)
    # host remount stimulus after identity/content change
    q = urllib.parse.urlencode({"profile": "bmw_mount", "tur": 8})
    c, stim2 = http("POST", f"http://{HOST}/api/lab/bmw-hu-stimulus?{q}", timeout=120)
    print("bmw_mount after media", c, stim2.get("ok"), stim2.get("err"))
    report["steps"].append({"media_cache_b": med2, "stim_after": stim2, "snap": snap()})

    print("=== 8) Sequence smoke: start seq_cache then abort")
    # wait offline end if still running
    for _ in range(15):
        st = wait_dut("msc")
        off = st.get("offline") or {}
        state = off.get("state") if isinstance(off, dict) else off
        if state in (None, "idle", "done", "stopped", False):
            break
        print(" wait offline", state)
        time.sleep(2)
    http("POST", f"http://{DUT}/api/offline/stop")
    c, seq = http(
        "POST",
        f"http://{DUT}/api/sequence/start",
        {
            "id": "seq_cache",
            "vehicle": "lab_debian",
            "profile_id": "lab_debian",
            "auto_arm": False,
        },
        timeout=30,
    )
    print("sequence/start", c, seq.get("ok"), seq.get("err"), (seq.get("runner") or {}).get("state"))
    time.sleep(1)
    c2, nxt = http("POST", f"http://{DUT}/api/sequence/next")
    print("sequence/next", c2, nxt)
    time.sleep(1)
    c3, ab = http("POST", f"http://{DUT}/api/sequence/abort")
    print("sequence/abort", c3, ab)
    report["steps"].append({"sequence": {"start": seq, "next": nxt, "abort": ab, "snap": snap()}})

    print("=== 9) Soft-detach (WiFi hygiene)")
    http("POST", f"http://{DUT}/api/usb/cycle", {"holdMs": 0})
    time.sleep(1)
    final = snap()
    report["final"] = final
    report["pass"] = True
    # minimal success criteria
    if (report["steps"][4].get("delta_msc_reads") or 0) <= 0 and not (
        novel.get("sequential_256") or {}
    ).get("ok"):
        # still ok if scsi hist moved
        hist = after_novel.get("scsi_hist") or {}
        if not any(int(hist.get(k) or 0) > 0 for k in hist):
            report["pass"] = False

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=2))
    print("wrote", OUT)
    print("RESULT", "PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
