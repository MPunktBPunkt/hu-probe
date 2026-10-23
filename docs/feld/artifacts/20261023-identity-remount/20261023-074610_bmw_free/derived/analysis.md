# Analyse 20261023-074610_bmw_free

- Fahrzeug/Szenario: bmw / free · FW 0.3.19 (28249deb6e73)
- Medium: `ident_vol` (Quelle media_layout)
- Zeitbasis: t_us → Werte MEASURED
- TUR: 54 gesamt, 31 Burst-Intervalle, Leerlauf Median 2.2388 s (p10 2.2387, p90 2.2389, n=22)

## Mount-Signatur
- t=0.247 s: `0/8,0/8,256/40,8/8,24/8,56/8,120/8,16/8,128/8,64/8,512/8` (abweichend), TUR bis Signaturende: 32

## Read-Episoden

| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |
|---:|---|---:|---:|---|
| 1.968 | TRACK1.MP3 | 310 | 1.0 | tail_then_head |
| 18.141 | TRACK2.MP3 | 310 | 1.0 | head_first |
| 44.64 | TRACK3.MP3 | 310 | 1.0 | head_first |

## Folgetrack-Reads
- TRACK1.MP3 → TRACK2.MP3: 16.173 s nach Start, Vorlauf vor Ende 10.341 s (MEASURED, head_first)
- TRACK2.MP3 → TRACK3.MP3: 26.499 s nach Start, Vorlauf vor Ende 0.015 s (MEASURED, head_first)

## Auswahllatenz
- keine Marker mit folgendem Read

tail_then_head: 1 von 3 Episoden ['TRACK1.MP3']

## TUR-Phase
- Periode 2238.78 ms aus 22 Raster-TURs; max. Abweichung über Spannen mit anderen Kommandos: 0.49 ms
  - t=17.745 s, k=1, Rest -0.12 ms, Ops ['read10']
  - t=44.61 s, k=1, Rest 0.49 ms, Ops ['read10']

Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.
