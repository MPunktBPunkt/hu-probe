# Feldplan BMW NBT Evo: Größenleiter, Rückwärtssprung, LBA 24, Replug-Kontrolle

> **Hinweis 2026-10-24:** Die PiDrive-P0-Leiter (158 KiB / 1 / 4 / 16 MiB, Fresh+Cycle) liegt unter  
> [`2026-10-24-feldplan-groessenleiter-p0.md`](2026-10-24-feldplan-groessenleiter-p0.md) (`seq_size_ladder`).  
> Dieser Plan bleibt die ältere `seq_size` / `size_1_4_8`-Matrix (1/4/8 MiB + Nebenachsen).

**Stand:** 2026-10-20 · **Firmware:** 0.2.41 (gegenüber 0.2.40: Fix gelbes Blinken/Neustart nach dem Upload, Diagnose-Log, `seq_size` im Fein-Modus `fine_data`; Presets und Sequenzen sonst wie seit 0.2.31) · **Basis:** [Feld 20.10., Abschnitt 7](2026-10-20-bmw-free-offline-0.2.40.md#7-nachauswertung-rohdaten-nachgerechnet-2026-10-20-nachmittags)
**Regel:** Die Vorhersagen unten stehen **vor** der Fahrt fest. Nach der Fahrt wird nur abgehakt, welche eingetroffen ist; die Vorhersagen werden nicht nachträglich angepasst.
**Geltung:** nur BMW NBT Evo. Keine Aussage „Feld PASS“ ohne die hier beschriebene Messung und Auswertung.

---

## Vor der Fahrt (Labor)

| # | Prüfung | Abnahme |
|---|---------|---------|
| L-1 | `size_1_4_8`, `dur_mix`, `tail_probe` je per `POST /api/media/variant` an Lab-Debian aktivieren (falls aus [GATE-01-Plan](2026-10-19-feldsequenz-gate01-s6.md) L-2/L-3 noch offen) | `mount` fehlerfrei, `fsck.fat -n` ohne Fehler, Dateigrößen wie in `GET /api/media` |
| L-2 | `analyze_session.py` mit den neuen Abschnitten auf einer der Sessions vom 20.10. | `tur_phase.period_ms` ≈ 2238,8; `atoms` meldet 14 vollständige Atome; `plug_timeline` für 075935/080508 ≈ 1,51 s |
| L-3 | Collector neu gestartet (Fix B44) | neue Sessions tragen `parse_errors` pro Session und `collector_parse_errors_total` |
| L-4 | FW 0.2.41: Experiment `free`, 60 s, Fahrzeug `bmw` am Lab-Host; nach dem Upload 3 min beobachten. Vorher `boot_generation` aus `GET /api/status` notieren | LED nach dem Upload grün bzw. blau, **kein** gelber Blink-Zyklus, kein Neustart (`boot_generation` unverändert, `uptime_ms` läuft weiter). `GET /api/diag/log` als Datei sichern (auch bei Fehlschlag) |
| L-5 | Fein-Modus: Checkbox „DATA je 4 KiB“, 60 s mit einem großen Read | `capture_health.fine_data = true`; `analyze_session.py` zeigt Abschnitt „Chunk-Takt“ |

Auf dem Lab-Host (Linux) gelten die NBT-Vorhersagen nicht; dort nur Funktion prüfen.

---

## 1. Größenleiter `seq_size` (Kern dieser Fahrt)

- **Ziel:** Fenster-Modell A8 prüfen und klären, ob R21 („~15 s“) das Nachladen eines Fensters ist.
- **Setup:** Sequenz `seq_size` unverändert (S1M 90 s, S4M 180 s, S8M 180 s, `pidrive_l3` 180 s). Medium `size_1_4_8` = 1/4/8 MiB @128 kbit/s, `spc` 8.
- **Marker:** BOOT bei `play_start` (Track gewählt) und bei jedem hörbaren Wechsel.
- **Auswertung:** `python tools/analyze_session.py sessions/<id> --kbps 128`, Abschnitt `read_windows` (`cbw_blocks`, `matches_window_model`, `refill_intervals_s`, `read_ahead_s_if_playing_from_first`). Ab 0.2.41 läuft `seq_size` mit `fine_data`; Abschnitt `chunk_pacing` zeigt den 4-KiB-Takt innerhalb jedes CBW (Pausen > 10 ms).

**Vorhersagen (festgelegt 2026-10-20):**

| Hypothese | Erwartung in `read_windows` | Folge |
|---|---|---|
| **H-A Fenster, bedarfsgesteuert** (A8 + R21-Umdeutung) | Pro Datei mehrere Läufe zu je `240,240,32` (= 512 Blöcke). `refill_intervals_s` ≈ 16,4 s (± 2 s) bei 128 kbit/s. S4M in 180 s: ~11 Fenster, nicht die ganze Datei | R21 = Fenster-Nachladen; PiDrive-Live-Nachschub im 16-s-Takt planen |
| **H-B Read-All (Eager-EOF, R2)** | Ein zusammenhängender Lauf vom Kopf bis EOF in ~1 MB/s (S4M ≈ 4 s, S8M ≈ 8 s), CBW-Folge ein Vielfaches von `240,240,32` | Fenstergeometrie bleibt richtig, Takt nicht; R21 bleibt ungeklärt |
| **H-C Read-Ahead bis Grenze, dann bedarfsgesteuert** | Erst ein Block aus N Fenstern hintereinander, danach Fenster im ~16-s-Takt. N × 256 KiB = HU-Puffer | Puffergröße der HU direkt ablesbar |

Sonderfälle, die notiert werden: Ein Fenster, das nicht `240,240,32` ist (außer am Dateiende), widerlegt A8. Ein Fenster, das kürzer als 16 s vor dem Bedarf kommt, spricht für eine Wasserstandsgrenze statt „Fenster leer“.

## 2. Dauer-Regression `seq_dur`

- **Ziel:** Ergebnis von `demo_v1` (Folgetrack erst am Trackwechsel, kein Vorlesen) bei anderen Dauern bestätigen; Fenstertakt bei 48 kbit/s.
- **Setup:** Sequenz `seq_dur` unverändert. Dateigrößen bei 48 kbit/s ungefähr: D010 ≈ 118 Blöcke, D026 ≈ 305, D060 ≈ 704, D180 ≈ 2110.
- **Auswertung:** `python tools/analyze_session.py sessions/<seq_dur_*> --kbps 48 --compare derived/seq_dur_compare.json`

**Vorhersagen:**

- D010 wird mit einem CBW gelesen (< 240 Blöcke), D026 als `240,65` (wie `demo_v1` `240,69`).
- Folgetrack wird erst am Trackwechsel gelesen (`atom_end_to_next_read_s` ≈ Trackdauer − ~0,15 s), nicht davor. Falls doch früher: `lead_before_end_s` notieren, dann ist das Verhalten dauer- oder größenabhängig.
- Unter H-A: D060 liest `240,240,32` und nach ≈ 43,7 s (256 KiB @48 kbit/s) den Rest (~192 Blöcke). D180 liest Fenster bei ≈ 0 / 43,7 / 87,4 / 131 s. Unter H-B: alles am Trackwechsel.

## 3. Rückwärtssprung (tail_then_head-Modell)

- **Ziel:** Prüfen, ob tail_then_head eine Elevator-Reihenfolge ist (Rückwärtssprung in den LBAs) und kein Wiedereinstiegs-Zustand.
- **Wichtig:** Der Zieltrack darf seit dem Mount **noch nicht gelesen** worden sein. Sonst greift der HU-Cache (A-B-A ohne Reads, R9) und es gibt keine Reads.
- **Setup:** Experiment `free`, Medium `dur_mix`, 10 min. Neu mounten (OTG ab/an).
- **Ablauf:** D180 wählen (BOOT). Nach ~20 s D026 wählen (BOOT), das ist der Rückwärtssprung. Nach ~20 s D060 wählen (BOOT), das ist der Vorwärtssprung. Danach Autoplay laufen lassen.
- **Auswertung:** `atoms.atoms[].order` und `atoms.transitions[].to_order`.

**Vorhersagen:**

| Wechsel | Elevator-Modell | Wiedereinstiegs-Modell (verworfen, Kontrolle) |
|---|---|---|
| D180 → D026 (rückwärts, neu) | `tail_then_head` (65 Blöcke Ende, dann 240 Kopf) | `head_first` |
| D026 → D060 (vorwärts, neu) | `head_first` | `head_first` |
| Autoplay-Umlauf auf den ersten Track (falls im Fenster) | `tail_then_head` nur, wenn der Track > 240 Blöcke hat und nicht im Cache ist | — |

Falls der Erst-Read nach dem Mount ein Resume von D180 ist (letzter Track der Vorfahrt), ändert das nichts: Der zuletzt gelesene LBA liegt dann ebenfalls hinter D026.

## 4. LBA 24/8: Sequenz `seq_struct`, Schritt 2 (`tail_probe`)

- **Ziel:** Ist LBA 24/8 ein Dateisystem-Probe auf ein absolutes Offset?
- **Setup:** `seq_struct`, mindestens Schritt 2 (`tail_probe`, 40 reservierte Sektoren). Die anderen Schritte nach Zeit.
- **Auswertung:** `mount_signatures` (READ10-Folge pro Mount, `matches_reference`); Klassifikator zeigt LBA 24 als Reserved.

**Vorhersagen:**

- **Dateisystem-Probe (erwartet):** Mount liest weiter `24/8` zweimal, obwohl LBA 24 jetzt im Reserved-Bereich liegt.
- **Datei-relativ (Gegenhypothese):** Die Reads wandern auf ein Offset relativ zu TRACK1 oder zur Datenregion; `24/8` verschwindet.
- tail_then_head bei TRACK1 nur beim Umlauf T3 → T1 (wie bisher), sonst head-first.

## 5. Replug-Kontrolle (Resume)

- **Ziel:** Resume an der Aussteckstelle bestätigen (heute n = 1) und klären, ob die Identität das Resume steuert.
- **Setup:** Experiment `free`, 10 min, Medium `cache_a`.
- **Ablauf A (gleiche Identität, 3×):** TRACK1 wählen. Jeweils mitten in einem Track OTG physisch ab- und nach ~5 s wieder anstecken. BOOT direkt **vor** dem Ausstecken, BOOT beim **hörbaren** Wiedereinsatz, BOOT beim nächsten hörbaren Trackwechsel. Serial vor und nach dem Replug im Session-Header prüfen.
- **Ablauf B (neue Identität):** Medium auf `ident_b` (Serial `HP0B01`, Label `HUPROBE_B`, Volume-ID `0x48500B01`, Inhalt wie `cache_a`), dann ein Replug wie in A.

**Vorhersagen:**

- A: Nach jedem Replug liest die HU zuerst den zuletzt gespielten Track als volles Atom ab Block 0 (≈ 1,51 s nach `set_address`). Der nächste Track folgt nach der Restlaufzeit ab der Aussteckstelle (Marker-Abstand ≈ `atom_end_to_next_read_s`), nicht nach voller Trackdauer.
- B: offen. Bricht Resume, beginnt die HU mit einem anderen Track oder ab Trackanfang (nächster Read nach voller Dauer). Bleibt Resume, hängt die Position nicht an Serial, Label oder Volume-ID.

## 6. Reihenfolge und Zeitbedarf

| Schritt | Zeit | Bedingung |
|---|---|---|
| 1 `seq_size` | ~11 min + Wechsel | immer |
| 2 `seq_dur` | ~8 min + Wechsel | immer |
| 3 Rückwärtssprung | ~5 min | immer |
| 4 `tail_probe` | ~2 min (nur Schritt 2) | immer |
| 5 Replug A, dann B | ~10 min | im Stand |

Bei einem Pufferüberlauf teilt der Runner einen Sequenzschritt selbst (`splits` im Status); das ist kein Fehler, wird aber notiert.

---

## Nach der Fahrt

1. Sessions sichern, `analyze_session.py` pro Session (`--kbps 128` für `size_1_4_8`, `--kbps 48` für `dur_mix`), `--compare` pro Sequenz.
2. Jede Vorhersage oben als eingetroffen / nicht eingetroffen / nicht messbar markieren und in einen Feldbericht unter `docs/feld/` übernehmen.
3. Profil `profiles/bmw_nbt_evo.json` nachziehen: `read_window_blocks` (HYPOTHESIS → FACT oder REJECTED), `tail_then_head`, `mount_lba24_8_twice`, `remount_resume_position`.
4. PiDrive-Matrix A5/A8 und Zeile „Nächste Datei vorlesen“ nachziehen.
