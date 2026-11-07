# Audit-Intake: usb-bot-observation-2026-instrument-sources

**Kampagne:** closed · Standard 0.7.1 · PR gemerged (`99b8bc3`)  
**Evidence:** `EVID-USBBOT-INST-2026-10-14-*` · C10–C13 PASS  
**Zweck hier:** Desk-Audit-Wissen für hu-probe GATE-01B (Device-Logger), nicht Code-Übernahme aus Fremdquellen.

## Zentrale Erkenntnisse

1. Logger muss am **TinyUSB Class-Driver** hängen (`mscd_xfer_cb` nach CBW-`memcpy`, CSW/STATUS, Control, **NEED_RESET`) — nicht nur Adafruit/`onRead`.
2. Tag, CDB, Residue liegen im Stack **bevor** App-Callbacks laufen → echte `bot_cbw`/`bot_csw`-Events sind machbar.
3. **usbsnoop** eignet sich als Lab-Host-Validate (PC), nicht als Fahrzeug-Capture; Lizenz `UNKNOWN` → nur Research-Kontrast, **keine** Code-Übernahme.
4. Physische Paketebene und NBT-CDB/Sense/Timing bleiben bewusst **UNKNOWN**.

## Mapping → hu-probe

| Audit-Hook | 0.2.8 | 0.2.9 |
|------------|-------|-------|
| CBW nach `mscd_xfer_cb` | Snapshot `_mscd_itf` an App-Cbs / `onRead` | zusätzlich `--wrap=mscd_xfer_cb` |
| CSW / STATUS | `complete_cb` + `poll` | Wrap nach Real-Call + bestehender Fallback |
| MSC Control | wrap `mscd_control_xfer_cb` | unverändert |
| NEED_RESET | — | `usb_ev=bot_need_reset` |

Epistemik unverändert: TinyUSB Device-Stack ≠ Wire-Paketebene.

## Bewusst nicht aus dem Audit

- Kein Lab-Lauf / kein Fahrzeug in der Audit-Kampagne
- Kein paralleles Technology-Sheet in hu-probe
- Kill-Kriterien für den nächsten Instrumentierungs-Lab bleiben präregistriert, Messlauf offen

## Nächster Schritt nach 0.2.9

Kontrollierter Lab gegen Host-Decode (libre Tool; usbsnoop nur Kontrast) → danach BMW-S0-Fahrzeugbaseline.
