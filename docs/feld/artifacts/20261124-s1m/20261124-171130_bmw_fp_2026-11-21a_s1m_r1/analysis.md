# Analyse 20261124-171130_bmw_fp_2026-11-21a_s1m_r1

- Fahrzeug/Szenario: bmw / fp_2026-11-21a_s1m_r1 · FW 0.3.41 (c318b1e3c1c6)
- Medium: `size_1m` (Quelle media_layout)
- Zeitbasis: capture_t_us → Werte MEASURED
- TUR: 161 gesamt, 55 Burst-Intervalle, Leerlauf Median 2.2388 s (p10 2.2382, p90 2.2389, n=104)

## Mount-Signatur
- t=0.243 s: `0/8,0/8,32/40,8/8,24/8,120/8,16/8,128/8,512/8,4096/8,0/8` (abweichend), TUR bis Signaturende: 32
- t=189.086 s: `0/8,0/8,32/40,8/8,24/8,120/8,16/8,128/8,512/8,4096/8,0/8` (abweichend), TUR bis Signaturende: 32

## Read-Episoden

| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |
|---:|---|---:|---:|---|
| 2.11 | TRACK1.MP3 | 160 | 0.078 | head_first |
| 2.196 | TRACK2.MP3 | 160 | 0.078 | head_first |
| 2.295 | TRACK3.MP3 | 160 | 0.078 | head_first |
| 2.528 | TRACK1.MP3 | 1880 | 0.918 | partial_offset |
| 203.886 | TRACK1.MP3 | 2048 | 1.0 | tail_then_head |

## Folgetrack-Reads
- TRACK1.MP3 → TRACK2.MP3: 0.086 s nach Start, Vorlauf vor Ende 65.585 s (MEASURED, head_first)
- TRACK2.MP3 → TRACK3.MP3: 0.099 s nach Start, Vorlauf vor Ende 65.572 s (MEASURED, head_first)
- TRACK3.MP3 → TRACK1.MP3: 0.233 s nach Start, Vorlauf vor Ende 65.438 s (MEASURED, partial_offset)

## Auswahllatenz
- keine Marker mit folgendem Read

tail_then_head: 1 von 5 Episoden ['TRACK1.MP3']

## TUR-Phase
- Periode 2238.56 ms aus 99 Raster-TURs; max. Abweichung über Spannen mit anderen Kommandos: 24.14 ms
  - t=163.231 s, k=12, Rest -9.45 ms, Ops ['inquiry']
  - t=190.087 s, k=1, Rest 8.68 ms, Ops ['mode_sense6', 'prevent_allow', 'read10', 'read_capacity10']
  - t=192.334 s, k=1, Rest -24.14 ms, Ops ['prevent_allow', 'read10']

## Plug-Zeitkette

| Mount (s) | set_address→config (ms) | config→GET_MAX_LUN (ms) | MAX_LUN→mount_done (ms) | mount_done→1. Read (ms) | set_address→1. Read (s) |
|---:|---:|---:|---:|---:|---:|
| 46.12 | 24.5 | 994.9 | 739.2 | 133.4 | 1.8919 |
| 234.964 | 22.4 | 997.0 | 9484.8 | 4318.1 | 14.8224 |

## Datei-Atome

| t Start (s) | Datei | Reihenfolge | CBWs (Block/Anzahl) | Start→Start (ms) | gap_us | Gesamt (ms) | vollständig |
|---:|---|---|---|---|---|---:|---|
| 47.987912 | TRACK1.MP3 | head_first | 0/32 32/128 | [16.71] | [4888] | 75.05 | False |
| 48.073292 | TRACK2.MP3 | head_first | 0/32 32/128 | [15.99] | [4292] | 74.31 | False |
| 48.172993 | TRACK3.MP3 | head_first | 0/32 32/128 | [16.14] | [4336] | 74.46 | False |
| 48.405324 | TRACK1.MP3 | partial_offset | 880/176 640/240 1056/240 160/240 400/240 1296/240 1536/240 1776/240 2016/24 | [85.67, 116.75, 116.68, 116.93, 116.54, 117.49, 116.74, 116.59] | [4182, 4090, 4096, 4383, 4067, 4041, 4142, 4039] | 911.43 | False |
| 249.763679 | TRACK1.MP3 | tail_then_head | 480/32 240/240 0/240 512/240 752/240 992/240 1232/240 1472/64 1536/240 1776/240 2016/32 | [15.93, 116.9, 117.76, 116.67, 116.78, 116.64, 116.45, 31.17, 116.56, 116.54] | [4052, 4281, 5095, 4070, 4047, 3985, 4033, 3898, 4072, 4041] | 993.13 | True |

- TRACK1.MP3 → TRACK2.MP3 (head_first): Start→Start 0.08538 s, Atom-Ende→nächster Read 0.010332 s
- TRACK2.MP3 → TRACK3.MP3 (head_first): Start→Start 0.099701 s, Atom-Ende→nächster Read 0.025391 s
- TRACK3.MP3 → TRACK1.MP3 (partial_offset): Start→Start 0.232331 s, Atom-Ende→nächster Read 0.15787 s

## Lesefenster (Modell: 512 Blöcke, CBW ≤ 240)
- TRACK1.MP3 (2048 Blöcke, 128 kbit/s): t=47.988 ab Block 0: [32, 128, 240, 240, 240, 176, 240, 240, 240, 240, 24] (weicht ab), Vorlauf 65.28 s; t=249.764 ab Block 0: [240, 240, 32, 240, 240, 240, 240, 64, 240, 240, 32] (weicht ab), Vorlauf -136.24 s; Abstände [201.776] s
- TRACK2.MP3 (2048 Blöcke, 128 kbit/s): t=48.073 ab Block 0: [32, 128] (weicht ab), Vorlauf 5.12 s; Abstände [] s
- TRACK3.MP3 (2048 Blöcke, 128 kbit/s): t=48.173 ab Block 0: [32, 128] (weicht ab), Vorlauf 5.12 s; Abstände [] s

## Chunk-Takt (4 KiB, fine_data=True)
- 34 READ10 mit ≥ 2 DATA-Records; Chunks pro CBW p50 16.0, max 30
- Start→Start zwischen Chunks: p50 3.896 ms, p95 3.94 ms, max 4.76 ms (n=549)
- Pausen > 10 ms: 0

## Flash-Checkpoints (Heimfahrt)
- Fahrt #34 · Ende power_loss · aus Flash nachgeladen: ja · FW bei Aufnahme 0.3.41 · Medium size_1m
- 5 Checkpoints, Abstand p50 60.025999999999996 s, max 60.051 s; längster Schreibvorgang 47 ms
- Stromverlust: Daten nach dem letzten Checkpoint fehlen (Ende der Aufnahme ≠ Ende der Fahrt).

Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.
