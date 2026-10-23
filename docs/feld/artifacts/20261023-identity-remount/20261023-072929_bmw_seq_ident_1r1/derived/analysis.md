# Analyse 20261023-072929_bmw_seq_ident_1r1

- Fahrzeug/Szenario: bmw / seq_ident_1r1 · FW 0.3.19 (28249deb6e73)
- Medium: `cache_a_pin` (Quelle media_layout)
- Zeitbasis: capture_t_us → Werte MEASURED
- TUR: 2 gesamt, 1 Burst-Intervalle

## Mount-Signatur
- t=0.021 s: `0/8,256/40,8/8,24/8,56/8,120/8,16/8,128/8,64/8,512/8,32/8` (abweichend), TUR bis Signaturende: 98
- t=720.111 s: `0/8,0/8,256/40,8/8,24/8,56/8,120/8,16/8,128/8,64/8,512/8` (abweichend), TUR bis Signaturende: 32
- t=842.215 s: `0/8,0/8,256/40,8/8,24/8,56/8,120/8,16/8,128/8,64/8,512/8` (abweichend), TUR bis Signaturende: 32

## Read-Episoden

| t Start (s) | Datei | Blöcke | Abdeckung | Reihenfolge |
|---:|---|---:|---:|---|
| 338.885 | TRACK1.MP3 | 310 | 1.0 | head_first |
| 357.877 | TRACK2.MP3 | 310 | 1.0 | tail_then_head |
| 384.378 | TRACK3.MP3 | 310 | 1.0 | head_first |
| 721.713 | TRACK1.MP3 | 310 | 1.0 | tail_then_head |
| 723.886 | TRACK2.MP3 | 310 | 1.0 | head_first |
| 750.387 | TRACK3.MP3 | 310 | 1.0 | head_first |
| 843.82 | TRACK3.MP3 | 310 | 1.0 | tail_then_head |
| 860.511 | TRACK1.MP3 | 310 | 1.0 | tail_then_head |
| 887.014 | TRACK2.MP3 | 310 | 1.0 | head_first |

## Folgetrack-Reads
- TRACK1.MP3 → TRACK2.MP3: 18.992 s nach Start, Vorlauf vor Ende 7.522 s (MEASURED, tail_then_head)
- TRACK2.MP3 → TRACK3.MP3: 26.501 s nach Start, Vorlauf vor Ende 0.013 s (MEASURED, head_first)
- TRACK3.MP3 → TRACK1.MP3: 337.335 s nach Start, Vorlauf vor Ende -310.821 s (MEASURED, tail_then_head)
- TRACK1.MP3 → TRACK2.MP3: 2.173 s nach Start, Vorlauf vor Ende 24.341 s (MEASURED, head_first)
- TRACK2.MP3 → TRACK3.MP3: 26.501 s nach Start, Vorlauf vor Ende 0.013 s (MEASURED, head_first)
- TRACK3.MP3 → TRACK1.MP3: 16.691 s nach Start, Vorlauf vor Ende 9.823 s (MEASURED, tail_then_head)
- TRACK1.MP3 → TRACK2.MP3: 26.503 s nach Start, Vorlauf vor Ende 0.011 s (MEASURED, head_first)

## Auswahllatenz
- keine Marker mit folgendem Read

tail_then_head: 4 von 9 Episoden ['TRACK2.MP3', 'TRACK1.MP3', 'TRACK3.MP3', 'TRACK1.MP3']
Media-Loss-Reaktion (post_finalize): 728 Kommandos, ['tur', 'request_sense', 'tur', 'request_sense', 'tur', 'request_sense', 'tur', 'request_sense', 'tur', 'request_sense', 'tur', 'request_sense', 'prevent_allow', 'request_sense', 'tur', 'request_sense', 'tur', 'request_sense', 'tur', 'request_sense', 'tur', 'request_sense', 'tur', 'request_sense']

