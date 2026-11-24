# Analyse 12917_bmw_drive_home

- Fahrzeug/Szenario: bmw / drive_home · FW 0.3.41 (c318b1e3c1c6)
- Medium: `size_158` (Quelle media_layout)
- Zeitbasis: capture_t_us → Werte MEASURED
- TUR: 385 gesamt, 31 Burst-Intervalle, Leerlauf Median 2.2388 s (p10 2.2387, p90 2.2389, n=353)

## Mount-Signatur
- t=0.245 s: `0/8,0/8,32/40,8/8,24/8,120/8,16/8,128/8,512/8,4096/8,0/8` (abweichend), TUR bis Signaturende: 32

## Read-Episoden

| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |
|---:|---|---:|---:|---|
| 2.136 | TRACK2.MP3 | 310 | 0.994 | tail_then_head |
| 12.465 | TRACK3.MP3 | 310 | 0.994 | head_first |
| 16.706 | TRACK1.MP3 | 310 | 0.994 | tail_then_head |

## Folgetrack-Reads
- TRACK2.MP3 → TRACK3.MP3: 10.329 s nach Start, Vorlauf vor Ende -0.403 s (MEASURED, head_first)
- TRACK3.MP3 → TRACK1.MP3: 4.241 s nach Start, Vorlauf vor Ende 5.685 s (MEASURED, tail_then_head)

## Auswahllatenz
- keine Marker mit folgendem Read

tail_then_head: 2 von 3 Episoden ['TRACK2.MP3', 'TRACK1.MP3']

## TUR-Phase
- Periode 2238.79 ms aus 353 Raster-TURs; max. Abweichung über Spannen mit anderen Kommandos: 0.02 ms
  - t=10.747 s, k=1, Rest -0.0 ms, Ops ['read10']
  - t=15.225 s, k=1, Rest -0.02 ms, Ops ['read10']

## Plug-Zeitkette

| Mount (s) | set_address→config (ms) | config→GET_MAX_LUN (ms) | MAX_LUN→mount_done (ms) | mount_done→1. Read (ms) | set_address→1. Read (s) |
|---:|---:|---:|---:|---:|---:|
| 20.793 | 22.4 | 996.9 | 765.6 | 128.4 | 1.9134 |

## Datei-Atome

| t Start (s) | Datei | Reihenfolge | CBWs (Block/Anzahl) | Start→Start (ms) | gap_us | Gesamt (ms) | vollständig |
|---:|---|---|---|---|---|---:|---|
| 22.684364 | TRACK2.MP3 | tail_then_head | 240/70 0/240 | [34.13] | [3045] | 146.71 | False |
| 33.012665 | TRACK3.MP3 | head_first | 0/240 240/70 | [116.7] | [4066] | 147.83 | False |
| 37.254008 | TRACK1.MP3 | tail_then_head | 240/70 0/240 | [34.29] | [3153] | 147.18 | False |

- TRACK2.MP3 → TRACK3.MP3 (head_first): Start→Start 10.328301 s, Atom-Ende→nächster Read 10.18159 s
- TRACK3.MP3 → TRACK1.MP3 (tail_then_head): Start→Start 4.241343 s, Atom-Ende→nächster Read 4.093512 s

## Lesefenster (Modell: 512 Blöcke, CBW ≤ 240)
- TRACK1.MP3 (312 Blöcke, 128 kbit/s): t=37.254 ab Block 0: [240, 70], Vorlauf 9.92 s; Abstände [] s
- TRACK2.MP3 (312 Blöcke, 128 kbit/s): t=22.684 ab Block 0: [240, 70], Vorlauf 9.92 s; Abstände [] s
- TRACK3.MP3 (312 Blöcke, 128 kbit/s): t=33.013 ab Block 0: [240, 70], Vorlauf 9.92 s; Abstände [] s

## Chunk-Takt (4 KiB, fine_data=True)
- 10 READ10 mit ≥ 2 DATA-Records; Chunks pro CBW p50 9.0, max 30
- Start→Start zwischen Chunks: p50 3.895 ms, p95 3.934 ms, max 4.634 ms (n=123)
- Pausen > 10 ms: 0

## Flash-Checkpoints (Heimfahrt)
- Fahrt #32 · Ende boot_long · aus Flash nachgeladen: ja · FW bei Aufnahme 0.3.41 · Medium size_158
- 13 Checkpoints, Abstand p50 60.446 s, max 60.449 s; längster Schreibvorgang 29 ms

Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.
