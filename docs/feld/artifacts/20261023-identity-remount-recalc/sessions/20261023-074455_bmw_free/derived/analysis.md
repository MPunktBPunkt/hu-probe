# Analyse 20261023-074455_bmw_free

- Fahrzeug/Szenario: bmw / free · FW 0.3.19 (28249deb6e73)
- Medium: `ident_label` (Quelle media_layout)
- Zeitbasis: t_us → Werte MEASURED
- TUR: 62 gesamt, 30 Burst-Intervalle, Leerlauf Median 2.2388 s (p10 2.2388, p90 2.2389, n=31)

## Mount-Signatur
- t=0.198 s: `0/8,0/8,256/40,8/8,24/8,56/8,120/8,16/8,128/8,64/8,512/8` (abweichend), TUR bis Signaturende: 32

## Read-Episoden

| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |
|---:|---|---:|---:|---|
| 1.785 | TRACK1.MP3 | 310 | 1.0 | tail_then_head |
| 8.501 | TRACK2.MP3 | 310 | 1.0 | head_first |
| 35.005 | TRACK3.MP3 | 310 | 1.0 | head_first |

## Folgetrack-Reads
- TRACK1.MP3 → TRACK2.MP3: 6.716 s nach Start, Vorlauf vor Ende 19.798 s (MEASURED, head_first)
- TRACK2.MP3 → TRACK3.MP3: 26.504 s nach Start, Vorlauf vor Ende 0.01 s (MEASURED, head_first)

## Auswahllatenz
- keine Marker mit folgendem Read

tail_then_head: 1 von 3 Episoden ['TRACK1.MP3']

## TUR-Phase
- Periode 2238.78 ms aus 31 Raster-TURs; max. Abweichung über Spannen mit anderen Kommandos: 0.02 ms
  - t=6.693 s, k=1, Rest -0.02 ms, Ops ['read10']
  - t=33.558 s, k=1, Rest -0.0 ms, Ops ['read10']

Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.
