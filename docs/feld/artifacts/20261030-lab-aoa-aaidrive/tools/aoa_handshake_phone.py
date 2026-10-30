#!/usr/bin/env python3
import time, usb.core, usb.util, sys

VID, PID = 0x22D9, 0x2764
STRINGS = [
    ("manufacturer", "BMW"),
    ("model", "NBT Evo"),
    ("description", "Headunit"),
    ("version", "1.0"),
    ("uri", "http://www.bmw.com"),
    ("serial", "HU-PROBE-LAB"),
]

def find_phone():
    return usb.core.find(idVendor=VID, idProduct=PID)

def find_acc():
    for pid in (0x2D00, 0x2D01, 0x2D04, 0x2D05):
        d = usb.core.find(idVendor=0x18D1, idProduct=pid)
        if d is not None:
            return d, pid
    return None, None

dev = find_phone()
if dev is None:
    print("FAIL: OnePlus not found")
    sys.exit(1)
print(f"phone bus={dev.bus} addr={dev.address}")

try:
    cfg = dev.get_active_configuration()
    for intf in cfg:
        try:
            if dev.is_kernel_driver_active(intf.bInterfaceNumber):
                dev.detach_kernel_driver(intf.bInterfaceNumber)
                print("detached", intf.bInterfaceNumber)
        except Exception as e:
            print("detach", e)
except Exception as e:
    print("cfg", e)

data = bytes(dev.ctrl_transfer(0xC0, 51, 0, 0, 2, 3000))
print("51", data.hex(), "ver", int.from_bytes(data, "little"))

for idx, (name, s) in enumerate(STRINGS):
    payload = s.encode("utf-8") + b"\x00"
    dev.ctrl_transfer(0x40, 52, 0, idx, payload, 3000)
    print(f"52 idx={idx} {name}={s!r} ok")

dev.ctrl_transfer(0x40, 53, 0, 0, None, 3000)
print("53 START ok")

print("waiting for re-enum 18D1:2Dxx ...")
acc = None
pid = None
for i in range(40):
    time.sleep(0.25)
    acc, pid = find_acc()
    if acc is not None:
        print(f"ACCESSORY {i*0.25:.2f}s -> 18d1:{pid:04x} bus={acc.bus} addr={acc.address}")
        break
    if find_phone() is None and i % 4 == 0:
        print(f"  ... phone gone @ {i*0.25:.2f}s")
else:
    print("FAIL: no accessory re-enum")
    for d in usb.core.find(find_all=True):
        if d.bus == 2:
            print(f"  bus2 {d.idVendor:04x}:{d.idProduct:04x} addr={d.address}")
    sys.exit(2)

try:
    cfg = acc.get_active_configuration()
except Exception:
    acc.set_configuration()
    cfg = acc.get_active_configuration()
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
print(f"bulk out={None if ep_out is None else hex(ep_out.bEndpointAddress)} in={None if ep_in is None else hex(ep_in.bEndpointAddress)}")

total = 0
chunks = []
if ep_in is not None:
    t0 = time.time()
    while time.time() - t0 < 8.0:
        try:
            chunk = bytes(ep_in.read(ep_in.wMaxPacketSize, timeout=500))
            total += len(chunk)
            chunks.append(chunk)
            hx = chunk[:64].hex()
            more = "..." if len(chunk) > 64 else ""
            print(f"IN {len(chunk)}B {hx}{more}")
        except usb.core.USBTimeoutError:
            continue
        except usb.core.USBError as e:
            print("IN err", e)
            break
print(f"bulk_in_total={total} chunks={len(chunks)}")
if chunks:
    print("FIRST_BYTES", chunks[0][:32].hex())
print("HANDSHAKE_DONE")
