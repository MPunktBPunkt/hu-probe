#!/usr/bin/env python3
"""Lab regression on OTG Host(.88) × hu-probe(.89) FW 0.3.36.

1) NVS sticky for msc / aoa_observe / aoa_phone_like / aoa_id_303a
2) AOA Lab A/B against Analyzer Host aoa-handshake (observe mode)

Not claimed: BMW wire, BCL, EP0 outside vendor callback.
"""
from __future__ import annotations

import json
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

DUT = "192.168.178.89"
HOST = "192.168.178.88"
MODES = ("msc", "aoa_observe", "aoa_phone_like", "aoa_id_303a")


def http(method: str, url: str, body: dict | None = None, timeout: float = 30):
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
                return r.status, {"_raw": raw[:300]}
    except urllib.error.HTTPError as e:
        raw = e.read().decode(errors="replace")
        try:
            return e.code, json.loads(raw)
        except Exception:
            return e.code, {"_raw": raw[:300], "err": f"http_{e.code}"}
    except Exception as e:
        return 0, {"err": str(e)}


def wait_dut(expect_fw: str | None = "0.3.36", timeout_s: float = 90.0):
    t0 = time.time()
    last = None
    while time.time() - t0 < timeout_s:
        code, st = http("GET", f"http://{DUT}/api/status", timeout=8)
        last = (code, st)
        if code == 200 and isinstance(st, dict):
            fw = st.get("fw") or st.get("version")
            if expect_fw is None or fw == expect_fw or str(fw).startswith("0.3.36"):
                return st
        time.sleep(2)
    raise RuntimeError(f"DUT not ready: {last}")


def gadget_of(st: dict) -> str:
    return st.get("usb_gadget") or (st.get("connected_app") or {}).get("usb_gadget") or "?"


def set_mode(mode: str) -> dict:
    code, cfg = http("POST", f"http://{DUT}/api/config", {"usbGadget": mode, "enableHub": False})
    print(f"  config {mode}: http={code} body={cfg}")
    code2, reb = http("POST", f"http://{DUT}/api/connected/gadget/reboot", timeout=15)
    print(f"  reboot: http={code2} body={reb}")
    time.sleep(6)
    st = wait_dut()
    got = gadget_of(st)
    ok = got == mode
    print(f"  after boot usb_gadget={got} sticky_ok={ok}")
    return {"mode": mode, "ok": ok, "usb_gadget": got, "config": cfg, "status_fw": st.get("fw")}


def soft_detach_mode_invariant(st_before: dict) -> dict:
    mode = gadget_of(st_before)
    # Soft cycle via remount/soft API if available
    code, body = http("POST", f"http://{DUT}/api/usb/cycle", {"holdMs": 400}, timeout=10)
    print(f"  soft-cycle http={code} {body}")
    time.sleep(2)
    st = wait_dut()
    got = gadget_of(st)
    return {"ok": got == mode, "before": mode, "after": got, "soft": body}


