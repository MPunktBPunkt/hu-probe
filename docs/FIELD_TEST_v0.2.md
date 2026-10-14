# Feldtest v0.2.0 — erster HU-Lauf

## Flash (USB-UART, nicht OTG)

```bash
cd firmware
pio run -e huprobe-s3 -t upload
```

Artifact: `firmware/dist/hu-probe.0.2.0.esp32s3.bin`

OTG-Port der S3 geht zur **HU**; Flash über UART-Bridge.

## Im Auto

1. OTG in die HU stecken (USB Device).
2. Erwartung: HU erkennt Stick `HUPROBE` / Serial `HP0001`.
3. WiFi: gespeichertes Heimnetz, sonst SoftAP `ESP-HuProbe-Setup`.
4. Handy/Laptop: `http://<ip>/` oder `http://huprobe-<mac6>.local/`
5. Session startet **automatisch** (bmw / s0_plug_enum).
6. Status prüfen: `msc_plugged`, `msc_reads`, `read_rate_kb_s`.
7. Optional: **USB Soft-Cycle** / **Media Remount**.
8. Collector (wenn erreichbar): `hu-probe-collector` → Sessions unter `sessions/`.

## Was dieser Build kann

- Echte TinyUSB-MSC mit Demo-FAT (256 KiB)
- Soft-USB-Cycle (`tud_disconnect`/`connect`)
- Media-Remount (`mediaPresent`)
- Read-Events mit `t_us` / `gap_us` (Sample-Ring → JSONL)
- Auto-Session + WebUI

## Was er noch nicht kann (GATE-01 offen)

- Roh-CBW/CDB/CSW
- Große PiDrive-Slot-Geometrie / Silence-MP3-Slots
- Fingerprint/Diff

## Erfolgs-Kriterien heute

- [ ] HU zeigt USB-Medium
- [ ] `msc_reads` steigt nach Plug
- [ ] Soft-Cycle: HU enumeriert erneut
- [ ] Optional: Events im Collector
