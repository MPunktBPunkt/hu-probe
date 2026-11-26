# Analyse 20261125-213213_bmw_fp_2026-11-21a_s1m_r1

- Fahrzeug/Szenario: bmw / fp_2026-11-21a_s1m_r1 · FW 0.3.41 (c318b1e3c1c6)
- Medium: `size_1m` (Quelle media_layout)
- Zeitbasis: capture_t_us → Werte MEASURED
- TUR: 297 gesamt, 30 Burst-Intervalle, Leerlauf Median 2.2388 s (p10 2.2387, p90 2.2389, n=266)

## Mount-Signatur
- t=0.244 s: `0/8,0/8,32/40,8/8,24/8,120/8,16/8,128/8,512/8,4096/8,0/8` (abweichend), TUR bis Signaturende: 32

## Read-Episoden

| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |
|---:|---|---:|---:|---|
| 1.984 | TRACK1.MP3 | 160 | 0.078 | head_first |
| 2.07 | TRACK2.MP3 | 160 | 0.078 | head_first |
| 2.158 | TRACK3.MP3 | 160 | 0.078 | head_first |

## Folgetrack-Reads
- TRACK1.MP3 → TRACK2.MP3: 0.086 s nach Start, Vorlauf vor Ende 65.585 s (MEASURED, head_first)
- TRACK2.MP3 → TRACK3.MP3: 0.088 s nach Start, Vorlauf vor Ende 65.583 s (MEASURED, head_first)

## Auswahllatenz
- keine Marker mit folgendem Read

tail_then_head: 0 von 3 Episoden []

## TUR-Phase
- Periode 2238.79 ms aus 266 Raster-TURs; max. Abweichung über Spannen mit anderen Kommandos: None ms

## Plug-Zeitkette

| Mount (s) | set_address→config (ms) | config→GET_MAX_LUN (ms) | MAX_LUN→mount_done (ms) | mount_done→1. Read (ms) | set_address→1. Read (s) |
|---:|---:|---:|---:|---:|---:|
| 75.948 | 22.7 | 996.7 | 2948.5 | -2205.1 | 1.7628 |

## Datei-Atome

| t Start (s) | Datei | Reihenfolge | CBWs (Block/Anzahl) | Start→Start (ms) | gap_us | Gesamt (ms) | vollständig |
|---:|---|---|---|---|---|---:|---|
| 77.687707 | TRACK1.MP3 | head_first | 0/32 32/128 | [16.62] | [4839] | 74.96 | False |
| 77.773691 | TRACK2.MP3 | head_first | 0/32 32/128 | [17.62] | [5771] | 75.97 | False |
| 77.861573 | TRACK3.MP3 | head_first | 0/32 32/128 | [15.93] | [4213] | 74.33 | False |

- TRACK1.MP3 → TRACK2.MP3 (head_first): Start→Start 0.085984 s, Atom-Ende→nächster Read 0.011028 s
- TRACK2.MP3 → TRACK3.MP3 (head_first): Start→Start 0.087882 s, Atom-Ende→nächster Read 0.011908 s

## Lesefenster (Modell: 512 Blöcke, CBW ≤ 240)
- TRACK1.MP3 (2048 Blöcke, 128 kbit/s): t=77.688 ab Block 0: [32, 128] (weicht ab), Vorlauf 5.12 s; Abstände [] s
- TRACK2.MP3 (2048 Blöcke, 128 kbit/s): t=77.774 ab Block 0: [32, 128] (weicht ab), Vorlauf 5.12 s; Abstände [] s
- TRACK3.MP3 (2048 Blöcke, 128 kbit/s): t=77.862 ab Block 0: [32, 128] (weicht ab), Vorlauf 5.12 s; Abstände [] s

## Chunk-Takt (4 KiB, fine_data=True)
- 10 READ10 mit ≥ 2 DATA-Records; Chunks pro CBW p50 4.5, max 16
- Start→Start zwischen Chunks: p50 3.894 ms, p95 3.966 ms, max 4.696 ms (n=66)
- Pausen > 10 ms: 0

## Flash-Checkpoints (Heimfahrt)
- Fahrt #41 · Ende timer · aus Flash nachgeladen: ja · FW bei Aufnahme 0.3.41 · Medium size_1m
- 9 Checkpoints, Abstand p50 60.4465 s, max 60.448 s; längster Schreibvorgang 24 ms

Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.
