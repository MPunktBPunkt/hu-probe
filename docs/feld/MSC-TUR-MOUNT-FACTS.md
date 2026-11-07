# Unabhängige Auswertung BMW NBT Evo — alle Sessions bis 2026-10-19

**Stand:** 2026-10-19 · **Basis:** alle `sessions/*_bmw_*` (26 BMW-Sessions, FW 0.2.11 … 0.2.30), Lab-Debian als Referenz
**Anlass:** Review von [`2026-10-19_Bericht.txt`](../../../../2026-10-19_Bericht.txt) (Cursor / Mistral / GPT / Grok) zu `20261019-075344_bmw_s2_prefetch`
**Methode:** Rohdaten (`events.jsonl`) direkt, BOT-Tags, CBW-LBA/Blocks, TUR-Raster; Firmware-Code `OfflineCapture`, `MscBotTap`, `ProbeClock`, `MscGadget`
**Epistemik:** `FACT` (Rohdaten + Code eindeutig) · `MEASURED` (Zahl, Deutung offen) · `INFERRED` · `HYPOTHESIS` · `REJECTED`
**Geltung:** nur BMW NBT Evo. Nicht auf Mazda übertragen. Kein Feld-PASS ohne neue Messung.

---

## 1. Korrekturen am Multi-KI-Bericht

### K1 — „Enum-Grammatik" ist die Media-Loss-Reaktion der HU · `FACT`

- Die HU vergibt BOT-Tags monoton.
- Der Offline-Puffer von `075344` enthält **genau die Tags 81–122**: 42 CBW, 42 CSW und 39 DATA, zusammen 123 Records (= `offline_records`).
- Die Folge `TUR status=1 → REQUEST_SENSE → PREVENT_ALLOW status=1` trägt die Tags **131–146**.
- Diese Events laufen **live** nach `MscGadget::hideMedia()` im Finalize (`OfflineCapture::poll` → `requestStop("auto_media_hide")` → `beginFinalize`). Sie mischen sich mit dem Replay, weil `MscBotTap::drain` bei `!capturing()` live emittiert.
- Die Tags 123–130 fehlen: Sie wurden emittiert, bevor die Upload-Session aktiv war, und von `ReadInstrument` verworfen (`!session_->active()`).
- Enumeration ist nicht enthalten, weil `soft_usb=false` galt; der erste Tag ist 81, die HU hatte also schon 80 Kommandos geschickt.
- **Neu:** „Enum-Grammatik" ist als Deutung `REJECTED`. Die Folge ist die Reaktion der HU auf `NOT READY / MEDIUM NOT PRESENT` (`MEDIA-LOSS-GRAMMAR`, `FACT`).

### K2 — TUR-Leerlauf = 2,239 s, nicht „~1 TUR/s" · `FACT`

- Datenbasis: alle Abstände aufeinanderfolgender TUR außerhalb von Mount-Bursts, 19 BMW-Sessions mit Leerlauf, **n = 382**.
- Ergebnis: p10 / Median / p90 = **2238 / 2239 / 2239 ms**; 351 von 382 Werten liegen im 2,2-s-Bin.
- Der Timer läuft phasenstarr weiter, auch über Reads hinweg, und ist damit unabhängig von der Leseaktivität.
- „~1 Hz", „0,8–1/s" und „Golden ≈ 1/s" sind `REJECTED`.
- Die frühere „Session-TUR-Hz ~2,06" (`EVID-HUPROBE-2026-10-15-012`) ist ein Fenstermittel inklusive Mount-Burst und keine Leerlauf-Rate.

### K3 — 57,33 s `gap_us` ist keine Latenz zwischen Enumeration und Read · `FACT` (Code) / `INFERRED` (Zeitlage)

