# Analyse 20261022-081410_bmw_free

- Fahrzeug/Szenario: bmw / free · FW 0.3.17 (2318c2923284, dirty)
- Medium: `drive_mix` (Quelle media_layout)
- Zeitbasis: t_us → Werte MEASURED
- TUR: 223 gesamt, 32 Burst-Intervalle, Leerlauf Median 2.2388 s (p10 2.2387, p90 2.239, n=189)

## Mount-Signatur
- t=0.24 s: `0/8,0/8,96/40,8/8,24/8,56/8,16/8,64/8,512/8,32/8,131072/8` (abweichend), TUR bis Signaturende: 32

## Read-Episoden

| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |
|---:|---|---:|---:|---|
| 2.536 | INTRO.MP3 | 160 | 0.312 | head_first |
| 2.608 | NOTAG.MP3 | 32 | 0.023 | head_first |
| 2.668 | ROCK/R01.MP3 | 160 | 0.023 | head_first |
| 2.988 | ROCK/R02.MP3 | 160 | 0.014 | head_first |
| 3.056 | ROCK/R03.MP3 | 160 | 0.017 | head_first |
| 3.169 | INTRO.MP3 | 320 | 0.625 | partial_offset |
| 3.348 | ROCK/R04.MP3 | 160 | 0.012 | head_first |
| 3.419 | ROCK/CLASSIC/C01.MP3 | 160 | 0.019 | head_first |
| 3.531 | ROCK/CLASSIC/C02.MP3 | 160 | 0.021 | head_first |
| 3.651 | ROCK/CLASSIC/C03.MP3 | 160 | 0.021 | head_first |
| 4.191 | ROCK/CLASSIC/LIVE/L03.MP3 | 160 | 0.019 | head_first |
| 4.49 | ROCK/CLASSIC/LIVE/1977/B01.MP3 | 2192 | 0.101 | partial_offset |
| 6.011 | ROCK/CLASSIC/LIVE/1977/B02.MP3 | 160 | 0.02 | head_first |
| 6.11 | POP/P01.MP3 | 160 | 0.013 | head_first |
| 6.177 | POP/P02.MP3 | 160 | 0.014 | head_first |
| 6.443 | POP/CHARTS/H01.MP3 | 128 | 0.009 | partial_offset |
| 6.997 | JAZZ/J03.MP3 | 160 | 0.014 | head_first |
| 7.105 | JAZZ/QUARTET/Q01.MP3 | 128 | 0.009 | partial_offset |
| 7.167 | JAZZ/QUARTET/Q02.MP3 | 160 | 0.012 | head_first |
| 7.232 | JAZZ/QUARTET/Q03.MP3 | 160 | 0.022 | head_first |
| 7.328 | HOERBUCH/KAPITEL/K01.MP3 | 160 | 0.023 | head_first |
| 7.337 | HOERBUCH/KAPITEL/K02.MP3 | 160 | 0.021 | head_first |
| 7.337 | HOERBUCH/KAPITEL/K03.MP3 | 160 | 0.023 | head_first |
| 7.337 | HOERBUCH/KAPITEL/K04.MP3 | 160 | 0.022 | head_first |
| 7.337 | PODCAST/E01.MP3 | 160 | 0.016 | head_first |
| 7.337 | PODCAST/E02.MP3 | 160 | 0.013 | head_first |
| 7.337 | PODCAST/E03.MP3 | 32 | 0.003 | head_first |
| 7.337 | MIX/M01.MP3 | 160 | 0.019 | head_first |
| 7.337 | MIX/M02.MP3 | 160 | 0.012 | head_first |
| 7.337 | MIX/M03.MP3 | 160 | 0.022 | head_first |
| 9.799 | NOTAG.MP3 | 1368 | 0.972 | partial_offset |
| 44.571 | HOERBUCH/KAPITEL/K01.MP3 | 2688 | 0.382 | partial_offset |
| 51.915 | HOERBUCH/KAPITEL/K02.MP3 | 2688 | 0.347 | partial_offset |
| 60.919 | HOERBUCH/KAPITEL/K01.MP3 | 3776 | 0.536 | partial_offset |
| 73.315 | HOERBUCH/KAPITEL/K01.MP3 | 408 | 0.058 | partial_offset |
| 108.449 | HOERBUCH/KAPITEL/K02.MP3 | 4888 | 0.631 | partial_offset |
| 113.842 | HOERBUCH/KAPITEL/K03.MP3 | 6640 | 0.974 | partial_offset |
| 120.116 | HOERBUCH/KAPITEL/K04.MP3 | 7104 | 0.974 | partial_offset |
| 158.159 | JAZZ/J01.MP3 | 2688 | 0.179 | partial_offset |
| 162.384 | JAZZ/J01.MP3 | 8832 | 0.588 | partial_offset |
| 208.046 | JAZZ/J01.MP3 | 664 | 0.044 | partial_offset |

