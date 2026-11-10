# Abschlussbericht: hu-probe C1 Identity Gate Stand (FW 0.3.36)

| | |
|--|--|
| **Datum** | 2026-11-07 (Feld I-303A) · Soft/Lab 2026-11-06 |
| **Repo** | MPunktBPunkt/hu-probe |
| **FW** | **0.3.36** |
| **Quellen** | esp32.usb-analyzer 0.0.52 Feldmatrix; technology-audit PR #32 · [I303A-Feld](I303A-FELD-2026-11-07.md) |

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
- Lab MEASURED 2026-11-06: sticky alle 4 Modes PASS; AOA Lab A/B PASS; MSC-Novel Instrument PASS ([Lab-Berichte](lab/C1-036-STICKY-AOA.md)).

**Bereits bekannte Feld-FACTS (unverändert, nicht neu gemessen):**

- `aoa_observe` `18D1:2D00` → HU Zubehör + Reconnect; Callback 51/52/53 absent; Bulk RX absent (2026-10-30).
- `aoa_phone_like` `04E8:6860` → HU REJECTED.
- Analyzer MSC `303a:1001` am NBT → P-MOUNT inkl. MODE_SENSE(6)+PREVENT; AOA 51/52/53=0 (Analyzer 2026-11-06).
- **I-303A Feld 2026-11-07:** Vendor-only `303A:1001` → HU ohne nutzbares Medium (kein Name / nicht unterstützt); Probe Attach ja, AOA-Callbacks 0. MSC-Kontrolle gleicher Abend: Dateien sichtbar. Details: [I303A-FELD-2026-11-07.md](I303A-FELD-2026-11-07.md).
- **I-303A Reval 2026-11-10:** gleiche Negativ-HU bei beiden USBs (kein Versorgungsartefakt). MSC: Soft-Detach bei `lab_debian`; `cache_place` ohne Dateiliste; `cache_a` ok; HU zeigte Arduino-`iProduct` Espressif (Fix 0.3.37). [Feldnotiz](FELD-2026-11-10-I303A-REVAL-MSC.md).

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
- I-303A bestätigt: **gleiche VID/PID-Familie ohne MSC ≠ Stick-Pfad**; Class/Config entscheidet mit.
- **H-Reconnect:** Reconnect-Loop bei `18D1:2D00` *kann* fehlenden BCL/Etch/SAS-Peer (G3/G4) bedeuten statt reinem USB-Transportfehler — nicht messbar auf aktueller Schicht ([Kampagne](C1-IDENTITY-KAMPAGNE.md)).

---

## UNKNOWN

- Ob die HU auf dem Bus 51/52/53 sendet, wenn der Vendor-Callback 0 zählt (EP0 außerhalb `USBVendor.onRequest`).
- Welche Identity/Config den AOA-/Connected-App-Pfad am NBT auslöst (Phone-Referenz = P1).
- BCL/Etch/RHMI/**SAS** am HU-USB-Bus (G3/G4).
- `enumeration_count` / `configuration_count` / `control_in/out` / READ-size-/LBA-Histogramme auf hu-probe (explizit null).

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

**Phase:** G1-Identity (I-303A) **feld-dokumentiert** → Real-Phone-Transport.

| Prio | Handlung |
|------|----------|
| ~~P0~~ | ~~I-303A~~ — erledigt ([Feldnotiz](I303A-FELD-2026-11-07.md)) |
| **P1** | **FIELD-CONNECTED-REAL-01** — Phone×MyBMW/AAIdrive×F20×Analyzer ([Plan](FIELD-CONNECTED-REAL-01.md)) |
| **P2** | **FX-G3-01** — wenn Bulk da: `sas_*`-Sequenz korrelieren |
| **P3** | ESP-Nachbau nur mit REAL-PHONE-Referenz |

**Freeze:** Blind-Knock · BCL-on-ESP · SAS-FW · L0 als Blocker · weitere Identity-Matrix · Desk ohne Feldartefakt.

Kampagne: [C1-IDENTITY-KAMPAGNE.md](C1-IDENTITY-KAMPAGNE.md)
