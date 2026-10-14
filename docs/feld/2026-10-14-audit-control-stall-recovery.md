# Audit-Intake: usb-control-stall-recovery-2026

**Kampagne:** closed · Standard 0.7.1 · PR #6 gemerged (`5fab7f5`)  
**Evidence:** `EVID-USBCTRL-2026-10-14-001`–`009` · Exit 1–8 erfüllt · C10–C13 PASS  
**Vorläufer:** [instrument-sources](2026-10-14-audit-instrument-sources.md) (`99b8bc3`)  
**Zweck hier:** Desk-Hook-Karte für hu-probe GATE-01 Control/STALL/Enum — **kein** Code-Import, **kein** Mess-Claim.

Quelle (technology-audit): `audits/usb-control-stall-recovery-2026/` · Bericht `docs/reports/2026-10-14-usb-control-stall-recovery.md`

## Zentrale Erkenntnisse (Desk-FACT)

1. **Mount ≠ Address:** Enumeration endet erst bei `SET_CONFIGURATION n` → `tud_mount_cb`. `SET_ADDRESS` allein ≠ mounted / Klassen-EPs aktiv.
2. **cfg 0 → umount:** `SET_CONFIGURATION 0` → `tud_umount_cb`; cfg≠0 → `process_set_config` dann Mount.
3. **Clear-Halt heilt NEED_RESET nicht:** Bei `MSC_STAGE_NEED_RESET` stallt der Stack weiter („Clear Stall cannot resolve“). Nur **BOT Reset (`0xFF` / `MSC_REQ_RESET`)** → `proc_bot_reset`.
4. **Außerhalb NEED_RESET:** Clear-Halt auf EP IN kann CSW nachholen; auf EP OUT in CMD-Stage `prepare_cbw`.
5. **ESP-Port:** `tud_mount_cb`/`tud_umount_cb` koppeln MSC-Storage-Ownership + `TINYUSB_EVENT_ATTACHED/DETACHED` (esp_tinyusb).
6. **Replug ohne VBUS:** Oft **kein** `tud_umount_cb` — State auch bei **Mount** clearen; Mount-Hook = sichere Epoch.
7. **USB↔WiFi:** Fremd-Issues nur **HYPOTHESIS** — keine Root-Cause für die Sonde.

## Hook-Karte → hu-probe (Implementierungs-Hypothese)

| Event-Kandidat | Stelle | Pflichtfelder | hu-probe heute |
|----------------|--------|---------------|----------------|
| `usb_set_address` | `usbd.c` SET_ADDRESS | wValue (addr) | ❌ fehlt (braucht usbd-Hook) |
| `usb_set_config` | `usbd.c` SET_CONFIGURATION | cfg_num (0 vs n) | ❌ fehlt |
| `usb_mount` / `usb_umount` | `tud_mount_cb` / `tud_umount_cb` | Epoch; ggf. ATTACHED/DETACHED | ❌ / nur Soft-attach teilweise |
| `ep_stall` / `ep_clear_halt` | stall / CLEAR_FEATURE Halt | ep_addr | ~ `clear_halt` via MSC-Control-Wrap; Stall-Epoch fehlt |
| `msc_need_reset` | `MSC_STAGE_NEED_RESET` | beide Bulk stalled; **kein** Tag weiterführen | ✅ `usb_ev=bot_need_reset` (0.2.9) |
| `msc_bot_reset` | BOT Reset `0xFF` | neue Epoch (Stage/Sense clear) | ~ MSC Class-Control geloggt; Epoch-Trennung schärfen |
| `bot_cbw` / `bot_csw` | unverändert INST | siehe Vorläufer | ✅ 0.2.8/0.2.9 |

## Mapping-Regeln für die Implementierung

| Regel | Konsequenz |
|-------|------------|
| Address ≠ Mount | Getrennte Events; Status-LED/Session-Phase nicht an Address hängen |
| Clear-Halt ≠ geheilt | Nie NEED_RESET durch Clear-Halt als recovered loggen |
| BOT Reset = neue Epoch | `bot_tag`-Korrelation über NEED_RESET **abbrechen** |
| Replug ohne VBUS | State-Clear in Mount-Pfad; Umount nicht voraussetzen |
| WiFi | Lab-Dimension; Desk-Issues nicht als FACT zitieren |

## Präregistrierte Kill-Kriterien (Lab, noch offen)

1. `SET_CONFIGURATION n` ohne Mount-Event oder cfg≠n → Fail  
2. NEED_RESET fälschlich als „geheilt durch Clear-Halt“ → Fail  
3. Replug ohne VBUS als Umount-Sequenz behauptet → Fail  
4. WiFi-Issue als Root-Cause der Sonde → Integritäts-Kill  
5. Timing/S1–S3 aus diesem Desk abgeleitet → Integritäts-Kill  

## Bewusst nicht aus dem Audit

- Kein Lab-/Fahrzeug-Messlauf  
- Kein Timing / S1–S3 / Wire / NBT  
- Kein Code-Import aus Fremdquellen  
- Kein paralleles Technology-Sheet in hu-probe  

## Nächster Schritt in hu-probe

1. **P0 Timing** (`t_us` Capture in Pending.Data) — Blocker, Audit ersetzt das nicht  
2. **Control/STALL-Hooks** nach dieser Karte (usbd wrap / mount_cb) → GATE-01 Control-Checkboxen  
3. Lab-Dry-Run gegen Kill-Kriterien 1–3  
4. Kein weiteres Fremd-Audit für diese Pfade nötig  
