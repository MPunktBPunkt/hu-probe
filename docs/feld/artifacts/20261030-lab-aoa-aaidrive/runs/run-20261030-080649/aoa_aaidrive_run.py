#!/usr/bin/env python3
from __future__ import annotations
import json, sys, time, traceback
from datetime import datetime, timezone
import usb.core, usb.util

LOG_PATH = sys.argv[1] if len(sys.argv) > 1 else "/tmp/aoa_aaidrive.jsonl"

def log(event: str, **kw):
    rec = {"ts": datetime.now(timezone.utc).isoformat(), "event": event, **kw}
    line = json.dumps(rec, ensure_ascii=False)
    print(line, flush=True)
    with open(LOG_PATH, "a", encoding="utf-8") as f:
        f.write(line + "\n")

# URI empty to avoid browser popup; keep BMW manufacturer for AAIdrive filter
STRINGS = [
    ("manufacturer", "BMW"),
    ("model", "NBT Evo"),
    ("description", "Headunit"),
    ("version", "1.0"),
    ("uri", ""),  # no landing page
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
            log("detach", iface=0)
    except Exception as e:
        log("detach_err", err=str(e))
    try:
        dev.set_configuration()
        log("set_configuration")
    except Exception as e:
        log("set_configuration_err", err=str(e))
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
    if ep_in is None:
        log("listen_skip", label=label)
        return chunks
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
        return True
    except Exception as e:
        log("bulk_out_err", label=label, err=str(e), type=type(e).__name__)
        return False

def handshake(dev):
    # Control transfers: don't require claim; release claim if held
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

def wait_phone(timeout=90.0):
    log("wait_phone", timeout=timeout)
    t0 = time.time()
    last = None
    while time.time() - t0 < timeout:
        ph = find_phone()
        acc, pid = find_acc()
        state = (bool(ph), getattr(ph, "address", None), bool(acc), getattr(acc, "address", None))
        if state != last:
            log("usb_state", phone=state[0], phone_addr=state[1], acc=state[2], acc_addr=state[3],
                acc_pid=(f"{pid:04x}" if pid else None))
            last = state
        # Prefer fresh MTP phone for new handshake
        if ph is not None and acc is None:
            return ph, None, None
        # If only accessory (sticky from before), return that
        if acc is not None and ph is None and (time.time() - t0) > 3:
            # wait a bit more for replug to MTP
            pass
        time.sleep(0.3)
    return find_phone(), *find_acc()

def main():
    open(LOG_PATH, "w").close()
    log("run_start", note="no_uri_bmw_strings_full_usb_log")
    phone, acc, pid = wait_phone(90)
    if phone is None and acc is None:
        log("fail", err="timeout_no_device")
        return 1

    if phone is not None:
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
        # wait re-enum
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
    else:
        log("path", path="already_accessory", pid=f"{pid:04x}")

    prepare(acc)
    ep_in, ep_out = bulk_eps(acc)
    log("eps", out=None if not ep_out else hex(ep_out.bEndpointAddress),
        inn=None if not ep_in else hex(ep_in.bEndpointAddress))

    # Long window while user starts AAIdrive
    log("operator", msg="start_AAIdrive_now")
    listen(ep_in, 20.0, "aaidrive_window_1")
    # gentle host probes (timeouts logged, not fatal)
    for label, blob in [("hex4", bytes.fromhex("12345678")), ("zeros8", b"\x00"*8)]:
        send_out(ep_out, blob, label)
        listen(ep_in, 5.0, f"after_{label}")
    listen(ep_in, 40.0, "aaidrive_window_2")
    log("run_done")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
