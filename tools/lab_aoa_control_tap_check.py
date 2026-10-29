#!/usr/bin/env python3
"""Lab instrument check for C1b-P0.5-A AoaControlTap coverage.

Sends known AOA vendor SETUPs (51/52/53) from a Linux host to the probe over
USB OTG and compares host TX with probe /api/status counters.

Requires:
  - probe online (default 192.168.178.88) in aoa_observe or aoa_phone_like
  - USB cable host↔probe OTG data
  - pyusb + libusb (use repo .venv-lab)

Does NOT Soft-Cycle (that often kills WiFi under OTG). Uses link/start softAttach
(FW ≥0.3.26) or /api/usb/remount → aoa_soft_attach.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.error
import urllib.request

try:
    import usb.core
    import usb.util
except ImportError:
    print("NEED: python3 -m venv .venv-lab && .venv-lab/bin/pip install pyusb", file=sys.stderr)
    sys.exit(2)

VID_ACC, PID_ACC = 0x18D1, 0x2D00
VID_PH, PID_PH = 0x04E8, 0x6860


def http_json(url: str, method: str = "GET", body: bytes | None = None, timeout: float = 5):
    req = urllib.request.Request(url, data=body, method=method)
    if body is not None:
        req.add_header("Content-Type", "application/json")
    with urllib.request.urlopen(req, timeout=timeout) as r:
        raw = r.read()
        return r.status, json.loads(raw.decode() or "{}") if raw else {}


def status(ip: str) -> dict:
    _, d = http_json(f"http://{ip}/api/status")
    return d


def ensure_armed_and_attached(ip: str) -> None:
    st = status(ip)
    ca = st.get("connected_app") or {}
    if not ca.get("link_observing"):
        try:
            code, d = http_json(f"http://{ip}/api/connected/link/start", method="POST", body=b"")
            print("arm", code, d.get("ok"), (d.get("connected_app") or {}).get("state"))
        except urllib.error.HTTPError as e:
            print("arm http", e.code, e.read()[:120])
            if e.code == 409:
                # busy/session — stop and retry once
                try:
                    http_json(f"http://{ip}/api/session/stop", method="POST", body=b"")
                    http_json(f"http://{ip}/api/connected/link/stop", method="POST", body=b"")
                except Exception:
                    pass
                time.sleep(0.5)
                code, d = http_json(f"http://{ip}/api/connected/link/start", method="POST", body=b"")
                print("arm retry", code, d.get("ok"))
    # Explicit softAttach without Soft-Cycle
    try:
        code, d = http_json(f"http://{ip}/api/usb/remount", method="POST", body=b"{}")
        print("attach", code, d.get("action"), d.get("ok"), d.get("note"))
    except Exception as e:
        print("attach note:", e)
    time.sleep(1.5)


def find_dev(retries: int = 10):
    for i in range(retries):
        for vid, pid, name in (
            (VID_ACC, PID_ACC, "accessory"),
            (VID_PH, PID_PH, "phone_like"),
        ):
            d = usb.core.find(idVendor=vid, idProduct=pid)
            if d is not None:
                return d, name, vid, pid
        time.sleep(0.4)
    return None, None, None, None


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--ip", default="192.168.178.88")
    ap.add_argument("--skip-usb", action="store_true")
    args = ap.parse_args()

    st = status(args.ip)
    ca = st.get("connected_app") or {}
    tr = ca.get("transport") or {}
    print(
        f"probe fw={st.get('fw')} gadget={st.get('usb_gadget') or ca.get('usb_gadget')} "
        f"profile={ca.get('aoa_identity_profile')} "
        f"setup={tr.get('setup_count')} vendor={tr.get('vendor_setup_count')} "
        f"drops={tr.get('drops')} 51/52/53={tr.get('aoa51')}/{tr.get('aoa52')}/{tr.get('aoa53')}"
    )
    if args.skip_usb:
        return 0

    ensure_armed_and_attached(args.ip)
    before = status(args.ip).get("connected_app", {}).get("transport", {})

    dev, name, vid, pid = find_dev()
    if dev is None:
        print("USB device not found (18D1:2D00 or 04E8:6860). Cable/OTG?", file=sys.stderr)
        return 1
    print(f"usb {name} {vid:04x}:{pid:04x} bus={dev.bus} addr={dev.address}")

    # LXC often exposes sysfs USB but not /dev/bus/usb/BBB/DDD — ctrl_transfer then → ENODEV.
    import os

    usbfs = f"/dev/bus/usb/{dev.bus:03d}/{dev.address:03d}"
    if not os.path.exists(usbfs):
        print(
            f"FAIL: usbfs node missing ({usbfs}). "
            "Host sees descriptors via sysfs, but this container cannot issue control transfers. "
            "Pass through the USB device node into the LXC (or run the check on the USB host).",
            file=sys.stderr,
        )
        return 3

    try:
        if dev.is_kernel_driver_active(0):
            dev.detach_kernel_driver(0)
    except Exception as e:
        print("detach_kernel:", e)

    try:
        data = bytes(dev.ctrl_transfer(0xC0, 51, 0, 0, 2, 2000))
        print(f"GET_PROTOCOL -> {data.hex()} (expect 0100 LE = AOA1)")
    except usb.core.USBError as e:
        print(f"GET_PROTOCOL err: {e}")

    try:
        payload = b"LabHost\x00"
        dev.ctrl_transfer(0x40, 52, 0, 0, payload, 2000)
        print(f"SEND_STRING idx0 len={len(payload)} ok")
    except usb.core.USBError as e:
        print(f"SEND_STRING err: {e}")

    try:
        dev.ctrl_transfer(0x40, 53, 0, 0, 0, 2000)
        print("START ok")
    except usb.core.USBError as e:
        print(f"START err: {e}")

    try:
        bytes(dev.ctrl_transfer(0x80, 0x00, 0, 0, 2, 1000))
        print("GET_STATUS (standard) issued")
    except usb.core.USBError as e:
        print(f"GET_STATUS err (ok if stall/filtered): {e}")

    time.sleep(1.0)
    after = status(args.ip).get("connected_app", {}).get("transport", {})
    keys = (
        "setup_count",
        "vendor_setup_count",
        "drops",
        "aoa51",
        "aoa52",
        "aoa53",
        "aoa_control_count",
    )
    print("delta:")
    for k in keys:
        b, a = before.get(k) or 0, after.get(k) or 0
        print(f"  {k}: {b} -> {a} (Δ{a - b})")

    ok = True
    if (after.get("aoa51") or 0) <= (before.get("aoa51") or 0):
        print("FAIL: aoa51 did not increase — tap/callback may not see host vendor SETUP")
        ok = False
    else:
        print("PASS: aoa51 increased (vendor path instrumented)")
    if (after.get("setup_count") or 0) < (after.get("vendor_setup_count") or 0):
        print("WARN: setup_count < vendor_setup_count")
        ok = False
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
