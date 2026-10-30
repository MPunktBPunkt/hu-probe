#!/usr/bin/env python3
"""Phase-1 AOA host run against phone: handshake + bulk RX/TX logging."""
from __future__ import annotations
import json, sys, time, traceback
from datetime import datetime, timezone
import usb.core, usb.util

LOG_PATH = sys.argv[1] if len(sys.argv) > 1 else "/tmp/aoa_run.jsonl"

def log(event: str, **kw):
    rec = {"ts": datetime.now(timezone.utc).isoformat(), "event": event, **kw}
    line = json.dumps(rec, ensure_ascii=False)
    print(line, flush=True)
    with open(LOG_PATH, "a", encoding="utf-8") as f:
        f.write(line + "\n")

STRINGS = [
    ("manufacturer", "BMW"),
    ("model", "NBT Evo"),
    ("description", "Headunit"),
    ("version", "1.0"),
    ("uri", "http://www.bmw.com"),
    ("serial", "HU-PROBE-LAB"),
]

def find_phone():
    return usb.core.find(idVendor=0x22D9, idProduct=0x2764)

def find_acc():
    for pid in (0x2D00, 0x2D01, 0x2D04, 0x2D05):
        d = usb.core.find(idVendor=0x18D1, idProduct=pid)
        if d is not None:
            return d, pid
    return None, None

def detach_all(dev):
    try:
        cfg = dev.get_active_configuration()
    except Exception as e:
        log("cfg_err", err=str(e))
        try:
            dev.set_configuration()
            cfg = dev.get_active_configuration()
        except Exception as e2:
            log("set_cfg_err", err=str(e2))
            return
    for intf in cfg:
        try:
            if dev.is_kernel_driver_active(intf.bInterfaceNumber):
                dev.detach_kernel_driver(intf.bInterfaceNumber)
                log("detach", iface=intf.bInterfaceNumber)
        except Exception as e:
            log("detach_err", iface=intf.bInterfaceNumber, err=str(e))

def bulk_eps(dev):
    try:
        cfg = dev.get_active_configuration()
    except Exception:
        dev.set_configuration()
        cfg = dev.get_active_configuration()
    intf = cfg[(0, 0)]
    ep_in = usb.util.find_descriptor(
        intf,
        custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_IN
        and usb.util.endpoint_type(e.bmAttributes) == usb.util.ENDPOINT_TYPE_BULK,
    )
    ep_out = usb.util.find_descriptor(
        intf,
        custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_OUT
        and usb.util.endpoint_type(e.bmAttributes) == usb.util.ENDPOINT_TYPE_BULK,
    )
    return ep_in, ep_out

def listen(ep_in, seconds: float, label: str):
    if ep_in is None:
        log("listen_skip", label=label, reason="no_ep_in")
        return []
    chunks = []
    t0 = time.time()
    log("listen_start", label=label, seconds=seconds)
    while time.time() - t0 < seconds:
        try:
            chunk = bytes(ep_in.read(ep_in.wMaxPacketSize, timeout=500))
            chunks.append(chunk)
            log("bulk_in", label=label, n=len(chunk), hex=chunk[:128].hex())
        except usb.core.USBTimeoutError:
            continue
        except usb.core.USBError as e:
            log("bulk_in_err", label=label, err=str(e))
            break
    log("listen_end", label=label, total=sum(len(c) for c in chunks), chunks=len(chunks))
    return chunks

def send_out(ep_out, data: bytes, label: str):
    if ep_out is None:
        log("bulk_out_skip", label=label)
        return
    n = ep_out.write(data, timeout=2000)
    log("bulk_out", label=label, n=n, hex=data.hex())

def handshake(dev):
    detach_all(dev)
    data = bytes(dev.ctrl_transfer(0xC0, 51, 0, 0, 2, 3000))
    log("aoa51", hex=data.hex(), ver=int.from_bytes(data, "little"))
    for idx, (name, s) in enumerate(STRINGS):
        payload = s.encode("utf-8") + b"\x00"
        dev.ctrl_transfer(0x40, 52, 0, idx, payload, 3000)
        log("aoa52", idx=idx, name=name, value=s)
    dev.ctrl_transfer(0x40, 53, 0, 0, None, 3000)
    log("aoa53_start")

def wait_acc(timeout=12.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        acc, pid = find_acc()
        if acc is not None:
            log("accessory", pid=f"{pid:04x}", bus=acc.bus, addr=acc.address, t=time.time() - t0)
            return acc, pid
        time.sleep(0.2)
    return None, None

def main():
    open(LOG_PATH, "w").close()
    log("run_start")
    phone = find_phone()
    acc, pid = find_acc()
    log("inventory", phone=bool(phone), phone_addr=getattr(phone, "address", None),
        acc=bool(acc), acc_pid=(f"{pid:04x}" if pid else None), acc_addr=getattr(acc, "address", None))

    # If already accessory: listen + HU-like knocks. Else handshake first.
    if acc is not None and phone is None:
        log("path", path="already_accessory")
        ep_in, ep_out = bulk_eps(acc)
        log("eps", out=None if ep_out is None else hex(ep_out.bEndpointAddress),
            inn=None if ep_in is None else hex(ep_in.bEndpointAddress))
        listen(ep_in, 5.0, "pre_tx")
        # Desk BCL knocks / probes (host=HU role)
        for label, blob in [
            ("hex4", bytes.fromhex("12345678")),
            ("bcl_knock_ascii8", b"OKOKOKOK"),  # rejected desk pattern, still logged A/B
            ("zeros8", b"\x00" * 8),
        ]:
            send_out(ep_out, blob, label)
            listen(ep_in, 3.0, f"after_{label}")
        listen(ep_in, 15.0, "final_idle")
        log("run_done", mode="already_accessory")
        return 0

    if phone is None:
        log("fail", err="no_phone_no_acc")
        return 1

    log("path", path="handshake")
    try:
        handshake(phone)
    except Exception as e:
        log("handshake_err", err=str(e), tb=traceback.format_exc())
        return 1

    acc, pid = wait_acc()
    if acc is None:
        log("fail", err="no_reenum")
        return 2

    ep_in, ep_out = bulk_eps(acc)
    log("eps", out=None if ep_out is None else hex(ep_out.bEndpointAddress),
        inn=None if ep_in is None else hex(ep_in.bEndpointAddress))
    listen(ep_in, 5.0, "post_enum")
    for label, blob in [
        ("hex4", bytes.fromhex("12345678")),
        ("zeros8", b"\x00" * 8),
    ]:
        send_out(ep_out, blob, label)
        listen(ep_in, 4.0, f"after_{label}")
    listen(ep_in, 20.0, "final_idle")
    log("run_done", mode="handshake")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
