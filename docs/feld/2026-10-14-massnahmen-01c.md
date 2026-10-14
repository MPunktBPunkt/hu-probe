# Maßnahmen — Abend 2026-10-14 (nach Control/STALL-Audit + Timing-Triage)

**Repo:** hu-probe HEAD `87b5db0` (+ lokale Docs)  
**Audits:** BOT INST `99b8bc3` · Control/STALL `5fab7f5` (PR #6, Kampagne closed)  
**Triage:** [review-abend-triage](2026-10-14-review-abend-triage.md) · Intakes: [instrument-sources](2026-10-14-audit-instrument-sources.md), [control-stall](2026-10-14-audit-control-stall-recovery.md)

Kein weiteres Fremd-Audit für Control/STALL nötig. Nächste Arbeit = **Code in hu-probe**.

## Priorisierte Maßnahmen

| Prio | Maßnahme | Status | Hinweis |
|------|----------|--------|---------|
| **P0** | `t_us`-Fix: Pending Capture-Zeit + `stampAt`; Validator Regression | **done in 0.2.12** | + `publishUs` Clamp in 0.2.13 (Remount/Mount) |
| **P0b** | Restart-Timeout ~90 s → ~25 s | **done in 0.2.12** | Quick Win |
| **P1** | Control/Enum-Hooks nach Audit-Karte: `SET_ADDRESS`, `SET_CONFIGURATION`, `tud_mount_cb`/`tud_umount_cb` | **done in 0.2.13** | Lab Kill 1 PASS |
| **P1** | STALL / CLEAR_HALT / BOT-Reset als getrennte Epochs; Clear-Halt ≠ NEED_RESET-Heilung | **done in 0.2.13** | Lab Kill 2 PASS |
| **P1** | State-Clear bei Mount (Replug ohne VBUS oft ohne Umount) | **done in 0.2.13** | Lab Kill 3 PASS |
| **P2** | Lab-Triple 0.2.12+ (nach P0): Soft+Remount+`dd`, Gap+BOT, Kill-Kriterien Control | **done** | [lab 0.2.13](2026-10-14-lab-debian-s0-0.2.13.md) |
| **P2** | Lange unbroken BMW-S0-Timeline | **offen** | nach Timing-Fix / stabilem WiFi |
| **P3** | S1 Eager-EOF / S2 Prefetch / S3 Cache | **offen** | Instrument reicht nach P0 |
| **P4** | GATE-01 formal schließen (Dry-Run) | **offen** | Lab-Kill ok; formal Close + BMW-Timeline |
| **P5** | Mazda | zuletzt | Stub bleibt |

## Was das Control-Audit für GATE-01 ändert

| Vorher (Abend-Triage) | Nach Audit |
|----------------------|------------|
| „P5 Control-Sniff — unsicher wie“ | **Hook-Karte konkret** — Implementierung, kein weiteres Desk |
| Clear-Halt / NEED_RESET vermischt denkbar | **getrennt** — nur BOT Reset heilt NEED_RESET |
| Soft-detach ≈ Host-Umount | **falsch oft** — Mount-Hook decisive |
| WiFi-Root aus Issues ableiten | **REJECTED** — bleibt Lab-Hypothese |

## Bewusst unverändert

- Timing/seq-`t_us` = eigene Codearbeit (P0)  
- S1–S3 / Wire / NBT = nicht aus Audit  
- Lab ≠ BMW; kein Mazda-Übertrag  

## Empfohlene FW-Linie

`0.2.12` Timing · `0.2.13` Control/Mount/STALL + Lab-Triple + Kill 1–3 PASS → GATE-01 Close-Kandidat (formal + BMW-Timeline noch offen).
