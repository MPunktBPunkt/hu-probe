# Abschlussbericht: hu-probe C1 Identity Gate Stand (FW 0.3.36)

| | |
|--|--|
| **Datum** | 2026-11-06 |
| **Repo** | MPunktBPunkt/hu-probe |
| **FW** | **0.3.36** |
| **Quellen** | esp32.usb-analyzer 0.0.52 Feldmatrix; technology-audit PR #32 |

**Methodenherkunft:** Die Host-Behaviour-/Fingerprinting-Methode wurde aus den realen Feldmessungen des `esp32.usb-analyzer` übernommen; sie ist dort durch BMW/TV/Soundbar/FRITZ!Box/Windows-Messwerte empirisch validiert.  
**Nur die tatsächlich auf hu-probe beobachtbaren Größen dürfen als MEASURED bezeichnet werden.**

---

## FACT

**Geändert / implementiert (Software):**

- B1 sticky `usb_gadget`: Load-Precedence über `huprobe::resolveGadgetMode` (sticky `hpgad` > legacy); Host-Regression `tools/gadget_nvs`.
- SessionMetrics + UsbSessionTimeline: Counter mit **null=UNKNOWN** vs **0=gemessen absent**; Status unter `connected_app.session_metrics`.
- Neuer Boot-Mode **`aoa_id_303a`**: Vendor-only `0x303A:0x1001` (kein MSC) für Identity-Isolation.
- MSC: SCSI-Opcode-Histogramm inkl. `mode_sense6` / `prevent_allow` (aus CBW-Tap).
- Factory-Reset-API dokumentiert (`POST /api/factory-reset`).
- Host-Tests: `gadget_nvs`, `session_metrics`, `bcl_c1a` grün; Firmware-Build `huprobe-s3` OK.

**Bereits bekannte Feld-FACTS (unverändert, nicht neu gemessen):**

- `aoa_observe` `18D1:2D00` → HU Zubehör + Reconnect; Callback 51/52/53 absent; Bulk RX absent (2026-10-30).
- `aoa_phone_like` `04E8:6860` → HU REJECTED.
- Analyzer MSC `303a:1001` am NBT → P-MOUNT inkl. MODE_SENSE(6)+PREVENT; AOA 51/52/53=0 (Analyzer 2026-11-06).

**BMW-Feld mit FW 0.3.36:** in dieser Session **nicht** ausgeführt → siehe NEXT.

---

## FIX

| Bug | Ursache | Fix |
|-----|---------|-----|
| B1 `usb_gadget` nicht sticky (Lab 0.3.34) | Mode im crowded `huprobe`-Blob; silent drop | Dedicated NVS `hpgad`/`mode` + verify (0.3.35); host-getestete Resolve-Logik + `aoa_id_303a` in Valid-Set (0.3.36) |

Nicht symptomatisch „nur UI“: Save-Pfad schreibt zuerst `hpgad`; Load: sticky gewinnt. Soft-Detach ändert Mode nicht.

---

## MEASURED (hu-probe Instrument)

Auf AOA-Plane (`visibility_scope=usbvendor_onrequest_only`):

`attach_count`, `detach_count`, `reconnect_count`, `control_setup_count`, `vendor_setup_count`, `aoa_51/52/53`, `aoa51_respond_ok`, `bulk_rx/tx`, `rx/tx_bytes`, Timeline-Events, `first/last/session_duration_ms`.

Auf MSC-Plane: Plug/Unplug, SCSI-Opcode-Histogramm (`tur`, `inquiry`, `mode_sense6`, `prevent_allow`, `read_capacity10`, `read10`, `write10`, `other`), Read/Write Counts+Bytes.

---

## INFERENCE

- Analyzer-MSC-Negativ (`303a:1001` → kein AOA) und hu-probe Accessory-Positiv-UI (`18D1:2D00`) zusammen legen nahe, dass **Identity + Class/Configuration** den HU-Pfad steuern — nicht „MSC-Endpoint antwortet“.
- `aoa_id_303a` ist der nächste empirische Schritt, um VID/PID vs. MSC-Class zu trennen.

---

## UNKNOWN

- Ob die HU auf dem Bus 51/52/53 sendet, wenn der Vendor-Callback 0 zählt (EP0 außerhalb `USBVendor.onRequest`).
- Welche Identity/Config den AOA-/Connected-App-Pfad am NBT auslöst.
- BCL/Etch/RHMI am HU-USB-Bus.
- `enumeration_count` / `configuration_count` / `control_in/out` / READ-size-/LBA-Histogramme auf hu-probe (explizit null).
- BMW-Reaktion auf `aoa_id_303a` (noch nicht im Feld).

---

## REJECTED

- „BMW unterstützt kein AOA“ — nur aus MSC-Identity-Negativ oder Callback=0.
- „BMW sendet keinen USB-Verkehr“ — aus `setup_count=0` / `rx=0`.
- „phone-like Identity reicht für AOA“ — Feld REJECTED bei `04E8:6860`.
- „DEADBEEF/Etch/BCL bereits am BMW-HU-USB gemessen“.
- Soft-Detach als Mode-Switch; accidental MSC+Vendor Composite.

---

## NEXT

1. Lab-Checkliste sticky + Soft-Detach auf Board abhaken.  
2. BMW **I-303A** mit FW 0.3.36 / `aoa_id_303a` — eine Variable.  
3. Ergebnis als FACT Identity+Config→HU-Reaktion **oder** FACT kein AOA-Start auf Messschicht.  
4. EP0-Core-Hook erst wenn Identity-Gate die Messlücke als Blocker bestätigt.

Kampagne: [2026-11-06-c1-identity-gate-kampagne.md](2026-11-06-c1-identity-gate-kampagne.md)
