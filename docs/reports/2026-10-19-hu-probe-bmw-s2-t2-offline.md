# hu-probe BMW S2 – T2 Offline PASS

**Feldabschluss 2026-10-19**

| | |
|--|--|
| Repository | `MPunktBPunkt/hu-probe` |
| FW | 0.2.30 |
| BMW | NBT Evo |
| Status | **T2 offline belegt** — Track-Geometrie T1/T2/T3 komplett |
| Feldbericht | [`docs/feld/2026-10-19-bmw-s2-t2-offline.md`](../feld/2026-10-19-bmw-s2-t2-offline.md) |
| Checkliste | [`docs/feld/2026-10-18-t2-hold-checklist.md`](../feld/2026-10-18-t2-hold-checklist.md) |
| Rohsession | `sessions/20261019-075344_bmw_s2_prefetch/` |

---

## Ergebnis

```
CBW:     (318,240) + (558,69)
Covered: 318 … 626   (= 309 LBA)
Bytes:   158208
Morph:   head_then_tail
Case:    A  →  T2 offline FACT
```

Session `20261019-075344_bmw_s2_prefetch`: `transport=ram_offline`, `integrity_pass=true`, `offline_records=123`, `overflow=0`, `upload=delivered`, `end_reason=auto_media_hide`.

## Track-Karte (geschlossen)

```
                 309 LBA
        <------------------------>

T1     |========================|     9 … 317   FACT live + offline
T2                              |========================|  318 … 626  FACT live + offline
T3                                                        |========================|  627 … 935  FACT live + offline
```

## Offen

- GATE-01 Prefetch-Lead (Marker + B7)
- S3 Cache-Hit
- NEED_RESET / Magenta-Häufigkeit operativ

## Auftrag erledigt

P0 der Checkliste „Direkt T2 Hold“: **done**.
