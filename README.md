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

## Status (v0.1.1)

- Schema v2 eingefroren (monotonic µs, BOT CBW/DATA/CSW, USB-Events, capture_health)
- Firmware: ProbeClock (`esp_timer`), Drop/TCP-Accounting, Sim erzeugt BOT-Kette
- MSC/TinyUSB noch Stub — **GATE-01 offen**
- Collector unverändert „langweilig“
- BMW-Profil: Expectations als `FACT` aus PiDrive-Feldwissen gekennzeichnet

Als Nächstes: TinyUSB-MSC so verdrahten, dass GATE-01 geschlossen werden kann (kein Sim mehr).