def aoa_lab_ab() -> dict:
    # Ensure observe mode
    st = wait_dut()
    if gadget_of(st) != "aoa_observe":
        set_mode("aoa_observe")
        st = wait_dut()

    # Arm link (soft-attach + session)
    code, start = http("POST", f"http://{DUT}/api/connected/link/start", timeout=20)
    print(f"  link/start http={code} {start}")
    time.sleep(1)

    # Lab A: respond on
    code, hyp = http(
        "POST",
        f"http://{DUT}/api/connected/link/hypothesis_respond",
        {"hypothesis_respond": True},
        timeout=10,
    )
    print(f"  hyp A http={code} {hyp}")

    st0 = wait_dut()
    tr0 = (st0.get("connected_app") or {}).get("transport") or {}
    sm0 = (st0.get("connected_app") or {}).get("session_metrics") or {}
    a0 = int(tr0.get("aoa51") or sm0.get("aoa_51") or 0)
    ok0 = int(tr0.get("aoa51_respond_ok") or sm0.get("aoa51_respond_ok") or 0)

    # Host handshake
    hq = urllib.parse.urlencode({"profile": "lab", "wait_reenum": 0})
    hcode, hs = http("POST", f"http://{HOST}/api/lab/aoa-handshake?{hq}", timeout=45)
    print(f"  host handshake A http={hcode} {hs}")
    time.sleep(1)
    st1 = wait_dut()
    tr1 = (st1.get("connected_app") or {}).get("transport") or {}
    sm1 = (st1.get("connected_app") or {}).get("session_metrics") or {}
    a1 = int(tr1.get("aoa51") or sm1.get("aoa_51") or 0)
    ok1 = int(tr1.get("aoa51_respond_ok") or sm1.get("aoa51_respond_ok") or 0)
    a52 = int(tr1.get("aoa52") or sm1.get("aoa_52") or 0)
    a53 = int(tr1.get("aoa53") or sm1.get("aoa_53") or 0)
    lab_a = {
        "host_ok": bool(isinstance(hs, dict) and hs.get("ok")),
        "host": hs,
        "aoa51_delta": a1 - a0,
        "aoa51_ok_delta": ok1 - ok0,
        "aoa52": a52,
        "aoa53": a53,
        "vid": tr1.get("vid"),
        "pid": tr1.get("pid"),
        "visibility": sm1.get("visibility_scope"),
        "enumeration_count": sm1.get("enumeration_count"),  # expect null/UNKNOWN
    }
    lab_a["pass"] = lab_a["host_ok"] and lab_a["aoa51_delta"] > 0 and lab_a["aoa51_ok_delta"] > 0
    print(f"  Lab A pass={lab_a['pass']} {lab_a}")

    # Lab B: respond off
    code, hypb = http(
        "POST",
        f"http://{DUT}/api/connected/link/hypothesis_respond",
        {"hypothesis_respond": False},
        timeout=10,
    )
    print(f"  hyp B http={code} {hypb}")
    st2 = wait_dut()
    tr2 = (st2.get("connected_app") or {}).get("transport") or {}
    sm2 = (st2.get("connected_app") or {}).get("session_metrics") or {}
    a2 = int(tr2.get("aoa51") or sm2.get("aoa_51") or 0)
    ok2 = int(tr2.get("aoa51_respond_ok") or sm2.get("aoa51_respond_ok") or 0)

    hcode, hsb = http("POST", f"http://{HOST}/api/lab/aoa-handshake?{hq}", timeout=45)
    print(f"  host handshake B http={hcode} {hsb}")
    time.sleep(1)
    st3 = wait_dut()
    tr3 = (st3.get("connected_app") or {}).get("transport") or {}
    sm3 = (st3.get("connected_app") or {}).get("session_metrics") or {}
    a3 = int(tr3.get("aoa51") or sm3.get("aoa_51") or 0)
    ok3 = int(tr3.get("aoa51_respond_ok") or sm3.get("aoa51_respond_ok") or 0)
    lab_b = {
        "host_ok": bool(isinstance(hsb, dict) and hsb.get("ok")),
        "host_err": (hsb or {}).get("err"),
        "host": hsb,
        "aoa51_delta": a3 - a2,
        "aoa51_ok_delta": ok3 - ok2,
    }
    # expect host fail and respond_ok flat; aoa51 may still +1
    lab_b["pass"] = (not lab_b["host_ok"]) and lab_b["aoa51_ok_delta"] == 0
    print(f"  Lab B pass={lab_b['pass']} {lab_b}")

    # identity check for 303a later separate
    return {"lab_a": lab_a, "lab_b": lab_b, "timeline_size": sm3.get("timeline_size")}


def main() -> int:
    print("=== DUT/HOST baseline")
    st = wait_dut()
    print("DUT fw", st.get("fw"), "gadget", gadget_of(st))
    hcode, hst = http("GET", f"http://{HOST}/api/status", timeout=15)
    print("HOST http", hcode, "role", (hst or {}).get("role"), "dut", (hst or {}).get("dut"))

    print("\n=== Sticky NVS matrix")
    sticky = []
    for mode in MODES:
        print(f"-- {mode}")
        sticky.append(set_mode(mode))

    print("\n=== Soft-detach invariant (current mode)")
    st = wait_dut()
    soft = soft_detach_mode_invariant(st)

    print("\n=== AOA Lab A/B (aoa_observe)")
    aoa = aoa_lab_ab()

    print("\n=== Leave DUT on aoa_observe for inspection (sticky already tested)")
    report = {
        "fw": "0.3.36",
        "dut": DUT,
        "host": HOST,
        "sticky": sticky,
        "soft_detach": soft,
        "aoa": aoa,
    }
    sticky_ok = all(r["ok"] for r in sticky)
    aoa_ok = aoa["lab_a"]["pass"] and aoa["lab_b"]["pass"]
    report["pass"] = sticky_ok and aoa_ok
    print(json.dumps(report, indent=2))
    out = "/home/martin/projects/hu-probe/docs/feld/artifacts/20261106-lab-c1-sticky-aoa.json"
    try:
        import os

        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "w") as f:
            json.dump(report, f, indent=2)
        print("wrote", out)
    except Exception as e:
        print("write fail", e)
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
