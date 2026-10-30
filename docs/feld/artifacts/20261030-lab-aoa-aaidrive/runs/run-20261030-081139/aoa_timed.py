#!/usr/bin/env python3
from __future__ import annotations
import json, sys, time, traceback
from datetime import datetime, timezone
from pathlib import Path
import usb.core, usb.util

LOG_PATH = sys.argv[1]
MARKER = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("/tmp/START_AAIDRIVE_NOW")

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
    ("uri", ""),
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

def prepare(dev):
    try:
        if dev.is_kernel_driver_active(0):
            dev.detach_kernel_driver(0)
    except Exception:
        pass
    try:
        dev.set_configuration()
    except Exception:
        pass
    try:
        usb.util.claim_interface(dev, 0)
        log("claim", iface=0)
    except Exception as e:
        log("claim_err", err=str(e))

def bulk_eps(dev):
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

def listen(ep_in, seconds, label):
    chunks = []
    log("listen_start", label=label, seconds=seconds)
    t0 = time.time()
    while time.time() - t0 < seconds:
        try:
            chunk = bytes(ep_in.read(ep_in.wMaxPacketSize, timeout=400))
            chunks.append(chunk)
            log("bulk_in", label=label, n=len(chunk), hex=chunk[:256].hex())
        except usb.core.USBTimeoutError:
            continue
        except usb.core.USBError as e:
            log("bulk_in_err", label=label, err=str(e))
            break
    log("listen_end", label=label, total=sum(map(len, chunks)), chunks=len(chunks))
    return chunks

def send_out(ep_out, data, label):
    try:
        n = ep_out.write(data, timeout=1500)
        log("bulk_out", label=label, n=n, hex=data.hex())
    except Exception as e:
        log("bulk_out_err", label=label, err=str(e), type=type(e).__name__)

def handshake(dev):
    try:
        usb.util.release_interface(dev, 0)
    except Exception:
        pass
    try:
        if dev.is_kernel_driver_active(0):
            dev.detach_kernel_driver(0)
    except Exception:
        pass
    data = bytes(dev.ctrl_transfer(0xC0, 51, 0, 0, 2, 3000))
    log("aoa51", hex=data.hex(), ver=int.from_bytes(data, "little"))
    for idx, (name, s) in enumerate(STRINGS):
        payload = s.encode("utf-8") + b"\x00"
        dev.ctrl_transfer(0x40, 52, 0, idx, payload, 3000)
        log("aoa52", idx=idx, name=name, value=s)
    dev.ctrl_transfer(0x40, 53, 0, 0, None, 3000)
    log("aoa53_start")

def wait_mtp(timeout=120.0):
    log("wait_phone", timeout=timeout, hint="unplug_replug_then_wait")
    t0 = time.time()
    last = None
    saw_gone = False
    while time.time() - t0 < timeout:
        ph, acc = find_phone(), find_acc()[0]
        state = (bool(ph), getattr(ph, "address", None), bool(acc), getattr(acc, "address", None))
        if state != last:
            log("usb_state", phone=state[0], phone_addr=state[1], acc=state[2], acc_addr=state[3])
            last = state
        if ph is None and acc is None:
            saw_gone = True
        if ph is not None and acc is None and (saw_gone or (time.time() - t0) > 8):
            return ph
        time.sleep(0.25)
    return find_phone()

def main():
    open(LOG_PATH, "w").close()
    MARKER.unlink(missing_ok=True)
    log("run_start", note="timed_aaidrive_after_marker")

    phone = wait_mtp(120)
    if phone is None:
        # fallback already accessory
        acc, pid = find_acc()
        if acc is None:
            log("fail", err="timeout")
            return 1
        log("path", path="already_accessory")
    else:
        log("path", path="handshake_from_mtp")
        try:
            handshake(phone)
        except Exception as e:
            log("handshake_err", err=str(e), tb=traceback.format_exc())
            return 1
        try:
            usb.util.dispose_resources(phone)
        except Exception:
            pass
        t0 = time.time()
        acc = None
        while time.time() - t0 < 15:
            acc, pid = find_acc()
            if acc is not None:
                log("accessory", pid=f"{pid:04x}", addr=acc.address, t=round(time.time()-t0, 3))
                break
            time.sleep(0.15)
        if acc is None:
            log("fail", err="no_reenum")
            return 2

    prepare(acc)
    ep_in, ep_out = bulk_eps(acc)
    log("eps", out=hex(ep_out.bEndpointAddress), inn=hex(ep_in.bEndpointAddress))

    # Signal operator NOW
    MARKER.write_text("START_AAIDRIVE_NOW\n", encoding="utf-8")
    log("operator", msg="START_AAIDRIVE_NOW")
    print("======== START AAIDRIVE NOW ========", flush=True)

    # Quiet listen first (app may TX first) — 45s
    listen(ep_in, 45.0, "after_start_signal")
    # then host probes
    for label, blob in [("hex4", bytes.fromhex("12345678")), ("zeros8", b"\x00"*8)]:
        send_out(ep_out, blob, label)
        listen(ep_in, 8.0, f"after_{label}")
    listen(ep_in, 30.0, "final")
    log("run_done")
    MARKER.unlink(missing_ok=True)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
