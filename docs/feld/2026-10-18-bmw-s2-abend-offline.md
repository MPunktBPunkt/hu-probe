# BMW S2 Abend — Offline Einzelläufe (2026-10-18, FW 0.2.30)

**Kontext:** Nachmittags Live + erster Offline-PASS (`175203`); abends zu Hause weitere Offline-Arms mit Operator-Protokoll „ein Track halten“.  
**Probe:** `.89` · Collector `192.168.178.187` · `soft_usb=false` · `media_hide` · typisch `measure_ms=90000`.  
**Vollständige Multi-KI-Basis:** `2026-10-18-bmw-s2-0.2.30-multi-ki-review.md`.

## Kurzfazit

1. **Drei Offline Integrity-PASS** am Auto: `175203` (T3), `212136` (T1), `212635` (T3).
2. **Ein Wechsel pro Lauf** funktioniert operativ besser als Multi-Track (weniger Magenta/NEED_RESET).
3. **T2 offline noch nicht belegt** — geplanter T2-Lauf `212635` lieferte wieder **T3-Burst** (Operator: Start 1 → AP 2 → Hold 2; Buffer trotzdem 627–931).
4. **Zwei Burst-Morphologien offline:**
   - **T3-rein:** 39× `kind=file`, READ-Starts 627…931, covered 627…938
   - **T1-Hold:** 39 Reads Starts ~249…241, Mix `file`+`dir`+`fat`, Span bis LBA ~313
5. Teil-Uploads (Magenta mitten im Finalize) reproduzierbar: Burst oft da, Footer fehlt → validate FAIL.

## Offline-Session-Tabelle

| Session | validate | offline_rec | Reads | Band | Burst-Detail | Operator-Notiz |
|---------|----------|------------:|------:|------|--------------|----------------|
| `172832` | FAIL | — | 0 | — | nur session start | früher Fehlversuch |
| **`175203`** | **PASS** | 97 | 39 | **T3** | file-only 627→931 / cov 627–938 | Nachmittag, Autoplay T3 |
| `175404` | FAIL | — | 39 | T3 | wie oben | Footer fehlt |
| `211058` | FAIL | — | 39 | T3 | wie oben | 120 s-Arm, Magenta |
| `211423` | FAIL | — | 37 | T1* | 249→225, file/dir/fat | NEED_RESET, Track hing |
| `211811` | FAIL | — | 39 | T1* | 249→241, file/dir/fat | Teil-Upload |
| **`212136`** | **PASS** | 126 | 39 | **T1*** | 249→241, file 34 + dir 4 + fat 1 | Start T3 (Cache) → AP/Hold **1** |
| **`212635`** | **PASS** | 125 | 39 | **T3** | file-only 627→931 | Geplant T2 (Start 1→AP 2→Hold 2); Buffer = T3 |

\* T1* = nicht-monotone Start-Sequenz ~249…241, aber LBA-Span im T1-Band (9–313).

## Muster

### A. Offline PASS = genau ein 39er-Burst

Alle drei PASS-Läufe: **n_bursts = 1**, **n_reads = 39**, overflow=0, `auto_media_hide`, `upload=delivered`.

| PASS | Band | Morphology |
|------|------|------------|
| `175203` | T3 | reine File-Kette Step‑8 |
| `212136` | T1 | Hold-auf-1; FAT/DIR-Anteil |
| `212635` | T3 | reine File-Kette (trotz T2-Absicht) |

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

## Epistemik-Update (Abend)

| Claim | Klasse |
|-------|--------|
| Offline PASS mit genau einem 39er-Burst (T1* oder T3) | **FACT** |
| T3 READ-Starts 627…931 / covered 627…938 offline | **FACT** (2× PASS + Repros) |
| T1-Hold-Burst offline (`212136`) | **FACT** (Morphologie ≠ reines T3) |
| T2 offline = 318…622 | **offen** (Versuch `212635` verfehlt) |
| Ein-Track-Halten reduziert Multi-Burst-Last | **INFERENCE** (operativ erfolgreich) |
| Prefetch-Lead / GATE-01 | **OPEN** |

## Nächste Schritte

1. Optional: gezielter Offline-Lauf **nur T2** — bei 2 starten und halten (nicht über 1→2 navigieren).
2. FW: B7 Timestamps; NEED_RESET beim Offline-Arm entschärfen; Heap/`buffer_alloc`.
3. GATE-01 erst mit Markern + Timing-Fix.

## Fazit

Der Abend liefert zwei weitere Offline-PASS und zeigt, dass **kontrolliertes Ein-Track-Halten** messbar ist (`212136` = T1). Der T2-Versuch endete als T3-PASS — wertvoll als Repro, nicht als T2-Beleg. Feldtag damit für Offline-Integrität und T1/T3-Muster geschlossen; T2 offline und Prefetch-Lead bleiben offen.