- `gap_us = now − lastDataEndUs_` wird global gemessen, auch über den Arm-Zeitpunkt hinweg.
- `075344` enthält 7 TUR vor dem ersten Read und 33 danach. Mit 2,239 s pro TUR liegt der Read bei etwa **15,7 s nach dem Arm**, danach folgen etwa 74 s bis zum Ende des 90-s-Fensters.
- Der letzte DATA-Transfer vor dem Arm lag also etwa **41,6 s vor dem Arm**, in der Vorbereitung.
- Die Deutung „HU braucht 57 s von Enum bis Read" ist `REJECTED`.

### K4 — „Prefetch geschwächt / read-all-on-selection" (M3) ist nicht belegt · `HYPOTHESIS`

- Dass nach dem T2-Read kein Folgetrack gelesen wurde, ist mehrdeutig:
  - Cache-Hit, weil T3 oder T1 schon vor dem Arm gelesen war (identische Bytes, identische Größe);
  - Repeat-Modus;
  - wirklich kein Prefetch.
- `20261014-165304`: Es gibt Marker `track1/2/3_autoplay`, aber nur einen T2-Read, was auf Cache-Hits deutet.
- Solange die Tracks byte-identisch sind, kann keine Session M3 entscheiden. M3 wird von „neu" auf `HYPOTHESIS` zurückgestuft.

### K5 — B21 (Klassifikator) gelöst · `FACT`

- Die Bänder `fat = 1–16` und `dir = 17–48` in `MscGadget::tagForLba` sind fest kodiert. Das Demo-Image hat aber FAT = LBA 1–6, Root = 7–8 und Daten ab 9.
- T1-Reads auf LBA 9 / 17 / 25 / 33 / 41 werden deshalb als 1 × `fat` + 4 × `dir` + 34 × `file` klassifiziert, zusammen 39 Einträge (z. B. `212136`).
- Das ist ein Instrument-Artefakt und keine HU-Semantik. Fix: Klassifikator aus dem aktiven Medium (FW 0.2.31).

### K6 — B7 Ursache gefunden · `FACT` (Code)

- `OfflineCapture::appendPending` speichert `rec.tUs` korrekt.
- Das Replay läuft nach `Session::start()` (≈ 135,1 s). `ProbeClock::publishUs()` klemmt jeden älteren Wert auf das Watermark, sodass alle `t_us` auf den Sessionstart kollabieren.
- Die Original-tUs liegen intakt im RAM.
- Fix (FW 0.2.31): Das Replay emittiert zusätzlich `capture_t_us` (ungeklemmt), `capture_seq` und `origin:"offline"`.

### K7 — B20 Build-Label · `FACT`

- `fw_sha 5a33518…` gehört zu einem Build mit Label `0.2.30` aus einem anderen Baum.
- Fix: Das Build-Skript schreibt `FW_GIT_DIRTY`, und Release-Builds (`HUPROBE_RELEASE=1`) brechen bei einem Dirty-Tree oder einer Tag-Abweichung ab.

---

## 2. Neue bzw. bisher unbekannte HU-Parameter

