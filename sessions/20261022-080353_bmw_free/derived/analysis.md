# Analyse 20261022-080353_bmw_free

- Fahrzeug/Szenario: bmw / free · FW 0.3.17 (2318c2923284, dirty)
- Medium: `cache_place` (Quelle media_layout)
- Zeitbasis: t_us → Werte MEASURED
- TUR: 78 gesamt, 30 Burst-Intervalle, Leerlauf Median 2.2388 s (p10 2.2387, p90 2.2389, n=47)

## Mount-Signatur
- t=0.205 s: `0/8,0/8,256/40,8/8,24/8,56/8,120/8,16/8,128/8,64/8,512/8` (abweichend), TUR bis Signaturende: 32

## Read-Episoden

| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |
|---:|---|---:|---:|---|
| 1.974 | TRACK2.MP3 | 310 | 1.0 | tail_then_head |
| 8.52 | TRACK3.MP3 | 310 | 1.0 | head_first |
| 12.897 | TRACK1.MP3 | 310 | 1.0 | tail_then_head |

## Folgetrack-Reads
- TRACK2.MP3 → TRACK3.MP3: 6.546 s nach Start, Vorlauf vor Ende 19.968 s (MEASURED, head_first)
- TRACK3.MP3 → TRACK1.MP3: 4.377 s nach Start, Vorlauf vor Ende 22.137 s (MEASURED, tail_then_head)

## Auswahllatenz
- keine Marker mit folgendem Read

tail_then_head: 2 von 3 Episoden ['TRACK2.MP3', 'TRACK1.MP3']

## TUR-Phase
- Periode 2238.78 ms aus 47 Raster-TURs; max. Abweichung über Spannen mit anderen Kommandos: 29.05 ms
  - t=6.409 s, k=1, Rest 29.05 ms, Ops ['read10']
  - t=10.887 s, k=1, Rest 0.06 ms, Ops ['read10']

Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.
