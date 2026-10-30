#!/usr/bin/env python3
import json, sys, time, traceback
from datetime import datetime, timezone
import usb.core, usb.util

LOG = sys.argv[1]
STRINGS = [
    ("manufacturer", "BMW"),
    ("model", "NBT Evo"),
    ("description", "Headunit"),
    ("version", "1.0"),
    ("uri", ""),
    ("serial", "HU-PROBE-LAB"),
]

def log(event, **kw):
    rec = {"ts": datetime.now(timezone.utc).isoformat(), "event": event, **kw}
    print(json.dumps(rec), flush=True)
    with open(LOG, "a") as f:
        f.write(json.dumps(rec) + "\n")

def find_phone():
    return usb.core.find(idVendor=0x22D9, idProduct=0x2764)

def find_acc():
    for pid in (0x2D00, 0x2D01, 0x2D04, 0x2D05):
        d = usb.core.find(idVendor=0x18D1, idProduct=pid)
        if d: return d, pid
    return None, None

def listen(ep_in, seconds, label):
    log("listen_start", label=label, seconds=seconds)
    t0=time.time(); n=0; c=0
    while time.time()-t0 < seconds:
        try:
            chunk=bytes(ep_in.read(ep_in.wMaxPacketSize, timeout=400))
            n+=len(chunk); c+=1
            log("bulk_in", label=label, n=len(chunk), hex=chunk[:256].hex())
        except usb.core.USBTimeoutError:
            continue
        except usb.core.USBError as e:
            log("bulk_in_err", label=label, err=str(e)); break
    log("listen_end", label=label, total=n, chunks=c)

open(LOG,"w").close()
log("handshake_begin")
ph=find_phone()
if not ph:
    log("fail", err="no_phone_mtp"); sys.exit(1)
log("phone", addr=ph.address)
try:
    if ph.is_kernel_driver_active(0):
        ph.detach_kernel_driver(0)
except Exception as e:
    log("detach_err", err=str(e))

data=bytes(ph.ctrl_transfer(0xC0,51,0,0,2,3000))
log("aoa51", hex=data.hex(), ver=int.from_bytes(data,"little"))
for idx,(name,s) in enumerate(STRINGS):
    ph.ctrl_transfer(0x40,52,0,idx,s.encode()+b"\x00",3000)
    log("aoa52", idx=idx, name=name, value=s)
ph.ctrl_transfer(0x40,53,0,0,None,3000)
log("aoa53_start")
try: usb.util.dispose_resources(ph)
except Exception: pass

acc=None
t0=time.time()
while time.time()-t0<12:
    acc,pid=find_acc()
    if acc:
        log("accessory", pid=f"{pid:04x}", addr=acc.address, t=round(time.time()-t0,3))
        break
    time.sleep(0.15)
if not acc:
    log("fail", err="no_reenum"); sys.exit(2)

try:
    if acc.is_kernel_driver_active(0):
        acc.detach_kernel_driver(0)
except Exception: pass
try: acc.set_configuration()
except Exception: pass
try: usb.util.claim_interface(acc,0)
except Exception as e: log("claim_err", err=str(e))

cfg=acc.get_active_configuration(); intf=cfg[(0,0)]
ep_in=usb.util.find_descriptor(intf, custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress)==usb.util.ENDPOINT_IN)
ep_out=usb.util.find_descriptor(intf, custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress)==usb.util.ENDPOINT_OUT)
log("eps", inn=hex(ep_in.bEndpointAddress), out=hex(ep_out.bEndpointAddress))
log("operator", msg="WATCH_CAR_ICON_now_then_AAIdrive")
print("======== HANDSHAKE DONE — Auto-Symbol checken, dann AAIdrive ========", flush=True)
listen(ep_in, 50.0, "post_handshake")
for label,blob in [("hex4", bytes.fromhex("12345678")), ("zeros8", b"\x00"*8)]:
    try:
        n=ep_out.write(blob, timeout=1500)
        log("bulk_out", label=label, n=n, hex=blob.hex())
    except Exception as e:
        log("bulk_out_err", label=label, err=str(e))
    listen(ep_in, 8.0, f"after_{label}")
listen(ep_in, 25.0, "final")
log("run_done")
