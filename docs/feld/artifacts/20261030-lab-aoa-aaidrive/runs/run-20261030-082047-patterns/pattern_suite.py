#!/usr/bin/env python3
"""HU-role USB bulk pattern suite against AAIdrive accessory."""
from __future__ import annotations
import json, struct, sys, time
from datetime import datetime, timezone
import usb.core, usb.util

LOG = sys.argv[1]

def log(event, **kw):
    rec = {"ts": datetime.now(timezone.utc).isoformat(), "event": event, **kw}
    print(json.dumps(rec, ensure_ascii=False), flush=True)
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(json.dumps(rec, ensure_ascii=False) + "\n")

def find_acc():
    for pid in (0x2D00, 0x2D01, 0x2D04, 0x2D05):
        d = usb.core.find(idVendor=0x18D1, idProduct=pid)
        if d:
            return d, pid
    return None, None

def bcl_hdr(cmd, src, dest, length):
    return struct.pack(">HHHH", cmd, src, dest, length)

# Desk-derived patterns (bmw-bcl-handshake.md) — HU role first
PATTERNS = [
    # (name, bytes, listen_s)
    ("listen_only_pre", b"", 5.0),
    ("hu_handshake_v4_test", bytes.fromhex("00040000000000080004000e00008000"), 5.0),
    ("hu_handshake_v3", bytes.fromhex("00040000000000080003000100008000"), 4.0),
    ("hu_handshake_v1", bytes.fromhex("00040000000000080001000100001000"), 4.0),
    ("hex4_session_init_phone_role", bytes.fromhex("12345678"), 4.0),
    ("hex4_x3_burst", bytes.fromhex("12345678") * 3, 3.0),
    ("selectproto3_phone_role", bytes.fromhex("00050000000000020003"), 3.0),
    ("knock_default_20b", bytes.fromhex("000800000000000c000000000000000000000001"), 4.0),
    ("open_etch_src1", bytes.fromhex("000100010fa40000"), 3.0),
    ("watchdog_ping", bytes.fromhex("000000001389000413371337"), 3.0),  # guess: dest 0x1389 + ping
    ("zeros4", b"\x00" * 4, 2.0),
    ("zeros8", b"\x00" * 8, 2.0),
    ("zeros16", b"\x00" * 16, 2.0),
    ("deadbeef_etch_magic", bytes.fromhex("deadbeef"), 3.0),
    ("ascii_OKOKOKOK", b"OKOKOKOK", 2.0),
    ("bcl_hdr_cmd4_len0", bcl_hdr(4, 0, 0, 0), 3.0),
    ("bcl_hdr_cmd1_open_etch", bcl_hdr(1, 1, 0x0FA4, 0), 3.0),
    # alternate handshake buffer sizes
    ("hu_handshake_buf1M", bytes.fromhex("00040000000000080004000100100000"), 4.0),
    ("hu_handshake_repeat", bytes.fromhex("00040000000000080004000e00008000") * 2, 4.0),
]

def prepare(dev):
    try:
        if dev.is_kernel_driver_active(0):
            dev.detach_kernel_driver(0)
            log("detach")
    except Exception as e:
        log("detach_err", err=str(e))
    try:
        dev.set_configuration()
        log("set_configuration")
    except Exception as e:
        log("set_configuration_note", err=str(e))
    try:
        usb.util.claim_interface(dev, 0)
        log("claim")
    except Exception as e:
        log("claim_err", err=str(e))

def eps(dev):
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
    t0 = time.time()
    while time.time() - t0 < seconds:
        try:
            chunk = bytes(ep_in.read(min(ep_in.wMaxPacketSize, 512), timeout=300))
            chunks.append(chunk)
            log("bulk_in", label=label, n=len(chunk), hex=chunk[:256].hex())
        except usb.core.USBTimeoutError:
            continue
        except usb.core.USBError as e:
            log("bulk_in_err", label=label, err=str(e), type=type(e).__name__)
            break
    total = sum(map(len, chunks))
    log("listen_end", label=label, total=total, chunks=len(chunks))
    return chunks

def send(ep_out, data, label, timeout=800):
    if not data:
        log("tx_skip", label=label)
        return True
    try:
        n = ep_out.write(data, timeout=timeout)
        log("bulk_out", label=label, n=n, hex=data[:64].hex(), len=len(data))
        return True
    except Exception as e:
        log("bulk_out_err", label=label, err=str(e), type=type(e).__name__, len=len(data))
        return False

def main():
    open(LOG, "w").close()
    log("suite_start", role="HU_against_AAIdrive_accessory")
    acc, pid = find_acc()
    if not acc:
        log("fail", err="no_accessory")
        return 1
    log("accessory", pid=f"{pid:04x}", addr=acc.address)
    prepare(acc)
    ep_in, ep_out = eps(acc)
    if not ep_in or not ep_out:
        log("fail", err="no_eps")
        return 1
    log("eps", inn=hex(ep_in.bEndpointAddress), out=hex(ep_out.bEndpointAddress),
        wMaxIn=ep_in.wMaxPacketSize, wMaxOut=ep_out.wMaxPacketSize)

    any_rx = False
    any_tx_ok = False
    for name, data, listen_s in PATTERNS:
        log("pattern", name=name, tx_len=len(data))
        if data:
            ok = send(ep_out, data, name)
            any_tx_ok = any_tx_ok or ok
            # small gap
            time.sleep(0.05)
        chunks = listen(ep_in, listen_s, name)
        if chunks:
            any_rx = True
            log("RESPONSE", name=name, first_hex=chunks[0][:64].hex())

    # Final long listen in case phone responds late
    chunks = listen(ep_in, 10.0, "final_idle")
    if chunks:
        any_rx = True
    log("suite_done", any_rx=any_rx, any_tx_ok=any_tx_ok)
    return 0 if any_tx_ok else 2

if __name__ == "__main__":
    raise SystemExit(main())
