#!/usr/bin/env python3
from __future__ import annotations
import json, struct, sys, time
from datetime import datetime, timezone
import usb.core, usb.util

LOG = sys.argv[1]
STRINGS = [("manufacturer","BMW"),("model","NBT Evo"),("description","Headunit"),
           ("version","1.0"),("uri",""),("serial","HU-PROBE-LAB")]

def log(event, **kw):
    rec={"ts":datetime.now(timezone.utc).isoformat(),"event":event,**kw}
    print(json.dumps(rec), flush=True)
    open(LOG,"a").write(json.dumps(rec)+"\n")

def find_phone():
    return usb.core.find(idVendor=0x22D9, idProduct=0x2764)
def find_acc():
    for pid in (0x2D00,0x2D01,0x2D04,0x2D05):
        d=usb.core.find(idVendor=0x18D1, idProduct=pid)
        if d: return d, pid
    return None, None

def bcl_hdr(cmd,src,dest,length):
    return struct.pack(">HHHH", cmd,src,dest,length)

PATTERNS = [
    ("pre_listen", b"", 3.0),
    ("hu_handshake_v4", bytes.fromhex("00040000000000080004000e00008000"), 5.0),
    ("hu_handshake_v3", bytes.fromhex("00040000000000080003000100008000"), 4.0),
    ("hex4", bytes.fromhex("12345678"), 4.0),
    ("hex4_repeat_1s_x5", None, 0),  # special
    ("selectproto3", bytes.fromhex("00050000000000020003"), 3.0),
    ("knock20", bytes.fromhex("000800000000000c000000000000000000000001"), 4.0),
    ("open_etch", bytes.fromhex("000100010fa40000"), 3.0),
    ("deadbeef", bytes.fromhex("deadbeef"), 2.0),
    ("zeros8", b"\x00"*8, 2.0),
]

def listen(ep_in, seconds, label):
    chunks=[]; t0=time.time()
    while time.time()-t0<seconds:
        try:
            c=bytes(ep_in.read(512, timeout=250)); chunks.append(c)
            log("bulk_in", label=label, n=len(c), hex=c[:128].hex())
        except usb.core.USBTimeoutError:
            continue
        except usb.core.USBError as e:
            log("bulk_in_err", label=label, err=str(e)); break
    log("listen_end", label=label, total=sum(map(len,chunks)), chunks=len(chunks))
    return chunks

def send(ep_out, data, label):
    try:
        n=ep_out.write(data, timeout=1000)
        log("bulk_out", label=label, n=n, hex=data[:64].hex())
        return True
    except Exception as e:
        log("bulk_out_err", label=label, err=str(e), type=type(e).__name__)
        return False

open(LOG,"w").close()
log("start")

# Wait for MTP phone (user may need to replug)
log("wait_mtp")
ph=None
for i in range(60):
    ph=find_phone(); acc,_=find_acc()
    if ph and not acc:
        log("phone", addr=ph.address); break
    if i==0 or i%5==0:
        log("poll", phone=bool(ph), acc=bool(acc))
    time.sleep(0.5)
else:
    # try handshake from existing if only phone
    ph=find_phone()
if not ph:
    log("fail", err="no_phone"); sys.exit(1)

try:
    if ph.is_kernel_driver_active(0): ph.detach_kernel_driver(0)
except Exception: pass
data=bytes(ph.ctrl_transfer(0xC0,51,0,0,2,3000))
log("aoa51", hex=data.hex())
for idx,(_,s) in enumerate(STRINGS):
    ph.ctrl_transfer(0x40,52,0,idx,s.encode()+b"\x00",3000)
ph.ctrl_transfer(0x40,53,0,0,None,3000)
log("aoa53")
try: usb.util.dispose_resources(ph)
except Exception: pass

acc=None
t0=time.time()
while time.time()-t0<12:
    acc,pid=find_acc()
    if acc:
        log("accessory", pid=f"{pid:04x}", addr=acc.address, t=round(time.time()-t0,3))
        break
    time.sleep(0.1)
if not acc:
    log("fail", err="no_reenum"); sys.exit(2)

# claim immediately
try:
    if acc.is_kernel_driver_active(0): acc.detach_kernel_driver(0)
except Exception: pass
try: acc.set_configuration()
except Exception: pass
try: usb.util.claim_interface(acc,0); log("claim")
except Exception as e: log("claim_err", err=str(e))

cfg=acc.get_active_configuration(); intf=cfg[(0,0)]
ep_in=usb.util.find_descriptor(intf, custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress)==usb.util.ENDPOINT_IN)
ep_out=usb.util.find_descriptor(intf, custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress)==usb.util.ENDPOINT_OUT)
log("eps", inn=hex(ep_in.bEndpointAddress), out=hex(ep_out.bEndpointAddress))
print("======== HANDSHAKE OK — AAIdrive should show connected; patterns starting ========", flush=True)
time.sleep(1.0)  # let AAIdrive open accessory FD

any_rx=False; any_tx=False
for name, data, lis in PATTERNS:
    if name=="hex4_repeat_1s_x5":
        log("pattern", name=name)
        for i in range(5):
            if send(ep_out, bytes.fromhex("12345678"), f"hex4_{i}"): any_tx=True
            ch=listen(ep_in, 1.0, f"hex4_{i}")
            if ch: any_rx=True
        continue
    log("pattern", name=name, tx_len=len(data) if data else 0)
    if data:
        if send(ep_out, data, name): any_tx=True
        time.sleep(0.05)
    ch=listen(ep_in, lis, name)
    if ch:
        any_rx=True
        log("RESPONSE", name=name, hex=ch[0][:64].hex())

ch=listen(ep_in, 8.0, "final")
if ch: any_rx=True
log("done", any_rx=any_rx, any_tx=any_tx)
print(f"RESULT any_tx={any_tx} any_rx={any_rx}", flush=True)