## TUR-Phase
- Periode 2238.78 ms aus 357 Raster-TURs; max. Abweichung über Spannen mit anderen Kommandos: 10.79 ms
  - t=154.916 s, k=1, Rest -0.04 ms, Ops ['prevent_allow', 'request_sense']
  - t=157.154 s, k=1, Rest -0.01 ms, Ops ['request_sense']
  - t=159.393 s, k=1, Rest 0.02 ms, Ops ['request_sense']
  - t=161.632 s, k=1, Rest -0.0 ms, Ops ['request_sense']
  - t=163.871 s, k=1, Rest 0.03 ms, Ops ['request_sense']
  - t=166.109 s, k=1, Rest 0.03 ms, Ops ['request_sense']
  - t=168.348 s, k=1, Rest -0.05 ms, Ops ['request_sense']
  - t=170.587 s, k=1, Rest 0.26 ms, Ops ['request_sense']
  - t=172.826 s, k=1, Rest -0.29 ms, Ops ['request_sense']
  - t=175.065 s, k=1, Rest 0.3 ms, Ops ['request_sense']
  - t=177.304 s, k=1, Rest -0.23 ms, Ops ['request_sense']
  - t=179.542 s, k=1, Rest 0.06 ms, Ops ['request_sense']
  - t=181.781 s, k=1, Rest -0.12 ms, Ops ['request_sense']
  - t=184.02 s, k=1, Rest 10.79 ms, Ops ['request_sense']
  - t=186.269 s, k=1, Rest -10.77 ms, Ops ['request_sense']
  - t=188.497 s, k=1, Rest -0.02 ms, Ops ['request_sense']
  - t=190.736 s, k=1, Rest -0.01 ms, Ops ['request_sense']
  - t=192.975 s, k=1, Rest 0.03 ms, Ops ['request_sense']
  - t=195.214 s, k=1, Rest -0.02 ms, Ops ['request_sense']
  - t=197.452 s, k=1, Rest 0.54 ms, Ops ['request_sense']
  - t=199.692 s, k=1, Rest -0.54 ms, Ops ['request_sense']
  - t=201.93 s, k=1, Rest -0.04 ms, Ops ['request_sense']
  - t=204.169 s, k=1, Rest 0.06 ms, Ops ['request_sense']
  - t=206.408 s, k=1, Rest 0.08 ms, Ops ['request_sense']
  - t=208.646 s, k=1, Rest -0.14 ms, Ops ['request_sense']
  - t=210.885 s, k=1, Rest 0.21 ms, Ops ['request_sense']
  - t=213.124 s, k=1, Rest 0.27 ms, Ops ['request_sense']
  - t=215.363 s, k=1, Rest -0.44 ms, Ops ['request_sense']
  - t=217.602 s, k=1, Rest 0.0 ms, Ops ['request_sense']
  - t=219.84 s, k=1, Rest -0.09 ms, Ops ['request_sense']
  - t=222.079 s, k=1, Rest 0.01 ms, Ops ['request_sense']
  - t=224.318 s, k=1, Rest 1.27 ms, Ops ['request_sense']
  - t=226.558 s, k=1, Rest -1.21 ms, Ops ['request_sense']
  - t=228.795 s, k=1, Rest -0.09 ms, Ops ['request_sense']
  - t=231.034 s, k=1, Rest 0.38 ms, Ops ['request_sense']
  - t=233.273 s, k=1, Rest -0.26 ms, Ops ['request_sense']
  - t=235.512 s, k=1, Rest -0.08 ms, Ops ['request_sense']
  - t=237.751 s, k=1, Rest -0.04 ms, Ops ['request_sense']
  - t=239.989 s, k=1, Rest 0.19 ms, Ops ['request_sense']
  - t=242.228 s, k=1, Rest -0.21 ms, Ops ['request_sense']
  - t=244.467 s, k=1, Rest 0.1 ms, Ops ['request_sense']
  - t=246.706 s, k=1, Rest 1.32 ms, Ops ['request_sense']
  - t=248.946 s, k=1, Rest -0.71 ms, Ops ['request_sense']
  - t=251.184 s, k=1, Rest 0.08 ms, Ops ['request_sense']
  - t=253.423 s, k=1, Rest -0.79 ms, Ops ['request_sense']
  - t=255.661 s, k=1, Rest 0.0 ms, Ops ['request_sense']
  - t=257.9 s, k=1, Rest -0.04 ms, Ops ['request_sense']
  - t=260.138 s, k=1, Rest 0.17 ms, Ops ['request_sense']
  - t=262.377 s, k=1, Rest 0.27 ms, Ops ['request_sense']
  - t=264.616 s, k=1, Rest -0.43 ms, Ops ['request_sense']
  - t=266.855 s, k=1, Rest 0.19 ms, Ops ['request_sense']
  - t=269.094 s, k=1, Rest -0.2 ms, Ops ['request_sense']
  - t=271.332 s, k=1, Rest 0.02 ms, Ops ['request_sense']
  - t=273.571 s, k=1, Rest -0.0 ms, Ops ['request_sense']
  - t=275.81 s, k=1, Rest -0.02 ms, Ops ['request_sense']
  - t=278.049 s, k=1, Rest 0.09 ms, Ops ['request_sense']
  - t=280.287 s, k=1, Rest -0.0 ms, Ops ['request_sense']
  - t=282.526 s, k=1, Rest 0.01 ms, Ops ['request_sense']
  - t=284.765 s, k=1, Rest -0.03 ms, Ops ['request_sense']
  - t=287.004 s, k=1, Rest 0.52 ms, Ops ['request_sense']
  - t=289.243 s, k=1, Rest -0.18 ms, Ops ['request_sense']
  - t=291.482 s, k=1, Rest -0.36 ms, Ops ['request_sense']
  - t=293.72 s, k=1, Rest 1.45 ms, Ops ['request_sense']
  - t=295.96 s, k=1, Rest -1.51 ms, Ops ['request_sense']
  - t=298.198 s, k=1, Rest 0.03 ms, Ops ['request_sense']
  - t=300.436 s, k=1, Rest 0.13 ms, Ops ['request_sense']
  - t=302.675 s, k=1, Rest -0.18 ms, Ops ['request_sense']
  - t=304.914 s, k=1, Rest 1.59 ms, Ops ['request_sense']
  - t=307.154 s, k=1, Rest -1.4 ms, Ops ['request_sense']
  - t=309.392 s, k=1, Rest 0.07 ms, Ops ['request_sense']
  - t=311.631 s, k=1, Rest 0.88 ms, Ops ['request_sense']
  - t=313.87 s, k=1, Rest -1.17 ms, Ops ['request_sense']
  - t=316.108 s, k=1, Rest -0.0 ms, Ops ['request_sense']
  - t=318.347 s, k=1, Rest 0.01 ms, Ops ['request_sense']
  - t=320.585 s, k=1, Rest -0.03 ms, Ops ['request_sense']
  - t=322.824 s, k=1, Rest 0.85 ms, Ops ['request_sense']
  - t=325.064 s, k=1, Rest -0.82 ms, Ops ['request_sense']
  - t=327.302 s, k=1, Rest -0.02 ms, Ops ['request_sense']
  - t=329.541 s, k=1, Rest -0.02 ms, Ops ['request_sense']
  - t=331.779 s, k=1, Rest 0.06 ms, Ops ['request_sense']
  - t=334.018 s, k=1, Rest -0.02 ms, Ops ['request_sense']
  - t=336.257 s, k=1, Rest 0.07 ms, Ops ['request_sense']
  - t=357.685 s, k=1, Rest 0.03 ms, Ops ['read10']
  - t=382.312 s, k=1, Rest 0.07 ms, Ops ['read10']
  - t=748.833 s, k=1, Rest -0.02 ms, Ops ['read10']
  - t=859.493 s, k=1, Rest -0.03 ms, Ops ['read10']
  - t=886.358 s, k=1, Rest 0.02 ms, Ops ['read10']

