# hu-probe

Messplattform für Head-Unit-USB-Host-Verhalten (BMW NBT Evo als Golden Probe, Mazda CX-30 als Discovery-Ziel).

Die Sonde läuft auf einem **ESP32-S3** als USB-Gadget (MSC und/oder AOA-Observe), meldet sich am **esp-hub** (`fwType: hu-probe`), streamt High-Rate-Events per **TCP/JSONL** an einen lokalen Collector und bietet eine schlanke On-Device-WebUI.

**FW im Baum:** **0.3.41** (`-DFW_VERSION` in `firmware/platformio.ini`). Nicht kompiliert, nicht auf Hardware geprüft. Letzter Lauf im Fahrzeug: **0.3.38** (2026-11-11).

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

OTA-Artifact-Name (Hub): `hu-probe.<semver>.esp32s3.bin` → Hub-`firmware/`. SoftAP-Setup: `ESP-HuProbe-Setup`.

GitHub-Update (ab 0.3.0, im Fahrzeug ungetestet): Tag `v<FW_VERSION>` lässt `firmware.yml` das Manifest `ota/manifest.json` schreiben; installiert wird erst bei `enabled: true`. Ablauf und Freigabe: [docs/FIRMWARE-ABLAUF.md](docs/FIRMWARE-ABLAUF.md).

## Pendelfahrt (ab 0.3.41)

Messaufträge stehen in [`field/plan.json`](field/plan.json) (nur lesend, ohne Token). Mit `online_only` ist der Handy-Hotspot der Schalter: Hotspot an und RST bei laufender HU startet die nächste Messung, Hotspot aus misst nicht. Fahrten bleiben im Flash und gehen im Heimnetz an den Collector. Eine neue `plan`-ID setzt den Planzähler auf 0; dieselbe ID zählt weiter (Messung 4, 5, 6 …). GitHub liefert die Raw-Datei bis zu etwa 5 min aus dem Cache. Ein Job kann eine READ10-Bremse mitbringen (`"stall"`, wie `/api/stall`), z. B. Live-Tempo über ganze Dateien als Messung für den PiDrive-Live-Slot ([docs/SEQ-STALL.md](docs/SEQ-STALL.md)).

LED im Auto: lila (weißer Blitz alle 2 s) oder grün = Messung läuft, RST würde das Ende abschneiden. Gelb oder orange blinkend = Fahrt ist im Flash, RST ist unbedenklich. Vor dem RST einer Folgemessung BOOT lang und den Farbwechsel abwarten (bis ~25 s).

Bedienung und Abnahme: [docs/ANWEISUNG-0.3.41.md](docs/ANWEISUNG-0.3.41.md) · Technik: [docs/FIRMWARE-ABLAUF.md](docs/FIRMWARE-ABLAUF.md) (Abschnitt Feldplan).

## Dokumentation (Kurz)

**Aktueller Stand:** **[docs/AKTUELL.md](docs/AKTUELL.md)** · Index: [docs/README.md](docs/README.md).

| Plane | Einstieg |
|-------|----------|
| **Jetzt** | [AKTUELL](docs/AKTUELL.md) · [C1-STAND](docs/feld/C1-STAND.md) |
| Storage (MSC) | [KONZEPT](docs/KONZEPT.md) · [GATES](docs/GATES.md) · [SCHEMA](docs/SCHEMA.md) |
| Control (Connected) | [CONNECTED-APP](docs/CONNECTED-APP.md) · [GATES-Connected](docs/GATES-Connected.md) |

Sibling: `pidrive` (Produkt/HU-Facts) · `usb-analyzer` · `technology-audit`.

## Status

Feldstand bleibt 0.3.38. Im Baum, aber nicht kompiliert und nicht im Fahrzeug: 0.3.39 (RAM-Replay-Fix), 0.3.40 (DCD-Hook, AOA-Epochs), 0.3.41 (Feldplan). Kein PASS dafür.

- **MSC:** Golden Path + SynthFat/Media-Variation; USB-`iProduct` = `hu-probe` (0.3.37); GATE-01 Lab-Kern PASS, Wire-Paketebene UNKNOWN
- **Connected:** I-303A Feld + Reval erledigt; nächster Engpass **FIELD-CONNECTED-REAL-01** (P1)
- **Nicht claimen:** AOA erkannt ≠ BCL ≠ SAS ≠ RHMI-Menü
