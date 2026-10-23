# hu-probe self-recalculation dataset — 2026-10-23

Source: `MPunktBPunkt/hu-probe` @ `62cb4f88428d2dc8da235e2adf88b0d07e7647f8`  
Built from: `local docs/feld/artifacts/20261023-identity-remount`

This package contains the raw archived sessions plus normalized JSONL
views so another AI/analyst can independently recalculate the findings.

## Start here

1. Read `reports/2026-10-23-gesamtbericht-identity-remount-multireview.md`
2. Read `reports/2026-10-23-wire-nachtrag-074319-074455.md` (32+128+6 vs tail_then_head)
3. Use `manifest.json` epistemic_warnings
4. Recalculate from primary event streams (not derived/)

## Primary evidence

- `sessions/*/events.jsonl` — original event streams, preserved unchanged
- `sessions/*/session.json` — original session metadata
- `events_normalized.jsonl` — all events from all five sessions
- `read10.jsonl` — every READ10 CBW (`scsi.opcode == read10`)
- `media_layout.jsonl` — media/file layout events
- `lifecycle.jsonl` — session/phase/USB/status/capture-health/marker
- `integrity_checks.json` — packaging-time structural checks

## Reference layer

`derived_reference.json` and archived `derived/analysis.*` are reference
calculations only. They must not replace recalculation from primary events.

## Session warnings

| Session | Role |
|---------|------|
| `074319` | RST-confounded Serial attempt — **not** Serial causality proof |
| `074455` | **core** Label cycle |
| `074749` | **core** Serial+Label+Vol cycle |
| `074908` | **core** Volume-ID cycle |
| `072929` | offline I0 — contextual only |

## Known synthetic medium (cache_a family)

- sector_count: 32768
- TRACK1: LBA 289, 310 blocks, 158473 bytes
- TRACK2: LBA 599, 310 blocks, 158473 bytes
- TRACK3: LBA 909, 310 blocks, 158473 bytes
- expected small-file atom: 240-block head + 70-block tail
- common cold-start scan pattern: 32 + 128 blocks per file

## Recommended recalculations

1. Parse every READ10 from the primary events
2. Map every READ10 range to file extents
3. Detect 32+128 head scans
4. Detect complete 310-block atoms
5. Classify head-first versus tail-first order
6. Recalculate gaps from `t_us` / `capture_t_us`
7. Recalculate TUR intervals from TUR CBWs
8. Compare behavior before/after USB cycle (`ev=usb` / `phase`)
9. Keep wire facts separate from operator Resume observations

## Rebuild

```bash
# from repo root (local artifacts, recommended)
python3 tools/build_identity_remount_recalc_2026_10_23.py

# or fetch from GitHub raw at pinned commit
python3 tools/build_identity_remount_recalc_2026_10_23.py --from-github
```