## Plug-Zeitkette

| Mount (s) | set_address→config (ms) | config→GET_MAX_LUN (ms) | MAX_LUN→mount_done (ms) | mount_done→1. Read (ms) | set_address→1. Read (s) |
|---:|---:|---:|---:|---:|---:|
| 218.459 | 20.6 | None | None | None | 338.8848 |
| 938.549 | 22.4 | 997.0 | None | None | 1.6247 |
| 1060.653 | 22.5 | 996.8 | None | None | 1.6282 |

## Datei-Atome

| t Start (s) | Datei | Reihenfolge | CBWs (Block/Anzahl) | Start→Start (ms) | gap_us | Gesamt (ms) | vollständig |
|---:|---|---|---|---|---|---:|---|
| 557.323663 | TRACK1.MP3 | head_first | 0/240 240/70 | [122.7] | [5018] | 155.05 | True |
| 576.315911 | TRACK2.MP3 | tail_then_head | 240/70 0/240 | [33.83] | [3052] | 145.27 | True |
| 602.817034 | TRACK3.MP3 | head_first | 0/240 240/70 | [115.71] | [3946] | 146.48 | True |
| 940.15176 | TRACK1.MP3 | tail_then_head | 240/70 0/240 | [34.36] | [3483] | 146.91 | True |
| 942.324751 | TRACK2.MP3 | head_first | 0/240 240/70 | [122.26] | [4370] | 154.62 | True |
| 968.825705 | TRACK3.MP3 | head_first | 0/240 240/70 | [120.86] | [4170] | 153.05 | True |
| 1062.259123 | TRACK3.MP3 | tail_then_head | 240/70 0/240 | [33.91] | [3237] | 144.79 | True |
| 1078.950054 | TRACK1.MP3 | tail_then_head | 240/70 0/240 | [35.67] | [3233] | 152.35 | True |
| 1105.452319 | TRACK2.MP3 | head_first | 0/240 240/70 | [121.08] | [4167] | 153.18 | True |

