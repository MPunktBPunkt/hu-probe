# Feld-Checkliste — Direkt T2 Hold (P0 nach CBW-Korrektur)

**Datum:** 2026-10-18 (nächster Feldlauf) · **erledigt 2026-10-19** (`20261019-075344` Fall A)  
**FW:** ≥ 0.2.30 · Probe Auto `.89` · Collector `.187`  
**Übergabe:** [`docs/reports/2026-10-18-hu-probe-bmw-s2-abend-offline.md`](../reports/2026-10-18-hu-probe-bmw-s2-abend-offline.md) · Ergebnis: [`2026-10-19-bmw-s2-t2-offline.md`](2026-10-19-bmw-s2-t2-offline.md)

**Primärmetrik:** Track-Atom `(head,240)+(head+240,69)` = **309 LBA / 158208 B** — nicht „39 × 4 KiB“.

---

## 0. Vor dem Auto (5 min)

- [ ] Frischer RST auf der Probe; kurz warten (vermeidet `buffer_alloc_failed`)
- [ ] OTA/PiDrive-Hub nicht parallel (sonst Soft-Reset nach Offline)
- [ ] Experiment: `bmw` / `s2_prefetch` (oder Roh-Offline mit gleicher Marker-Idee)
- [ ] `soft_usb=false` · `end_strategy=media_hide` · `measure_ms` ≥ **90000**
- [ ] Notizblatt bereit: Session-ID, Marker-Zeiten (Wanduhr ok bis B7-Fix)
- [ ] Wissen: **nicht** T1→T2 navigieren — **direkt T2** starten und halten

### Marker-Sequenz (BOOT kurz = nächster Marker)

Empfohlen für diesen Lauf (manuell / BOOT-Zyklus):

| # | Marker | Wann |
|---|--------|------|
| 1 | `play_start` | T2 hörbar startet / Play bestätigt |
| 2 | `hold_2` | Track 2 gehalten, **kein** Skip zu 3 |
| 3 | `audible_change` | nur falls Hörwechsel zu anderem Track |
| 4 | `stop` | Ende / BOOT lang |

Optional (wenn ableitbar oder manuell notiert): `first_read_next_track` = Zeitpunkt, ab dem man den nächsten Track *erwartet* hätte (für späteren Abgleich mit erstem CBW ≠ T2-Atom).

> Standard-Profil `s2_prefetch` hat `play_start → track_next → audible_change → stop`.  
> Für **Hold-T2** `track_next` **nicht** setzen, solange gehalten wird — sonst vermischt die Epistemik Skip und Hold. Bei Bedarf Marker-Namen im Protokoll überschreiben: `hold_2` statt `track_next`.

---

## 1. Arm & Capture

- [ ] WebUI: **Offline-Messung starten** (oder `POST /api/experiment/start`)
- [ ] LED: lila = Offline aktiv
- [ ] Nach ARM: **direkt Track 2** wählen/starten (Playlist-Position 2, nicht über 1 tippen)
- [ ] `play_start` (BOOT kurz)
- [ ] Track 2 **halten** ≥ 30–45 s hörbar
- [ ] `hold_2` (BOOT kurz) — explizit „noch 2, kein Skip“
- [ ] Weiter halten bis Timer / `stop` (BOOT lang)
- [ ] Während Capture: **kein RST**, kein Multi-Track-Tippen

### Abbruch / Magenta

- [ ] Magenta (NEED_RESET) → Lauf verwerfen, RST, neu armieren
- [ ] Teil-Upload (Footer fehlt) → validate FAIL; Burst ggf. im Collector, aber **kein** PASS-Claim

---

## 2. Sofort-Auswertung (CBW, nicht Event-Count)

Nach Integrity **PASS** aus `events.jsonl` / derived ableiten:

| Feld | Soll |
|------|------|
| CBW-Paar | zwei READ10: `(lba, 240)` dann `(lba+240, 69)` — oder Hold-Morphologie |
| Bytes | **158208** (= 38×4096 + 2560) |
| Covered | `head … head+308` (**309** LBA) |
| Events | 39 Reads erklärt durch 30+9 — **nicht** als 39×4096 rechnen |

