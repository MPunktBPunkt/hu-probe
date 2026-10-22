# Gesamtbericht + Prüfung: BMW NBT Media-Leiter 2026-10-22

**Repo:** `MPunktBPunkt/hu-probe` @ **`e1da0c6`** (dieser Bericht: Folgecommit)  
**FW:** 0.3.17 · Probe `.89` · `tcp_live` · HU BMW NBT Evo  
**Quellen:** Feldbericht [`2026-10-22-bmw-media-ladder-live.md`](2026-10-22-bmw-media-ladder-live.md), Multi-KI-Reviews (Mistral / GPT / Grok), eigene Wire-Nachrechnung (Cursor) an `events.jsonl` + `bot_cbw`.

---

## 0. Ampel (Konsens nach Prüfung)

| Bereich | Status | Prüfung |
|---|---|---|
| Live-Capture / Integrity BMW (5× PASS) | 🟢 | bestätigt (Sessions im Repo) |
| Directory-Walk + File-Head-Scan | 🟢 FACT | Wire `id3_cover` / `bitrate_mix` |
| Head-Geometrie **32+128 = 160 Blöcke (~80 KiB)** | 🟢 FACT | `bot_cbw` READ10 |
| Tail-Prefetch Autoplay (`id3_cover`) | 🟢 Struktur FACT | CBW 160 @ 22,511 / 49,090 / 75,511 s |
| Lead ≈ 7,5–8 s | 🟡 VARIABLE | ohne `audible_change`; vs. demo_v1 Lead≈0 (B55) |
| Bulk **240 Blöcke** / ~1,04 MiB/s | 🟢 FACT | `bitrate_mix` hist |
| TUR idle **2238,78 ms** | 🟢 FACT | eigene Messung n=57 idle in `081152` |
| Browse 8.3 / Player ID3 | 🟢 FACT | UI + Mediumstitel |
| `tail_then_head` = LBA-Elevator | 🟡 STRONG | `cache_place` Reihenfolge bestätigt |
| Compare-Metrik Scan/Play vermischt (B47) | 🟠 | bestätigt |
| Lab-Replay `replay_records=0` (B50) | 🟠 | Session `065150` |
| Cache-Key / Identity | 🔴 UNKNOWN | unverändert |

---

## 1. Direkte Antwort: Cursor-Frage / PiDrive

### Frage
> Liest die HU die komplette Ordnerstruktur zu Beginn, aber nicht den Inhalt der Files?  
> → PiDrive: vollständiges Menü darstellen, Inhalt danach anpassen?

### Geprüfte Antwort
**„Nur Struktur, kein Inhalt“ ist falsch.**  
**„Struktur + Dateikopf (~80 KiB), Rest dynamischer“ ist die tragfähige Formulierung.**

| Ebene | Was die HU beim Scan tut | PiDrive-Vertrag |
|---|---|---|
| Directory / FAT | Baum-Walk (`ROOT`→`ROCK`→…) | **muss** stabil sein |
| Dateianfang | typisch **READ10 32 + 128** (= 160 Blöcke ≈ 80 KiB) | **muss** gültig/konsistent sein (ID3, MP3-Header, Größe/FAT) |
| Kleine Dateien | oft **nahezu komplett** im Scan (`B032`) | Payload schon „verbraucht“ |
| Großer Rest | später Prefetch/Bulk (240er) | **Kandidat** für dynamische Erzeugung |
| Browse-UI | 8.3 aus Directory | unabhängig vom Player-Titel |
| Player-UI | ID3 aus Dateikopf | kommt aus Head, nicht aus Directory-Namen |

**Nicht zulässig:** beliebige Audio-Payload hinter gemeldeter Größe/FAT/EOF. Die HU kann den Rest später anfordern.

---

## 2. Datenbasis

