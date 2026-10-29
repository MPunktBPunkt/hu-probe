# C1b Maßnahmen nach B1/B2b — 2026-10-29 Abend

**Quellen:** Feldbericht [B2b](2026-10-29-c1b-b2b-knock-feld.md) · Multi-KI (Mistral/GPT/Gemini/Grok) · Desk AOA/AOSP · FOLLOW-ON #9/#11  
**Repo:** `main` @ `49667dc` · FW **0.3.32**

---

## Konsens (übernommen)

| Punkt | Maßnahme |
|-------|----------|
| Blind-Knock-/VID-Kette beendet | **Stop** weiterer hex4/Timing/VID-Versuche ohne Wire |
| `18D1:2D00` = Already-Accessory | `51/52/53=0` bei `aoa_observe` **nicht** als AOA-Negativ werten |
| B1 `04E8:6860` | Fingerprint **REJECTED** — kein AOA-Kill, Filter-INFERENCE |
| B2b hex4 | TX MEASURED, HU-Antwort **ABSENT** auf getestetem Bulk-Pfad |
| Spinner = „warte auf App“ | nur **INFERENCE**, nicht FACT |
| Nächster Erkenntnisschritt | **nur** FOLLOW-ON **#9** Referenz-Capture |
| BT-SPP (#11) | erst **nach** Capture-Entscheidung, nicht vorher bauen |

---

## Maßnahmen (priorisiert)

### M1 — FOLLOW-ON #9 Referenz-Capture (jetzt)

**Ziel:** Ein funktionierender Phone+Connected-Lauf am NBT Evo mit USB-Wire (ideal) oder maximaler Phone-seitiger Sicht.

**Checkliste Capture (muss beantworten):**

1. Phone-Composite: MTP? Interfaces? VID/PID/Strings?
2. Sendet HU `51/52/53/58`?
3. Re-Enum auf `18D1:2Dxx`?
4. Wer sendet erstes Bulk? Richtung + Timing + erste Bytes
5. **Zusatz (GPT):** Bleibt Connected aktiv, wenn USB-Bulk fehlt? → USB vs BT

**Akzeptanz:** ≥1 Connect bis Spinner weg / App sichtbar; Control-Trace + Bulk beider Richtungen (oder dokumentiert absent).

**Hardware (wählen, nicht alles nötig):**

| Option | Nutzen | Aufwand |
|--------|--------|---------|
| A HS-USB-Analyzer am Phone↔HU-Kabel | volle Control+Bulk | hoch |
| B Phone USB-debug + Logs (`USB_STATE`, Accessory-Intents) | Teilbild, Control oft unsichtbar | mittel |
| C Parallel hu-probe Session als Kontrast | nur Vergleich Enum/UI | niedrig, allein unzureichend |

**Deliverable:** Feldnotiz + Hex/PCAP-Auszug der ersten N Bulk-Bytes + Deskriptor-Tabelle; Entscheidung laut M2.

### M2 — Entscheidungstabelle (nach Capture)

| Capture zeigt | Aktion |
|---------------|--------|
| AOA + Bulk + BCL-artig | Phone-Fingerprint nachbauen → gezielter B2-Retry (echte Init) |
| AOA + Bulk, kein BCL | Application-Protokoll neu; kein BCL-Framer |
| Kein AOA, MTP + Connected | AOA-für-Connected-HYP verwerfen |
| USB nur UI, Connected über BT | **FOLLOW-ON #11** priorisieren (BT-SPP Stub → Gate C1c) |

### M3 — Doc/Epistemik (sofort, ohne Feld)

1. Feldbericht B2b: AOA-Reklassifizierung + weichere Formulierung (Bulk-Pfad / Spinner) — **erledigt in diesem Commit-Zyklus**.
2. `GATES-Connected.md`: B1/B2b Status + Verweis #9.
3. technology-audit FOLLOW-ON #9/#12: Feldstatus notieren (hex4 getestet, negativ).

### M4 — Explizit nicht tun

- Blind VID/PID / Knock / Timing
- `51/52/53=0` bei Already-Accessory als AOA-FAIL dokumentieren
- Soft-Composite MSC+AOA
- BclFramer/Etch/SAS/RHMI vor BCL-RX
- USB-BCL als FACT schreiben
- BT-SPP implementieren vor Capture-Entscheidung

### M5 — Firmware (Pause)

FW **0.3.32** bleibt Feldstand. Keine Feature-Arbeit an Connected bis M1-Ergebnis. Nur Bugs/Ops (OTA/RSSI) wenn nötig.

---

## Gate-Kurzstatus

```
C1b-P0-0/A/C/D   PASS (Rohr)
C1b-P0-B         NOT OBSERVED (bei 2D00 erwartbar; phone_like: HU rejected vor Handshake)
C1b-P0-E         FAIL / absent
B1               NEGATIVE (Fingerprint)
B2b              NEGATIVE (keine HU-Antwort auf getestetem Bulk)
USB-BCL          UNKNOWN
BT-BCL           HYPOTHESIS (code-supported, gestärkt)
```

**Einziger offener Hebel für USB-BCL:** M1 Referenz-Capture.
