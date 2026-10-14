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

## Status (v0.2.13) — Control/STALL-Epochs

- FW **0.2.13**: `set_address` / `mount`·`umount`·`set_configuration` / `ep_stall`·`ep_clear_halt` / `msc_bot_reset`; Clear-Halt heilt NEED_RESET nicht; Mount clear BOT-Epoch
- FW **0.2.12**: Capture-`tUs` auf deferred BOT; WiFi-Restart ~25 s
- Feld 0.2.11: [Auswertung](docs/feld/2026-10-14-bmw-s0-0.2.11-auswertung.md) · [Abend-Triage](docs/feld/2026-10-14-review-abend-triage.md)
- Audit: [Control/STALL](docs/feld/2026-10-14-audit-control-stall-recovery.md) · [Maßnahmen 01C](docs/feld/2026-10-14-massnahmen-01c.md)
- GATE-01 **offen** (Lab-Dry-Run Kill-Kriterien; Wire UNKNOWN)
- Auto/Lab: Bin `firmware/dist/hu-probe.0.2.13.esp32s3.bin`

### Status-LED (ESP32-S3 RGB)

| Farbe | Bedeutung |
|-------|-----------|
| Gelb blink | WiFi-Portal / nicht verbunden |
| Blau dim | WiFi OK, USB nicht gesteckt — wartet auf HU |
| Cyan | USB gesteckt, Host da, wenig/keine Reads |
| Grün | HU liest aktiv (Messung läuft) |
| Orange blink | USB gesteckt, aber TCP zum Collector down |
| Magenta blink | TinyUSB `NEED_RESET` (BOT stuck) |
| Rot blink | Capture unhealthy (Drops / Sample-Overflow) |

```bash
cd firmware && pio run -e huprobe-s3 -t upload
# oder: ./tools/ota_car_esp.sh
```
