# Session-Vergleich

| Session | Medium | Zeitbasis | TUR Median (s) | Mount=Ref | Lag nach Start (s) | Vorlauf vor Ende (s) | Auswahl (s) | tail_then_head |
|---|---|---|---:|---|---|---|---|---|
| 20261124-171130_bmw_fp_2026-11-21a_s1m_r1 | size_1m | capture_t_us | 2.2388 | 0/2 | [0.09, 0.1, 0.23] | [65.58, 65.57, 65.44] | [] | 1/5 |
| 20261124-172301_bmw_fp_2026-11-21a_s1m_r1 | size_1m | capture_t_us | 2.2388 | 0/1 | [] | [] | [] | 1/1 |

| Session | TUR-Periode (ms) | Stecken→1. Read (s) | Atom-Ende→nächster Read (s) | Media-Loss erkannt (s) |
|---|---:|---|---|---:|
| 20261124-171130_bmw_fp_2026-11-21a_s1m_r1 | 2238.56 | [1.8919, 14.8224] | [0.010332, 0.025391, 0.15787] | None |
| 20261124-172301_bmw_fp_2026-11-21a_s1m_r1 | 2238.81 | [1.6971] | [] | None |

- Streuung Lag nach Start: 0.14 s, Streuung Vorlauf vor Ende: 0.14 s.
  Bei unterschiedlichen Trackdauern zeigt die kleinere Streuung die wahrscheinlichere Prefetch-Regel (INFERRED).
