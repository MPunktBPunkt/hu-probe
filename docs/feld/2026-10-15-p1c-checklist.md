# P1c — Profil-Update Checklist (parallel zu PiDrive F0)

**Status: done 2026-10-17** — Upstream `profiles/bmw_nbt_evo.json` schema 3.

Pin / Audit: `technology-audit` Kampagne `hu-probe-bmw-s0s2-field-2026` · Evidence `EVID-HUPROBE-2026-10-15-*` · PiDrive Matrix `HU-PARAMETER-MATRIX-2026-10-16.md` / MessCapture A7.

## Eingetragen (`origin: hu_probe_field`)

| Key | Wert | class |
|-----|------|-------|
| `gap_us_p50_file_s0` / `_s2` | ~3739 / ~3783 | FACT Messgröße; Caveat: nicht Scheduler |
| `read_event_n_mode` | 4096 | FACT Event |
| `read10_blocks_mode_common` | [8, 240] | FACT CBW; getrennt von Event-n |
| `read10_max_blocks` | 240 | FACT (S2 + PiDrive MessCapture 16.10.) |
| `writes_expected` | false | FACT gesehen |
| `mode_sense6_per_capacity` | 2.0 | FACT HU |
| `bot_source` | tinyusb | FACT Instrument |

## Abgestuft / nicht hochgestuft

- `eager_eof` / `prefetch_lead_s` / `cache_hit_*` / `remount_full_reread` → `INFERENCE` / `HYPOTHESIS` / `pidrive_field` bis LBA+Marker (P0b) bzw. S1/S3/S4
- `enum_s_approx` / `tur_hz_approx` → `INFERENCE` (nicht durch Mount→INQUIRY / Session-TUR ersetzt)
- `queue_depth` → `INFERENCE` (Tag-QD, Wire UNKNOWN)
- Busy/stall-off/menu → `INFERENCE` pidrive_field (Ohren-Marker)
- Burst-39 als Konstante → **REJECTED** (im Profil `rejected`)
- Gap/4096 als HU-Scheduler-Pace → **REJECTED**

## Aus PiDrive nachgezogen (nicht hu-probe-FACT)

| Key | Wert | origin |
|-----|------|--------|
| `full_file_512kib_block_seq` | `[240,240,32,240,240,32]` | `pidrive_field` MessCapture 16.10. (A7) |
| `fresh_select_without_p_bytes` | 368640 | `pidrive_field` INFERENCE |
| `read10_timeout_s_hyp` | ≥20 | `pidrive_derived` HYPOTHESIS (pro CBW) |

## Reihenfolge (Rest)

1. ~~P1c Profil-Patch Upstream~~ **done**
2. P0b S2 Formal LBA+Marker
3. S3 → S1 ≥4 MiB

PiDrive-F0-Auftrag: `pidrive/docs/betrieb/artifacts-2026-10-15-feld/UEBERGABE-F0-NAECHSTE-KI-2026-10-15.md`
