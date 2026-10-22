# Analyse 20261022-173030_bmw_drive_home

- Fahrzeug/Szenario: bmw / drive_home · FW 0.3.17 (2318c2923284, dirty)
- Medium: `drive_mix` (Quelle media_layout)
- Zeitbasis: capture_t_us → Werte MEASURED
- TUR: 208 gesamt, 31 Burst-Intervalle, Leerlauf Median 2.2388 s (p10 2.2387, p90 2.239, n=176)

## Mount-Signatur
- t=0.245 s: `0/8,0/8,96/40,8/8,24/8,56/8,16/8,64/8,512/8,32/8,131072/8` (abweichend), TUR bis Signaturende: 32

## Read-Episoden

| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |
|---:|---|---:|---:|---|
| 2.307 | ROCK/CLASSIC/C01.MP3 | 512 | 0.062 | head_first |
| 2.611 | ROCK/CLASSIC/C01.MP3 | 7696 | 0.936 | partial_offset |

## Folgetrack-Reads
- keine

## Auswahllatenz
- keine Marker mit folgendem Read

tail_then_head: 0 von 2 Episoden []

## TUR-Phase
- Periode 2238.79 ms aus 176 Raster-TURs; max. Abweichung über Spannen mit anderen Kommandos: None ms

## Plug-Zeitkette

| Mount (s) | set_address→config (ms) | config→GET_MAX_LUN (ms) | MAX_LUN→mount_done (ms) | mount_done→1. Read (ms) | set_address→1. Read (s) |
|---:|---:|---:|---:|---:|---:|
| 19.946 | 24.8 | 994.6 | 1304.0 | -236.2 | 2.0872 |

## Datei-Atome

| t Start (s) | Datei | Reihenfolge | CBWs (Block/Anzahl) | Start→Start (ms) | gap_us | Gesamt (ms) | vollständig |
|---:|---|---|---|---|---|---:|---|
| 22.008875 | ROCK/CLASSIC/C01.MP3 | head_first | 0/240 240/240 480/32 512/240 752/240 992/240 1232/240 1472/64 1536/240 1776/240 2016/240 4616/240 4856/240 5096/240 2256/240 2496/240 2736/240 5336/240 5576/240 5816/240 2976/240 3216/240 3456/240 6056/240 6296/240 6536/240 3696/240 3936/240 4176/240 6776/240 7016/240 7256/240 4416/200 7496/240 7736/240 7976/232 | [118.53, 117.96, 67.01, 117.17, 117.04, 133.63, 118.93, 61.18, 117.16, 117.9, 135.97, 117.34, 117.27, 117.03, 116.92, 116.77, 192.87, 116.97, 117.08, 117.07, 117.34, 117.63, 117.08, 117.08, 117.06, 117.09, 116.56, 117.53, 117.04, 116.89, 116.86, 116.77, 97.63, 116.48, 117.09] | [4901, 5115, 1837, 4189, 4072, 4494, 5786, 8510, 4556, 4795, 6424, 4210, 4014, 4143, 4321, 4085, 14059, 4052, 4189, 4142, 4529, 4591, 4132, 4105, 4153, 4095, 4066, 4691, 3999, 4051, 4096, 4123, 4014, 4045, 4015] | 4197.44 | False |


## Lesefenster (Modell: 512 Blöcke, CBW ≤ 240)
- ROCK/CLASSIC/C01.MP3 (8224 Blöcke, 160 kbit/s): t=22.009 ab Block 0: [240, 240, 32, 240, 240, 240, 240, 64, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 200, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 232] (weicht ab), Vorlauf 210.12 s; Abstände [] s

## Chunk-Takt (4 KiB, fine_data=True)
- 52 READ10 mit ≥ 2 DATA-Records; Chunks pro CBW p50 30.0, max 30
- Start→Start zwischen Chunks: p50 3.896 ms, p95 3.95 ms, max 5.598 ms (n=1043)
- Pausen > 10 ms: 0

## Flash-Checkpoints (Heimfahrt)
- Fahrt #24 · Ende power_loss · aus Flash nachgeladen: ja · FW bei Aufnahme 0.3.17 · Medium drive_mix
- 8 Checkpoints, Abstand p50 60.446 s, max 60.449 s; längster Schreibvorgang 135 ms
- Stromverlust: Daten nach dem letzten Checkpoint fehlen (Ende der Aufnahme ≠ Ende der Fahrt).

Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.
