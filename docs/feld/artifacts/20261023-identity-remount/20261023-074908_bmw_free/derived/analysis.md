# Analyse 20261023-074908_bmw_free

- Fahrzeug/Szenario: bmw / free · FW 0.3.19 (28249deb6e73)
- Medium: `ident_vol` (Quelle media_layout)
- Zeitbasis: t_us → Werte MEASURED
- TUR: 70 gesamt, 31 Burst-Intervalle, Leerlauf Median 2.2388 s (p10 2.2387, p90 2.2389, n=38)

## Mount-Signatur
- t=0.243 s: `0/8,0/8,256/40,8/8,24/8,56/8,120/8,16/8,128/8,64/8,512/8` (abweichend), TUR bis Signaturende: 32

## Read-Episoden

| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |
|---:|---|---:|---:|---|
| 1.94 | TRACK3.MP3 | 310 | 1.0 | tail_then_head |
| 8.202 | TRACK1.MP3 | 310 | 1.0 | tail_then_head |
| 17.701 | TRACK2.MP3 | 310 | 1.0 | head_first |

## Folgetrack-Reads
- TRACK3.MP3 → TRACK1.MP3: 6.262 s nach Start, Vorlauf vor Ende 20.252 s (MEASURED, tail_then_head)
- TRACK1.MP3 → TRACK2.MP3: 9.499 s nach Start, Vorlauf vor Ende 17.015 s (MEASURED, head_first)

## Auswahllatenz
- keine Marker mit folgendem Read

tail_then_head: 2 von 3 Episoden ['TRACK3.MP3', 'TRACK1.MP3']

## TUR-Phase
- Periode 2238.78 ms aus 38 Raster-TURs; max. Abweichung über Spannen mit anderen Kommandos: 0.05 ms
  - t=6.437 s, k=1, Rest -0.05 ms, Ops ['read10']
  - t=17.631 s, k=1, Rest 0.03 ms, Ops ['read10']

Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.