- TRACK1.MP3 → TRACK2.MP3 (tail_then_head): Start→Start 18.992248 s, Atom-Ende→nächster Read 18.837194 s
- TRACK2.MP3 → TRACK3.MP3 (head_first): Start→Start 26.501123 s, Atom-Ende→nächster Read 26.355855 s
- TRACK3.MP3 → TRACK1.MP3 (tail_then_head): Start→Start 337.334726 s, Atom-Ende→nächster Read 337.188244 s
- TRACK1.MP3 → TRACK2.MP3 (head_first): Start→Start 2.172991 s, Atom-Ende→nächster Read 2.026077 s
- TRACK2.MP3 → TRACK3.MP3 (head_first): Start→Start 26.500954 s, Atom-Ende→nächster Read 26.346336 s
- TRACK3.MP3 → TRACK1.MP3 (tail_then_head): Start→Start 16.690931 s, Atom-Ende→nächster Read 16.546145 s
- TRACK1.MP3 → TRACK2.MP3 (head_first): Start→Start 26.502265 s, Atom-Ende→nächster Read 26.349913 s

## Lesefenster (Modell: 512 Blöcke, CBW ≤ 240)
- TRACK1.MP3 (310 Blöcke, 48 kbit/s): t=557.324 ab Block 0: [240, 70], Vorlauf 26.45 s; t=940.152 ab Block 0: [240, 70], Vorlauf -356.37 s; t=1078.95 ab Block 0: [240, 70], Vorlauf -495.17 s; Abstände [382.828, 138.798] s
- TRACK2.MP3 (310 Blöcke, 48 kbit/s): t=576.316 ab Block 0: [240, 70], Vorlauf 26.45 s; t=942.325 ab Block 0: [240, 70], Vorlauf -339.56 s; t=1105.452 ab Block 0: [240, 70], Vorlauf -502.68 s; Abstände [366.009, 163.127] s
- TRACK3.MP3 (310 Blöcke, 48 kbit/s): t=602.817 ab Block 0: [240, 70], Vorlauf 26.45 s; t=968.826 ab Block 0: [240, 70], Vorlauf -339.56 s; t=1062.259 ab Block 0: [240, 70], Vorlauf -432.99 s; Abstände [366.009, 93.433] s

## Chunk-Takt (4 KiB, fine_data=None)
- 30 READ10 mit ≥ 2 DATA-Records; Chunks pro CBW p50 9.0, max 30
- Start→Start zwischen Chunks: p50 3.944 ms, p95 4.821 ms, max 5.151 ms (n=411)
- Pausen > 10 ms: 0

Kein Feld-PASS: Das Tool beschreibt nur, was in dieser Session gemessen wurde.
