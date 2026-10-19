# Feldsequenz BMW NBT Evo: GATE-01 bis S6

**Stand:** 2026-10-19 · **Firmware:** ab 0.2.31 (F1, SynthFat, Sequenz-Runner) · **Basis:** [unabhängige Auswertung](2026-10-19-unabhaengige-auswertung.md)
**Reihenfolge:** nach Erkenntnis pro Fahrt. Jeder Schritt baut auf dem vorherigen auf. Ein Schritt gilt erst als erledigt, wenn seine Abnahme erfüllt ist.
**Geltung:** nur BMW NBT Evo. Keine Aussage „Feld PASS“ ohne die hier beschriebene Messung.

---

## Vor der Fahrt (Labor, Pflicht)

Die Firmware 0.2.31 ist bisher weder gebaut noch auf Hardware getestet. Vor dem ersten Feldeinsatz deshalb einmal im Labor:

| # | Prüfung | Abnahme |
|---|---------|---------|
| L-1 | Release-Build mit `HUPROBE_RELEASE=1` auf getaggtem, sauberem Baum (`v0.2.31`) | Build bricht nicht ab; `session.fw_dirty = false` |
| L-2 | Jedes Preset per `POST /api/media/variant` an Lab-Debian aktivieren | `mount` ohne Fehler, `ls -R` zeigt die Dateien aus `GET /api/media`, `fsck.fat -n` meldet keine Fehler |
| L-3 | Je eine Datei aus `dur_mix`, `hdr_variants` und `size_1_4_8` mit `ffprobe` prüfen | Dauer ± 1 Frame wie in der Spec, Bitrate stimmt, Header-Variante erkennbar |
| L-4 | Stempel prüfen: `xxd` auf Frame-Offset 36 einer Datei aus `cache_a` und `cache_b` | `HP <node> <seed>` unterscheidet sich zwischen `cache_a` und `cache_b` |
| L-5 | Offline-Lauf 90 s auf Lab-Debian mit `demo_v1`, danach `tools/analyze_session.py` | `time_base.source = capture_t_us`, `integrity.complete = true`, keine Lücke in den BOT-Tags |
| L-6 | Sequenz `seq_cache` mit `auto_arm` an Lab-Debian | alle 4 Schritte laufen durch, je eine Session pro Schritt, `media.variant` passt zum Schritt |

Erst wenn L-1 bis L-6 erfüllt sind, wird die Sonde ins Fahrzeug gesteckt.

---

## 1. GATE-01: Messkette mit `demo_v1` abnehmen

- **Ziel:** Nachweis, dass F1 im Fahrzeug wirkt. Vergleichbar mit allen bisherigen Feldläufen, weil das Medium identisch ist.
- **Setup:** Medium `demo_v1`, Experiment `s2_prefetch`, 90 s, Ende `media_hide`, Soft-USB an.
- **Ablauf:** Offline-Messung starten. Nach dem Mount TRACK1 wählen, Autoplay bis TRACK3 laufen lassen. Marker per BOOT: `play_start`, `track_next`, `audible_change`, `stop`.
- **Abnahme:**
  - `capture_t_us` vorhanden und monoton; der Abstand der Leerlauf-TUR liegt bei etwa 2,24 s.
  - Der Klassifikator liefert für LBA 9–317 `file=TRACK1.MP3` und kein `fat`/`dir` mehr im Datenbereich (B21).
  - BOT-Tags lückenlos von der ersten CBW bis zur Media-Loss-Reaktion; `offline_post_finalize_records > 0`, `offline_post_finalize_dropped = 0`.
  - Automarker `mount_done`, `first_read`, `next_track_read`, `media_hide` vorhanden.
- **Auswertung:** `python tools/analyze_session.py sessions/<id>`
- **Bei Fehlschlag:** keine weiteren Schritte. Ursache im Labor klären.

## 2. S3 Cache: `SEQ-CACHE`

- **Ziel:** M3 und die fehlenden Reads klären (Cache-Hit gegen kein Prefetch). Cache-Schlüssel eingrenzen.
- **Setup:** Sequenz `seq_cache` (WebUI-Karte „Sequenz“). LED weiß blinkend heißt: Fahrer ist dran. BOOT kurz startet den Schritt, BOOT lang überspringt ihn.

| Schritt | Medium | Serial | Aktion | Erwartung, falls Content-Cache |
|---|---|---|---|---|
| 1 | `cache_a` | neu | TRACK1, Autoplay T1 → T2 → T3 | alle drei Tracks werden gelesen |
| 2 | `cache_a` | gleich | wie 1 | wenige oder keine Datei-Reads |
| 3 | `cache_b` | gleich | wie 1 | alle drei Tracks werden wieder gelesen |
| 4 | `ident_b` | fest `HP0B01` | wie 1 | Ergebnis zeigt, ob die Identität den Cache invalidiert |

- **Abnahme:** pro Schritt eine Session mit `integrity.complete = true` und je mindestens ein Marker `play_start`.
- **Entscheidung:**
  - Schritt 2 ohne Reads und Schritt 3 mit Reads: Der Cache hängt am Inhalt (oder an den FAT-Metadaten). M3 wird `REJECTED`.
  - Schritt 2 mit vollen Reads: kein persistenter Cache über Re-Enumeration hinweg.
  - Schritt 3 ohne Reads: Der Cache hängt nicht am Inhalt. Dann folgt `cache_place` (nur LBA geändert) als Zusatzschritt.

