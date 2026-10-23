# Analyse 20261023-074319_bmw_free

- Fahrzeug/Szenario: bmw / free · FW 0.3.19 (28249deb6e73)
- Medium: `ident_serial` (Quelle media_layout)
- Zeitbasis: t_us → Werte MEASURED
- TUR: 74 gesamt, 31 Burst-Intervalle, Leerlauf Median 2.2388 s (p10 2.2387, p90 2.2389, n=42)

## Mount-Signatur
- t=1.746 s: `0/8,0/8,256/40,8/8,24/8,56/8,120/8,16/8,128/8,64/8,512/8` (abweichend), TUR bis Signaturende: 32

## Read-Episoden

| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |
|---:|---|---:|---:|---|
| 3.509 | TRACK1.MP3 | 160 | 0.516 | head_first |
| 3.608 | TRACK2.MP3 | 160 | 0.516 | head_first |
| 3.691 | TRACK3.MP3 | 160 | 0.516 | head_first |
| 3.887 | TRACK1.MP3 | 144 | 0.465 | partial_offset |
| 22.638 | TRACK2.MP3 | 144 | 0.465 | partial_offset |
| 49.137 | TRACK3.MP3 | 144 | 0.465 | partial_offset |

## Folgetrack-Reads
- TRACK1.MP3 → TRACK2.MP3: 0.099 s nach Start, Vorlauf vor Ende 26.415 s (MEASURED, head_first)
- TRACK2.MP3 → TRACK3.MP3: 0.083 s nach Start, Vorlauf vor Ende 26.431 s (MEASURED, head_first)
- TRACK3.MP3 → TRACK1.MP3: 0.196 s nach Start, Vorlauf vor Ende 26.318 s (MEASURED, partial_offset)
- TRACK1.MP3 → TRACK2.MP3: 18.751 s nach Start, Vorlauf vor Ende 7.763 s (MEASURED, partial_offset)
- TRACK2.MP3 → TRACK3.MP3: 26.499 s nach Start, Vorlauf vor Ende 0.015 s (MEASURED, partial_offset)

## Auswahllatenz
- keine Marker mit folgendem Read

tail_then_head: 0 von 6 Episoden []

## TUR-Phase
- Periode 2238.78 ms aus 42 Raster-TURs; max. Abweichung über Spannen mit anderen Kommandos: 1.52 ms
  - t=21.524 s, k=1, Rest -1.52 ms, Ops ['read10']
  - t=48.388 s, k=1, Rest 0.03 ms, Ops ['read10']

Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.
