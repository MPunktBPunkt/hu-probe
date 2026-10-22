# Session-Vergleich

| Session | Medium | Zeitbasis | TUR Median (s) | Mount=Ref | Lag nach Start (s) | Vorlauf vor Ende (s) | Auswahl (s) | tail_then_head |
|---|---|---|---:|---|---|---|---|---|
| 20261020-075616_bmw_free | demo_v1 | capture_t_us | 2.2388 | 0/0 | [26.47] | [-0.01] | [] | 0/2 |
| 20261020-075935_bmw_free | demo_v1 | capture_t_us | 2.2388 | 0/1 | [1.09, 26.47] | [25.38, -0.01] | [] | 0/3 |
| 20261020-080508_bmw_free | demo_v1 | capture_t_us | 2.2388 | 1/1 | [1.58, 26.47] | [24.89, -0.01] | [] | 1/3 |

| Session | TUR-Periode (ms) | Stecken→1. Read (s) | Atom-Ende→nächster Read (s) | Media-Loss erkannt (s) |
|---|---:|---|---|---:|
| 20261020-075616_bmw_free | 2238.78 | [] | [26.325263] | 1.2385 |
| 20261020-075935_bmw_free | 2239.22 | [1.513] | [0.93617, 26.325097] | 0.3936 |
| 20261020-080508_bmw_free | 2238.78 | [1.5185] | [1.427388, 26.323552] | 2.1822 |

- Streuung Lag nach Start: 25.38 s, Streuung Vorlauf vor Ende: 25.39 s.
  Bei unterschiedlichen Trackdauern zeigt die kleinere Streuung die wahrscheinlichere Prefetch-Regel (INFERRED).
