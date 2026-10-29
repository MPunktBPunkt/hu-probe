#!/usr/bin/env python3
"""Lab instrument check for C1b-P0.5-A AoaControlTap coverage.

Sends known AOA vendor SETUPs (51/52/53) from a Linux host to the probe over
USB OTG and compares host TX with probe /api/status counters.

Requires:
  - probe online (default 192.168.178.88) in aoa_observe or aoa_phone_like
  - USB cable host↔probe OTG data
  - pyusb + libusb (pip install pyusb)

Does not change probe protocol replies beyond what hypothesis_respond already does.
"""

from __future__ import annotations

import argparse
import sys
import time
import urllib.request
import json

try:
    import usb.core
    import usb.util
except ImportError:
    print("NEED: pip install pyusb", file=sys.stderr)
    sys.exit(2)

# Accessory observe default
VID_ACC, PID_ACC = 0x18D1, 0x2D00
# Phone-like B1
VID_PH, PID_PH = 0x04E8, 0x6860


def status(ip: str) -> dict:
    with urllib.request.urlopen(f"http://{ip}/api/status", timeout=3) as r:
        return json.load(r)


def find_dev():
    for vid, pid, name in (
        (VID_ACC, PID_ACC, "accessory"),
        (VID_PH, PID_PH, "phone_like"),
    ):
        d = usb.core.find(idVendor=vid, idProduct=pid)
        if d is not None:
            return d, name, vid, pid
    return None, None, None, None


def ctrl_out(dev, bm, breq, wValue=0, wIndex=0, data=None, timeout=1000):
    return dev.ctrl_transfer(bm, breq, wValue, wIndex, data if data is not None else 0, timeout)


def ctrl_in(dev, bm, breq, wValue=0, wIndex=0, length=2, timeout=1000):
    return bytes(dev.ctrl_transfer(bm, breq, wValue, wIndex, length, timeout))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--ip", default="192.168.178.88")
    ap.add_argument("--skip-usb", action="store_true", help="only print probe status")
    args = ap.parse_args()

    st = status(args.ip)
    ca = st.get("connected_app") or {}
    tr = ca.get("transport") or {}
    print(
        f"probe fw={st.get('fw')} gadget={st.get('usb_gadget') or ca.get('usb_gadget')} "
        f"setup={tr.get('setup_count')} vendor={tr.get('vendor_setup_count')} "
        f"drops={tr.get('drops')} 51/52/53={tr.get('aoa51')}/{tr.get('aoa52')}/{tr.get('aoa53')}"
    )
    if args.skip_usb:
        return 0

    # arm observe if possible
    try:
        req = urllib.request.Request(
            f"http://{args.ip}/api/connected/link/start", method="POST", data=b""
        )
        with urllib.request.urlopen(req, timeout=5) as r:
            print("arm", r.status, r.read()[:120])
    except Exception as e:
        print("arm note:", e)

    before = status(args.ip).get("connected_app", {}).get("transport", {})

    dev, name, vid, pid = find_dev()
    if dev is None:
        print("USB device not found (expected 18D1:2D00 or 04E8:6860). Plug OTG?", file=sys.stderr)
        return 1
    print(f"usb {name} {vid:04x}:{pid:04x}")

    # Detach kernel driver if any
    try:
        if dev.is_kernel_driver_active(0):
            dev.detach_kernel_driver(0)
    except Exception:
        pass

    # AOA GET_PROTOCOL (51) IN vendor device
    try:
        data = ctrl_in(dev, 0xC0, 51, 0, 0, 2)
        print(f"GET_PROTOCOL -> {data.hex()} (expect 0100 LE = AOA1)")
    except usb.core.USBError as e:
        print(f"GET_PROTOCOL err: {e}")

    # SEND_STRING (52) OUT — manufacturer index 0
    try:
        payload = b"LabHost\x00"
        ctrl_out(dev, 0x40, 52, 0, 0, payload)
        print(f"SEND_STRING idx0 len={len(payload)} ok")
    except usb.core.USBError as e:
        print(f"SEND_STRING err: {e}")

    # START (53) OUT
    try:
        ctrl_out(dev, 0x40, 53, 0, 0, None)
        print("START ok")
    except usb.core.USBError as e:
        print(f"START err: {e}")

    # also a non-vendor SETUP if host stack allows — often filtered before callback
    try:
        ctrl_in(dev, 0x80, 0x00, 0, 0, 2)  # GET_STATUS device (standard)
        print("GET_STATUS (standard) issued")
    except usb.core.USBError as e:
        print(f"GET_STATUS err (ok if stall): {e}")

    time.sleep(0.8)
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
    ok = True
    for k in keys:
        b, a = before.get(k) or 0, after.get(k) or 0
        print(f"  {k}: {b} -> {a} (Δ{a - b})")
    if (after.get("aoa51") or 0) <= (before.get("aoa51") or 0):
        print("FAIL: aoa51 did not increase — tap/callback may not see host vendor SETUP")
        ok = False
    else:
        print("PASS: aoa51 increased (vendor path instrumented)")
    if (after.get("setup_count") or 0) < (after.get("vendor_setup_count") or 0):
        print("WARN: setup_count < vendor_setup_count (unexpected)")
        ok = False
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
