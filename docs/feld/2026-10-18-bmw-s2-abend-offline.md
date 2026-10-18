# BMW S2 Abend — Offline Einzelläufe (2026-10-18, FW 0.2.30)

**Kontext:** Nachmittags Live + erster Offline-PASS (`175203`); abends zu Hause weitere Offline-Arms mit Operator-Protokoll „ein Track halten“.  
**Probe:** `.89` · Collector `192.168.178.187` · `soft_usb=false` · `media_hide` · typisch `measure_ms=90000`.  
**Vollständige Multi-KI-Basis:** `2026-10-18-bmw-s2-0.2.30-multi-ki-review.md`.

> **Korrektur 2026-10-18 (Rohdaten-Audit):** Track-Atom = `(head,240)+(head+240,69)` = **309 LBA / 158208 B**.  
> Covered T3 = **627…935** (nicht …938). „312 LBA“ / „3 Meta“ / „39×4 KiB“ = **REJECTED**.  
> Übergabe: [`docs/reports/2026-10-18-hu-probe-bmw-s2-abend-offline.md`](../reports/2026-10-18-hu-probe-bmw-s2-abend-offline.md) · Checkliste: [`2026-10-18-t2-hold-checklist.md`](2026-10-18-t2-hold-checklist.md).  
> **Meta im Repo (M2/B10):** [`audits/hu-probe-bmw-s2-abend-offline-2026-10-18/session-summaries.json`](../../audits/hu-probe-bmw-s2-abend-offline-2026-10-18/session-summaries.json) — kind-Counts, Start-Listen, CBWs ohne vollständige `events.jsonl`.

## Kurzfazit

1. **Drei Offline Integrity-PASS** am Auto: `175203` (T3), `212136` (T1), `212635` (T3).
2. **Ein Wechsel pro Lauf** funktioniert operativ besser als Multi-Track (weniger Magenta/NEED_RESET).
3. **T2 offline noch nicht belegt** — geplanter T2-Lauf `212635` lieferte wieder **T3-Atom** (Operator: Start 1 → AP 2 → Hold 2; Buffer `(627,240)+(867,69)`).
4. **Zwei Burst-Morphologien offline:**
   - **T3-rein:** CBW `(627,240)+(867,69)` · 39 file-Reads (30+9) · covered **627…935**
   - **T1-Hold:** dieselben T1-CBWs vermutlich `tail_then_head`; Starts ~249…241, Mix `file`+`dir`+`fat`, Span 9…317
5. Teil-Uploads (Magenta mitten im Finalize) reproduzierbar: Burst oft da, Footer fehlt → validate FAIL.

## Offline-Session-Tabelle

| Session | validate | offline_rec | Reads | Band | Burst-Detail | Operator-Notiz |
|---------|----------|------------:|------:|------|--------------|----------------|
| `172832` | FAIL | — | 0 | — | nur session start | früher Fehlversuch |
| **`175203`** | **PASS** | 97 | 39 | **T3** | `(627,240)+(867,69)` / cov **627–935** | Nachmittag, Autoplay T3 |
| `175404` | FAIL | — | 39 | T3 | wie oben | Footer fehlt |
| `211058` | FAIL | — | 39 | T3 | wie oben | 120 s-Arm, Magenta |
| `211423` | FAIL | — | 37 | T1* | 249→225, file/dir/fat | NEED_RESET, Track hing |
| `211811` | FAIL | — | 39 | T1* | 249→241, file/dir/fat | Teil-Upload |
| **`212136`** | **PASS** | 126 | 39 | **T1*** | `(249,69)+(9,240)` tail_then_head; file34/dir4/fat1; Span 9…317 | Start T3 (Cache) → AP/Hold **1** |
| **`212635`** | **PASS** | 125 | 39 | **T3** | `(627,240)+(867,69)` | Geplant T2 (Start 1→AP 2→Hold 2); Buffer = T3 |

