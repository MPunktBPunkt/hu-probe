# Analyse 20261126-173143_bmw_fp_2026-11-21a_s1m_r1

- Fahrzeug/Szenario: bmw / fp_2026-11-21a_s1m_r1 · FW 0.3.41 (c318b1e3c1c6)
- Medium: `size_158` (Quelle media_layout)
- Zeitbasis: capture_t_us → Werte MEASURED
- TUR: 298 gesamt, 31 Burst-Intervalle, Leerlauf Median 2.2388 s (p10 2.2387, p90 2.2389, n=266)

## Mount-Signatur
- t=0.246 s: `0/8,0/8,32/40,8/8,24/8,120/8,16/8,128/8,512/8,4096/8,0/8` (abweichend), TUR bis Signaturende: 32

## Read-Episoden

| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |
|---:|---|---:|---:|---|
| 6.382 | TRACK2.MP3 | 32 | 0.103 | partial_offset |
| 6.398 | TRACK1.MP3 | 480 | 1.0 | tail_then_head |
| 6.633 | TRACK2.MP3 | 240 | 0.769 | partial_offset |
| 6.75 | TRACK3.MP3 | 240 | 0.769 | partial_offset |

## Folgetrack-Reads
- TRACK2.MP3 → TRACK1.MP3: 0.016 s nach Start, Vorlauf vor Ende 9.91 s (MEASURED, tail_then_head)
- TRACK1.MP3 → TRACK2.MP3: 0.235 s nach Start, Vorlauf vor Ende 9.691 s (MEASURED, partial_offset)
- TRACK2.MP3 → TRACK3.MP3: 0.117 s nach Start, Vorlauf vor Ende 9.809 s (MEASURED, partial_offset)

## Auswahllatenz
- keine Marker mit folgendem Read

tail_then_head: 1 von 4 Episoden ['TRACK1.MP3']

## TUR-Phase
- Periode 2238.69 ms aus 266 Raster-TURs; max. Abweichung über Spannen mit anderen Kommandos: 24.48 ms
  - t=6.515 s, k=1, Rest -24.48 ms, Ops ['read10']
  - t=64.698 s, k=1, Rest 0.2 ms, Ops ['read10']
  - t=71.415 s, k=1, Rest -0.07 ms, Ops ['read10']
  - t=129.623 s, k=1, Rest -0.44 ms, Ops ['read10']
  - t=136.339 s, k=1, Rest -0.01 ms, Ops ['read10']

## Plug-Zeitkette

| Mount (s) | set_address→config (ms) | config→GET_MAX_LUN (ms) | MAX_LUN→mount_done (ms) | mount_done→1. Read (ms) | set_address→1. Read (s) |
|---:|---:|---:|---:|---:|---:|
| 47.34 | 22.2 | 997.1 | 631.9 | 4507.6 | 6.1588 |

## Datei-Atome

| t Start (s) | Datei | Reihenfolge | CBWs (Block/Anzahl) | Start→Start (ms) | gap_us | Gesamt (ms) | vollständig |
|---:|---|---|---|---|---|---:|---|
| 53.476256 | TRACK2.MP3 | partial_offset | 168/32 | [] | [] | 11.81 | False |
| 53.492048 | TRACK1.MP3 | tail_then_head | 240/240 0/240 | [116.77] | [4367] | 229.36 | True |
| 53.726725 | TRACK2.MP3 | partial_offset | 200/240 | [] | [] | 112.69 | False |
| 53.843899 | TRACK3.MP3 | partial_offset | 128/240 | [] | [] | 112.52 | False |

- TRACK2.MP3 → TRACK1.MP3 (tail_then_head): Start→Start 0.015792 s, Atom-Ende→nächster Read 0.003983 s
- TRACK1.MP3 → TRACK2.MP3 (partial_offset): Start→Start 0.234677 s, Atom-Ende→nächster Read 0.005316 s
- TRACK2.MP3 → TRACK3.MP3 (partial_offset): Start→Start 0.117174 s, Atom-Ende→nächster Read 0.004486 s

## Lesefenster (Modell: 512 Blöcke, CBW ≤ 240)
- TRACK1.MP3 (312 Blöcke, 128 kbit/s): t=53.492 ab Block 0: [240, 240], Vorlauf 15.36 s; Abstände [] s
- TRACK2.MP3 (312 Blöcke, 128 kbit/s): t=53.476 ab Block 168: [32, 240] (weicht ab), Vorlauf 14.08 s; Abstände [] s
- TRACK3.MP3 (312 Blöcke, 128 kbit/s): t=53.844 ab Block 128: [240], Vorlauf 11.78 s; Abstände [] s

## Chunk-Takt (4 KiB, fine_data=True)
- 37 READ10 mit ≥ 2 DATA-Records; Chunks pro CBW p50 30.0, max 30
- Start→Start zwischen Chunks: p50 3.897 ms, p95 3.952 ms, max 4.662 ms (n=745)
- Pausen > 10 ms: 0

## Flash-Checkpoints (Heimfahrt)
- Fahrt #43 · Ende timer · aus Flash nachgeladen: ja · FW bei Aufnahme 0.3.41 · Medium size_1m
- 9 Checkpoints, Abstand p50 60.446 s, max 60.448 s; längster Schreibvorgang 43 ms

Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.
