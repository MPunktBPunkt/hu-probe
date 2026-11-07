# hu-probe

Messplattform für Head-Unit-USB-Host-Verhalten (BMW NBT Evo als Golden Probe, Mazda CX-30 als Discovery-Ziel).

Die Sonde läuft auf einem **ESP32-S3** als USB-Gadget (MSC und/oder AOA-Observe), meldet sich am **esp-hub** (`fwType: hu-probe`), streamt High-Rate-Events per **TCP/JSONL** an einen lokalen Collector und bietet eine schlanke On-Device-WebUI.

**FW aktuell:** **0.3.36** (`-DFW_VERSION` in `firmware/platformio.ini`).

## Repo-Layout

| Pfad | Inhalt |
|------|--------|
| `docs/` | Konzept, Gates, Schema — **[Index](docs/README.md)** |
| `firmware/` | PlatformIO ESP32-S3 Firmware |
| `collector/` | TCP-Empfänger, Session-Store, Web-UI |
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

OTA-Artifact-Name: `hu-probe.<semver>.esp32s3.bin` → Hub-`firmware/`. SoftAP-Setup: `ESP-HuProbe-Setup`.

## Dokumentation (Kurz)

**Aktueller Stand:** **[docs/AKTUELL.md](docs/AKTUELL.md)** · Index: [docs/README.md](docs/README.md).

| Plane | Einstieg |
|-------|----------|
| **Jetzt** | [AKTUELL](docs/AKTUELL.md) · [C1-STAND](docs/feld/C1-STAND.md) |
| Storage (MSC) | [KONZEPT](docs/KONZEPT.md) · [GATES](docs/GATES.md) · [SCHEMA](docs/SCHEMA.md) |
| Control (Connected) | [CONNECTED-APP](docs/CONNECTED-APP.md) · [GATES-Connected](docs/GATES-Connected.md) |

Sibling: `pidrive` (Produkt/HU-Facts) · `usb-analyzer` · `technology-audit`.

## Status (0.3.36)

- **MSC:** Golden Path + SynthFat/Media-Variation; GATE-01 Lab-Kern PASS, Wire-Paketebene UNKNOWN
- **Connected:** C1a/C1a.5/C1b-0 PASS; C1b-P0 PARTIAL am NBT (P0-E ABSENT); nächster Gatekeeper **I-303A** (G1), dann FIELD-CONNECTED-REAL-01
- **Nicht claimen:** AOA erkannt ≠ BCL ≠ SAS ≠ RHMI-Menü
