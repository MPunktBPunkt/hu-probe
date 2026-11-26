# Analyse 20261126-173225_bmw_drive_home

- Fahrzeug/Szenario: bmw / drive_home · FW 0.3.41 (c318b1e3c1c6)
- Medium: `size_158` (Quelle media_layout)
- Zeitbasis: capture_t_us → Werte MEASURED
- TUR: 486 gesamt, 31 Burst-Intervalle, Leerlauf Median 2.2388 s (p10 2.2386, p90 2.2389, n=454)

## Mount-Signatur
- t=0.244 s: `0/8,0/8,32/40,8/8,24/8,120/8,16/8,128/8,512/8,4096/8,0/8` (abweichend), TUR bis Signaturende: 32

## Read-Episoden

| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |
|---:|---|---:|---:|---|
| 1.803 | TRACK1.MP3 | 160 | 0.513 | head_first |
| 1.888 | TRACK2.MP3 | 160 | 0.513 | head_first |
| 1.975 | TRACK3.MP3 | 160 | 0.513 | head_first |
| 2.21 | TRACK1.MP3 | 144 | 0.462 | partial_offset |
| 4.461 | TRACK2.MP3 | 144 | 0.462 | partial_offset |
| 14.378 | TRACK3.MP3 | 144 | 0.462 | partial_offset |

## Folgetrack-Reads
- TRACK1.MP3 → TRACK2.MP3: 0.085 s nach Start, Vorlauf vor Ende 9.841 s (MEASURED, head_first)
- TRACK2.MP3 → TRACK3.MP3: 0.087 s nach Start, Vorlauf vor Ende 9.839 s (MEASURED, head_first)
- TRACK3.MP3 → TRACK1.MP3: 0.235 s nach Start, Vorlauf vor Ende 9.691 s (MEASURED, partial_offset)
- TRACK1.MP3 → TRACK2.MP3: 2.251 s nach Start, Vorlauf vor Ende 7.675 s (MEASURED, partial_offset)
- TRACK2.MP3 → TRACK3.MP3: 9.917 s nach Start, Vorlauf vor Ende 0.009 s (MEASURED, partial_offset)

## Auswahllatenz
- keine Marker mit folgendem Read

tail_then_head: 0 von 6 Episoden []

## TUR-Phase
- Periode 2238.78 ms aus 454 Raster-TURs; max. Abweichung über Spannen mit anderen Kommandos: 0.49 ms
  - t=13.193 s, k=1, Rest 0.49 ms, Ops ['read10']

## Plug-Zeitkette

| Mount (s) | set_address→config (ms) | config→GET_MAX_LUN (ms) | MAX_LUN→mount_done (ms) | mount_done→1. Read (ms) | set_address→1. Read (s) |
|---:|---:|---:|---:|---:|---:|
| 20.889 | 24.4 | 995.3 | 664.5 | -101.0 | 1.5831 |

## Datei-Atome

| t Start (s) | Datei | Reihenfolge | CBWs (Block/Anzahl) | Start→Start (ms) | gap_us | Gesamt (ms) | vollständig |
|---:|---|---|---|---|---|---:|---|
| 22.447897 | TRACK1.MP3 | head_first | 0/32 32/128 | [16.43] | [4662] | 74.8 | False |
| 22.53252 | TRACK2.MP3 | head_first | 0/32 32/128 | [20.68] | [8791] | 78.86 | False |
| 22.619665 | TRACK3.MP3 | head_first | 0/32 32/128 | [16.11] | [4311] | 74.29 | False |
| 22.854443 | TRACK1.MP3 | partial_offset | 160/144 | [] | [] | 66.0 | False |
| 25.105846 | TRACK2.MP3 | partial_offset | 160/144 | [] | [] | 65.98 | False |
| 35.023377 | TRACK3.MP3 | partial_offset | 160/144 | [] | [] | 65.97 | False |

- TRACK1.MP3 → TRACK2.MP3 (head_first): Start→Start 0.084623 s, Atom-Ende→nächster Read 0.009826 s
- TRACK2.MP3 → TRACK3.MP3 (head_first): Start→Start 0.087145 s, Atom-Ende→nächster Read 0.008281 s
- TRACK3.MP3 → TRACK1.MP3 (partial_offset): Start→Start 0.234778 s, Atom-Ende→nächster Read 0.160484 s
- TRACK1.MP3 → TRACK2.MP3 (partial_offset): Start→Start 2.251403 s, Atom-Ende→nächster Read 2.185406 s
- TRACK2.MP3 → TRACK3.MP3 (partial_offset): Start→Start 9.917531 s, Atom-Ende→nächster Read 9.851551 s

## Lesefenster (Modell: 512 Blöcke, CBW ≤ 240)
- TRACK1.MP3 (312 Blöcke, 128 kbit/s): t=22.448 ab Block 0: [32, 128, 144] (weicht ab), Vorlauf 9.73 s; Abstände [] s
- TRACK2.MP3 (312 Blöcke, 128 kbit/s): t=22.533 ab Block 0: [32, 128] (weicht ab), Vorlauf 5.12 s; t=25.106 ab Block 160: [144], Vorlauf 7.15 s; Abstände [2.573] s
- TRACK3.MP3 (312 Blöcke, 128 kbit/s): t=22.62 ab Block 0: [32, 128] (weicht ab), Vorlauf 5.12 s; t=35.023 ab Block 160: [144], Vorlauf -2.68 s; Abstände [12.403] s

## Chunk-Takt (4 KiB, fine_data=True)
- 13 READ10 mit ≥ 2 DATA-Records; Chunks pro CBW p50 5.0, max 18
- Start→Start zwischen Chunks: p50 3.895 ms, p95 3.937 ms, max 4.686 ms (n=117)
- Pausen > 10 ms: 0

## Flash-Checkpoints (Heimfahrt)
- Fahrt #44 · Ende boot_long · aus Flash nachgeladen: ja · FW bei Aufnahme 0.3.41 · Medium size_158
- 16 Checkpoints, Abstand p50 60.447 s, max 60.449 s; längster Schreibvorgang 30 ms

Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.
