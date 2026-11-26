# Session-Vergleich

| Session | Medium | Zeitbasis | TUR Median (s) | Mount=Ref | Lag nach Start (s) | Vorlauf vor Ende (s) | Auswahl (s) | tail_then_head |
|---|---|---|---:|---|---|---|---|---|
| 20261125-213213_bmw_fp_2026-11-21a_s1m_r1 | size_1m | capture_t_us | 2.2388 | 0/1 | [0.09, 0.09] | [65.58, 65.58] | [] | 0/3 |
| 20261124-171130_bmw_fp_2026-11-21a_s1m_r1 | size_1m | capture_t_us | 2.2388 | 0/2 | [0.09, 0.1, 0.23] | [65.58, 65.57, 65.44] | [] | 1/5 |
| 20261117-172704_bmw_fp_2026-11-17a_s158_r1 | size_158 | capture_t_us | 2.2388 | 0/1 | [0.08, 0.09, 0.3, 2.25, 9.93] | [9.84, 9.84, 9.62, 7.67, -0.0] | [] | 0/6 |
| 12917_bmw_drive_home | size_158 | capture_t_us | 2.2388 | 0/1 | [10.33, 4.24] | [-0.4, 5.68] | [] | 2/3 |
| 20261125-211129_bmw_drive_home | size_158 | capture_t_us | 2.2388 | 0/1 | [0.08, 0.09] | [9.84, 9.84] | [] | 0/3 |
| 20261126-173225_bmw_drive_home | size_158 | capture_t_us | 2.2388 | 0/1 | [0.09, 0.09, 0.23, 2.25, 9.92] | [9.84, 9.84, 9.69, 7.67, 0.01] | [] | 0/6 |

| Session | TUR-Periode (ms) | Stecken→1. Read (s) | Atom-Ende→nächster Read (s) | Media-Loss erkannt (s) |
|---|---:|---|---|---:|
| 20261125-213213_bmw_fp_2026-11-21a_s1m_r1 | 2238.79 | [1.7628] | [0.011028, 0.011908] | None |
| 20261124-171130_bmw_fp_2026-11-21a_s1m_r1 | 2238.56 | [1.8919, 14.8224] | [0.010332, 0.025391, 0.15787] | None |
| 20261117-172704_bmw_fp_2026-11-17a_s158_r1 | 2238.78 | [1.6804] | [0.00897, 0.009876, 0.227766, 2.187884, 9.863938] | None |
| 12917_bmw_drive_home | 2238.79 | [1.9134] | [10.18159, 4.093512] | None |
| 20261125-211129_bmw_drive_home | 2238.79 | [1.531] | [0.008922, 0.009807] | None |
| 20261126-173225_bmw_drive_home | 2238.78 | [1.5831] | [0.009826, 0.008281, 0.160484, 2.185406, 9.851551] | None |

- Streuung Lag nach Start: 10.25 s, Streuung Vorlauf vor Ende: 65.98 s.
  Bei unterschiedlichen Trackdauern zeigt die kleinere Streuung die wahrscheinlichere Prefetch-Regel (INFERRED).
