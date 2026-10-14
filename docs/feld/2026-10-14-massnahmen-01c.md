# Maßnahmen — Abend 2026-10-14 (nach Control/STALL-Audit + Timing-Triage)

**Repo:** hu-probe HEAD `87b5db0` (+ lokale Docs)  
**Audits:** BOT INST `99b8bc3` · Control/STALL `5fab7f5` (PR #6, Kampagne closed)  
**Triage:** [review-abend-triage](2026-10-14-review-abend-triage.md) · Intakes: [instrument-sources](2026-10-14-audit-instrument-sources.md), [control-stall](2026-10-14-audit-control-stall-recovery.md)

Kein weiteres Fremd-Audit für Control/STALL nötig. Nächste Arbeit = **Code in hu-probe**.

## Priorisierte Maßnahmen

| Prio | Maßnahme | Status | Hinweis |
|------|----------|--------|---------|
| **P0** | `t_us`-Fix: Pending.Data Capture-Zeit + `stampAt`; Validator Regression | **offen** | Blocker vor jeder neuen Baseline |
| **P0b** | Restart-Timeout ~90 s → ~25 s | empfohlen | Quick Win, parallel zu P0 ok |
| **P1** | Control/Enum-Hooks nach Audit-Karte: `SET_ADDRESS`, `SET_CONFIGURATION`, `tud_mount_cb`/`tud_umount_cb` | **offen** | Desk fertig; usbd-/mount-Hooks |
| **P1** | STALL / CLEAR_HALT / BOT-Reset als getrennte Epochs; Clear-Halt ≠ NEED_RESET-Heilung | **teilweise** | `bot_need_reset` da; Stall/BOT-Reset-Epoch schärfen |
| **P1** | State-Clear bei Mount (Replug ohne VBUS oft ohne Umount) | **offen** | Audit `007`/`008` |
| **P2** | Lab-Triple 0.2.12+ (nach P0): Soft+Remount+`dd`, Gap+BOT, Kill-Kriterien Control | **offen** | nicht auf 0.2.11-Rohdaten |
| **P2** | Lange unbroken BMW-S0-Timeline | **offen** | nach Timing-Fix / stabilem WiFi |
| **P3** | S1 Eager-EOF / S2 Prefetch / S3 Cache | **offen** | Instrument reicht nach P0 |
| **P4** | GATE-01 formal schließen (Dry-Run) | **offen** | nach P0+P1+Lab |
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

`0.2.12` (Arbeitstitel): P0 Timing + P1 Control/Mount/STALL-Epochs → Lab-Dry-Run → erst dann GATE-01 Close-Kandidat.
