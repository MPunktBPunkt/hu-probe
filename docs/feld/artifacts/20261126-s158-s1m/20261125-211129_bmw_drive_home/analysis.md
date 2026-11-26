# Analyse 20261125-211129_bmw_drive_home

- Fahrzeug/Szenario: bmw / drive_home · FW 0.3.41 (c318b1e3c1c6)
- Medium: `size_158` (Quelle media_layout)
- Zeitbasis: capture_t_us → Werte MEASURED
- TUR: 834 gesamt, 31 Burst-Intervalle, Leerlauf Median 2.2388 s (p10 2.2387, p90 2.2389, n=802)

## Mount-Signatur
- t=0.239 s: `0/8,0/8,32/40,8/8,24/8,120/8,16/8,128/8,512/8,4096/8,0/8` (abweichend), TUR bis Signaturende: 32

## Read-Episoden

| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |
|---:|---|---:|---:|---|
| 1.747 | TRACK1.MP3 | 160 | 0.513 | head_first |
| 1.831 | TRACK2.MP3 | 160 | 0.513 | head_first |
| 1.921 | TRACK3.MP3 | 160 | 0.513 | head_first |

## Folgetrack-Reads
- TRACK1.MP3 → TRACK2.MP3: 0.084 s nach Start, Vorlauf vor Ende 9.842 s (MEASURED, head_first)
- TRACK2.MP3 → TRACK3.MP3: 0.09 s nach Start, Vorlauf vor Ende 9.836 s (MEASURED, head_first)

## Auswahllatenz
- keine Marker mit folgendem Read

tail_then_head: 0 von 3 Episoden []

## TUR-Phase
- Periode 2238.79 ms aus 802 Raster-TURs; max. Abweichung über Spannen mit anderen Kommandos: None ms

## Plug-Zeitkette

| Mount (s) | set_address→config (ms) | config→GET_MAX_LUN (ms) | MAX_LUN→mount_done (ms) | mount_done→1. Read (ms) | set_address→1. Read (s) |
|---:|---:|---:|---:|---:|---:|
| 20.388 | 23.5 | 996.0 | 683.7 | -172.1 | 1.531 |

## Datei-Atome

| t Start (s) | Datei | Reihenfolge | CBWs (Block/Anzahl) | Start→Start (ms) | gap_us | Gesamt (ms) | vollständig |
|---:|---|---|---|---|---|---:|---|
| 21.895161 | TRACK1.MP3 | head_first | 0/32 32/128 | [17.21] | [5310] | 75.44 | False |
| 21.979523 | TRACK2.MP3 | head_first | 0/32 32/128 | [21.8] | [10044] | 80.08 | False |
| 22.069415 | TRACK3.MP3 | head_first | 0/32 32/128 | [16.78] | [4970] | 75.15 | False |

- TRACK1.MP3 → TRACK2.MP3 (head_first): Start→Start 0.084362 s, Atom-Ende→nächster Read 0.008922 s
- TRACK2.MP3 → TRACK3.MP3 (head_first): Start→Start 0.089892 s, Atom-Ende→nächster Read 0.009807 s

## Lesefenster (Modell: 512 Blöcke, CBW ≤ 240)
- TRACK1.MP3 (312 Blöcke, 128 kbit/s): t=21.895 ab Block 0: [32, 128] (weicht ab), Vorlauf 5.12 s; Abstände [] s
- TRACK2.MP3 (312 Blöcke, 128 kbit/s): t=21.98 ab Block 0: [32, 128] (weicht ab), Vorlauf 5.12 s; Abstände [] s
- TRACK3.MP3 (312 Blöcke, 128 kbit/s): t=22.069 ab Block 0: [32, 128] (weicht ab), Vorlauf 5.12 s; Abstände [] s

## Chunk-Takt (4 KiB, fine_data=True)
- 10 READ10 mit ≥ 2 DATA-Records; Chunks pro CBW p50 4.5, max 16
- Start→Start zwischen Chunks: p50 3.894 ms, p95 3.925 ms, max 4.748 ms (n=66)
- Pausen > 10 ms: 0

## Flash-Checkpoints (Heimfahrt)
- Fahrt #40 · Ende max_duration · aus Flash nachgeladen: ja · FW bei Aufnahme 0.3.41 · Medium size_158
- 29 Checkpoints, Abstand p50 60.447 s, max 60.45 s; längster Schreibvorgang 24 ms

Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.
