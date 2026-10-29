# C1b-P0 AOA Observe — Feld 2026-10-29 (F20 / NBT Evo)

**Übergabe an nächste KI** · Auswerten / Hypothesen priorisieren · nicht sofort C1b-P1.

| | |
|--|--|
| **Gerät** | Auto-ESP `192.168.178.89` (PiDrive-USB) |
| **FW** | **0.3.23** (`aoa_observe` Boot) |
| **Lab** | `.88` ebenfalls 0.3.22/23 getestet |
| **Gate** | HU-PROBE-GATE-C1b-P0 ([GATES-Connected.md](../GATES-Connected.md)) |
| **Desk-Basis** | technology-audit `usb-aoa-bmw-connected-2026-desk` (EVID-AOA-2026-10-28-*) |
| **Session (letzte)** | `20261029-074709_lab_ca_c1_link` (Collector `192.168.178.187:9100`) |

---

## Executive Summary

Boot-Mode `usb_gadget=aoa_observe` (kein MSC) funktioniert. Die NBT Evo **erkennt** die Probe als USB-Zubehör („AOA Observe“, UI **Zubehörmodus**).  
**Kein** AOA-Control 51/52/53 und **kein** Bulk-RX trotz vielfachem Stecken (`attach` bis 6).  
Connected-App-Byte-Rohr ist damit **nicht** belegt. P0-B und P0-E bleiben OPEN.

---

## Was implementiert wurde (Software)

- `ConfigStore.usbGadget`: `msc` \| `aoa_observe` — Umschalten nur NVS + Reboot (P0-0)
- `AoaObserveGadget` + `AoaControlTap` + `AoaTransport` (`impl=observe`)
- Events `ca_usb` / `ca_transport`; **kein** Auto-`BclFramer` (P1 aus)
- WebUI-Tabs: Messung / Connected / System
- `/api/usb/cycle` auch in `aoa_observe` (Achtung: Soft-Cycle killt oft WiFi/Coex)
- Desk-Hypothese: Boot schon als Google Accessory `VID 0x18D1` / `PID 0x2D00`, Product „AOA Observe“
- 51/52/53 Hypothesis-Responder (falls Host sendet); String-Capture für req 52 (B3)
- Post-53 VID-Re-Enum: **NOT IMPLEMENTED** (Arduino TinyUSB lockt Descriptor)

Artefakte: `firmware/src/connected/`, Docs `CONNECTED-APP.md`, `GATES-Connected.md`, `SCHEMA.md` (`ca_usb`).

---

## Feld-Beobachtungen (MEASURED / Operator)

### USB / Probe (remote Status)

| Befund | Wert |
|--------|------|
| `usb_gadget` | `aoa_observe` |
| `msc_interface_present` | false |
| `vendor_interface_present` | true |
| `usb_stack_bound` | true |
| VID/PID präsentiert | `0x18D1:0x2D00` (Desk-Hypothese am Boot) |
| P0-0 / P0-A / P0-C / P0-D | beobachtet (Enum + mounted + Identity) |
| P0-B (51/52/53) | **0** über alle Läufe |
| P0-E (Bulk RX) | **0** |
| `attach_count` | mehrfach bis 6 (Replug) |

### iDrive UI (Operator)

1. Gerät erscheint **nicht** in USB-**Medienliste** — erwartet (kein MSC).
2. Anzeige sinngemäß: **USB-Gerät: Zubehörmodus** / Name **AOA Observe**.
3. Text sinngemäß:  
   - (1) Um Dateien auf dem Gerät aufzurufen, **USB-Modus aktivieren**.  
   - (2) Gerät entfernen und über USB verbinden.
4. Operator hat „USB-Modus“ aktiviert + OTG raus/rein — Probe sah Re-Enum/Reboot, **weiterhin kein** 51/52/53/Bulk.

### Ops-Fallen

- Zweiter USB (WLAN/OTG-Netz) vs. Daten-USB zur HU verwechselt → Probe offline für Remote-Messung.
- Soft-Cycle (`/api/usb/cycle`) unter OTG → WiFi oft DOWN (Coex). Für Feld-Observe **kein** Soft-Cycle.

---

## Epistemik

