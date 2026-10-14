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

## Status (v0.2.9) — GATE-01B Class-Driver-Hooks

- BMW-S0-Baseline (FW 0.2.2): 4 KiB Mode, Gap-p50 3724 µs, 6/8 Fenster = 22-Read — **durch 0.2.9-Feld ersetzen**
- FW **0.2.8**: echte CBW/CDB/CSW aus TinyUSB (`bot_source=tinyusb`) + MSC-Class-Control
- FW **0.2.9**: `--wrap=mscd_xfer_cb` + `usb_ev=bot_need_reset` ([Audit-Intake](docs/feld/2026-10-14-audit-instrument-sources.md))
- Lab vs Auto: `lab_debian` vs `bmw` · [Maßnahmen 01B](docs/feld/2026-10-14-massnahmen-01b.md)
- GATE-01 **offen** (Standard-Control SET_ADDRESS/… noch lückenhaft; Wire-Paketebene UNKNOWN)
- Auto: `./tools/ota_car_esp.sh` → Bin `firmware/dist/hu-probe.0.2.9.esp32s3.bin`

```bash
cd firmware && pio run -e huprobe-s3 -t upload
# oder: ./tools/ota_car_esp.sh
```
