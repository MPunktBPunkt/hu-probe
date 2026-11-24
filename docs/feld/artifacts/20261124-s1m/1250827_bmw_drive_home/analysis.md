# Analyse 1250827_bmw_drive_home

- Fahrzeug/Szenario: bmw / drive_home · FW 0.3.41 (c318b1e3c1c6)
- Medium: `size_158` (Quelle media_layout)
- Zeitbasis: capture_t_us → Werte MEASURED
- TUR: 579 gesamt, 31 Burst-Intervalle, Leerlauf Median 2.2388 s (p10 2.2386, p90 2.239, n=547)

## Mount-Signatur
- t=0.237 s: `0/8,0/8,32/40,8/8,24/8,120/8,16/8,128/8,512/8,4096/8,0/8` (abweichend), TUR bis Signaturende: 32

## Read-Episoden

| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |
|---:|---|---:|---:|---|
| 1.865 | TRACK1.MP3 | 310 | 0.994 | tail_then_head |
| 4.612 | TRACK2.MP3 | 310 | 0.994 | head_first |
| 14.532 | TRACK3.MP3 | 310 | 0.994 | head_first |

## Folgetrack-Reads
- TRACK1.MP3 → TRACK2.MP3: 2.747 s nach Start, Vorlauf vor Ende 7.179 s (MEASURED, head_first)
- TRACK2.MP3 → TRACK3.MP3: 9.92 s nach Start, Vorlauf vor Ende 0.006 s (MEASURED, head_first)

## Auswahllatenz
- keine Marker mit folgendem Read

tail_then_head: 1 von 3 Episoden ['TRACK1.MP3']

## TUR-Phase
- Periode 2238.79 ms aus 547 Raster-TURs; max. Abweichung über Spannen mit anderen Kommandos: 0.04 ms
  - t=13.208 s, k=1, Rest -0.04 ms, Ops ['read10']

## Plug-Zeitkette

| Mount (s) | set_address→config (ms) | config→GET_MAX_LUN (ms) | MAX_LUN→mount_done (ms) | mount_done→1. Read (ms) | set_address→1. Read (s) |
|---:|---:|---:|---:|---:|---:|
| 20.548 | 22.5 | 996.9 | 783.0 | -152.7 | 1.6496 |

## Datei-Atome

| t Start (s) | Datei | Reihenfolge | CBWs (Block/Anzahl) | Start→Start (ms) | gap_us | Gesamt (ms) | vollständig |
|---:|---|---|---|---|---|---:|---|
| 22.174705 | TRACK1.MP3 | tail_then_head | 240/70 0/240 | [34.52] | [3372] | 147.12 | False |
| 24.921764 | TRACK2.MP3 | head_first | 0/240 240/70 | [118.01] | [5257] | 149.19 | False |
| 34.842321 | TRACK3.MP3 | head_first | 0/240 240/70 | [116.49] | [4034] | 147.65 | False |

- TRACK1.MP3 → TRACK2.MP3 (head_first): Start→Start 2.747059 s, Atom-Ende→nächster Read 2.599938 s
- TRACK2.MP3 → TRACK3.MP3 (head_first): Start→Start 9.920557 s, Atom-Ende→nächster Read 9.771366 s

## Lesefenster (Modell: 512 Blöcke, CBW ≤ 240)
- TRACK1.MP3 (312 Blöcke, 128 kbit/s): t=22.175 ab Block 0: [240, 70], Vorlauf 9.92 s; Abstände [] s
- TRACK2.MP3 (312 Blöcke, 128 kbit/s): t=24.922 ab Block 0: [240, 70], Vorlauf 9.92 s; Abstände [] s
- TRACK3.MP3 (312 Blöcke, 128 kbit/s): t=34.842 ab Block 0: [240, 70], Vorlauf 9.92 s; Abstände [] s

## Chunk-Takt (4 KiB, fine_data=True)
- 10 READ10 mit ≥ 2 DATA-Records; Chunks pro CBW p50 9.0, max 30
- Start→Start zwischen Chunks: p50 3.893 ms, p95 3.933 ms, max 4.677 ms (n=123)
- Pausen > 10 ms: 0

## Flash-Checkpoints (Heimfahrt)
- Fahrt #33 · Ende boot_long · aus Flash nachgeladen: nein · FW bei Aufnahme 0.3.41 · Medium size_158
- 20 Checkpoints, Abstand p50 60.447 s, max 60.449 s; längster Schreibvorgang 29 ms

Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.
