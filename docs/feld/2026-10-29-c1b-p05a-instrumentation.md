# C1b-P0.5-A — Control-Tap Instrumentation + Phone-like B1 Prep

**Datum:** 2026-10-29  
**FW:** **0.3.27** (P0.5-A 0.3.24 · phone-like 0.3.25 · softAttach-on-link 0.3.26 · **P0-0 MSC leak fix** 0.3.27)  
**Audit:** technology-audit PR #28 / `C1b-PLAN.md`

## Was gebaut wurde

### P0.5-A (observation-only)
- `AoaControlTap` publiziert **alle** SETUPs, die `USBVendor.onRequest` erreichen
- Counter: `setup_count`, `vendor_setup_count`, `drops` (+ Legacy `aoa_control_count` = Vendor)
- Events: `stage`, `request_type`
- **Keine** Änderung an TX / BCL / Enum / hypothesis_respond-Logik

### B1 Prep: `aoa_phone_like`
- Neuer NVS-Boot: `usb_gadget=aoa_phone_like` (XOR mit `msc` / `aoa_observe`)
- Identity: `0x04E8:0x6860`, Manufacturer `Android`, Product `AOA Phone-like`
- Gleicher USBVendor-only Stack (kein MSC)
- Ziel: HU kann klassischen 51→52→53-Pfad versuchen (Desk-Hypothese)

## Lab-Fund 2026-10-29: P0-0 MSC-Leak (behoben in 0.3.27)

Host sah unter `aoa_observe` trotzdem **Composite MSC(08)+Vendor(ff)**, weil `static USBMSC`
im Konstruktor `tinyusb_enable_interface(MSC)` aufruft — auch ohne `MscGadget::begin()`.
Fix: USBMSC erst in `MscGadget::begin()` allokieren. Verifikation: Host nur noch 1× Interface class=ff.

Soft-Cycle unter OTG am Lab kann WiFi/Reboot auslösen — lieber `link/start` softAttach oder
`POST /api/usb/remount` → `aoa_soft_attach`.

## Lab-Instrument-Check

```bash
# Probe .88, OTG Host↔Probe, pyusb (venv)
python3 -m venv .venv-lab && .venv-lab/bin/pip install pyusb
.venv-lab/bin/python tools/lab_aoa_control_tap_check.py --ip 192.168.178.88
```

Erwartet: `aoa51` steigt nach Host-`GET_PROTOCOL`; zeigt, dass der Tap Vendor-SETUPs sieht.

**Lab-Host-Hinweis (DebianCursor LXC):** sysfs zeigt `18d1:2d00`, aber `/dev/bus/usb/00x/0yy`
fehlt → pyusb `ctrl_transfer` = ENODEV. Descriptor-Check (Interface-Anzahl) geht; Control-Tap-Check
braucht USB-Device-Node-Passthrough.

## Feld (Auto) — wenn .89 online

1. OTA `hu-probe.0.3.25.esp32s3.bin`
2. Optional zuerst weiter `aoa_observe` + Link-Start → prüfen `setup_count`/`drops` während HU „USB-Modus“
3. Dann NVS `aoa_phone_like` + Reboot + Link-Start → 51/52/53?

## Nicht tun
Composite MSC+AOA, Knock-TX, BclFramer-on-wire, Post-53 Re-Enum-Engineering.
