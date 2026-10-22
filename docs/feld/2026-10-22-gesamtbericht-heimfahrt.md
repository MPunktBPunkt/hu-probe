# Gesamtbericht: BMW Heimfahrt Offline-Capture 2026-10-22

**Probe:** Auto-ESP `.89` · FW **0.3.17** · Medium `drive_mix` · `autoCapture` / Experiment `drive_home`  
**Collector:** `192.168.178.187:9100` · Upload abends vollständig (`#22`–`#24` `uploaded=true`)  
**HU:** BMW NBT Evo · Operator: 1–2 Stops, lange Ordner-Schleife, bewusster Wechsel „erster Song“ zwischen Ordnern  
**Kurznotiz:** [`2026-10-22-heimfahrt-auswertung.md`](2026-10-22-heimfahrt-auswertung.md) · Analyzer unter `sessions/*/derived/`  
**Reviews:** GPT-Konsens (§9) · Grok-Übergabe [`2026-10-22-uebergabe-heimfahrt-grok.md`](2026-10-22-uebergabe-heimfahrt-grok.md)

---

## 0. Ampel (nach GPT-Review)

| Bereich | Status | Beleg |
|---|---|---|
| Flash-Journal + Stored-Replay → Collector | 🟢 | Trips `#22`/`#23`/`#24` delivered, Integrity 1 |
| Remount-Sequenzen (Stops) | 🟢 FACT | `#22` @0,24 + **595,7 s**; `#23` @0,25 + **448,5 s** |
| **Remount ≠ Cold Start** | 🟢 STRONG | `#22` 2. Mount mit **395xxx-LBA-Probe**; `#23` Resume in ROCK/R04-Kontext |
| Eager Body / ~90 % File-Cache | 🟢 FACT | `#23` R01–C04 Abdeckung ~93–99 % |
| Nachbar-Prefetch aggressiv | 🟢 FACT | C0x Folgetrack in wenigen Sekunden; teils ~4 ms Atom-Abstand |
| LBA-Scheduler (Interleave) | 🟢 STRONG | R04: Regionen verschachtelt, kein strikter Elevator |
| ~8 s Prefetch-Lead als Allgemeinparameter | 🔴 verworfen | nur situationsgebunden; Vorlauf oft 140–240 s |
| TUR idle **2,2388 s** | 🟢 FACT | drei Heimfahrt-Sessions + Media-Leiter |
| 4‑KiB-Takt / 240er READ10 / ~1 MiB/s | 🟢 FACT | `#23` p50 3,896 ms, n≈10 k |
| Play Intent ≠ großer Read | 🟠 | C01–C04 fast komplett ohne Audible-Beweis |
| Cache-Key / Identity | 🟠 | unverändert offen |
| Audible GATE | 🔴 | weiter nötig |

---

## 1. Datenbasis

| Trip (Flash) | Bytes | Ende | Session | Events / Reads | Remounts |
|---|---:|---|---|---:|---:|
| **#22** | 752 379 | `power_loss` | `12777_bmw_drive_home` | 50 187 / 23 189 | **2** |
| **#23** | 377 662 | `max_duration` | `20261022-171751_bmw_drive_home` | 24 456 / 10 765 | **2** |
| **#24** | 45 206 | `power_loss` | `20261022-173030_bmw_drive_home` | 2 924 / 1 121 | 1 |

Partials/TCP-Reconnects: `20261022-165516_*`, `20261022-165747_*` (Prefix von `#22`).

Artefakte: `docs/feld/artifacts/20261022-heimfahrt/`.

---

## 2. Trip #22 — Hauptfahrt

**Mount-Signaturen** (`capture_t_us`):

| t | Signatur (gekürzt) | Deutung |
|---|---|---|
| ≈ 0,24 s | `0/8,0/8,96/40,…,131072/8` | normale Initialsignatur |
| ≈ **595,7 s** | `0/1,0/1,0/8,**395136/8,395248/8**,8/8,395256/1…395260/1` | **andere** Signatur — positionsbezogene Probe (Semantik 395xxx: UNKNOWN) |

Danach nicht „Medium von vorne“, sondern INTRO-Probe → wieder **MIX/M01** (Kontext der Etappe vor dem Stop).

**Ablauf:** JAZZ → MIX (lange) → Remount → PODCAST → POP/CHARTS-Browse → POP → ROCK bis `power_loss`.

---

## 3. Trip #23 — Fortsetzung / max_duration

**Mounts:** ≈ 0,25 s (normale Signatur) und ≈ **448,5 s**.

Vor Remount: **ROCK/R04** über Minuten. Nach Remount:

