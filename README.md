# hu-probe

Messplattform für Head-Unit-USB-Host-Verhalten (BMW NBT Evo als Golden Probe, Mazda CX-30 als Discovery-Ziel).

Die Sonde läuft auf einem **ESP32-S3** als USB-MSC-Gadget mit Timing-Instrumentierung, meldet sich am **esp-hub** (`fwType: hu-probe`), streamt High-Rate-Events per **TCP/JSONL** an einen lokalen Collector und bietet eine schlanke On-Device-WebUI.

## Repo-Layout

| Pfad | Inhalt |
|------|--------|
| `docs/` | Konzept, Event-/Session-Schema |
| `firmware/` | PlatformIO ESP32-S3 Firmware |
| `collector/` | TCP-Empfänger, Session-Store, einfache Web-UI |
| `profiles/` | Golden `bmw_nbt_evo`, Stub `mazda_cx30` |
| `sessions/` | Aufgezeichnete Messungen (gitignored) |

## Schnellstart

### Collector

```bash
cd collector
python3 -m venv .venv && source .venv/bin/activate
pip install -e .
hu-probe-collector --host 0.0.0.0 --port 9100 --web-port 8088
```

Sessions landen unter `sessions/YYYYMMDD-HHMMSS_<vehicle>_<scenario>/`.

### Firmware

```bash
cd firmware
pio run -e huprobe-s3
pio run -e huprobe-s3 -t upload
```

OTA-Artifact-Name: `hu-probe.<semver>.esp32s3.bin` → Hub-`firmware/`.

SoftAP-Setup: `ESP-HuProbe-Setup`.

## Dokumentation

- [Konzept](docs/KONZEPT.md)
- [Schema v2](docs/SCHEMA.md) — `t_us`, USB/BOT/SCSI, `capture_health`
- [GATE-01 Measurement Integrity](docs/GATES.md)
- [Szenarien](docs/SZENARIEN.md)
- [Medienvariation](docs/MEDIA_VARIATION.md)
- [Feldauswertung BMW S0](docs/feld/2026-10-14-bmw-s0-auswertung.md) · [Review-Triage](docs/feld/2026-10-14-review-triage.md)

## Status (v0.2.4) — Capture-Härtung + 512 KiB-Medium

- BMW-S0-Baseline (FW 0.2.2): 4 KiB Mode, Gap-p50 3724 µs, 6/8 Fenster = 22-Read
- FW **0.2.3**: Capture-`t_us` · **0.2.4**: TCP peek→TX→pop, thread-safe Seq, `msc_sample_overflow` in capture_health, **512 KiB** Silence-MP3-FAT
- Tools: `validate_session`, `fingerprint_session`, `correlate_windows`, `build_demo_fat`
- OTA `.88`/`.89` ausstehend (Geräte offline) · GATE-01 offen · Mazda-Profil null

```bash
cd firmware && pio run -e huprobe-s3 -t upload
# oder: ./tools/ota_car_esp.sh
```
