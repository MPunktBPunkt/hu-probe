# Analyse 20261117-172704_bmw_fp_2026-11-17a_s158_r1

- Fahrzeug/Szenario: bmw / fp_2026-11-17a_s158_r1 · FW 0.3.41 (c318b1e3c1c6)
- Medium: `size_158` (Quelle media_layout)
- Zeitbasis: capture_t_us → Werte MEASURED
- TUR: 298 gesamt, 31 Burst-Intervalle, Leerlauf Median 2.2388 s (p10 2.2387, p90 2.2388, n=266)

## Mount-Signatur
- t=0.245 s: `0/8,0/8,32/40,8/8,24/8,120/8,16/8,128/8,512/8,4096/8,0/8` (abweichend), TUR bis Signaturende: 32

## Read-Episoden

| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |
|---:|---|---:|---:|---|
| 1.902 | TRACK1.MP3 | 160 | 0.513 | head_first |
| 1.985 | TRACK2.MP3 | 160 | 0.513 | head_first |
| 2.07 | TRACK3.MP3 | 160 | 0.513 | head_first |
| 2.372 | TRACK1.MP3 | 144 | 0.462 | partial_offset |
| 4.626 | TRACK2.MP3 | 144 | 0.462 | partial_offset |
| 14.556 | TRACK3.MP3 | 144 | 0.462 | partial_offset |

## Folgetrack-Reads
- TRACK1.MP3 → TRACK2.MP3: 0.083 s nach Start, Vorlauf vor Ende 9.843 s (MEASURED, head_first)
- TRACK2.MP3 → TRACK3.MP3: 0.085 s nach Start, Vorlauf vor Ende 9.841 s (MEASURED, head_first)
- TRACK3.MP3 → TRACK1.MP3: 0.302 s nach Start, Vorlauf vor Ende 9.624 s (MEASURED, partial_offset)
- TRACK1.MP3 → TRACK2.MP3: 2.254 s nach Start, Vorlauf vor Ende 7.672 s (MEASURED, partial_offset)
- TRACK2.MP3 → TRACK3.MP3: 9.93 s nach Start, Vorlauf vor Ende -0.004 s (MEASURED, partial_offset)

## Auswahllatenz
- keine Marker mit folgendem Read

tail_then_head: 0 von 6 Episoden []

## TUR-Phase
- Periode 2238.78 ms aus 266 Raster-TURs; max. Abweichung über Spannen mit anderen Kommandos: 0.04 ms
  - t=13.144 s, k=1, Rest -0.04 ms, Ops ['read10']

## Plug-Zeitkette

| Mount (s) | set_address→config (ms) | config→GET_MAX_LUN (ms) | MAX_LUN→mount_done (ms) | mount_done→1. Read (ms) | set_address→1. Read (s) |
|---:|---:|---:|---:|---:|---:|
| 17.74 | 23.2 | 996.2 | 911.1 | -250.1 | 1.6804 |

## Datei-Atome

| t Start (s) | Datei | Reihenfolge | CBWs (Block/Anzahl) | Start→Start (ms) | gap_us | Gesamt (ms) | vollständig |
|---:|---|---|---|---|---|---:|---|
| 19.396829 | TRACK1.MP3 | head_first | 0/32 32/128 | [16.22] | [4359] | 74.62 | False |
| 19.480424 | TRACK2.MP3 | head_first | 0/32 32/128 | [16.46] | [4630] | 74.75 | False |
| 19.565055 | TRACK3.MP3 | head_first | 0/32 32/128 | [16.1] | [4296] | 74.45 | False |
| 19.867273 | TRACK1.MP3 | partial_offset | 160/144 | [] | [] | 66.02 | False |
| 22.121177 | TRACK2.MP3 | partial_offset | 160/144 | [] | [] | 66.03 | False |
| 32.051143 | TRACK3.MP3 | partial_offset | 160/144 | [] | [] | 66.12 | False |

- TRACK1.MP3 → TRACK2.MP3 (head_first): Start→Start 0.083595 s, Atom-Ende→nächster Read 0.00897 s
- TRACK2.MP3 → TRACK3.MP3 (head_first): Start→Start 0.084631 s, Atom-Ende→nächster Read 0.009876 s
- TRACK3.MP3 → TRACK1.MP3 (partial_offset): Start→Start 0.302218 s, Atom-Ende→nächster Read 0.227766 s
- TRACK1.MP3 → TRACK2.MP3 (partial_offset): Start→Start 2.253904 s, Atom-Ende→nächster Read 2.187884 s
- TRACK2.MP3 → TRACK3.MP3 (partial_offset): Start→Start 9.929966 s, Atom-Ende→nächster Read 9.863938 s

## Lesefenster (Modell: 512 Blöcke, CBW ≤ 240)
- TRACK1.MP3 (312 Blöcke, 128 kbit/s): t=19.397 ab Block 0: [32, 128, 144] (weicht ab), Vorlauf 9.73 s; Abstände [] s
- TRACK2.MP3 (312 Blöcke, 128 kbit/s): t=19.48 ab Block 0: [32, 128] (weicht ab), Vorlauf 5.12 s; t=22.121 ab Block 160: [144], Vorlauf 7.09 s; Abstände [2.641] s
- TRACK3.MP3 (312 Blöcke, 128 kbit/s): t=19.565 ab Block 0: [32, 128] (weicht ab), Vorlauf 5.12 s; t=32.051 ab Block 160: [144], Vorlauf -2.76 s; Abstände [12.486] s

## Chunk-Takt (4 KiB, fine_data=True)
- 13 READ10 mit ≥ 2 DATA-Records; Chunks pro CBW p50 5.0, max 18
- Start→Start zwischen Chunks: p50 3.895 ms, p95 3.938 ms, max 4.758 ms (n=117)
- Pausen > 10 ms: 0

## Flash-Checkpoints (Heimfahrt)
- Fahrt #20 · Ende timer · aus Flash nachgeladen: ja · FW bei Aufnahme 0.3.41 · Medium size_158
- 9 Checkpoints, Abstand p50 60.4465 s, max 60.448 s; längster Schreibvorgang 30 ms

Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.
