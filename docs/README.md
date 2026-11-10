# hu-probe — Dokumentationsindex

**Stand:** 2026-11-10 · FW **0.3.37**  
**Einstieg Stand:** [`AKTUELL.md`](AKTUELL.md) · Root-[`README.md`](../README.md)  
Lange Alt-Reviews: nur unter [`feld/archiv/`](feld/archiv/README.md) — nicht Einstieg.

Zwei Instrumente, eine Sonde: **Storage** (USB-MSC) und **Control** (Connected App). Claims nicht vermischen.

---

## Kanonisch (aktuell)

| Dokument | Rolle |
|----------|--------|
| [KONZEPT.md](KONZEPT.md) | Ziel, Systembild, Planes |
| [SCHEMA.md](SCHEMA.md) | Event-/Session-Schema |
| [GATES.md](GATES.md) | HU-PROBE-GATE-01 (Messintegrität MSC) |
| [CONNECTED-APP.md](CONNECTED-APP.md) | Control-Plane Architektur + Modulplan |
| [GATES-Connected.md](GATES-Connected.md) | G1–G4 + Lab-Gates C1a…C5 |
| [SZENARIEN.md](SZENARIEN.md) | Experiment-Szenarien |
| [MEDIA_VARIATION.md](MEDIA_VARIATION.md) | SynthFat / Presets |
| [OFFLINE-CAPTURE.md](OFFLINE-CAPTURE.md) | Offline-Messlauf |
| [FIRMWARE-ABLAUF.md](FIRMWARE-ABLAUF.md) | Boot/Loop-Ablauf |
| [HARDWARE-YD-ESP32-S3.md](HARDWARE-YD-ESP32-S3.md) | Board |
| [KOMMENTARSTANDARD.md](KOMMENTARSTANDARD.md) | Code-Kommentare |
| [SEQ-STALL.md](SEQ-STALL.md) | Stall/BUSY-Sequenzen |

---

## Feld — jetzt lesen

Index: [`feld/README.md`](feld/README.md)

| Thema | Dokument |
|-------|----------|
| **Dashboard C1 / Connected** | [C1-STAND](feld/C1-STAND.md) |
| P0 I-303A Runsheet | [I303A-RUNSHEET](feld/I303A-RUNSHEET.md) |
| P1 Phone×HU Real | [FIELD-CONNECTED-REAL-01](feld/FIELD-CONNECTED-REAL-01.md) |
| Kampagne Identity | [C1-IDENTITY-KAMPAGNE](feld/C1-IDENTITY-KAMPAGNE.md) |
| MSC Größenleiter GO | [MSC-GROESSENLEITER-GO](feld/MSC-GROESSENLEITER-GO.md) |
| MSC Heimfahrt | [MSC-HEIMFAHRT](feld/MSC-HEIMFAHRT.md) |
| MSC Identity/Remount | [MSC-IDENTITY-REMOUNT](feld/archiv/2026-10-22-msc-kampagne/MSC-IDENTITY-REMOUNT.md) |

Ältere Tagesberichte → [`feld/archiv/`](feld/archiv/README.md).

---

## Ownership (Cross-Repo)

| Thema | Hier | PiDrive |
|-------|------|---------|
| G1–G4 / C1a–C5 / `ca_*` | **dieses Repo** | nur Link |
| Desk Connected/AAIdrive / Pfad C | Kurz + Link | [`CONNECTED-AAIDRIVE-STACK`](https://github.com/MPunktBPunkt/pidrive/blob/main/docs/fahrzeug/CONNECTED-AAIDRIVE-STACK.md) |
| Normative HU-MSC-Facts | Feld-Roh | [`HU-Technical-Facts`](https://github.com/MPunktBPunkt/pidrive/blob/main/docs/fahrzeug/HU-Technical-Facts.md) |
| MSC Produkt-Ampel | — | [`MSC-AKTUELL`](https://github.com/MPunktBPunkt/pidrive/blob/main/docs/betrieb/MSC-AKTUELL.md) |
