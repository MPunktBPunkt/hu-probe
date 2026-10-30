#!/usr/bin/env python3
"""Lab AOA interface control — HTTP probe plane + USB host plane.

Controls the DebianCursor↔hu-probe lab cut (default .88). USB host actions
need a live OTG data cable AND /dev/bus/usb nodes in this LXC (sysfs alone
is not enough — ctrl_transfer → ENODEV).

Subcommands:
  doctor          inventory HTTP + USB + usbfs readiness
  status          compact probe Connected status
  gadget          set usbGadget (NVS); optional --reboot
  arm             link/start + softAttach (no Soft-Cycle)
  disarm          link/stop
  soft-attach     remount → aoa_soft_attach only
  host-scan       list AOA-relevant USB devices (sysfs/pyusb)
  host-passthrough  pct-set current AOA usbfs node into LXC (via PVE SSH)
  host-handshake  send 51 → 52(strings) → 53; compare probe counters
  host-bulk       Bulk OUT (optional) + IN listen; compare probe rx counters
  tap-check       same as tools/lab_aoa_control_tap_check.py (compat)

Examples:
  .venv-lab/bin/python tools/lab_aoa_iface.py doctor
  .venv-lab/bin/python tools/lab_aoa_iface.py gadget aoa_observe --reboot
  .venv-lab/bin/python tools/lab_aoa_iface.py arm
  .venv-lab/bin/python tools/lab_aoa_iface.py host-handshake --profile bmw_lab
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

try:
    import usb.core
    import usb.util
except ImportError:
    usb = None  # type: ignore

VID_ACC, PID_ACC = 0x18D1, 0x2D00
VID_PH, PID_PH = 0x04E8, 0x6860

# AOA string indices (AOSP): manufacturer, model, description, version, URI, serial
STRING_PROFILES = {
    "lab": ("LabHost", "hu-probe-lab", "Lab AOA Host", "0.1", "https://local/lab", "LAB001"),
    # Hypothesis strings for Connected-stack experiments — NOT measured NBT values.
    "bmw_lab": ("BMW", "NBT", "Headunit", "1.0", "http://www.bmw.com", "HU-PROBE-LAB"),
    "minimal": ("Android", "AOA", "Accessory", "1.0", "http://localhost", "1"),
}


def http_json(url: str, method: str = "GET", body: bytes | None = None, timeout: float = 8):
    req = urllib.request.Request(url, data=body, method=method)
    if body is not None:
        req.add_header("Content-Type", "application/json")
    with urllib.request.urlopen(req, timeout=timeout) as r:
        raw = r.read()
        return r.status, json.loads(raw.decode() or "{}") if raw else {}


def http_json_ok(url: str, method: str = "GET", body: bytes | None = None, timeout: float = 8):
    try:
        return http_json(url, method=method, body=body, timeout=timeout)
    except urllib.error.HTTPError as e:
        raw = e.read()
        try:
            data = json.loads(raw.decode() or "{}")
        except Exception:
            data = {"raw": raw[:200].decode(errors="replace")}
        return e.code, data
    except Exception as e:
        return 0, {"err": str(e)}


def probe_status(ip: str) -> dict:
    _, d = http_json(f"http://{ip}/api/status")
    return d


def ca_transport(st: dict) -> dict:
    return ((st.get("connected_app") or {}).get("transport") or {})


def print_status(st: dict) -> None:
    ca = st.get("connected_app") or {}
    tr = ca.get("transport") or {}
    p0 = ca.get("p0") or {}
    wifi = st.get("wifi") or {}
    print(
        f"fw={st.get('fw')} gadget={st.get('usb_gadget') or ca.get('usb_gadget')} "
        f"profile={ca.get('aoa_identity_profile')} link={ca.get('link_observing')} "
        f"mounted={tr.get('vendor_mounted')} wifi_rssi={wifi.get('rssi')}"
    )
    print(
        f"  P0-0={p0.get('P0_0')} A={p0.get('P0_A')} B={p0.get('P0_B')} "
        f"C={p0.get('P0_C')} D={p0.get('P0_D')} E={p0.get('P0_E')}"
    )
    print(
        f"  setup={tr.get('setup_count')}/{tr.get('vendor_setup_count')} "
        f"drops={tr.get('drops')} 51/52/53={tr.get('aoa51')}/{tr.get('aoa52')}/{tr.get('aoa53')} "
        f"rx={tr.get('rx_bytes')} tx={tr.get('tx_bytes')} bulk_rx_ev={tr.get('bulk_rx_events')}"
    )


def usbfs_path(bus: int, addr: int) -> str:
    return f"/dev/bus/usb/{bus:03d}/{addr:03d}"


def usbfs_ok(dev) -> tuple[bool, str]:
    path = usbfs_path(dev.bus, dev.address)
    if os.path.exists(path):
        return True, path
    return False, path


def sysfs_usb_inventory() -> list[dict]:
    root = Path("/sys/bus/usb/devices")
    out = []
    if not root.is_dir():
        return out
    for d in sorted(root.iterdir()):
        vid_p = d / "idVendor"
        if not vid_p.is_file():
            continue
        def cat(name: str) -> str:
            p = d / name
            return p.read_text().strip() if p.is_file() else ""

        bus = int(cat("busnum") or 0)
        addr = int(cat("devnum") or 0)
        node = usbfs_path(bus, addr) if bus and addr else ""
        out.append(
            {
                "sysfs": d.name,
                "vid": cat("idVendor"),
                "pid": cat("idProduct"),
                "product": cat("product"),
                "manufacturer": cat("manufacturer"),
                "bus": bus,
                "addr": addr,
                "usbfs": node,
                "usbfs_exists": bool(node and os.path.exists(node)),
            }
        )
    return out


def find_aoa_targets(retries: int = 1, delay: float = 0.3):
    if usb is None:
        return []
    found = []
    for _ in range(retries):
        for vid, pid, name in (
            (VID_ACC, PID_ACC, "accessory_2d00"),
            (0x18D1, 0x2D01, "accessory_2d01"),
            (VID_PH, PID_PH, "phone_like"),
        ):
            for d in usb.core.find(find_all=True, idVendor=vid, idProduct=pid) or []:
                found.append((d, name, vid, pid))
        if found:
            break
        time.sleep(delay)
    return found


def ensure_armed(ip: str) -> None:
    st = probe_status(ip)
    ca = st.get("connected_app") or {}
    if not ca.get("link_observing"):
        code, d = http_json_ok(f"http://{ip}/api/connected/link/start", method="POST", body=b"")
        if code == 409:
            http_json_ok(f"http://{ip}/api/session/stop", method="POST", body=b"")
            http_json_ok(f"http://{ip}/api/connected/link/stop", method="POST", body=b"")
            time.sleep(0.5)
            code, d = http_json_ok(f"http://{ip}/api/connected/link/start", method="POST", body=b"")
        print(f"arm http={code} ok={d.get('ok')} err={d.get('err')}")
    code, d = http_json_ok(f"http://{ip}/api/usb/remount", method="POST", body=b"{}")
    print(f"attach http={code} action={d.get('action')} ok={d.get('ok')} note={d.get('note')}")
    time.sleep(1.2)


def cmd_doctor(args: argparse.Namespace) -> int:
    print(f"=== HTTP probe {args.ip} ===")
    code, st = http_json_ok(f"http://{args.ip}/api/status", timeout=4)
    if code != 200:
        print(f"FAIL: probe unreachable ({code}) {st}")
        probe_ok = False
    else:
        print_status(st)
        probe_ok = True
        g = st.get("usb_gadget") or (st.get("connected_app") or {}).get("usb_gadget")
        if g not in ("aoa_observe", "aoa_phone_like"):
            print(f"WARN: usb_gadget={g} — host-handshake needs aoa_* boot")

    print("=== USB sysfs (this LXC) ===")
    inv = sysfs_usb_inventory()
    aoa_ish = []
    for e in inv:
        mark = ""
        if e["vid"] in ("18d1", "04e8") or e["pid"] in ("2d00", "2d01", "6860"):
            mark = "  << AOA-ish"
            aoa_ish.append(e)
        print(
            f"  {e['sysfs']:8} {e['vid']}:{e['pid']} bus={e['bus']} addr={e['addr']} "
            f"usbfs={'OK' if e['usbfs_exists'] else 'MISSING'} {e['product'][:40]}{mark}"
        )
    if not inv:
        print("  (no USB devices in sysfs)")

    print("=== usbfs /dev/bus/usb ===")
    bus_root = Path("/dev/bus/usb")
    if not bus_root.is_dir() or not any(bus_root.iterdir()):
        print("  EMPTY — ctrl_transfer impossible in this container")
        print("  Fix on Proxmox host: pass cgroup device nodes for bus/usb into LXC")
        print("  (sysfs mount alone is insufficient; need /dev/bus/usb/BBB/DDD).")
        usbfs_ready = False
    else:
        nodes = list(bus_root.glob("*/*"))
        print(f"  {len(nodes)} node(s) present")
        usbfs_ready = True

    print("=== pyusb ===")
    if usb is None:
        print("  FAIL: pyusb missing — .venv-lab/bin/pip install pyusb")
        pyusb_ok = False
    else:
        print("  OK")
        pyusb_ok = True
        targets = find_aoa_targets()
        if targets:
            for d, name, vid, pid in targets:
                ok, path = usbfs_ok(d)
                print(f"  target {name} {vid:04x}:{pid:04x} → {path} ({'ready' if ok else 'ENODEV'})")
        else:
            print("  no 18D1:2D0x / 04E8:6860 currently enumerated")

    print("=== collector ===")
    try:
        with urllib.request.urlopen("http://127.0.0.1:8088/", timeout=2) as r:
            print(f"  web :8088 → HTTP {r.status}")
    except Exception as e:
        print(f"  web :8088 → down ({e})")
    try:
        import socket

        s = socket.create_connection(("127.0.0.1", 9100), timeout=1)
        s.close()
        print("  tcp :9100 → open")
    except Exception as e:
        print(f"  tcp :9100 → closed ({e})")

    print("=== verdict ===")
    http_ctrl = probe_ok
    usb_host = pyusb_ok and usbfs_ready and bool(aoa_ish or find_aoa_targets())
    print(f"  HTTP probe control: {'YES' if http_ctrl else 'NO'}")
    print(f"  USB host control:   {'YES' if usb_host else 'NO'}")
    if http_ctrl and not usb_host:
        if not aoa_ish and probe_ok:
            print("  → Plug Lab OTG data cable: host hub ↔ probe .88 (not charge-only).")
        if not usbfs_ready:
            print("  → Restore usbfs device nodes in LXC, then re-run doctor.")
    return 0 if http_ctrl else 1


def cmd_status(args: argparse.Namespace) -> int:
    print_status(probe_status(args.ip))
    return 0


def cmd_gadget(args: argparse.Namespace) -> int:
    mode = args.mode
    if mode not in ("msc", "aoa_observe", "aoa_phone_like"):
        print(f"bad mode {mode}", file=sys.stderr)
        return 2
    body = json.dumps({"usbGadget": mode}).encode()
    code, d = http_json_ok(f"http://{args.ip}/api/config", method="POST", body=body)
    print(f"config http={code} {d}")
    if not args.reboot:
        print("NVS saved; boot identity changes only after --reboot")
        return 0 if code == 200 else 1
    code, d = http_json_ok(f"http://{args.ip}/api/connected/gadget/reboot", method="POST", body=b"")
    print(f"reboot http={code} {d}")
    print("waiting for probe…")
    for i in range(40):
        time.sleep(1.0)
        c, st = http_json_ok(f"http://{args.ip}/api/status", timeout=2)
        if c == 200:
            g = st.get("usb_gadget") or (st.get("connected_app") or {}).get("usb_gadget")
            print(f"back online fw={st.get('fw')} gadget={g} after {i+1}s")
            print_status(st)
            return 0 if g == mode else 1
    print("FAIL: probe did not return", file=sys.stderr)
    return 1


def cmd_arm(args: argparse.Namespace) -> int:
    ensure_armed(args.ip)
    print_status(probe_status(args.ip))
    return 0


def cmd_disarm(args: argparse.Namespace) -> int:
    code, d = http_json_ok(f"http://{args.ip}/api/connected/link/stop", method="POST", body=b"")
    print(f"disarm http={code} {d.get('ok')} {d.get('err')}")
    return 0 if code in (200, 409) else 1


def cmd_soft_attach(args: argparse.Namespace) -> int:
    code, d = http_json_ok(f"http://{args.ip}/api/usb/remount", method="POST", body=b"{}")
    print(f"http={code} action={d.get('action')} ok={d.get('ok')} note={d.get('note')}")
    return 0 if d.get("ok") else 1


def cmd_host_passthrough(args: argparse.Namespace) -> int:
    """Map current AOA sysfs device → /dev/bus/usb into this LXC via Proxmox pct set."""
    import subprocess

    targets = []
    for e in sysfs_usb_inventory():
        if e["vid"] in ("18d1", "04e8") and e["pid"] in ("2d00", "2d01", "6860"):
            targets.append(e)
    if not targets:
        print("no AOA-ish device in sysfs — arm/soft-attach first?", file=sys.stderr)
        return 1
    e = targets[0]
    host_path = f"/dev/bus/usb/{e['bus']:03d}/{e['addr']:03d}"
    print(f"passthrough {e['sysfs']} {e['vid']}:{e['pid']} → CT{args.vmid} {host_path}")
    # Proxmox unprivileged LXC: same pattern as ttyACM0 (devN).
    cmd = [
        "ssh",
        "-o",
        "BatchMode=yes",
        "-o",
        "ConnectTimeout=8",
        args.pve,
        f"pct set {args.vmid} -dev{args.dev_slot} path={host_path},mode=0666",
    ]
    try:
        out = subprocess.check_output(cmd, text=True, stderr=subprocess.STDOUT)
        if out.strip():
            print(out.strip())
    except subprocess.CalledProcessError as ex:
        print(ex.output or ex, file=sys.stderr)
        # pct may warn about unrelated missing devices but still apply
        if "does not exist" not in (ex.output or ""):
            return 1
    time.sleep(0.4)
    local = usbfs_path(e["bus"], e["addr"])
    if os.path.exists(local):
        print(f"OK local {local}")
        return 0
    print(f"WARN: {local} still missing in LXC — check pct config / cgroup", file=sys.stderr)
    return 3


def cmd_host_scan(_args: argparse.Namespace) -> int:
    if usb is None:
        print("NEED pyusb", file=sys.stderr)
        return 2
    for e in sysfs_usb_inventory():
        print(
            f"{e['sysfs']:8} {e['vid']}:{e['pid']} {e['product']!r} "
            f"usbfs={'OK' if e['usbfs_exists'] else 'MISSING'}"
        )
    targets = find_aoa_targets()
    if not targets:
        print("no AOA targets")
        return 1
    for d, name, vid, pid in targets:
        ok, path = usbfs_ok(d)
        print(f"TARGET {name} {vid:04x}:{pid:04x} {path} ready={ok}")
    return 0


def _claim(dev) -> None:
    try:
        if dev.is_kernel_driver_active(0):
            dev.detach_kernel_driver(0)
    except Exception as e:
        print(f"detach_kernel: {e}")
    try:
        usb.util.claim_interface(dev, 0)
    except Exception as e:
        print(f"claim_interface: {e}")


def cmd_host_handshake(args: argparse.Namespace) -> int:
    if usb is None:
        print("NEED pyusb", file=sys.stderr)
        return 2
    strings = STRING_PROFILES[args.profile]
    if args.arm:
        ensure_armed(args.ip)
    before = ca_transport(probe_status(args.ip)) if not args.no_probe else {}

    targets = find_aoa_targets(retries=8, delay=0.4)
    if not targets:
        print("USB target not found (18D1:2D0x or 04E8:6860). Cable/OTG?", file=sys.stderr)
        return 1
    dev, name, vid, pid = targets[0]
    ok, path = usbfs_ok(dev)
    print(f"usb {name} {vid:04x}:{pid:04x} bus={dev.bus} addr={dev.address}")
    if not ok:
        print(
            f"FAIL: usbfs node missing ({path}). "
            "Host sees sysfs descriptors but cannot issue control transfers in this LXC.",
            file=sys.stderr,
        )
        return 3

    _claim(dev)

    # 51 GET_PROTOCOL
    try:
        data = bytes(dev.ctrl_transfer(0xC0, 51, 0, 0, 2, 2000))
        print(f"51 GET_PROTOCOL → {data.hex()} (AOA1=0100 LE)")
    except usb.core.USBError as e:
        print(f"51 err: {e}")

    # 52 SEND_STRING for each index
    for idx, s in enumerate(strings):
        payload = s.encode("utf-8") + b"\x00"
        try:
            dev.ctrl_transfer(0x40, 52, 0, idx, payload, 2000)
            print(f"52 SEND_STRING idx={idx} {s!r} ok")
        except usb.core.USBError as e:
            print(f"52 idx={idx} err: {e}")

    # optional 58 ACCESSORY_GET_STRING? skip — AOSP host path uses 51/52/53
    try:
        dev.ctrl_transfer(0x40, 53, 0, 0, 0, 2000)
        print("53 START ok")
    except usb.core.USBError as e:
        print(f"53 err: {e}")

    time.sleep(1.0)
    if args.no_probe:
        return 0
    after = ca_transport(probe_status(args.ip))
    print("probe delta:")
    ok_pass = True
    for k in ("setup_count", "vendor_setup_count", "aoa51", "aoa52", "aoa53", "drops"):
        b, a = before.get(k) or 0, after.get(k) or 0
        print(f"  {k}: {b} → {a} (Δ{a - b})")
    if (after.get("aoa51") or 0) <= (before.get("aoa51") or 0):
        print("FAIL: aoa51 did not increase")
        ok_pass = False
    else:
        print("PASS: vendor SETUP path seen (aoa51↑)")
    # After START, phone_like may re-enum away; accessory boot stays.
    print_status(probe_status(args.ip))
    return 0 if ok_pass else 1


def cmd_host_bulk(args: argparse.Namespace) -> int:
    if usb is None:
        print("NEED pyusb", file=sys.stderr)
        return 2
    before = ca_transport(probe_status(args.ip)) if not args.no_probe else {}
    targets = find_aoa_targets(retries=6, delay=0.4)
    acc = [t for t in targets if t[1].startswith("accessory")]
    pick = (acc or targets)
    if not pick:
        print("no USB target", file=sys.stderr)
        return 1
    dev, name, vid, pid = pick[0]
    ok, path = usbfs_ok(dev)
    print(f"usb {name} {vid:04x}:{pid:04x}")
    if not ok:
        print(f"FAIL: usbfs missing {path} — run host-passthrough", file=sys.stderr)
        return 3
    cfg = dev.get_active_configuration()
    intf = cfg[(0, 0)]
    ep_out = usb.util.find_descriptor(
        intf, custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_OUT
    )
    ep_in = usb.util.find_descriptor(
        intf, custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_IN
    )
    print(f"eps out={None if ep_out is None else hex(ep_out.bEndpointAddress)} "
          f"in={None if ep_in is None else hex(ep_in.bEndpointAddress)}")
    out_hex = args.out_hex or ("12 34 56 78" if args.hex4 else "")
    if out_hex:
        if ep_out is None:
            print("no OUT ep", file=sys.stderr)
            return 1
        data = bytes.fromhex(out_hex.replace(" ", ""))
        n = ep_out.write(data, timeout=2000)
        print(f"OUT {n} B: {data.hex()}")
        time.sleep(0.3)
    if ep_in is not None and args.seconds > 0:
        deadline = time.time() + args.seconds
        total = 0
        while time.time() < deadline:
            try:
                chunk = bytes(ep_in.read(ep_in.wMaxPacketSize, timeout=500))
                total += len(chunk)
                print(f"IN {len(chunk)} B: {chunk[:64].hex()}{'…' if len(chunk) > 64 else ''}")
            except usb.core.USBTimeoutError:
                continue
            except usb.core.USBError as e:
                print(f"IN err: {e}")
                break
        print(f"bulk_in_total={total}")
    if not args.no_probe:
        after = ca_transport(probe_status(args.ip))
        br, ar = before.get("rx_bytes") or 0, after.get("rx_bytes") or 0
        print(f"probe rx_bytes {br} → {ar} (Δ{ar - br}) bulk_rx_ev={after.get('bulk_rx_events')}")
        print_status(probe_status(args.ip))
        if out_hex and ar <= br:
            print("FAIL: probe did not see Bulk OUT", file=sys.stderr)
            return 1
        if out_hex:
            print("PASS: host Bulk OUT observed on probe")
    return 0


def cmd_tap_check(args: argparse.Namespace) -> int:
    # Compat path: arm + minimal 51/52/53 like lab_aoa_control_tap_check.py
    args.profile = "lab"
    args.arm = True
    args.no_probe = False
    return cmd_host_handshake(args)


def build_parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ip", default="192.168.178.88", help="Lab probe (default .88)")
    sp = ap.add_subparsers(dest="cmd", required=True)

    sp.add_parser("doctor", help="HTTP + USB + usbfs readiness")
    sp.add_parser("status", help="probe Connected status")

    p = sp.add_parser("gadget", help="set usbGadget in NVS")
    p.add_argument("mode", choices=("msc", "aoa_observe", "aoa_phone_like"))
    p.add_argument("--reboot", action="store_true")

    sp.add_parser("arm", help="link/start + softAttach")
    sp.add_parser("disarm", help="link/stop")
    sp.add_parser("soft-attach", help="aoa_soft_attach only")
    sp.add_parser("host-scan", help="USB inventory / AOA targets")

    p = sp.add_parser("host-passthrough", help="pct-set AOA usbfs node into LXC")
    p.add_argument("--pve", default="root@192.168.178.108")
    p.add_argument("--vmid", type=int, default=100)
    p.add_argument("--dev-slot", type=int, default=3, help="pct devN slot (default 3)")

    p = sp.add_parser("host-handshake", help="AOA 51/52/53 from this host")
    p.add_argument("--profile", choices=sorted(STRING_PROFILES), default="lab")
    p.add_argument("--arm", action="store_true", help="arm+attach before TX")
    p.add_argument("--no-probe", action="store_true", help="USB only (e.g. real phone)")

    p = sp.add_parser("host-bulk", help="Bulk OUT/IN against probe or phone")
    p.add_argument("--seconds", type=float, default=1.0)
    p.add_argument("--out-hex", default="", help="hex bytes to Bulk OUT first")
    p.add_argument("--hex4", action="store_true", help="OUT 12 34 56 78 (B2b knock)")
    p.add_argument("--no-probe", action="store_true")

    sp.add_parser("tap-check", help="compat: arm + lab 51/52/53 counter check")
    return ap


def main(argv: list[str] | None = None) -> int:
    ap = build_parser()
    args = ap.parse_args(argv)
    dispatch = {
        "doctor": cmd_doctor,
        "status": cmd_status,
        "gadget": cmd_gadget,
        "arm": cmd_arm,
        "disarm": cmd_disarm,
        "soft-attach": cmd_soft_attach,
        "host-scan": cmd_host_scan,
        "host-passthrough": cmd_host_passthrough,
        "host-handshake": cmd_host_handshake,
        "host-bulk": cmd_host_bulk,
        "tap-check": cmd_tap_check,
    }
    return dispatch[args.cmd](args)


if __name__ == "__main__":
    sys.exit(main())