### Ergebnismatrix (eine Zelle ankreuzen)

| Fall | Capture | Deutung | Aktion |
|------|---------|---------|--------|
| **A** | `(318,240)+(558,69)` · covered 318…626 | T2 offline **FACT** | Track-Geometrie komplett; Prefetch separat |
| **B** | `(627,240)+(867,69)` · covered 627…935 | T3 während Hold-2-Protokoll | Prefetch-Kandidat; Marker-Zeiten prüfen |
| **C** | T1-Atom / `tail_then_head` + dir/fat | Zustands-/Re-Entry | gegen Prefetch-einfach; wie `212136` |
| **D** | kein Track-Burst | Cache-Hit / Timing / Play-Intent | Idle-Zeiten + S3 vorbereiten |

- [ ] Fall: ___
- [ ] Session-ID: ___
- [ ] validate: PASS / FAIL
- [ ] Covered LBA: ___ … ___
- [ ] Morphologie: `head_then_tail` / `tail_then_head` / `mixed_meta` / `none`

### Fall B — Prefetch-Diskrimination (wenn T3 erscheint)

- [ ] War `hold_2` **vor** dem ersten T3-CBW gesetzt? (Wanduhr / Marker-seq)
- [ ] Hörbar durchgehend Track 2 bis nach dem Burst?
- [ ] `audible_change` erst **nach** T3-CBW → Prefetch-Lead-Kandidat
- [ ] Hörwechsel schon vor/während Burst → eher Autoplay-Leak / Operator-Timing

Ohne `capture_t_us` nur qualitative Ordnung (Marker-Reihenfolge + Ohr). Quantitativer Lead = **OPEN** bis B7.

---

## 3. Pro-Session-Ablage (minimale Pipeline)

Pro PASS notieren oder in derived ablegen:

```
session_id:
atom: (head,240)+(head+240,69) | other | none
bytes:          # Soll 158208
covered: lo-hi  # Soll head … head+308
morphology: head_then_tail | tail_then_head | mixed_meta | none
markers: play_start / hold_2 / audible_change / stop  (seq + Wanduhr)
case: A|B|C|D
```

---

## 4. Parallel / danach (nicht im selben Lauf vermischen)

| Prio | Maßnahme | Fehlende HU-Daten |
|------|----------|-------------------|
| **P0b** | FW B7: `capture_t_us` ≠ `replay_t_us` (+ optional `capture_gap_us`) | Pace + Prefetch-Lead quantitativ |
| **P1** | FAT/Datei: Größe & Clusterkette T1/T2/T3 == 309 LBA? | Atom = Datei vs HU-Fenster |
| **P1b** | Split 240\|69 an Clustergrenze? Placement-Variation | Host-Policy vs Mediengeometrie |
| **P2** | S3: Replay gleicher Track → kein Atom? | Cache-Hit-Regel |
| **P2b** | ~~`212136` CBW-Mengen == T1-Atom?~~ **done** (Audit: `tail_then_head`) | Ursache Reihenfolge offen |

---

## 5. Was nicht mehr gemessen / behauptet wird

- [ ] Kein Claim „312 LBA“ / „3 Meta“ / „39 × 4 KiB = Burst“
- [ ] Kein Primärmetrik „n=39“ ohne CBW-Zerlegung
- [ ] T3 covered immer als **627…935** (nicht …938)

---

## 6. Pass-Kriterien dieses P0

| Kriterium | Erfüllt? |
|-----------|----------|
| Offline Integrity PASS, overflow=0 | |
| Genau ein Track-Atom **oder** klar Fall D | |
| Operator: direkt T2, Hold dokumentiert | |
| Marker mindestens `play_start` + `hold_2` | |
| CBW-Paar / Covered / Bytes ausgewertet | |
| Fall A–D zugeordnet | |

**Gate-Hinweis:** GATE-01 Prefetch-Lead bleibt **OPEN**, bis Fall B+Marker+`capture_t_us` vorliegen. Fall A allein schließt T2 offline und diskriminiert M3 — das ist schon ein voller P0-Erfolg.
