# Analyse 20261022-081152_bmw_free

- Fahrzeug/Szenario: bmw / free · FW 0.3.17 (2318c2923284, dirty)
- Medium: `id3_cover` (Quelle media_layout)
- Zeitbasis: t_us → Werte MEASURED
- TUR: 90 gesamt, 29 Burst-Intervalle, Leerlauf Median 2.2388 s (p10 2.2387, p90 2.2389, n=60)

## Mount-Signatur
- t=0.203 s: `0/8,0/8,256/40,8/8,24/8,56/8,120/8,16/8,128/8,64/8,512/8` (abweichend), TUR bis Signaturende: 32

## Read-Episoden

| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |
|---:|---|---:|---:|---|
| 2.201 | COVER1.MP3 | 160 | 0.495 | head_first |
| 2.23 | COVER2.MP3 | 160 | 0.495 | head_first |
| 2.23 | COVER3.MP3 | 160 | 0.495 | head_first |
| 2.23 | NOCOVER.MP3 | 160 | 0.495 | head_first |
| 2.23 | COVER1.MP3 | 160 | 0.495 | partial_offset |
| 20.966 | COVER2.MP3 | 160 | 0.495 | partial_offset |
| 47.545 | COVER3.MP3 | 160 | 0.495 | partial_offset |
| 73.967 | NOCOVER.MP3 | 160 | 0.495 | partial_offset |

## Folgetrack-Reads
- COVER1.MP3 → COVER2.MP3: 0.029 s nach Start, Vorlauf vor Ende 26.485 s (MEASURED, head_first)
- COVER2.MP3 → COVER3.MP3: 0.0 s nach Start, Vorlauf vor Ende 26.514 s (MEASURED, head_first)
- COVER3.MP3 → NOCOVER.MP3: 0.0 s nach Start, Vorlauf vor Ende 26.514 s (MEASURED, head_first)
- NOCOVER.MP3 → COVER1.MP3: 0.0 s nach Start, Vorlauf vor Ende 26.514 s (MEASURED, partial_offset)
- COVER1.MP3 → COVER2.MP3: 18.736 s nach Start, Vorlauf vor Ende 7.778 s (MEASURED, partial_offset)
- COVER2.MP3 → COVER3.MP3: 26.579 s nach Start, Vorlauf vor Ende -0.065 s (MEASURED, partial_offset)
- COVER3.MP3 → NOCOVER.MP3: 26.422 s nach Start, Vorlauf vor Ende 0.092 s (MEASURED, partial_offset)

## Auswahllatenz
- keine Marker mit folgendem Read

tail_then_head: 0 von 8 Episoden []

## TUR-Phase
- Periode 2238.78 ms aus 59 Raster-TURs; max. Abweichung über Spannen mit anderen Kommandos: 0.02 ms
  - t=20.06 s, k=1, Rest 0.0 ms, Ops ['read10']
  - t=46.926 s, k=1, Rest -0.02 ms, Ops ['read10']
  - t=73.791 s, k=1, Rest -0.0 ms, Ops ['read10']

Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.