| Session | Medium | Rolle |
|---|---|---|
| `61434_bmw_free` | `cache_b` | Warm-up |
| `20261022-080353_bmw_free` | `cache_place` | Elevator / `tail_then_head` |
| `20261022-080544_bmw_free` | `dur_mix` | Dauer-Leiter |
| `20261022-080752_bmw_free` | `bitrate_mix` | Bulk 240, Scan-nahe-Vollständigkeit B032 |
| `20261022-081152_bmw_free` | `id3_cover` | **Referenz** Tail-Prefetch + Head 32+128 |
| `20261022-081410_bmw_free` | `drive_mix` | Baum + große Files (Hörbuch/Jazz) |
| `20261022-065150_lab_debian_free` | Lab offline | FAIL Replay |

Artefakte: `sessions/*/derived/`, `derived/*_compare.*`, `docs/feld/artifacts/`.

---

## 3. Wire-Prüfung (Cursor) — Kernzahlen

### 3.1 Head-Scan `id3_cover` (`081152`) — FACT

Aus `bot_cbw` READ10 im Scan (t≈3,75 s), gemappt auf Extents:

| Datei | Scan-CBWs (Blöcke @ file_off) | Deutung |
|---|---|---|
| COVER1 | 32@0 + 128@32 + 3@320 + 160@160 | **komplett** im Scan (323 Blöcke) |
| COVER2/3/NOCOVER | 32@0 + 128@32 (+ kurzer Tip 3 Blöcke nahe EOF) | **Head 160** (+ Tip) |

**Geometrie 32+128 bestätigt.** Mistral/GPT/Grok hier korrekt.

### 3.2 Tail-Prefetch `id3_cover` — FACT (Struktur)

Späte READ10 (einzige File-Reads nach t>10):

| t (s) | Datei | blocks | file_off |
|---:|---|---:|---:|
| 22,511 | COVER2 | **160** | **160** |
| 49,090 | COVER3 | **160** | **160** |
| 75,511 | NOCOVER | **160** | **160** |

Δt ≈ 26,58 / 26,42 s ≈ `dur_ms` 26514. Danach keine weiteren File-Reads → hörbarer Start aus Cache.  
**Lead ≈ 8 s** bleibt 🟡 (kein `audible_change`-Anker; Annahme Start≈Scan-Ende).

### 3.3 `bitrate_mix` — FACT

- READ10-Hist: **240** dominant (Bulk).
- `B032` (Extent 472): Scan-CBWs inkl. 32+128+Rest → Summe Scan-Blöcke ≥ Extent → **nahezu voll im Scan** (Bericht „v1 ohne Grün“).
- `B320`: mehrere CONT-Wellen bis EOF, kein Rewind (Bericht bestätigt).

### 3.4 TUR — FACT

Eigene Idle-Diffs `081152` (CBW opcode 00, Fenster 2,2–2,3 s): **n=57, p50=2238,786 ms, mean=2238,785 ms**.  
Stimmt mit Analysis-Median 2,2388 s und Multi-KI-Wert 2238,78 ms überein.

### 3.5 Elevator / `cache_place` — STRONG

Dateiwechsel (file_blk):

- → TRACK2 bei **file_blk=240** (Tail zuerst)
- → TRACK3 bei **file_blk=0** (Head zuerst, LBA aufsteigend)
- → TRACK1 bei **file_blk=240** (Tail zuerst, Rücksprung)

Passt zum Elevator-Modell; Re-Entry-Deutung schwächer. Drittstichprobe (zwei ungelesene Spans) offen.

### 3.6 Nuance `drive_mix` (Korrektur zur Verallgemeinerung)

„Jede Datei nur 160 Blöcke im Scan“ gilt **nicht** pauschal für `drive_mix`:

- Directory-Walk: ja (62 dir-Reads, `ROCK`/`CLASSIC`/`JAZZ`/…).
- `R01`/`K01` u. a.: im frühen Fenster bereits `file_blk` bis Tausende (aggressiveres / längeres Einlesen).
- `J01`: großer Play-/Prefetch-Cluster erst ~160 s (Operator Jazz).

→ Head-160 ist die **klare Grammatik** in den kleinen Medien; große Bäume können mehr als nur den Head ziehen. Phasenmodell bleibt, Schwelle/Policy variabel.

