# Heimfahrt 2026-10-22 — Kurzauswertung Auto-Probe `.89`

**Vollbericht:** [`2026-10-22-gesamtbericht-heimfahrt.md`](2026-10-22-gesamtbericht-heimfahrt.md) · **Grok-Übergabe:** [`2026-10-22-uebergabe-heimfahrt-grok.md`](2026-10-22-uebergabe-heimfahrt-grok.md)

**FW:** 0.3.17 · Medium `drive_mix` · `autoCapture` war an  
**Trips:** `#22` (power_loss), `#23` (max_duration), `#24` (power_loss, kurz)  
**Sessions:** `12777_bmw_drive_home`, `20261022-171751_bmw_drive_home`, `20261022-173030_bmw_drive_home`  
(Partials/Reconnects: `20261022-165516_*`, `20261022-165747_*` — Prefix von `#22`)

Upload: alle drei Trips `uploaded=true`; danach `autoCapture=false` gesetzt.

## Kurzfassung

| Trip | Dauer (capture) | Remounts | Kern |
|------|-----------------|----------|------|
| **#22** | ~21 min | **2** (Start + ~10,3 min) | JAZZ → MIX (lange) → Stop/Remount → PODCAST → POP/CHARTS-Browse → POP → ROCK |
| **#23** | ~8,8 min | **2** (Start + ~7,8 min) | **ROCK/R04** lange (~6–7 min) → Remount → ROCK + ROCK/CLASSIC |
| **#24** | ~0,5 min | 1 (Start) | nur ROCK/CLASSIC/C01 — früher Stromverlust |

Passt zu **1–2 Stops** (Remount-Sequenzen in `#22` und `#23`) und zu einem **Ordner/Track, der lange „in der Schleife“ lief** (`MIX` in `#22`, `ROCK/R04` in `#23`).

## Trip #22 — `12777_bmw_drive_home`

Mount-Signaturen (Analyzer, `capture_t_us`):

- **t ≈ 0,2 s** — Erst-Enum
- **t ≈ 596 s (~10 min)** — zweite Enum nach Stop (umount/set_address/mount)

Ablauf (Read-Episoden / Folgetrack):

1. **JAZZ** J01 → J02 (kurz browsen/prefetch)
2. **MIX** M01 → M02 → M03 — längstes Hörfenster der ersten Etappe (~3+3+2 min Prefetch-Abstand)
3. Remount → kurz INTRO/Probe, dann nochmal MIX/M01
4. **PODCAST** E01–E03 (stark verzahntes Prefetch E02↔E03)
5. **POP/CHARTS** H01…H05 — dichtes Kopf-Wechseln (HU liest Nachbarn mit); kein sauberer A↔B-Ordner-Pingpong
6. **POP** P01 (~3 min) → P02
7. **ROCK** R01…R04 bis Stromende

Der bewusste Test „zwischen zwei Ordnern immer den ersten Song“ zeigt sich **nicht** als klares Ordner-A↔Ordner-B mit jeweils `*01`. Sichtbar ist eher **Inner-Folder-Prefetch** (CHARTS H01↔H02, ROCK R02↔R03↔R04) und Ordnerwechsel nur bei Navigation (JAZZ→MIX→PODCAST→POP→ROCK).

## Trip #23 — `20261022-171751_bmw_drive_home`

- Remount bei **~448 s**
- **ROCK/R04** dominiert die erste Phase (~6–7 min Listen-Proxy) — das ist der klare „lange Schleifen“-Kandidat
- Nach Remount: R01 → R02 → **ROCK/CLASSIC** C01…C04 → R03 → R04  
  Das wirkt wie Navigation ROCK ↔ Unterordner CLASSIC mit jeweils dem ersten sichtbaren Track — näher am beschriebenen Test als `#22`

## Trip #24 — kurz

Nur C01 nach Mount; `end_reason=power_loss`. Kein weiterer Remount.

## Datenqualität

- Zeitbasis **`capture_t_us`** ist zuverlässig; bei Stored-Replay sind `t_ms`/`t_us` teils kollabiert (`#23`/`#24`) — Analyzer nutzt bereits `capture_t_us`.
- Reconnect-Sessions (`165516`, `165747`) sind unvollständige TCP-Spiegel von `#22`, keine Extra-Fahrten.

## Operator

- Heimfahrt-Modus auf `.89` manuell aus (`autoCapture=false`).
- Ab **0.3.19:** Auto-Aus bei Heimnetz+Collector (einmaliger Durchlauf).
