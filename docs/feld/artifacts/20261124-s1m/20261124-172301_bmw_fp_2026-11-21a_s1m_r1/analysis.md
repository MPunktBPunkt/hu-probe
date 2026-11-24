# Analyse 20261124-172301_bmw_fp_2026-11-21a_s1m_r1

- Fahrzeug/Szenario: bmw / fp_2026-11-21a_s1m_r1 · FW 0.3.41 (c318b1e3c1c6)
- Medium: `size_1m` (Quelle media_layout)
- Zeitbasis: capture_t_us → Werte MEASURED
- TUR: 40 gesamt, 31 Burst-Intervalle, Leerlauf Median 2.2388 s (p10 2.2358, p90 2.2494, n=8)

## Mount-Signatur
- t=0.244 s: `0/8,0/8,32/40,8/8,24/8,120/8,16/8,128/8,512/8,4096/8,0/8` (abweichend), TUR bis Signaturende: 32

## Read-Episoden

| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |
|---:|---|---:|---:|---|
| 1.919 | TRACK1.MP3 | 2048 | 1.0 | tail_then_head |

## Folgetrack-Reads
- keine

## Auswahllatenz
- keine Marker mit folgendem Read

tail_then_head: 1 von 1 Episoden ['TRACK1.MP3']

## TUR-Phase
- Periode 2238.81 ms aus 8 Raster-TURs; max. Abweichung über Spannen mit anderen Kommandos: None ms

## Plug-Zeitkette

| Mount (s) | set_address→config (ms) | config→GET_MAX_LUN (ms) | MAX_LUN→mount_done (ms) | mount_done→1. Read (ms) | set_address→1. Read (s) |
|---:|---:|---:|---:|---:|---:|
| 47.613 | 22.5 | 996.8 | 567.6 | 110.1 | 1.6971 |

## Datei-Atome

| t Start (s) | Datei | Reihenfolge | CBWs (Block/Anzahl) | Start→Start (ms) | gap_us | Gesamt (ms) | vollständig |
|---:|---|---|---|---|---|---:|---|
| 49.287955 | TRACK1.MP3 | tail_then_head | 480/32 240/240 0/240 512/240 752/240 992/240 1232/240 1472/64 1536/240 1776/240 2016/32 | [15.76, 116.61, 117.73, 116.61, 116.78, 116.55, 116.77, 31.42, 116.56, 116.78] | [4024, 3945, 5213, 4043, 4010, 3991, 4069, 4017, 4040, 4074] | 993.47 | True |


## Lesefenster (Modell: 512 Blöcke, CBW ≤ 240)
- TRACK1.MP3 (2048 Blöcke, 128 kbit/s): t=49.288 ab Block 0: [240, 240, 32, 240, 240, 240, 240, 64, 240, 240, 32] (weicht ab), Vorlauf 65.54 s; Abstände [] s

## Chunk-Takt (4 KiB, fine_data=True)
- 15 READ10 mit ≥ 2 DATA-Records; Chunks pro CBW p50 30.0, max 30
- Start→Start zwischen Chunks: p50 3.895 ms, p95 3.943 ms, max 4.629 ms (n=257)
- Pausen > 10 ms: 0

## Flash-Checkpoints (Heimfahrt)
- Fahrt #35 · Ende power_loss · aus Flash nachgeladen: ja · FW bei Aufnahme 0.3.41 · Medium size_1m
- 1 Checkpoints, Abstand p50 None s, max None s; längster Schreibvorgang 42 ms
- Stromverlust: Daten nach dem letzten Checkpoint fehlen (Ende der Aufnahme ≠ Ende der Fahrt).

Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.