---

## 4. Konsens der Multi-KI-Reviews (übernommen)

Übernommen und durch Wire gestützt:

1. **Phasenmodell:** Enum → FS-Probes → Directory-Walk → File-Head → (optional) Tail/Bulk → Cache-only.  
2. **Browse ≠ Player-Datenquelle** (8.3 vs ID3).  
3. **B47:** Compare-Lag/Lead vermischt Scan und Prefetch — Analyzer muss trennen.  
4. **B55:** Lead demo_v1≈0 vs Leiter≈8 s — beide FACT-Kontexte, keine Konstante.  
5. **B50:** Lab-Replay-Regression blockiert Offline.  
6. **B48:** `mount_done` nach `first_read` in 0.3.x-Live.  
7. **PiDrive-Schichten:** Directory + FAT + Head stabil; Tail dynamisch *unter* Konsistenz von Größe/EOF.

Abgelehnt / relativiert:

- „Nur Ordnerstruktur“  
- „Prefetch immer 8 s“  
- „26,5 s = HU-Timer“ (Trackdauer)  
- „tail_then_head = Re-Entry“ als Leading-Erklärung  
- „Inhalt beliebig hinter gleicher Struktur“ ohne Head/Größe/FAT

---

## 5. Parameter-Matrix (für Profil / nächste KI)

### FACT (Profil-fähig)

| Parameter | Wert |
|---|---|
| TUR idle | 2238,78 ms |
| Head-Scan | 160 Blöcke = 32+128 (~80 KiB) — bestätigt an `id3_cover`/`bitrate_mix` |
| Bulk-CBW | 240 Blöcke (120 KiB) |
| Browse | Directory / 8.3 |
| Player-Metadaten | ID3 aus Dateikopf |
| Bulk-Richtung | sequentiell vorwärts |
| Writes | keine (dieser Datensatz) |

### STRONG / VARIABLE

| Thema | Status |
|---|---|
| Scan füllt Cache/Metadaten | STRONG |
| Tail-Prefetch im Track-Rhythmus | STRONG Struktur; Lead VARIABLE |
| Elevator für `tail_then_head` | STRONG, Drittstichprobe offen |
| drive_mix Scan-Tiefe >160 | VARIABLE / dateiabhängig |
| Cache-Key / Identity | UNKNOWN |

---

## 6. Auftrag an die nächste KI (priorisiert)

1. **P0** Audible-Tap (`id3_cover`/`bitrate_mix`) → GATE-01 + Lead-Messung.  
2. **P0** Analyzer **B47**: Scan-Fenster markieren, aus Prefetch-Stats ausschließen; Compare neu.  
3. **P1** Identity/Head-vs-Rest-Tests für PiDrive (A→B→A, Remount).  
4. **P1** Lab-Replay **B50** (`.88` vs `.89` ram_offline).  
5. **P2** Elevator-Drittstichprobe; `drive_mix` J01/K01 CBW-seitig voll nachziehen.  
6. **P2** `parse_errors=47` (B49); `mount_done` für 0.3.x labeln (B48).

**Nicht tun:** lag&lt;0,2 s als Prefetch lesen; Lead=8 s als Konstante; 0.2.40-`mount→first_read` auf 0.3.x Live anwenden.

---

## 7. Kurzfazit

Der Feldtag @ `e1da0c6` ist der datendichteste der Serie. Die Multi-KI-Auswertung ist in den Kern-FACTs **korrekt**; die Cursor-/Operator-Kurzformel „nur Struktur“ war **zu grob** und ist korrigiert.

Für **PiDrive:** vollständiges virtuelles Menü **und** konsistenter Dateikopf (~80 KiB + ID3/MP3 + Größe/FAT) sind Pflicht; der Audio-Rest ist der dynamische Kandidat — nicht der gesamte „Inhalt“.

Nächster Erkenntnissprung: Phasentrennung im Analyzer + Identity/Cache-Experimente, nicht „noch mehr READ10 ohne Anker“.
