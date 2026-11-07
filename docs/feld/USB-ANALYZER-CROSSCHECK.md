# Lab-Bericht: Cross-Check mit esp32.usb-analyzer (Bugs, Lücken, Messungen)

| | |
|--|--|
| **Datum** | 2026-11-02 |
| **hu-probe** | FW **0.3.34** · Board `.89` (YD-ESP32-S3 N8R2) |
| **Gegenstelle** | [esp32.usb-analyzer](https://github.com/MPunktBPunkt/esp32.usb-analyzer) Host `.88` **0.0.48–0.0.49** |
| **Topologie** | Analyzer-Host OTG ↔ USB-C ↔ hu-probe Gadget |
| **Evidence (Analyzer)** | `docs/evidence/LAB_HUPROBE_*` · `LAB_BMW_HU_STIMULUS_HUPROBE_*` · `LAB_HUPROBE_USB_GADGET_NVS_STICKY.md` |

**Claim-Klassen:** FACT / MEASURED / INFERENCE / UNKNOWN · **Not claimed:** echte BMW-NBT-Wire · BCL-on-HU · Path-B L0.

---

## 0. Ein-Satz-Urteil

Der Analyzer-Host als **kontrollierter USB-Host** entlarvt eine schwere **NVS-/Gadget-Mode-Lücke** (`usb_gadget` klebt auf `aoa_observe`) und zeigt zugleich, dass AOA Lab A/B gegen einen fremden Host **funktioniert** — während MSC-Golden-Path-Tests derzeit **lab-blockiert** sind.

---

## 1. Methode (warum der Aufbau)

Feldproblem: lange Wartezeiten auf HU-Zugriffe — Unklarheit **„HU liest nicht“ vs. „wir sind blind“**.

Mit Analyzer-Host:

1. Bekannte SCSI/AOA-Sequenz wird **aktiv** erzeugt.
2. hu-probe-Counter / Events müssen sich **zuordenbar** bewegen.
3. Counter=0 bei bekanntem Stimulus ⇒ kein Traffic (nicht stille Blindheit).

---

## 2. Bugs / Defekte (MEASURED)

### B1 — `usb_gadget` NVS nicht sticky (P0 Lab-Blocker)

| Schritt | Ergebnis |
|---------|----------|
| `POST /api/config` `usbGadget=msc` (JSON und form `plain=`) | RAM-Readback **msc**, oft `enableHub=false` |
| `POST /api/connected/gadget/reboot` | meldet `usbGadget=msc` |
| Nach Reboot | Status + Config wieder **`aoa_observe`**, `enableHub` wieder **true** |
| Gleiches Muster | `aoa_phone_like` → bootet wieder `aoa_observe` |
| Hub-Disable + 5 s Settle vor Save | ändert nichts |

**Folgen:**

- MSC Golden Path / SynthFat / BMW-MSC-Stimulus gegen hu-probe **nicht fahrbar** auf diesem Board, solange NVS `aoa_observe` hält.
- OTA löscht NVS nicht; kein HTTP-`factory-reset` gefunden.
- Operator kann glauben, den Modus gewechselt zu haben.

**NEXT:** Preferences-Write prüfen · Hub-Config-Overwrite ausschließen · `/api/factory-reset` oder UART `nvs erase` · Repro-Test „save → power-cycle ohne WiFi“.

### B2 — Kein HTTP-Weg zu frischem MSC-Default

Kein `/api/factory-reset` / NVS-Clear. Recovery nur UART/esptool oder Fix von B1.

---

## 3. Messungen (MEASURED) — was funktioniert

### M1 — AOA Lab A/B × Analyzer-Host

Boot: DUT **`18d1:2d00`** · product `AOA Observe`.

| Lab | Host | hu-probe Counter |
|-----|------|------------------|
| **A** `hypothesis_respond=true` | `aoa-handshake` protocolOk, stringsOk=6, startOk | `aoa51/52/53` ↑, `aoa51_respond_ok` ↑, P0_B gesehen |
| **B** `hypothesis_respond=false` | `err=aoa51_fail` | `aoa51` +1, **respond_ok unverändert** |

→ Respond-Gate verhält sich wie designed (C1b Lab A/B).  
**Not claimed:** Phone→Accessory-Reenum (brauchte sticky `aoa_phone_like`).

### M2 — BMW-MSC-Stimulus unter AOA (Negativkontrolle)

Host `bmw_mount` (INQUIRY/CAPACITY/TUR/READ10) gegen `aoa_observe`:

- Host: inquiry/capacity 0, TUR fail, READ fail  
- DUT: **`read_count` bleibt 0**

→ Korrekt: falsche USB-Plane. Zeigt: leere MSC-Counter bei bekanntem Stimulus = **kein MSC**, nicht „blinde Hooks“.

### M3 — MSC-Stimulus auf Golden Path

**Nicht ausgeführt** — blockiert durch B1.

---

## 4. Lücken / Blindstellen (nach Cross-Check)

| ID | Thema | Status nach Lab |
|----|--------|-----------------|
| L1 | MSC-Instrumentierung vs. Analyzer-Host | **offen** bis B1 gefixt |
| L2 | MODE_SENSE6 / PREVENT_ALLOW im Analyzer-Stimulus | Analyzer-Host-API-Lücke (EspUsbHost) — nicht hu-probe |
| L3 | Vollständiger EP0-Trace | unverändert (Observe = Vendor-Control-Sicht) |
| L4 | `aoa_phone_like` sticky | Teil von B1 |
| L5 | Feld: „HU kommt nicht“ | Methodisch entschärft: erst Lab-Stimulus, dann Auto |

---

## 5. Empfohlene Maßnahmen (Priorität)

1. **P0:** B1 fixen + Repro ohne Hub/WiFi.  
2. Nach Fix: `lab_bmw_hu_stimulus.py --dut huprobe` auf MSC (P-MOUNT/P-MREAD aus feld 2026-10-19).  
3. Optional: sticky `aoa_phone_like` → voller 51→53→Reenum gegen Analyzer-Host.  
4. Kurz: Factory-Reset-API oder dokumentierter NVS-Erase-Runbook.

---

## 6. Verweise

- Analyzer Evidence:  
  - https://github.com/MPunktBPunkt/esp32.usb-analyzer (Pfad `docs/evidence/LAB_HUPROBE_*`, `LAB_BMW_HU_STIMULUS_HUPROBE_*`)  
- Desk FACT Mount-Grammatik: [`MSC-TUR-MOUNT-FACTS.md`](MSC-TUR-MOUNT-FACTS.md) (P-MOUNT / P-MREAD)  
- Schwesterbericht: esp32.pidrive `docs/lab/USB-ANALYZER-CROSSCHECK.md`
