# FIELD-CONNECTED-REAL-01 — Real Phone × F20/NBT Evo × Analyzer

| | |
|--|--|
| **Priorität** | **P1** (nach I-303A = P0 G1-Abschluss) |
| **Ziel** | Erste zeitlich korrelierbare Transportstufe **G2→G3** am echten HU-USB; ideal **FX-G3-01** |
| **Nicht-Ziel** | „AAIdrive funktioniert“ · ESP-Emulation · weitere Blind-Knocks · L0-Analyzer-Bau · SAS selbst implementieren |
| **Desk** | [CONNECTED-AAIDRIVE-STACK](https://github.com/MPunktBPunkt/pidrive/blob/main/docs/fahrzeug/CONNECTED-AAIDRIVE-STACK.md) · Audit [SAS PR #34](https://github.com/MPunktBPunkt/technology-audit/pull/34) |

**Phase:** Protokoll-Doku ist ausreichend. Engpass = Grenze **Android-Connected-Stack ↔ BMW-HU-Transport**.

---

## Success Criteria (hart)

| Stufe | Kriterium | Gate |
|-------|-----------|------|
| **Minimum PASS** | ≥1 zeitlich korrelierbare Transportstufe über Enumeration hinaus (Bulk-Aktivität **oder** BCL-Knock-Korrelat) mit Android-Event + Analyzer-Timestamp | G2 sichtbar |
| **Ziel PASS** | Klartext-/Payload-Fenster mit Etch-ähnlichem Verkehr **oder** AAIdrive/Connected-Log mit `sas_*` **plus** korreliertem Bulk | G3 sichtbar |
| **FX-G3-01** | In **einer** Session: `sas_certificate` → Challenge → `sas_login` (Etch-Decode **oder** Logs+Tunnel) | G4 Desk→Feld |
| **FAIL / UNKNOWN ok** | Kein Bulk trotz Connected-UI → dokumentieren; **kein** Blind-ESP-Nachbau daraus | — |

**REJECTED als Success:** „Icon in HU“ allein · „Zubehör erkannt“ allein · Lab-`lab_bcl_on_aoa_bulk` als HU-Wire.

---

## Vier Beobachtungsebenen (gleichzeitig)

### A — Android

Logs (UTC, monotones Gerät):

- MyBMW / Connected + AAIdrive + `adb logcat` (USB/BT Tags)
- Events: `ACTION_CAR_ACCESSORY_ATTACHED`, `ACTION_CAR_APPLICATION_REGISTERING`, host/port, `security connected`, `iDrive connected`, MainService, Etch, `sas_certificate` / `sas_login`, RHMI
- **Welcher Prozess?** AAIdrive (Etch-Client) vs. Connected (CarAPI-Proxy `sas_*`)

Hinweis: Kein `sas_login` ≠ USB-Fail — MainService gated auf Security connected (EVID-SAS-030).

### B — USB Analyzer (Host-Level reicht)

Nicht nur „Gerät erkannt“:

- VID/PID, Config, Interfaces, EP0, Bulk IN/OUT, Re-Enum, Disconnect, Timing, Payload-Längen
- Timeline-Anker: first bulk → first payload → first reconnect ↔ Android-Event

**Kein L0-Pflicht:** Cleartext-Claim macht Host-Level-Bulk für G3/G4 potenziell ausreichend.

### C — HU (Operator)

USB connected? Accessory? Connected Apps / AAIdrive-Icon? Reconnect? RHMI? Audio?  
Icon ≠ volle RHMI-Init (AM-Placeholder).

### D — Vergleich Direct-ESP (gleicher Tag / gleiche HU)

| Phase | echtes Telefon | ESP `18D1:2D00` (bestehend) / ggf. frisches I-303A |
|-------|----------------|-----------------------------------------------------|
| Enumeration | | |
| AOA / Callback | | |
| Bulk | | |
| BCL | | |
| Etch | | |
| `sas_*` | | |
| RHMI | | |

Erster **entscheidender Divergenzpunkt** = Forschungsprodukt — nicht „ESP kann X“.

---

## Ablauf (kurz)

1. ~~I-303A~~ wenn noch offen — G1 abschließen, **keine** Identity-Matrix aufblasen ([Runsheet](I303A-RUNSHEET.md)).
2. Phone: MyBMW/Connected + AAIdrive; F20 Apps/6NR prüfen (sonst Pfad C tot).
3. Analyzer am USB-Pfad (Topologie dokumentieren: Phone↔HU vs. Hub).
4. Connect → 2–5 min stabile Session → gezieltes Disconnect/Reconnect (optional FX-G3-02 später).
5. Artefakte: logcat (UTC), Analyzer-Capture, HU-Notiz, Sync-Tabelle t0.
6. Bericht: FACT / UNKNOWN / INFERENCE — **kein** SAS-Wire-Claim ohne Korrelation.

Optional parallel: eine CarAPI-App (Spotify) — gleicher Wire-Tunnel, anderer Phone-Call-Graph (Audit PR #34).

---

## Freeze (bis nach diesem Test)

- Keine neuen AOA-51/52/53-Varianten
- Kein weiterer synthetischer BCL-on-ESP („lab_bcl“ reicht als Parser-Validierung)
- Keine SAS-Firmware-Implementierung (moderne MyBMW-Signierung UNKNOWN)
- Kein L0-Analyzer-Ausbau als Blocker
- Keine weitere Protokoll-Desk-Serie ohne Feldartefakt

**P2** = FX-G3-01 Vertiefung wenn Bulk da.  
**P3** = ESP-Rollen-Nachbau **nur** mit REAL-PHONE-Referenz (Timing/EP/Payload).
