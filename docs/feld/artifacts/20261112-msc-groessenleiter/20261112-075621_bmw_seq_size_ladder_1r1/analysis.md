# Analyse 20261112-075621_bmw_seq_size_ladder_1r1

- Fahrzeug/Szenario: bmw / seq_size_ladder_1r1 · FW 0.3.41 (c318b1e3c1c6)
- Medium: `size_158` (Quelle media_layout)
- Zeitbasis: capture_t_us → Werte MEASURED
- TUR: 137 gesamt, 30 Burst-Intervalle, Leerlauf Median 2.2388 s (p10 2.2387, p90 2.2388, n=106)

## Mount-Signatur
- t=0.244 s: `0/8,0/8,32/40,8/8,24/8,120/8,16/8,128/8,512/8,4096/8,0/8` (abweichend), TUR bis Signaturende: 32

## Read-Episoden

| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |
|---:|---|---:|---:|---|
| 1.696 | TRACK3.MP3 | 310 | 0.994 | tail_then_head |
| 3.657 | TRACK1.MP3 | 310 | 0.994 | tail_then_head |
| 13.579 | TRACK2.MP3 | 310 | 0.994 | head_first |

## Folgetrack-Reads
- TRACK3.MP3 → TRACK1.MP3: 1.961 s nach Start, Vorlauf vor Ende 7.965 s (MEASURED, tail_then_head)
- TRACK1.MP3 → TRACK2.MP3: 9.922 s nach Start, Vorlauf vor Ende 0.004 s (MEASURED, head_first)

## Auswahllatenz
- keine Marker mit folgendem Read

tail_then_head: 2 von 3 Episoden ['TRACK3.MP3', 'TRACK1.MP3']

## TUR-Phase
- Periode 2238.78 ms aus 106 Raster-TURs; max. Abweichung über Spannen mit anderen Kommandos: 0.04 ms
  - t=13.224 s, k=1, Rest -0.04 ms, Ops ['read10']

## Plug-Zeitkette

| Mount (s) | set_address→config (ms) | config→GET_MAX_LUN (ms) | MAX_LUN→mount_done (ms) | mount_done→1. Read (ms) | set_address→1. Read (s) |
|---:|---:|---:|---:|---:|---:|
| 357.313 | 22.5 | 996.9 | 3028.4 | -2572.8 | 1.475 |

## Datei-Atome

| t Start (s) | Datei | Reihenfolge | CBWs (Block/Anzahl) | Start→Start (ms) | gap_us | Gesamt (ms) | vollständig |
|---:|---|---|---|---|---|---:|---|
| 358.765652 | TRACK3.MP3 | tail_then_head | 240/70 0/240 | [34.57] | [3393] | 147.36 | False |
| 360.726393 | TRACK1.MP3 | tail_then_head | 240/70 0/240 | [34.34] | [3159] | 147.07 | False |
| 370.648542 | TRACK2.MP3 | head_first | 0/240 240/70 | [117.13] | [4517] | 148.31 | False |

- TRACK3.MP3 → TRACK1.MP3 (tail_then_head): Start→Start 1.960741 s, Atom-Ende→nächster Read 1.813378 s
- TRACK1.MP3 → TRACK2.MP3 (head_first): Start→Start 9.922149 s, Atom-Ende→nächster Read 9.775077 s

## Lesefenster (Modell: 512 Blöcke, CBW ≤ 240)
- TRACK1.MP3 (312 Blöcke, 128 kbit/s): t=360.726 ab Block 0: [240, 70], Vorlauf 9.92 s; Abstände [] s
- TRACK2.MP3 (312 Blöcke, 128 kbit/s): t=370.649 ab Block 0: [240, 70], Vorlauf 9.92 s; Abstände [] s
- TRACK3.MP3 (312 Blöcke, 128 kbit/s): t=358.766 ab Block 0: [240, 70], Vorlauf 9.92 s; Abstände [] s

## Chunk-Takt (4 KiB, fine_data=True)
- 10 READ10 mit ≥ 2 DATA-Records; Chunks pro CBW p50 9.0, max 30
- Start→Start zwischen Chunks: p50 3.893 ms, p95 3.949 ms, max 4.659 ms (n=123)
- Pausen > 10 ms: 0

Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.
