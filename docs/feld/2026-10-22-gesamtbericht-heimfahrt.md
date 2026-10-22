# Gesamtbericht: BMW Heimfahrt Offline-Capture 2026-10-22

**Probe:** Auto-ESP `.89` · FW **0.3.17** · Medium `drive_mix` · `autoCapture` / Experiment `drive_home`  
**Collector:** `192.168.178.187:9100` · Upload abends vollständig (`#22`–`#24` `uploaded=true`)  
**HU:** BMW NBT Evo · Operator: 1–2 Stops, lange Ordner-Schleife, bewusster Wechsel „erster Song“ zwischen Ordnern  
**Kurznotiz:** [`2026-10-22-heimfahrt-auswertung.md`](2026-10-22-heimfahrt-auswertung.md) · Analyzer unter `sessions/*/derived/`

---

## 0. Ampel

| Bereich | Status | Beleg |
|---|---|---|
| Flash-Journal + Stored-Replay → Collector | 🟢 | Trips `#22`/`#23`/`#24` delivered, Integrity 1 |
| Remount-Sequenzen (Stops) | 🟢 FACT | `#22` Mount @0,2 s + **596 s**; `#23` @0,25 s + **448 s** |
| Lange Hörphase / Ordner-Schleife | 🟢 | `#22` MIX-Etappe; `#23` **ROCK/R04 ~6–7 min** |
| Browse Prefetch (Nachbar-Tracks) | 🟢 FACT | POP/CHARTS H01↔H05; ROCK R02↔R04 verzahnt |
| Bewusster A↔B „erster Song“-Test | 🟡 | klarer in `#23` ROCK ↔ CLASSIC; in `#22` eher Navigation |
| `capture_t_us` als Zeitbasis | 🟢 | Analyzer; `t_ms` bei Replay teils kollabiert |
| Heimfahrt-Flag Auto-Aus | 🟠 danach | manuell `autoCapture=false`; ab **0.3.19** one-shot Heimnetz+Collector |

---

## 1. Datenbasis

| Trip (Flash) | Bytes | Ende | Session | Events / Reads | Remounts |
|---|---:|---|---|---:|---:|
| **#22** | 752 379 | `power_loss` | `12777_bmw_drive_home` | 50 187 / 23 189 | **2** |
| **#23** | 377 662 | `max_duration` | `20261022-171751_bmw_drive_home` | 24 456 / 10 765 | **2** |
| **#24** | 45 206 | `power_loss` | `20261022-173030_bmw_drive_home` | 2 924 / 1 121 | 1 |

Partials/TCP-Reconnects (kein Extra-Trip): `20261022-165516_*`, `20261022-165747_*` (Prefix von `#22`).

Artefakte: `docs/feld/artifacts/20261022-heimfahrt/`, Whitelist `sessions/12777_*` / `20261022-171751_*` / `20261022-173030_*`.

TUR idle Median in allen drei Analyzer-Läufen: **2,2388 s** (Raster wie Media-Leiter).

---

## 2. Trip #22 — Hauptfahrt (~21 min Capture)

**Mount-Signaturen** (`capture_t_us`, Analyzer):

| t | Bedeutung |
|---|---|
| ≈ 0,24 s | Erst-Enum nach Soft-Re-Enum |
| ≈ **595,7 s (~9,9 min)** | zweite Enum nach Stop (umount → set_address → mount) |

**Ablauf (Read-Episoden / Folgetrack):**

1. **JAZZ** J01 → J02 — kurzer Einstieg / Prefetch  
2. **MIX** M01 → M02 → M03 — längste zusammenhängende Hör-Etappe vor dem Stop (Folgetrack-Abstände ~3 / ~3,5 / ~2 min)  
3. **Remount** — kurz INTRO/Probe, erneuter Einstieg MIX/M01  
4. **PODCAST** E01–E03 — stark verzahntes Prefetch E02↔E03  
5. **POP/CHARTS** H01…H05 — dichtes Kopf-Wechseln (HU liest Nachbarn mit)  
6. **POP** P01 (~3 min Prefetch-Abstand zu P02) → P02  
7. **ROCK** R01…R04 bis `power_loss`

Checkpoints: 12× `ckpt:*` · `next_track_read`: 66.

**Operator-Abgleich:** ein Stop mitvollzogen (~10 min) → Remount FACT. MIX als „Ordner lange“ in Etappe 1.

---

## 3. Trip #23 — Fortsetzung / max_duration

**Mounts:** ≈ 0,25 s und ≈ **448,5 s (~7,5 min)**.

**Kern:**

- **ROCK/R04** dominiert vor dem Remount (große Episoden @2,6 s und @403 s; Listen-Proxy ~6–7 min) — klarster „Schleifen“-Beleg  
- Nach Remount: R04 → **R01** → **R02** → **ROCK/CLASSIC C01…C04** → R03 → R04  

Das ist die beste Annäherung an den Test „zwischen zwei Ordnern immer den ersten Song“: Wechsel ROCK-Root ↔ Unterordner CLASSIC, jeweils Einstieg über den ersten sichtbaren Track (`R01` / `C01`).

Ende: `max_duration` (Experiment-Cap 30 min) · 8 Checkpoints · 8 `next_track_read`.

---

## 4. Trip #24 — Kurzfragment

Nur **ROCK/CLASSIC/C01** nach Mount; `power_loss` früh. Ein Mount, keine zweite Remount-Sequenz. Kleiner Flash-Trip (45 KiB) — Rest der Fahrt fehlt oder war schon in `#23` gesichert.

---

## 5. Leseverhalten (übergreifend)

| Muster | Beobachtung |
|---|---|
| Head-first Prefetch | Folgetrack oft mit 32/512-Block-Kopf, dann Bulk — wie Media-Leiter |
| Verzahnung | CHARTS und ROCK: aktueller + nächster Track im Sub-Sekunden-Wechsel |
| Remount | gleiche Mount-Signatur-Familie wie Erst-Enum (`0/8,…,131072/8`), TUR bis Signaturende 32 |
| Cache nach Remount | HU startet nicht „kalt“ bei Datei 1 des Mediums, sondern nahe der zuletzt gehörten Region (ROCK/MIX) |

**Nicht belegt als Ordner-A↔Ordner-B-Pingpong in `#22`:** dort überwiegen lineare Navigation + Inner-Folder-Prefetch.

---

## 6. Capture-Pfad / Upload

- Ende `#22`/`#24`: `power_loss` (Zündung) — erwartetes Heimfahrt-Ende  
- `#23`: Timer `max_duration`  
- Replay serverseitig: `upload done status=delivered` · `integrity=1` · Heap während Upload ~193 KiB stabil  
- HTTP während Trip-Replay oft unreachable (bekannt); TCP zum Collector blieb bei RSSI ≈ −28 dBm ok  

---

## 7. Folgen für Firmware / Betrieb

1. **0.3.19:** Heimfahrt one-shot — am **Heimnetz** + Collector, Journal leer → `autoCapture=false`; Hotspot/kein WLAN → weiter messen.  
2. Manuelles Aus auf `.89` bereits gesetzt (vor dem Flash).  
3. Nächste Feldnacht: Flag an → Fahrt → daheim Boot → Flag soll von selbst aus sein.

---

## 8. Offene Punkte

- Exakte Wanduhr der Stops vs. Remount-Zeiten (kein GPS/Operator-Log mit Timestamps)  
- Ob CLASSIC-Wechsel in `#23` 1:1 der bewusste Test war oder HU-Browse — Operator-Bestätigung hilfreich  
- Lab-Replay **B50** (`.88`) unverändert offen (nicht Gegenstand dieser Fahrt)
