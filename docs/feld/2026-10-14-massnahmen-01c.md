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
| **P2** | Lange unbroken BMW-S0-Timeline | **blockiert** | OTG killt WiFi (~5–7 s); siehe [wifi-otg-0.2.15](2026-10-14-wifi-otg-0.2.15.md); Ersatz: Burst-Epistemik → [massnahmen-01d](2026-10-15-massnahmen-01d.md) |
| **P2b** | USB↔WiFi-Coexistenz (über Drain/Hub hinaus) | **offen** | Desk-Karte + Lab-Method; 0.2.14/15 unzureichend; parallel, kein S2/S3-Blocker |
| **P2c** | Lab Soft-Detach-Coex bei WiFi-Loss | **done in 0.2.16**/17 | OTG stecken lassen / unattended; nicht für BMW-Playback |
| **P3** | S1 Eager-EOF / S2 Prefetch / S3 Cache | **offen** | Desk-Messplan done (audit s1s2s3); Feld: S2/S3 vor S1 — [massnahmen-01d](2026-10-15-massnahmen-01d.md) |
| **P4** | GATE-01 formal schließen (Dry-Run) | **offen** | Lab-Kill ok; Burst-Kriterien + Close — 01d P1e |
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

`0.2.12` Timing · `0.2.13` Control/STALL + Lab-Triple/Kill PASS · `0.2.14`/`0.2.15` WiFi-Härtung (OTG-Kill ungelöst) → BMW-Timeline erst nach Coex-Fix.
