# Lab MEASURED — hu-probe 0.3.36 sticky NVS + AOA Lab A/B (.88×.89)

| | |
|--|--|
| **Datum** | 2026-11-06 |
| **Topology** | `.88` usb-analyzer Host 0.0.52 OTG ↔ USB-C ↔ `.89` hu-probe **0.3.36** |
| **Artefakte** | `artifacts/20261106-lab-aoa-ab.json` · `tools/lab_c1_sticky_aoa.py` |

**Not claimed:** BMW-HU wire · BCL · Bus-EP0 außerhalb `USBVendor.onRequest`.

---

## FACT — Sticky NVS (B1 Regression)

Save → `gadget/reboot` → Boot-Readback für alle vier Modes:

| Mode | sticky |
|------|--------|
| `msc` | PASS |
| `aoa_observe` | PASS |
| `aoa_phone_like` | PASS |
| `aoa_id_303a` | PASS |

Soft-Detach (`/api/usb/cycle`) ändert Mode **nicht**.

---

## FACT — AOA Lab A/B (foreign DUT)

Boot: `aoa_observe` · VID/PID `18D1:2D00` · Host sieht DUT `18d1:2d00` / product `AOA Observe`.

| Lab | Host | DUT counters |
|-----|------|--------------|
| **A** `hypothesis_respond=true` | `ok` · protocolOk · stringsOk=6 · startOk | `aoa51`+1 · `aoa52`+6 · `aoa53`+1 · `aoa51_respond_ok`+1 · timeline 1→17 |
| **B** `hypothesis_respond=false` | HTTP 500 · `err=aoa51_fail` | `aoa51`+1 · **`aoa51_respond_ok` unverändert** |

`session_metrics.enumeration_count` = **null** (UNKNOWN) — nicht als 0 gemeldet.  
`visibility_scope` = `usbvendor_onrequest_only`.

---

## Ops-Hinweis

Unter OTG kann `.88` LocalWeb hängen (TCP accept, kein HTTP). Soft-Detach DUT oder Heartbeat/OTA-Push erholen den Host. Hub sah `.88` weiter online.

---

## NEXT

BMW I-303A mit Mode `aoa_id_303a` (Lab sticky bereits PASS).