```text
R04 → R01 → R02 → CLASSIC/C01…C04 → R03 → R04
```

Kein „Mount → Datei 1 des Mediums“, sondern Kontextaufnahme + Nachbar-/Folgetracks. Nächster Einstieg oft Head (`0/240`) dann Eager Body (~90 %+).

Folgetrack-Abstände (Analyzer): R04→R01 9,4 s · R01→R02 7,9 s · R02→C01 24,6 s · C01→C02 4,9 s · … · C0x teils **~4 ms** Atom-Abstand.

---

## 4. Trip #24

Nur CLASSIC/C01 nach Mount; früher `power_loss`.

---

## 5. Read-Modi (Konsens)

| Modus | Verhalten | Beispiel |
|---|---|---|
| **A Head/Selection** | Kopf + Einstieg (u. a. 240er) | R01/C01 `@0` |
| **B Eager Body** | großer Teil bis ~90–99 % der Datei, 240 Blöcke / ~120 KiB, ~1 MiB/s | C01–C04, R01/R02 |
| **C Scheduler-Interleave** | mehrere LBA-Regionen / Dateien im selben Fenster verzahnt | R04 Sprünge `7464/…` dann wieder `1536/…`; CHARTS H01↔H05 |

**Elevator-Korrektur:** nicht „global strikt aufwärts“, sondern **LBA-Scheduler**, der Regionen priorisiert und umschaltet — erklärt `tail_then_head`, Nachbar-Prefetch und Sub-Sekunden-Dateiwechel.

**Prefetch-Lead:** MIX ~7 s vor Dateiende bleibt situationsgebunden FACT; ROCK/CLASSIC oft **140–240 s** Vorlauf → kein einzelner HU-Parameter „8 s“.

**Play Intent:** großer Eager-Read ≠ hörbare Wiedergabe (C01–C04 kurz hintereinander fast voll) → Audible-GATE bleibt P0.

---

## 6. Stabile Timing-Parameter (verstärkt)

| Parameter | Wert | Status |
|---|---|---|
| Idle TUR | **2,2388 s** | FACT |
| 4‑KiB-Chunk-Takt | p50 **3,896 ms** (`#23` n≈10 k) | FACT |
| READ10 Bulk | **240 Blöcke** / ~1 MiB/s | FACT |
| BUSY ohne Reset | `#23` R04: 16 Blöcke BUSY ~1,6 s, abandoned, weiter normal | FACT (Backpressure) |

---

## 7. Capture-Pfad / Upload

`power_loss` / `max_duration` wie erwartet; Replay `delivered` · `integrity=1`; HTTP während Replay oft tot, TCP ok.

---

## 8. Firmware / Betrieb

**0.3.19** (beide ESPs): Heimfahrt one-shot — Heimnetz + Collector + Journal leer → `autoCapture=false`; Hotspot/kein WLAN → messen.

---

## 9. GPT-Konsens — Modellkorrektur

Heimfahrt wiederholt nicht nur die Media-Leiter, sondern korrigiert das Modell:

1. **Remount ≠ Cold Start** — Kontext/positionsbezogene Probe (`#22` 395xxx) + Resume nahe letztem Bereich (`#23` R04).  
2. Prefetch = **mehrere Mechanismen**, nicht ein fester 8‑s-Lead.  
3. Scheduler **interleaved** Regionen/Dateien.  
4. Nachbarn werden **aggressiv / teils fast vollständig** vorbereitet.  
5. Eager-Read ≠ Play Intent.  
6. USB-Lieferseite weitgehend verstanden; nächster Sprung = **Audible + Play-Intent**, nicht weitere Media-Ladder-Varianten.  
7. Offene Blöcke vor robustem PiDrive-Live: **P0 Audible GATE**, **P0 Play-Intent-Mapping**, **P1 Cache-Key**, **P1 Remount-State-Semantik** (395xxx).

```text
USB Mount
  ├── FS / Identity-Probes
  ├── Directory / Media Model
  └── positions-/zustandsabhängige Probe   ← neu belegt (#22)
        ▼
  Candidate Files → Head → Eager Body → Neighbor Prefetch → Partial Refill
        ▼
  HU Cache ──┬── Decode → Audible   ← GATE fehlt
             └── Navigation → further reads
```

---

## 10. Nächster Schritt (explizit nicht)

**Nicht:** weitere Prefetch-Zahlen durch Media-Ladder-Varianten.

**Doch:** kontrollierter Lauf mit **hörbarem Marker** + Korrelation Read ↔ Play Intent; parallel Cache-Key (`seq_ident`) und Analyse der **395xxx**-LBAs am Remount.
