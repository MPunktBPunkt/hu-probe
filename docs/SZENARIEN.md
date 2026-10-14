# Szenario-Matrix

| ID | Zweck | BMW (Golden) | Mazda |
|----|-------|--------------|-------|
| `s0_plug_enum` | Baseline Enumeration | ~1 s / 4 KiB / TUR | messen |
| `s1_eager_eof` | Eager-Read bis EOF | 512 KiB Slot voll | messen |
| `s2_prefetch` | Prefetch / Auto-Next | Prefetch sichtbar | messen |
| `s3_cache_hit` | Replay ohne Reads | keine Reads | messen |
| `s4_remount` | Replug / Soft-RST | Full reread + P | messen |
| `s5_busy_budget` | Busy vs Stall-off | ~65 s vs ~6 s | Abort messen |
| `s6_identity` | Serial/Label Matrix | Cache/P-Verhalten | messen |
| `s7_lifecycle` | Sleep / Ignition | Marker + Phasen | messen |

Szenario-IDs erscheinen in `session.json`, Heartbeat-`ios` und Event-Feldern.
