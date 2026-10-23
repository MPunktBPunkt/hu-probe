# Analyse 20261023-074749_bmw_free

- Fahrzeug/Szenario: bmw / free · FW 0.3.19 (28249deb6e73)
- Medium: `ident_b` (Quelle media_layout)
- Zeitbasis: t_us → Werte MEASURED
- TUR: 52 gesamt, 31 Burst-Intervalle, Leerlauf Median 2.2388 s (p10 2.2386, p90 2.2776, n=20)

## Mount-Signatur
- t=0.209 s: `0/8,0/8,256/40,8/8,24/8,56/8,120/8,16/8,128/8,64/8,512/8` (abweichend), TUR bis Signaturende: 32

## Read-Episoden

| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |
|---:|---|---:|---:|---|
| 1.984 | TRACK2.MP3 | 310 | 1.0 | tail_then_head |
| 19.892 | TRACK3.MP3 | 310 | 1.0 | head_first |

## Folgetrack-Reads
- TRACK2.MP3 → TRACK3.MP3: 17.908 s nach Start, Vorlauf vor Ende 8.606 s (MEASURED, head_first)

## Auswahllatenz
- keine Marker mit folgendem Read

tail_then_head: 1 von 2 Episoden ['TRACK2.MP3']

## TUR-Phase
- Periode 2238.79 ms aus 19 Raster-TURs; max. Abweichung über Spannen mit anderen Kommandos: None ms

Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.