| Claim | Klasse | Hinweis |
|-------|--------|---------|
| HU enumeriert `aoa_observe`-Gadget / zeigt Zubehör | **MEASURED** (C) | UI + `vendor_mounted` / P0-A |
| AOA Control 51→52→53 am Wire | **absent** in dieser Session | Zähler 0 |
| BCL/Bulk Connected-Stream | **absent** / UNKNOWN weiter | P0-E fail diese Runde |
| Schon-Accessory-Boot reicht für Connected | **HYPOTHESIS verworfen oder geschwächt** | Enum ja, App-Rohr nein |
| „USB-Modus“ = MSC/MTP-Datei-Pfad | **HYPOTHESIS** stark | UI-Text „Dateien“; wir reagieren nicht |
| HU erwartet Phone-like Enum + eigenen 51→52→53 | **HYPOTHESIS** (Desk AOSP + AAIdrive) | nächster Versuch |
| HU erwartet nach Accessory eine Geräte-Aktion (App/Ack) | **HYPOTHESIS** (User) | siehe unten |

Desk-Referenz unverändert: AOA ≠ BCL; Accessory = USB Host = HU; Device = Phone-Rolle.

---

## Hypothese: USB-Modus / erwartete Aktion

**Beobachtung:** HU bietet bei Accessory-Erkennung einen Pfad „USB-Modus aktivieren“ (Dateien) an, nicht „Connected starten“.

**Deutung A (wahrscheinlich):** BMW-UI für Android-Accessory ohne passende Connected-App: Host will Gerät in **Mass Storage / File-Transfer** schalten. Unsere Probe hat keinen Phone-UI-Schalter und bleibt Vendor-AOA → Aktion verpufft. Passt zu AAIdrive-Desk „File Transfer oft vor Accessory“ (EVID-AOA-014) — hier umgekehrt: Accessory ohne File-Pfad.

**Deutung B:** Connected erwartet Identity/Strings/Filter (Manufacturer BMW am Host → Phone App). Wir booten schon als Google Accessory ohne vorherigen 52-String-Austausch → HU startet keinen App-Bulk.

**Deutung C (User):** HU wartet auf eine **explizite Geräte-Antwort** auf „USB-Modus“ (Control/Vendor/Mode-Switch). Wir loggen nur Vendor-SETUPs die TinyUSB an `USBVendor` liefert; Standard-Requests ggf. unvollständig sichtbar. **Zu prüfen:** alle Control-SETUPs loggen (nicht nur Vendor), und/oder MSC-Composite nur nach explizitem „USB-Modus“-Signal (widerspricht P0-0 Isolation — nur als separates Experiment).

**Nicht tun ohne neuen Gate-Schnitt:** Soft Composite MSC+AOA im gleichen Boot (zerstört MSC-Fingerprint-Vergleichbarkeit).

---

## Empfohlene nächste Schritte (Priorität)

1. **Auswertung dieser Session** (Collector/`ca_*` Events) + Bericht bestätigen.
2. **Experiment Phone-like Enum** (neuer Sub-Gate, z. B. `aoa_observe_phone`):
   - Espressif/Android-ähnliche VID/PID (nicht `0x18D1`)
   - 51/52/53 beantworten + String-Log (B3: HU sendet Manufacturer?)
   - Nach 53: Descriptor-Override + Soft-Reconnect auf `0x18D1:0x2D00` (heute NOT IMPLEMENTED — blockierend klären)
3. Parallel dokumentieren: Kill-Kriterium Audit K1 teilweise — „Switch zu 0x18D1“ entfällt wenn schon Accessory; K1 neu: „Bulk/Connected nach Accessory-Erkennung“.
4. Optional Lab: Linux-Host libusb sendet 51/52/53 → Desk-Verify Responder.
5. **Erst danach** C1b-P1 (`BclFramer` auf Wire-Bulk).

---

## PASS / FAIL Snapshot C1b-P0

| Level | Status Feld |
|-------|-------------|
| P0-0 Gadget isolation | PASS |
| P0-A Attach/Enum | PASS |
| P0-B AOA Control | FAIL / absent |
| P0-C VID/PID path | PASS (presented + HU UI Zubehör) |
| P0-D Bulk EPs identified | PASS (stack); Traffic nein |
| P0-E Bulk data | FAIL / absent |

**Gesamt P0 Wire-Ziel (B1 Connected-Rohr):** noch nicht erreicht — aber **positive Enum-Evidenz** und klare UI-Reaktion der HU.

---

## Dateien / Bins

- FW: `firmware/dist/hu-probe.0.3.23.esp32s3.bin`
- Code: `firmware/src/connected/**`, Config `usbGadget`, WebUI Tabs
- Docs: dieses File, `docs/CONNECTED-APP.md`, `docs/GATES-Connected.md`

## Ops Reminder

```text
usb_gadget=aoa_observe  → Connected Tab → link start
Zurück MSC: usb_gadget=msc + Reboot
Nicht Soft-Cycle unter Auto-OTG wenn WLAN-Messung nötig
```
