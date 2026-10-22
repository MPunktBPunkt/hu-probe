# Analyse 20261022-080544_bmw_free

- Fahrzeug/Szenario: bmw / free · FW 0.3.17 (2318c2923284, dirty)
- Medium: `dur_mix` (Quelle media_layout)
- Zeitbasis: t_us → Werte MEASURED
- TUR: 87 gesamt, 31 Burst-Intervalle, Leerlauf Median 2.2388 s (p10 2.2387, p90 2.2389, n=55)

## Mount-Signatur
- t=0.242 s: `0/8,0/8,32/40,8/8,24/8,120/8,16/8,128/8,512/8,4096/8,0/8` (abweichend), TUR bis Signaturende: 32

## Read-Episoden

| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |
|---:|---|---:|---:|---|
| 1.687 | D010.MP3 | 117 | 0.975 | head_first |
| 1.763 | D026.MP3 | 160 | 0.526 | head_first |
| 1.87 | D060.MP3 | 160 | 0.227 | head_first |
| 1.968 | D180.MP3 | 160 | 0.076 | head_first |
| 4.37 | D026.MP3 | 136 | 0.447 | partial_offset |
| 12.286 | D060.MP3 | 536 | 0.761 | partial_offset |
| 15.794 | D180.MP3 | 1936 | 0.92 | partial_offset |

## Folgetrack-Reads
- D010.MP3 → D026.MP3: 0.076 s nach Start, Vorlauf vor Ende 9.928 s (MEASURED, head_first)
- D026.MP3 → D060.MP3: 0.107 s nach Start, Vorlauf vor Ende 25.91 s (MEASURED, head_first)
- D060.MP3 → D180.MP3: 0.098 s nach Start, Vorlauf vor Ende 59.905 s (MEASURED, head_first)
- D180.MP3 → D026.MP3: 2.402 s nach Start, Vorlauf vor Ende 177.607 s (MEASURED, partial_offset)
- D026.MP3 → D060.MP3: 7.916 s nach Start, Vorlauf vor Ende 18.101 s (MEASURED, partial_offset)
- D060.MP3 → D180.MP3: 3.508 s nach Start, Vorlauf vor Ende 56.495 s (MEASURED, partial_offset)

## Auswahllatenz
- keine Marker mit folgendem Read

tail_then_head: 0 von 7 Episoden []

## TUR-Phase
- Periode 2238.78 ms aus 55 Raster-TURs; max. Abweichung über Spannen mit anderen Kommandos: 0.08 ms
  - t=10.724 s, k=1, Rest 0.06 ms, Ops ['read10']
  - t=15.201 s, k=1, Rest 0.08 ms, Ops ['read10']

Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.