## Folgetrack-Reads
- INTRO.MP3 → NOTAG.MP3: 0.072 s nach Start, Vorlauf vor Ende 14.948 s (MEASURED, head_first)
- NOTAG.MP3 → ROCK/R01.MP3: 0.06 s nach Start, Vorlauf vor Ende 44.948 s (MEASURED, head_first)
- ROCK/R01.MP3 → ROCK/R02.MP3: 0.32 s nach Start, Vorlauf vor Ende 149.701 s (MEASURED, head_first)
- ROCK/R02.MP3 → ROCK/R03.MP3: 0.068 s nach Start, Vorlauf vor Ende 179.941 s (MEASURED, head_first)
- ROCK/R03.MP3 → INTRO.MP3: 0.113 s nach Start, Vorlauf vor Ende 199.906 s (MEASURED, partial_offset)
- INTRO.MP3 → ROCK/R04.MP3: 0.179 s nach Start, Vorlauf vor Ende 14.841 s (MEASURED, head_first)
- ROCK/R04.MP3 → ROCK/CLASSIC/C01.MP3: 0.071 s nach Start, Vorlauf vor Ende 169.933 s (MEASURED, head_first)
- ROCK/CLASSIC/C01.MP3 → ROCK/CLASSIC/C02.MP3: 0.112 s nach Start, Vorlauf vor Ende 209.912 s (MEASURED, head_first)
- ROCK/CLASSIC/C02.MP3 → ROCK/CLASSIC/C03.MP3: 0.12 s nach Start, Vorlauf vor Ende 189.894 s (MEASURED, head_first)
- ROCK/CLASSIC/C03.MP3 → ROCK/CLASSIC/LIVE/L03.MP3: 0.54 s nach Start, Vorlauf vor Ende 239.473 s (MEASURED, head_first)
- ROCK/CLASSIC/LIVE/L03.MP3 → ROCK/CLASSIC/LIVE/1977/B01.MP3: 0.299 s nach Start, Vorlauf vor Ende 279.707 s (MEASURED, partial_offset)
- ROCK/CLASSIC/LIVE/1977/B01.MP3 → ROCK/CLASSIC/LIVE/1977/B02.MP3: 1.521 s nach Start, Vorlauf vor Ende 328.483 s (MEASURED, head_first)
- ROCK/CLASSIC/LIVE/1977/B02.MP3 → POP/P01.MP3: 0.099 s nach Start, Vorlauf vor Ende 289.912 s (MEASURED, head_first)
- POP/P01.MP3 → POP/P02.MP3: 0.067 s nach Start, Vorlauf vor Ende 199.952 s (MEASURED, head_first)
- POP/P02.MP3 → POP/CHARTS/H01.MP3: 0.266 s nach Start, Vorlauf vor Ende 184.759 s (MEASURED, partial_offset)
- POP/CHARTS/H01.MP3 → JAZZ/J03.MP3: 0.554 s nach Start, Vorlauf vor Ende 179.455 s (MEASURED, head_first)
- JAZZ/J03.MP3 → JAZZ/QUARTET/Q01.MP3: 0.108 s nach Start, Vorlauf vor Ende 359.911 s (MEASURED, partial_offset)
- JAZZ/QUARTET/Q01.MP3 → JAZZ/QUARTET/Q02.MP3: 0.062 s nach Start, Vorlauf vor Ende 239.951 s (MEASURED, head_first)
- JAZZ/QUARTET/Q02.MP3 → JAZZ/QUARTET/Q03.MP3: 0.065 s nach Start, Vorlauf vor Ende 299.951 s (MEASURED, head_first)
- JAZZ/QUARTET/Q03.MP3 → HOERBUCH/KAPITEL/K01.MP3: 0.096 s nach Start, Vorlauf vor Ende 269.905 s (MEASURED, head_first)
- HOERBUCH/KAPITEL/K01.MP3 → HOERBUCH/KAPITEL/K02.MP3: 0.009 s nach Start, Vorlauf vor Ende 300.007 s (MEASURED, head_first)
- HOERBUCH/KAPITEL/K02.MP3 → HOERBUCH/KAPITEL/K03.MP3: 0.0 s nach Start, Vorlauf vor Ende 330.004 s (MEASURED, head_first)
- HOERBUCH/KAPITEL/K03.MP3 → HOERBUCH/KAPITEL/K04.MP3: 0.0 s nach Start, Vorlauf vor Ende 290.011 s (MEASURED, head_first)
- HOERBUCH/KAPITEL/K04.MP3 → PODCAST/E01.MP3: 0.0 s nach Start, Vorlauf vor Ende 310.021 s (MEASURED, head_first)
- PODCAST/E01.MP3 → PODCAST/E02.MP3: 0.0 s nach Start, Vorlauf vor Ende 420.022 s (MEASURED, head_first)
- PODCAST/E02.MP3 → PODCAST/E03.MP3: 0.0 s nach Start, Vorlauf vor Ende 390.008 s (MEASURED, head_first)
- PODCAST/E03.MP3 → MIX/M01.MP3: 0.0 s nach Start, Vorlauf vor Ende 450.011 s (MEASURED, head_first)
- MIX/M01.MP3 → MIX/M02.MP3: 0.0 s nach Start, Vorlauf vor Ende 200.019 s (MEASURED, head_first)
- MIX/M02.MP3 → MIX/M03.MP3: 0.0 s nach Start, Vorlauf vor Ende 220.003 s (MEASURED, head_first)
- MIX/M03.MP3 → NOTAG.MP3: 2.462 s nach Start, Vorlauf vor Ende 237.551 s (MEASURED, partial_offset)
- NOTAG.MP3 → HOERBUCH/KAPITEL/K01.MP3: 34.772 s nach Start, Vorlauf vor Ende 10.236 s (MEASURED, partial_offset)
- HOERBUCH/KAPITEL/K01.MP3 → HOERBUCH/KAPITEL/K02.MP3: 7.344 s nach Start, Vorlauf vor Ende 292.672 s (MEASURED, partial_offset)
- HOERBUCH/KAPITEL/K02.MP3 → HOERBUCH/KAPITEL/K01.MP3: 9.004 s nach Start, Vorlauf vor Ende 321.0 s (MEASURED, partial_offset)
- HOERBUCH/KAPITEL/K01.MP3 → HOERBUCH/KAPITEL/K02.MP3: 35.134 s nach Start, Vorlauf vor Ende 264.882 s (MEASURED, partial_offset)
- HOERBUCH/KAPITEL/K02.MP3 → HOERBUCH/KAPITEL/K03.MP3: 5.393 s nach Start, Vorlauf vor Ende 324.611 s (MEASURED, partial_offset)
- HOERBUCH/KAPITEL/K03.MP3 → HOERBUCH/KAPITEL/K04.MP3: 6.274 s nach Start, Vorlauf vor Ende 283.737 s (MEASURED, partial_offset)
- HOERBUCH/KAPITEL/K04.MP3 → JAZZ/J01.MP3: 38.043 s nach Start, Vorlauf vor Ende 271.978 s (MEASURED, partial_offset)

## Auswahllatenz
- keine Marker mit folgendem Read

tail_then_head: 0 von 41 Episoden []

## TUR-Phase
- Periode 2238.84 ms aus 185 Raster-TURs; max. Abweichung über Spannen mit anderen Kommandos: 0.39 ms
  - t=8.374 s, k=1, Rest -0.03 ms, Ops ['read10']
  - t=44.194 s, k=1, Rest -0.02 ms, Ops ['read10']
  - t=50.911 s, k=1, Rest 0.04 ms, Ops ['read10']
  - t=71.38 s, k=1, Rest 0.39 ms, Ops ['read10']
  - t=157.413 s, k=1, Rest -0.0 ms, Ops ['read10']
  - t=206.666 s, k=1, Rest 0.05 ms, Ops ['read10']

Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.
