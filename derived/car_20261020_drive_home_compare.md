# Session-Vergleich

| Session | Medium | Zeitbasis | TUR Median (s) | Mount=Ref | Lag nach Start (s) | Vorlauf vor Ende (s) | Auswahl (s) | tail_then_head |
|---|---|---|---:|---|---|---|---|---|
| 20261020-200324_bmw_drive_home | drive_mix | capture_t_us | None | 0/1 | [] | [] | [] | 0/2 |
| 20261020-200018_bmw_drive_home | id3_cover | capture_t_us | None | 0/1 | [0.09, 0.09] | [26.43, 26.43] | [] | 0/3 |
| 12065_bmw_drive_home | drive_mix | capture_t_us | None | 0/1 | [] | [] | [] | 0/2 |
| 19147_bmw_drive_home | id3_cover | capture_t_us | None | 0/1 | [0.09, 0.09] | [26.43, 26.43] | [] | 0/3 |

| Session | TUR-Periode (ms) | Stecken→1. Read (s) | Atom-Ende→nächster Read (s) | Media-Loss erkannt (s) |
|---|---:|---|---|---:|
| 20261020-200324_bmw_drive_home | None | [1.677] | [] | None |
| 20261020-200018_bmw_drive_home | None | [1.5881] | [0.00737, 0.007337] | None |
| 12065_bmw_drive_home | None | [1.677] | [] | None |
| 19147_bmw_drive_home | None | [1.5881] | [0.00737, 0.007337] | None |

- Streuung Lag nach Start: 0.0 s, Streuung Vorlauf vor Ende: 0.0 s.
  Bei unterschiedlichen Trackdauern zeigt die kleinere Streuung die wahrscheinlichere Prefetch-Regel (INFERRED).