\* T1* = nicht-monotone Starts 249…→…9…241; CBW `(249,69)` dann `(9,240)` = `tail_then_head` derselben T1-Geometrie (**FACT**, Audit). Ursache der Reihenfolge: **INFERENCE**.

## Muster

### A. Offline PASS = genau ein Track-Atom (39 Reads = 30+9)

Alle drei PASS-Läufe: **n_bursts = 1**, **n_reads = 39**, overflow=0, `auto_media_hide`, `upload=delivered`.  
Primärmetrik: CBW-Atom `(240)+(69)` = 309 LBA / 158208 B (nicht 39×4096).

| PASS | Band | Morphology |
|------|------|------------|
| `175203` | T3 | `(627,240)+(867,69)` · file-only |
| `212136` | T1 | Hold-auf-1; FAT/DIR; vermutlich `tail_then_head` |
| `212635` | T3 | `(627,240)+(867,69)` (trotz T2-Absicht) |

### B. Operator-Protokoll „ein Track“

- **`212136`:** Start bei 3 (Cache, kein T3-USB) → Sprung/Hold **1** → ein USB-Burst im T1-Band. **Erfolg.**
- **`212635`:** Start 1 → AP 2 → Hold 2 vor 3 → trotzdem **T3-Burst** im Buffer. Mögliche Ursachen: Autoplay doch zu 3 vor Hold; Prefetch T3 trotz Hold 2; oder früherer T3-Cache-Miss. **T2 offline weiter offen.**

### C. Magenta / Teil-Upload

Wiederkehrend: NEED_RESET (magenta) → HU hängt → HTTP tot → Upload ohne `capture_health`. Burst im Collector oft schon teilweise da. Frischer RST vor Arm vermeidet `buffer_alloc_failed`.

### D. Live (Abend, ergänzend)

| Session | Muster |
|---------|--------|
| `8630` | Initial 50 + T3 39 + Tippen-1 39 (249) |
| `2809` | Initial + T3 39; Tippen-3 später Cache (`delta=0`) |
| `4028` / `2264` | T2 39 (318–622) live bestätigt |

## Epistemik-Update (Abend + Korrektur)

| Claim | Klasse |
|-------|--------|
| Offline PASS mit genau einem Track-Atom (T1* oder T3) | **FACT** |
| T3 = `(627,240)+(867,69)` / covered **627…935** offline | **FACT** (2× PASS + Repros) |
| T3 covered 627…938 / 312 LBA / „3 Meta“ | **REJECTED** |
| T1-Hold-Burst offline (`212136`) | **FACT** (Morphologie ≠ reines T3) |
| `212136` kinds file34/dir4/fat1; CBW `tail_then_head` | **FACT** (Audit) |
| Ursache Hold-Reihenfolge / Meta am Kopf | **INFERENCE** |
| T2 offline = `(318,240)+(558,69)` | **offen** (Versuch `212635` verfehlt) |
| Ein-Track-Halten reduziert Multi-Burst-Last | **INFERENCE** (operativ erfolgreich) |
| Prefetch-Lead / GATE-01 | **OPEN** |

## Nächste Schritte

1. **P0:** gezielter Offline-Lauf **nur T2** — bei 2 starten und halten (nicht über 1→2). Checkliste: [`2026-10-18-t2-hold-checklist.md`](2026-10-18-t2-hold-checklist.md).
2. FW: B7 Timestamps (`capture_t_us` ≠ `replay_t_us`); NEED_RESET / Heap.
3. GATE-01 erst mit Markern + Timing-Fix; Primärmetrik = Track-Atom, nicht „39er“.

## Fazit

Der Abend liefert zwei weitere Offline-PASS und zeigt, dass **kontrolliertes Ein-Track-Halten** messbar ist (`212136` = T1). Der T2-Versuch endete als T3-PASS — wertvoll als Repro, nicht als T2-Beleg. Feldtag damit für Offline-Integrität und T1/T3-Muster geschlossen; T2 offline und Prefetch-Lead bleiben offen.
