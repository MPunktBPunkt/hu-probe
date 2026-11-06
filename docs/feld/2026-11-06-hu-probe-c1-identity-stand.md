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
- Host-Tests: `gadget_nvs`, `session_metrics`, `bcl_c1a` — lokal und via CI `.github/workflows/host-tests.yml` (Binaries gitignored).
- Lab MEASURED 2026-11-06: sticky alle 4 Modes PASS; AOA Lab A/B PASS; MSC-Novel Instrument PASS ([Lab-Berichte](2026-11-06-lab-c1-036-sticky-aoa.md)).

**Bereits bekannte Feld-FACTS (unverändert, nicht neu gemessen):**

- `aoa_observe` `18D1:2D00` → HU Zubehör + Reconnect; Callback 51/52/53 absent; Bulk RX absent (2026-10-30).
- `aoa_phone_like` `04E8:6860` → HU REJECTED.
- Analyzer MSC `303a:1001` am NBT → P-MOUNT inkl. MODE_SENSE(6)+PREVENT; AOA 51/52/53=0 (Analyzer 2026-11-06).

**BMW-Feld mit FW 0.3.36:** **nicht** ausgeführt → **I-303A** ist der Engpass ([Runsheet](2026-11-06-i303a-feld-runsheet.md)).

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
- **H-Reconnect:** Reconnect-Loop bei `18D1:2D00` *kann* fehlenden BCL/Etch/SAS-Peer (G3/G4) bedeuten statt reinem USB-Transportfehler — nicht messbar auf aktueller Schicht ([Kampagne](2026-11-06-c1-identity-gate-kampagne.md)).

---

## UNKNOWN

- Ob die HU auf dem Bus 51/52/53 sendet, wenn der Vendor-Callback 0 zählt (EP0 außerhalb `USBVendor.onRequest`).
- Welche Identity/Config den AOA-/Connected-App-Pfad am NBT auslöst.
- BCL/Etch/RHMI/**SAS** am HU-USB-Bus (G3/G4).
- `enumeration_count` / `configuration_count` / `control_in/out` / READ-size-/LBA-Histogramme auf hu-probe (explizit null).
- BMW-Reaktion auf `aoa_id_303a` (noch nicht im Feld).

---

## REJECTED

- „BMW unterstützt kein AOA“ — nur aus MSC-Identity-Negativ oder Callback=0.
- „BMW sendet keinen USB-Verkehr“ — aus `setup_count=0` / `rx=0`.
- „phone-like Identity reicht für AOA“ — Feld REJECTED bei `04E8:6860`.
- „DEADBEEF/Etch/BCL bereits am BMW-HU-USB gemessen“.
- „AOA-Zubehör / Bulk ⇒ Connected-App (SAS) offen“ — G4 braucht Cert+Challenge; außerhalb hu-probe-Scope.
- „MyBMW exportiert SECURITY_SERVICE / liefert signChallenge“ als aktueller Default — nur Classic Connected ≤5.1 **[D]**; ≥5.1/MyBMW Export **weg**.
- „Jede Connected-App ist direkter Etch-Client“ — CarAPI: Connected führt `sas_*` stellvertretend (Audit PR #34).
- Soft-Detach als Mode-Switch; accidental MSC+Vendor Composite.

**Scope-FACT (Desk):** G1 (+teilweise G2 AOA) = hu-probe; BCL/Etch/SAS/RHMI = Phone/Connected-Teilprojekt. Referenz: [GATES-Connected](../GATES-Connected.md#referenzmodell--vier-gates-usb--connected) · PiDrive [HU-Technical-Facts §10](https://github.com/MPunktBPunkt/pidrive/blob/main/docs/fahrzeug/HU-Technical-Facts.md).

---

## NEXT

**Phase:** weg von Protokoll-Doku → ein instrumentierter Realtest.

| Prio | Handlung |
|------|----------|
| **P0** | **I-303A** — G1-Abschluss only ([Runsheet](2026-11-06-i303a-feld-runsheet.md)); keine Identity-Matrix danach |
| **P1** | **FIELD-CONNECTED-REAL-01** — Phone×MyBMW/AAIdrive×F20×Analyzer, 4 Ebenen ([Plan](2026-11-06-field-connected-real-01.md)) |
| **P2** | **FX-G3-01** — wenn Bulk da: `sas_*`-Sequenz korrelieren |
| **P3** | ESP-Nachbau nur mit REAL-PHONE-Referenz |

**Freeze:** Blind-Knock · BCL-on-ESP · SAS-FW · L0 als Blocker · weitere Desk-Serie ohne Feldartefakt.

Kampagne: [2026-11-06-c1-identity-gate-kampagne.md](2026-11-06-c1-identity-gate-kampagne.md)
