# C1 Identity-Gate Kampagne (BMW NBT Evo)

| | |
|--|--|
| **Datum** | 2026-11-06 |
| **FW** | **0.3.36** |
| **Fokus** | Welche Variable zwischen `303a:1001` (MSC), `18D1:2D00`, `04E8:6860` den HU-USB-Pfad steuert |
| **Nicht** | BCL/Etch · Blind-Wiederholung C1b-P0 · Analyzer-Duplikat |

**Methodenherkunft:** Host-Behaviour-/Fingerprinting aus `esp32.usb-analyzer` Feldmatrix 2026-11-06 (BMW/TV/Soundbar/FRITZ!Box/Windows).  
**Audit:** technology-audit PR #32 — MSC-Identity-Negativ verstärkt Gate-Hypothese; Trigger **UNKNOWN**.

---

## Bekannte Pole (nicht erneut als Hauptlauf)

| Identity | Mode | Feld-FACT (2026-10-30) |
|----------|------|------------------------|
| `18D1:2D00` Vendor | `aoa_observe` | HU Zubehör + Reconnect-Loop; Callback 51/52/53=0; Bulk RX=0 |
| `04E8:6860` Vendor | `aoa_phone_like` | HU REJECTED („nicht unterstützt“) |
| `303a:1001` **MSC** | Analyzer Gadget | P-MOUNT inkl. MODE_SENSE(6)+PREVENT; `aoa51/52/53=0` |

Epistemik: Callback/RX = 0 ≠ Bus-Negativ — [epistemik-messluecke](2026-10-30-epistemik-messluecke-ep0-bulk.md).

---

## Hypothese (eine Variable)

**H1:** VID/PID `303A:1001` allein (ohne MSC-Class) erklärt noch nicht den Stick-Pfad; die USB-Class/Interface entscheidet mit.

**Experiment `aoa_id_303a`:** gleiche VID/PID wie Analyzer-MSC, aber **Vendor-only** (`0xFF/0xFF/0x00`), kein MSC-Interface, `hypothesis_respond=false`.

---

## Protokoll (pro Steckversuch)

1. FW 0.3.36, Mode NVS sticky verifiziert ([Lab-Checkliste](2026-11-06-lab-nvs-sticky-checklist.md))
2. Status-Heartbeat über die volle Messdauer (`tools/feld_status_heartbeat.py`) — Web-UI kann sterben
3. Definierte Identity + Session-ID (`ca_c1_link` start)
4. Artefakte: Heartbeat-JSONL + `session_metrics`/`timeline[]` + Events JSONL
5. HU-UI Observation (ohne Interpretation während Messung)
6. Ergebnis als **FACT / UNKNOWN / INFERENCE** — Details: [I-303A-Runsheet](2026-11-06-i303a-feld-runsheet.md)

### Semantik Counter

| Wert | Bedeutung |
|------|-----------|
| `0` | auf Messschicht gemessen und **nicht** vorhanden |
| `null` / UNKNOWN | auf dieser Messschicht **nicht beobachtbar** |
| `visibility_scope` | z. B. `usbvendor_onrequest_only` |

---

## Matrix (Reihenfolge)

| ID | Mode | Variable | Status |
|----|------|----------|--------|
| Lab-S | sticky A/B/C/D | NVS | **PASS** ([Lab](2026-11-06-lab-c1-036-sticky-aoa.md)) |
| Lab-A | `aoa_observe` × Analyzer-Host | AOA Lab A/B | **PASS** (respond an/aus unterscheidbar) |
| Lab-MSC | MSC + Novel-Host | Stimulus/Instrument | **PASS** ([Lab](2026-11-06-lab-msc-novel-access.md)) — nicht BMW-Wire |
| I-MSC | `msc` (kurz am BMW) | MSC Golden Path | optional / Analyzer bereits P-MOUNT |
| **I-303A** | **`aoa_id_303a`** | **VID ohne MSC-Class** | **NEXT** — [Runsheet](2026-11-06-i303a-feld-runsheet.md) |
| I-ACC / I-PHN | nur bei Bedarf | Rebaseline | kurze Timeline, kein Blind-Knock |

**Vor I-303A kein weiterer Code:** kein EP0-Hook, kein BCL/Etch/SAS, keine weiteren MSC-Novel-Muster, keine Blind-Knocks.

SAS/Connected-Auth (Gate G4) sitzt **zwei Stufen über** I-303A — siehe [GATES-Connected § Vier Gates](../GATES-Connected.md#referenzmodell--vier-gates-usb--connected). Ändert den Feldlauf nicht.

---

## INFERENCE (nicht FACT)

**H-Reconnect (G3/G4-Erwartung):** Der Feld-Loop bei Direct-`18D1:2D00` (Zubehör-UI → Reconnect; Callback 51/52/53=0; Bulk absent) *könnte* bedeuten: HU schließt AOA-Accessory ab und erwartet danach BCL/Etch-Peer-Verkehr (ggf. Cert/SAS); ohne Peer resettet sie den Kanal.  
**Messbarkeit:** hu-probe G1/G2 kann das weder bestätigen noch widerlegen. Alternative (EP0 außerhalb Callback, Transport) bleibt offen — [epistemik](2026-10-30-epistemik-messluecke-ep0-bulk.md).

---

## Erlaubte Abschluss-Aussagen

- **FACT:** Identity X + configuration Y → HU reaction Z  
- **FACT:** Varianten erzeugen trotz unterschiedlicher Identity keinen beobachtbaren AOA-Start auf der aktuellen Messschicht  
- **REJECTED:** „AOA erkannt / Bulk ⇒ Connected-App (SAS) offen“  
- Rest: **UNKNOWN** (Bus außerhalb Callback, AOA-Trigger, BCL/Etch/SAS Wire)
