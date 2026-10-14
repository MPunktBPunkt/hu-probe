# hu-probe — Konzept

## Ziel

Eigene Head Units (BMW NBT Evo, Mazda CX-30) über eine gemeinsame USB-Messsonde vergleichen. Die Sonde misst **Host-Verhalten am MSC-Interface** (Enumeration, Read-Timing, Prefetch/Cache-Signaturen, Busy-Toleranz, Lifecycle), nicht CAN/MOST/Display-Hardware.

BMW NBT Evo dient als **Golden Probe** (stark belegte Muster aus technology-audit / pidrive). Mazda startet als leeres Discovery-Profil — keine stillen NBT-Annahmen.

## Systembild

```
Fahrzeug-HU ──USB MSC──► ESP32-S3 hu-probe
                            │ HubClient ──► esp-hub :8093 (register/OTA)
                            │ WebUI :80
                            └ TCP JSONL ──► Collector :9100
                                              └ sessions/ + Fingerprint/Diff
```

## Rollen

| Rolle | Inhalt |
|-------|--------|
| Stimulus | Virtuelles FAT-Medium mit festen Slot-Dateien |
| Observer | SCSI/BOT mit Wanduhr (INQUIRY, CAPACITY, TUR, READ10, …) |
| Actor | Szenarien: Stall/Busy, Soft-RST, Identity-Rotation, Marker |
| Reference | Profil `bmw_nbt_evo` |
| Discovery | Profil `mazda_cx30` (wachsend) |

## Hub-Vertrag

- Heartbeat ~30 s: `fwType: "hu-probe"`, `chipModel: "ESP32-S3"`
- `ios` nur Summary: `probe_state`, `session_id`, `read_rate_kb_s`, `tur_hz`, `scenario`, `vehicle`
- High-Rate-Daten **nicht** über Hub — nur TCP/Collector bzw. lokale API
- OTA: `hu-probe.<semver>.esp32s3.bin`
- mDNS: `huprobe-<mac6>.local` (lokale UI, nicht Hub-Discovery)

## Messkanäle (aus BMW-Tests)

| Muster | Kanal |
|--------|-------|
| Enum ~1 s, TUR ~1/s, keine Writes, QD=1 | `scsi`, `tur_hz`, `write_count` |
| Fast nur 4 KiB, Gap ~4 ms | `xfer_hist`, `gap_ms` |
| Eager-EOF, Prefetch ~15 s | `slot_progress`, `prefetch_lead_s` |
| Cache-Hit / Cold-Burst | `kind`, Host-Delta |
| Resume out-of-order um P | `resume_pattern` |
| Remount full re-read | `phase=remount` |
| Busy ~65 s vs Stall-off ~6 s | Szenario `busy_budget` + Marker |
| Lifecycle Sleep/Ignition | Operator-Marker + VBUS/Phasen |

Details: [SCHEMA.md](SCHEMA.md) (v2), [SZENARIEN.md](SZENARIEN.md), [GATES.md](GATES.md), [MEDIA_VARIATION.md](MEDIA_VARIATION.md).

## Measurement Integrity

Vor Feldbetrieb: **[HU-PROBE-GATE-01](GATES.md)** — µs-Timestamps, USB/BOT/SCSI-Roh-Evidenz, Drop-/TCP-Accounting. Solange GATE-01 offen ist, ist die Firmware ein vorbereitetes Instrument, kein audit-fähiger Messpfad.

Collector bleibt „langweilig“ (Transport + Session-Store). Intelligenz in `derived/` / Fingerprint / Diff.