| ID | Parameter | Wert | Klasse | Sessions |
|----|-----------|------|--------|----------|
| P-TUR | TUR-Leerlauf | 2239 ms (p10/p90 2238/2239), phasenstarr | `FACT` | 19 BMW-Sessions mit Leerlauf |
| P-MOUNT | Mount-Kommandos | INQUIRY ×1, READ_CAPACITY10 ×3, MODE_SENSE6 ×6, PREVENT_ALLOW ×7, **32 TUR** (≈15 als Burst in 0,05–0,29 s) | `FACT` | alle BMW-Mounts |
| P-MREAD | Mount-READ10-Folge | `0/8, 0/8, 24/8, 0/8, 0/1, 1/3, 7/2, 1/1, 2/1, 3/5, 24/8`; Variante: `3/5` → `3/1 + 4/4` | `FACT` | alle; Variante `195657`, `2363` |
| P-LBA24 | LBA 24/8 doppelt im Mount | liegt mitten in TRACK1 (9–317) | `MEASURED`, Zweck `UNKNOWN` | alle |
| P-RESUME | Resume-Read | zuletzt gespielter Track 0,2–0,3 s nach Mount-Ende | `MEASURED` | S0/S2 |
| P-NEXT | Zyklischer Next-Track-Read | T(n) → T(n+1), T3 → T1; ≈ 9–16 s nach Trackstart, also 11–18 s vor Ende bei 26,5 s Laufzeit (n ≈ 10) | `MEASURED` / Regel `INFERRED` | S0/S2 |
| P-SEL | Auswahllatenz Marker → READ | manuell 1,7–1,9 s (`171815`, `2280`); Autoplay-Sprung ≈ 0,2 s (`200036`) | `MEASURED` (Marker teils nachgezogen) | |
| P-TAIL | tail_then_head | T1 als Folgetrack in 8/9 Fällen Ende zuerst (`249/69`, dann `9/240`); T2/T3 und T1 als Erst-Read immer Kopf zuerst | `MEASURED` / Ursache `UNKNOWN` | S0/S2 |
| P-ATOM | Read-Atom | 240 Blöcke pro CBW plus Rest; 158 KiB-Datei in einem Burst | `FACT` für 158 KiB, offen für ≥ 1 MiB | alle |
| P-LOSS | Media-Loss-Grammatik | `TUR(1) → REQUEST_SENSE → PREVENT_ALLOW(1)` | `FACT` | `075344` |
| P-DEB | Lab-Debian zum Vergleich | TUR ≈ 2,0 s; Read-Größen 32/64/128/240 | `MEASURED` | `lab_debian_*` |

Hinweise:
- Bei P-NEXT lässt sich mit einer einzigen Trackdauer nicht trennen, ob die HU sich am Vorlauf vor dem Ende oder an der Verzögerung nach dem Start orientiert. Das klärt erst `SEQ-DUR`.
- Für P-TAIL gibt es den Verdacht einer Wechselwirkung mit dem Mount-Read 24/8, der im HU-Blockcache liegen könnte. Prüfung mit `tail_probe` (Trackstarts weg von LBA 24) und `hdr_variants` (ID3v1/APIC).
- P-ATOM ist für PiDrive entscheidend (8-MiB-Slots) und wird mit `SEQ-SIZE` geprüft.

---

## 3. Konsequenzen

| Bereich | Maßnahme |
|---------|----------|
| Firmware 0.2.31 | `capture_t_us` / `capture_seq` / `origin`, Post-Finalize-Puffer, Klassifikator aus dem Medium, Feld `file`, TUR-Statistik, Build-Check |
| Medium | synthetisches FAT16-Medium `SynthFat` mit Presets (siehe [`../MEDIA_VARIATION.md`](../MEDIA_VARIATION.md)) |
| Abläufe | Sequenzen `SEQ-DUR / SEQ-SIZE / SEQ-CACHE / SEQ-STRUCT` (siehe [`2026-10-19-feldsequenz-gate01-s6.md`](2026-10-19-feldsequenz-gate01-s6.md)) |
| Analyse | [`tools/analyze_session.py`](../../tools/analyze_session.py) → `derived/analysis.json` |
| PiDrive | `nbt_hu_sim.py --hu-model nbt_evo_2026_10`, `--replay-huprobe`; Facts E3 und Matrix korrigiert |
| Audit | `EVID-HUPROBE-2026-10-19-001 … 010` in technology-audit |

## 4. Offen

- Ob der Next-Track-Read am Vorlauf vor dem Ende oder an der Verzögerung nach dem Start hängt (`SEQ-DUR`).
- Cache-Schlüssel: Inhalt, LBA, Name oder Identität (`SEQ-CACHE`).
- Streaming gegen Read-All bei 1/4/8 MiB (`SEQ-SIZE`).
- Ursache von tail_then_head und Zweck von LBA 24/8 (`SEQ-STRUCT`).
- Ob die HU Serial, Label oder Volume-ID als Identität nutzt (S6).
