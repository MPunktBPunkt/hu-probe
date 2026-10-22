# Übergabebericht Heimfahrt 2026-10-22 (Grok)

**hu-probe / BMW NBT Evo · drive_mix · FW 0.3.17 → 0.3.19**  
Stand: Commits `28249de` / `c413480` · Sessions `#22`–`#24` delivered · Gesamtbericht [`2026-10-22-gesamtbericht-heimfahrt.md`](2026-10-22-gesamtbericht-heimfahrt.md) · Artefakte `docs/feld/artifacts/20261022-heimfahrt/`

---

## 1. Gesamturteil

Die Heimfahrt ist der bisher stärkste Erkenntnissprung nach der Media-Leiter. Sie bestätigt und verschärft mehrere Befunde und widerlegt gleichzeitig die zu einfachen Modelle („globaler Elevator“, „fester 8-s-Prefetch“, „Remount = Cold Start“, „großer Read = Play Intent“).

**Wichtigste neue Aussage:**  
Remount setzt den HU-Zustand **nicht** auf „Medium von vorne“ zurück. Die HU nimmt einen positions-/zustandsabhängigen Kontext wieder auf und baut gezielt Nachbar-Dateien auf. Prefetch besteht aus mehreren Mechanismen; der ~7–8-s-Lead ist nur situationsgebunden.

---

## 2. Neue / verschärfte Muster

### Remount ≠ Cold Start (STRONG FACT)

- **#22:** Zweiter Mount bei ~595,7 s zeigt 395xxx-LBA-Probes (`395136/8`, `395248/8`, `395256/1` …) statt der normalen Initialsignatur → positionsbezogene Prüfung.
- **#23:** Vor Remount läuft `ROCK/R04`; nach Remount (448,5 s) geht es in denselben Kontext weiter (`R04 → R01 → R02 → CLASSIC/C0x`).

Kein „Mount → kompletter Baum-Scan → Datei 1“.

### Drei Read-Modi (FACT/STRONG)

1. **Head/Selection** – `0/240` + `240/240` + `480/32` (o. ä.) pro Datei.
2. **Eager Body** – praktisch vollständige Datei (~90–99 % Coverage bei R01–C04 in #23).
3. **Neighbor Prefetch / Partial Refill** – aggressive Vorbereitung mehrerer Kandidaten, teilweise mit Interleave von LBA-Regionen innerhalb einer Datei (R04 zeigt verschachtelte Bereiche, u. a. um Block 3840 bei 13 312 Blocks Gesamtgröße).

### LBA-Scheduler (Revision des Elevator-Modells)

Kein globaler aufsteigender Elevator. Die HU priorisiert und interleavt mehrere angeforderte Regionen. Das erklärt `tail_then_head`, Nachbar-Prefetch und die kurzen 0,15–0,7-s-Wechsel zwischen Dateien besser.

### Nachbar-Prefetch extrem aggressiv (FACT)

In #23: C01→C02→C03→C04 mit Abständen bis ~4 ms zwischen Atomen; Coverage 93–99 %. Mehrere Dateien werden fast vollständig vorbereitet, ohne dass wir Audible beweisen können → Eager-Read ≠ Play Intent.

### Prefetch-Lead ist kontextabhängig (FACT, 8-s-Parameter verworfen)

- MIX-Situationen: ~7–8 s vor Ende (wie Media-Leiter).
- ROCK/CLASSIC in #23: 140–240 s Vorlauf.

Der Wert hängt vom Scheduler-/Browsing-/Cache-Zustand ab und darf nicht als HU-Konstante geführt werden.

### TUR + Transport bleiben ultra-stabil

- TUR idle: **2,2388 s** (drei Heimfahrt-Sessions + Media-Leiter).
- 4-KiB-Takt: p50 **3,896 ms** (n≈10 k in #23).
- 240-Block READ10 ≈ 120 KiB, ~1,0 MiB/s effektiv.

### BUSY als kontrollierter Backpressure

In #23: 16-Block-Read mit BUSY 1,623 s → abandoned, danach normale TUR/READ-Fortsetzung, kein Reset/Remount. Relevant für PiDrive-Pacing.

---

## 3. Aktualisiertes HU-Modell

```text
USB Mount
 ├─ Filesystem / Identity-Probes
 ├─ Directory / Media Model
 └─ positions-/zustandsabhängige Probe (395xxx o. ä.)
        ▼
 Candidate Files
 ├─ Head Read
 ├─ Eager Body Cache
 ├─ Neighbor Prefetch
 └─ Partial Refill / Cache Refresh
        ▼
 HU Cache
 ├─ Decode → Audible Playback   ← GATE-01 fehlt noch
 └─ Navigation → weitere Reads
```

Play Intent sitzt irgendwo zwischen Candidate Selection und Decode – isoliert noch nicht.

---

## 4. Konsequenzen für PiDrive

USB-/Datenlieferseite ist weitgehend kein Erkenntnisproblem mehr:

- Struktur + Head (~80 KiB) + korrekte Größen/Extents müssen stabil sein.
- 240-Block-Requests, ~1 MiB/s, mehrere Tracks gleichzeitig und Cache-/Prefetch-Reads sind beherrschbar.
- Dynamischer Tail ist weiterhin möglich.

Was noch fehlt (Priorität):

- **P0 Audible GATE** – Read → MP3-Inhalt → Decoder → hörbarer Track mit echtem Zeitmarker.
- **P0 Play-Intent-Unterscheidung** – nicht jeder große Read ist „jetzt spielen“.
- **P1 Cache-Key / Identity** (USB-Serial, Volume, FAT, Filename, Size, LBA, ID3, Inhalt?).
- **P1 Resume-/Remount-State** – was genau über den Remount erhalten bleibt (395xxx-Semantik klären).

---

## 5. Ampel (nach Heimfahrt)

| Bereich | Status |
|---|---|
| USB-MSC/BOT, FAT, Directory | 🟢 |
| File Head + Size/Extents | 🟢 |
| 240-Block / ~1 MiB/s / 4-KiB | 🟢 |
| TUR 2,2388 s | 🟢 |
| Eager Cache + Neighbor Prefetch | 🟢 |
| LBA-Scheduler (Interleave) | 🟢 STRONG |
| Remount ≠ Cold Start | 🟢 STRONG |
| Partial Refills | 🟢 |
| Cache Persistence / Key | 🟠 |
| Play Intent | 🟠 |
| Audible Live-Audio | 🔴 |

---

## 6. Was jetzt **nicht** mehr tun

- Weitere Media-Ladder-Varianten nur um Prefetch-Zahlen zu sammeln.
- „Prefetch = 8 s“ oder „Elevator immer aufwärts“ als Parameter führen.
- Großen File-Read automatisch als Play Intent werten.
- Remount als vollständigen Cache-Flush annehmen.

---

## 7. Nächster sinnvoller Schritt

Kontrollierter Lauf mit **hörbarem Marker** (GATE-01) + Korrelation Read ↔ Play Intent. Parallel: Analyse der 395xxx-LBAs am Remount und gezielter Cache-Key-Test (gleiche vs. geänderte Identity bei identischer Struktur/Head).

Die Heimfahrt hat das Modell von „USB-Host mit Prefetch“ zu „zustandsbehafteter Medien-Scheduler mit aggressivem Kandidaten-Cache“ geschärft. Die reine Lieferseite ist reif; der nächste Sprung kommt von Audible + Intent, nicht von noch mehr passiven Traces.
