# Analyse 20261022-080752_bmw_free

- Fahrzeug/Szenario: bmw / free · FW 0.3.17 (2318c2923284, dirty)
- Medium: `bitrate_mix` (Quelle media_layout)
- Zeitbasis: t_us → Werte MEASURED
- TUR: 137 gesamt, 31 Burst-Intervalle, Leerlauf Median 2.2388 s (p10 2.2387, p90 2.2389, n=105)

## Mount-Signatur
- t=0.194 s: `0/8,0/8,32/40,8/8,24/8,120/8,16/8,128/8,512/8,4096/8,0/8` (abweichend), TUR bis Signaturende: 32

## Read-Episoden

| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |
|---:|---|---:|---:|---|
| 1.706 | B032.MP3 | 160 | 0.339 | head_first |
| 1.815 | B128.MP3 | 160 | 0.085 | head_first |
| 1.976 | B320.MP3 | 160 | 0.034 | head_first |
| 2.172 | B032.MP3 | 304 | 0.644 | partial_offset |
| 54.66 | B128.MP3 | 1704 | 0.91 | partial_offset |
| 114.4 | B320.MP3 | 2688 | 0.573 | partial_offset |
| 121.495 | B320.MP3 | 1832 | 0.391 | partial_offset |

## Folgetrack-Reads
- B032.MP3 → B128.MP3: 0.109 s nach Start, Vorlauf vor Ende 59.894 s (MEASURED, head_first)
- B128.MP3 → B320.MP3: 0.161 s nach Start, Vorlauf vor Ende 59.842 s (MEASURED, head_first)
- B320.MP3 → B032.MP3: 0.196 s nach Start, Vorlauf vor Ende 59.807 s (MEASURED, partial_offset)
- B032.MP3 → B128.MP3: 52.488 s nach Start, Vorlauf vor Ende 7.515 s (MEASURED, partial_offset)
- B128.MP3 → B320.MP3: 59.74 s nach Start, Vorlauf vor Ende 0.263 s (MEASURED, partial_offset)

## Auswahllatenz
- keine Marker mit folgendem Read

tail_then_head: 0 von 7 Episoden []

## TUR-Phase
- Periode 2238.78 ms aus 104 Raster-TURs; max. Abweichung über Spannen mit anderen Kommandos: 0.27 ms
  - t=53.333 s, k=2, Rest -0.02 ms, Ops ['read10']
  - t=113.78 s, k=1, Rest 0.27 ms, Ops ['read10']
  - t=120.496 s, k=1, Rest -0.04 ms, Ops ['read10']

Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.
