# C1 Identity-Gate Kampagne (BMW NBT Evo)

| | |
|--|--|
| **Datum** | 2026-11-06 |
| **FW** | **0.3.36** |
| **Phase** | Desk/Lab geschlossen → **Feld: G1 abschließen, dann Real-Phone** |
| **Nicht** | weitere Protokoll-Doku · Blind-Knock · BCL-on-ESP · SAS-FW · L0-Analyzer als Blocker |

**Methodenherkunft:** usb-analyzer Feldmatrix 2026-11-06.  
**Audits:** technology-audit PR #32 (Identity) · PR #34 (SAS CarAPI/Cleartext/AIDL).

---

## Prioritäten (jetzt)

| Prio | ID | Ziel | Doc |
|------|-----|------|-----|
| **P0** | **I-303A** | G1 Identity abschließen (billig) | [Runsheet](2026-11-06-i303a-feld-runsheet.md) |
| **P1** | **FIELD-CONNECTED-REAL-01** | G2→G3 am echten Phone×HU×Analyzer | [Plan](2026-11-06-field-connected-real-01.md) |
| **P2** | **FX-G3-01** | `sas_certificate`→Challenge→`sas_login` korreliert | Audit + Plan §Success |
| **P3** | ESP-Rollen-Nachbau | nur mit REAL-PHONE-Referenz | — |

I-303A beantwortet **nicht** „wie sieht Connected auf USB aus?“ — das ist P1.

---

## Gate-Lage (Kurz)

| Gate | Inhalt | Stand |
|------|--------|-------|
| G1 | USB Identity/Enumeration | gut belegt; I-303A schließt Vendor-only `303A:1001` |
| G2 | AOA/BT + BCL Transport | AOA teilweise; **BCL HU-Wire UNKNOWN** |
| G3 | BCL→Etch→`sas_*` | **UNKNOWN im Fahrzeug** |
| G4 | SAS Auth | Desk sehr gut; Real-Session UNKNOWN; MyBMW-Signierung UNKNOWN |
| G5 | RHMI/AV/CDS/AM | abhängig G3/G4 |

---

## Bekannte Pole (nicht erneut als Hauptlauf)

| Identity | Mode | Feld-FACT |
|----------|------|-----------|
| `18D1:2D00` Vendor | `aoa_observe` | Zubehör + Reconnect; Callback 51/52/53=0; Bulk RX=0 (2026-10-30) |
| `04E8:6860` Vendor | `aoa_phone_like` | HU REJECTED |
| `303a:1001` **MSC** | Analyzer | P-MOUNT; `aoa51/52/53=0` |

---

## I-303A (P0) — eine Variable

**H1:** VID/PID `303A:1001` ohne MSC-Class ≠ Stick-Pfad; Class/Interface entscheidet mit.  
Mode `aoa_id_303a`, `hypothesis_respond=false`. Heartbeat Pflicht.

Nach PASS/FAIL-Dokumentation: **Identity-Matrix nicht aufblasen** → P1.

---

## Freeze (bis nach P1)

- keine neuen AOA-Kommandos / Blind-Knocks  
- kein synthetischer BCL-on-ESP als „HU-Beweis“  
- keine SAS-Implementierung auf der Probe  
- kein L0-Analyzer vor Real-Phone  
- keine weitere Desk-Serie ohne Feldartefakt  

---

## INFERENCE (nicht FACT)

**H-Reconnect:** Direct-`18D1:2D00`-Loop *kann* fehlenden Connected-Peer bedeuten (BCL/Etch/SAS). Messschicht G1 kann das nicht entscheiden — P1 liefert den Divergenzvergleich Phone vs ESP.

---

## Erlaubte Abschluss-Aussagen

- **FACT:** Identity X + configuration Y → HU reaction Z (I-303A)  
- **FACT:** korrelierte Transportstufe G2/G3 in Real-Phone-Session (P1)  
- **REJECTED:** AOA/Bulk ⇒ Connected-App offen · jede App = Etch-Client · Lab-BCL = HU-Wire  
- Rest: **UNKNOWN**
