# P1c — Profil-Update Checklist (parallel zu PiDrive F0)

**Nicht Hauptpfad.** PiDrive macht F0 am NBT. Hier nur Epistemik im Profil bereinigen.

Pin / Audit: `technology-audit` Kampagne `hu-probe-bmw-s0s2-field-2026` · Evidence `EVID-HUPROBE-2026-10-15-*` · Upstream noch `bmw_nbt_evo.json` gemischt PiDrive-FACT.

## Darf rein (`origin: hu_probe_field`)

| Key | Wert | class |
|-----|------|-------|
| `gap_us_p50_file_s0` / `_s2` | ~3739 / ~3783 | FACT Messgröße; Caveat: nicht Scheduler |
| `read_event_n_mode` | 4096 | FACT Event |
| `read10_blocks_mode_common` | [8, 240, …] | FACT CBW; getrennt von Event-n |
| `writes_expected` | false | FACT gesehen |
| `mode_sense6_per_capacity` | 2.0 | FACT HU |
| `bot_source` | tinyusb | FACT Instrument |

## Nicht hochstufen

- `eager_eof` / `prefetch_lead_s` / `cache_hit_*` → bleiben hypothesis / pidrive_field bis LBA+Marker (P0b)
- Burst-39 als Konstante → **REJECTED**
- Gap/4096 als HU-Scheduler-Pace

## Reihenfolge

1. P1c Profil-Patch Upstream  
2. P0b S2 Formal LBA+Marker  
3. S3 → S1 ≥4 MiB  

PiDrive-F0-Auftrag: `pidrive/docs/betrieb/artifacts-2026-10-15-feld/UEBERGABE-F0-NAECHSTE-KI-2026-10-15.md`
