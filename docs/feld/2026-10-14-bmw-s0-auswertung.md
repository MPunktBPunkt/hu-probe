# Feldauswertung — BMW S0 (2026-10-14)

**Session:** `sessions/20261014-081637_bmw_s0_plug_enum/`  
**FW:** hu-probe `0.2.2` · ESP `192.168.178.89` · MAC `68:b6:b3:2b:c5:68`  
**Zeit:** 08:16:37–08:22:30 (+02:00) · ~6 min  
**Capture:** `dropped=0`, `tcp_disconnects=0`, `complete=true`, highwater=96

## Setup

- Medium: Demo-FAT **256 KiB** (3 kleine Dateien) — kein PiDrive-Slot-Streaming
- Szenario: `s0_plug_enum` + physische OTG-Replugs + Autoplay-Kreis im HU
- Stimulus/Observer: USB-MSC-Gadget + Read-Instrument (µs), TCP → Collector

## Kernergebnisse (FACT aus dieser Session)

| Größe | Wert |
|-------|------|
| Events | 474 |
| Reads | 240 (~717 KiB) |
| USB-/Phase-Events | 24 / 22 |
| Physische Plug/Unplug (Phase) | 11 / 11 |
| Read-Größe Mode | **4096** (132×; auch 512/2048/2560) |
| Gap-p50 (Pace, &lt;10 s) | **~3,7 ms** |
| File-Reads | 55 (~175 KiB), LBA 51–83 |
| Eager großer Body-Stream | **nein** (Medium zu klein) |

### Pro Replug-Fenster

Nach jedem Plug ähnliches Muster: BOOT → FAT → ROOT → wenige DATA-Reads (~22 Reads / ~64 KiB in typischen Fenstern). Ein Fenster (Index 7) mit deutlich mehr Aktivität (94 Reads) — vermutlich dichteres Enum/Scan nach Replug.

## Abgleich Golden `bmw_nbt_evo`

| Erwartung (PiDrive-Feld) | Diese Session |
|--------------------------|---------------|
| 4 KiB dominant | **ja** (FACT) |
| Gap idle ≈ 4 ms | **~3,7 ms p50** (FACT, nahe) |
| Eager-EOF / Prefetch-Strom | **nicht prüfbar** — 256 KiB-Demo |

Epistemik: Claims gelten nur für dieses Medium + FW 0.2.2. Keine Übertragung auf Mazda.

## Limitationen (nächster Bau)

1. Größere Slot-Geometrie (Silence-MP3 / 512 KiB+) für Autoplay-/Prefetch-Messung  
2. GATE-01: Roh-BOT CBW/CDB/CSW  
3. VBUS-Verlust beim OTG-Ziehen → kurze WLAN-Lücke (Events bis dahin waren geflusht)

## Artefakte

- `events.jsonl` — Rohstrom  
- `fingerprint.json` — verdichtetes Profil  
- `derived/reads.jsonl`, `derived/usb_phase.jsonl`
