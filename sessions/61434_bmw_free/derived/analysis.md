# Analyse 61434_bmw_free

- Fahrzeug/Szenario: bmw / free · FW 0.3.17 (2318c2923284, dirty)
- Medium: `cache_b` (Quelle media_layout)
- Zeitbasis: t_us → Werte MEASURED
- TUR: 83 gesamt, 40 Burst-Intervalle, Leerlauf Median 2.2388 s (p10 2.1805, p90 2.2392, n=42)

## Mount-Signatur

## Read-Episoden

| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |
|---:|---|---:|---:|---|
| 0.043 | TRACK1.MP3 | 310 | 1.0 | tail_then_head |
| 0.043 | TRACK2.MP3 | 310 | 1.0 | head_first |
| 0.043 | TRACK3.MP3 | 310 | 1.0 | head_first |

## Folgetrack-Reads
- TRACK1.MP3 → TRACK2.MP3: 0.0 s nach Start, Vorlauf vor Ende 26.514 s (MEASURED, head_first)
- TRACK2.MP3 → TRACK3.MP3: 0.0 s nach Start, Vorlauf vor Ende 26.514 s (MEASURED, head_first)

## Auswahllatenz
- keine Marker mit folgendem Read

tail_then_head: 1 von 3 Episoden ['TRACK1.MP3']

## TUR-Phase
- Periode 2238.78 ms aus 38 Raster-TURs; max. Abweichung über Spannen mit anderen Kommandos: None ms

Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.