## 3. Dauer: `SEQ-DUR`

- **Ziel:** Prefetch-Regel trennen: Vorlauf vor dem Ende oder Verzögerung nach dem Start.
- **Setup:** Sequenz `seq_dur` mit Medium `dur_mix` (10 / 26 / 60 / 180 s).
- **Abnahme:** mindestens je zwei Folgetrack-Reads nach D026, D060 und D180.
- **Auswertung:** `python tools/analyze_session.py sessions/<seq_dur_*> --compare derived/seq_dur_compare.json`
- **Entscheidung:**
  - Die Streuung von `lead_before_end_s` ist klein (< 3 s) und die von `lag_after_start_s` groß: Regel „Vorlauf vor Ende“.
  - Umgekehrt: Regel „Verzögerung nach Start“.
  - D010: Wird der Folgetrack schon vor dem Start von D010 gelesen? Das zeigt eine Mindestdauer.
- **PiDrive:** Das Ergebnis geht als `next_track_rule` in `nbt_hu_sim.py --hu-model nbt_evo_2026_10` und in das Lab-Profil.

## 4. S1 Größe: `SEQ-SIZE` mit PiDrive-Layout

- **Ziel:** Streaming gegen Read-All, Refill-Schwelle und Chunking bei 1 / 4 / 8 MiB. Direkt relevant für die PiDrive-Slots.
- **Setup:** Sequenz `seq_size` (Schritte 1–3 `size_1_4_8`, Schritt 4 `pidrive_l3`).
- **Abnahme:** pro Schritt eine vollständige Session. Bei einem Pufferüberlauf teilt der Runner den Schritt automatisch in zwei halbe Läufe (`splits` im Status).
- **Auswertung:** Episoden pro Datei: `coverage`, Abstand zwischen den Episoden, Blöcke pro CBW.
- **Entscheidung:**
  - `coverage ≈ 1` in einer Episode: Read-All.
  - Mehrere Episoden im Abstand der Abspielzeit: Streaming. Aus dem Abstand und der Bitrate ergibt sich die Puffergröße der HU.

## 5. Struktur und tail_then_head: `SEQ-STRUCT`

- **Ziel:** Traversierung über Ordner, Ursache von tail_then_head und Zweck von LBA 24/8.
- **Setup:** Sequenz `seq_struct` (`folders_30`, `tail_probe`, `hdr_variants`, `frag_mix`).
- **Auswertung und Entscheidung:**
  - `folders_30`: Werden beim Mount alle Verzeichnisse gelesen (Scan) oder erst bei Auswahl (On-Demand)?
  - `tail_probe`: Liest die HU beim Mount weiter LBA 24/8, obwohl dort jetzt der Reserved-Bereich liegt? Dann ist es ein festes Offset. Bleibt tail_then_head bei TRACK1 bestehen?
  - `hdr_variants`: Tritt tail_then_head nur bei `H_ID3V1` auf? Dann ist es eine Tag-Suche am Dateiende.
  - `frag_mix`: Folgen die Reads der Cluster-Kette, und bleibt das 240-Block-Atom an Fragmentgrenzen erhalten?

## 6. S6 Identität

- **Ziel:** Ob die HU Serial, Label oder Volume-ID als Identität nutzt.
- **Setup:** drei eigene Specs auf Basis von `cache_a`, jeweils nur ein Merkmal geändert (`usb_serial`, `fs_label`, `volume_id`), per WebUI „Eigene Spezifikation“. Experiment `s6_identity`.
- **Ablauf:** je Variante TRACK1 wählen und Autoplay. Zwischen den Varianten Re-Enumeration (Wechselart „USB Re-Enum“).
- **Entscheidung:** Das Merkmal, nach dessen Änderung die HU wieder alle Tracks liest, ist Teil des Identitätsschlüssels.

## 7. Bremsen (BUSY): `SEQ-STALL` — nur im Stand

- **Ziel:** Timeout pro READ10 (Parameter-Matrix A4, P0), Reaktion bei Überschreitung, Ursache der Menülatenz unter Bremse (A3/R35).
- **Setup:** Sequenz `seq_stall`, Details und Auswertung in [`SEQ-STALL.md`](../SEQ-STALL.md). Erst nach GATE-01, weil die Haltezeit aus `capture_t_us` gemessen wird.
- **Reihenfolge:** Leiter 2 → 5 → 10 → 20 s. Die Schritte 30/60/120 s erst, wenn bis 20 s alles toleriert wurde. Danach Menü-Test (FAT/DIR gebremst) und Live-Tempo B032/B128/B320.
- **Abbruch:** Meldet die HU das Medium ab und erholt sich nicht innerhalb von 60 s, die Sequenz abbrechen (BOOT lang). Den Stick ab- und wieder anstecken und den Vorfall notieren.

---

## Nach jeder Fahrt

1. Sessions sichern (`sessions/`), dann `tools/analyze_session.py` pro Session und `--compare` pro Sequenz.
2. Ergebnisse mit Klasse (`MEASURED`/`INFERRED`) in einen Feldbericht unter `docs/feld/` übernehmen.
3. Neue Evidenz in technology-audit (`EVID-HUPROBE-…`) nachziehen. P-NEXT und P-TAIL bleiben `INFERRED`, bis `SEQ-DUR` und `SEQ-STRUCT` gelaufen sind.
4. PiDrive-Lab-Profil und `nbt_hu_sim.py` nachziehen.
