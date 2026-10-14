# Gates

## HU-PROBE-GATE-01 — Measurement Integrity

**Status:** open (Stufe A ≥0.2.6; Stufe B BOT/SCSI ab **0.2.8**, Class-Driver-Hooks hardened **0.2.9**; Standard-Control-Sniff noch lückenhaft)

### Zeit & Sequenz

- [x] Monotonic µs-Timestamp (`t_us`) auf Events (Capture-Zeit ab 0.2.3)
- [x] Global per-Session `seq` (thread-safe); Lücken über Collector `recv_seq_gaps` + TCP peek→pop (0.2.4)
- [x] `gap_us` aus µs-Uhr (uint32)

### USB Enumeration / Control

- [x] USB attach/detach/suspend/resume sichtbar (Host + Soft)
- [~] Control-Transfer-Visibility: **MSC-Class** (GET_MAX_LUN / BOT_RESET) ab 0.2.8; Standard-Device-Requests noch nicht vollständig
- [~] Descriptor-Capture: **gadget_identity**-Events bei Attach (synthetisch) + ggf. Class-Control
- [ ] SET_ADDRESS / SET_CONFIGURATION in der Timeline (usbd-Hook ausstehend)
- [~] Endpoint / Direction auf USB-Events (teilweise)
- [ ] STALL / CLEAR_HALT / Reset-Recovery

### BOT / SCSI Evidenz

- [x] BOT CBW: `raw_hex` + Decode — **`bot_source=tinyusb`**, Host-Tag (0.2.8)
- [x] SCSI CDB: aus Host-CBW (READ10/WRITE10/TUR/…)
- [x] DATA: expected/actual/dur/gap
- [x] CSW: Host-CSW bytes (`synthesized=false`)
- [x] Inter-command gap (`gap_us`) aus Capture-Zeit
- [x] Echte CBW/CSW-Bytes aus TinyUSB Device-State (nicht Wire-Capture)
- [x] Class-Driver-Hook `--wrap=mscd_xfer_cb` (CBW/STATUS nach Real-Call) + `usb_ev=bot_need_reset` (0.2.9)
- [ ] Wire-Paketebene / NBT-Sense/Timing — bewusst UNKNOWN (Audit)

### Capture Health / Forensik

- [x] Event-Drop-Accounting (`dropped`, `queue_highwater`)
- [x] `capture_health` (+ `published`, `tcp_tx_*`, `msc_sample_overflow`)
- [x] TCP-Disconnect-Accounting; silent TX-loss behoben (0.2.4)
- [x] Firmware-Version + **Git-SHA / Build-ID** in Session (`fw_sha`, `fw_build_id`)
- [x] Probe-Identity (MAC, USB-Serial, Volume-Label) in Session
- [x] Rohdaten `events.jsonl` unverändert; Derived unter `derived/`
- [~] VBUS-Tail: last `seq`/`sid` in NVS bei capture_health (kein voller Event-Ring über Power-Loss)

### Freigabe

**Closed** erst wenn Stufe B vollständig (BOT ok ab 0.2.8/0.2.9 + Standard-Control-Sniff) + Lab-Dry-Run `dropped=0` / keine `recv_seq_gaps`.  
Stufe A (0.2.6) = Synth. Stufe B BOT (0.2.8) = TinyUSB-Host-CBW/CSW; **0.2.9** = Class-Driver-Hooks (`mscd_xfer_cb` / NEED_RESET). Wire-Paketebene bleibt UNKNOWN.

Verwandt: Technology-Audit `usb-bot-observation-2026-instrument-sources` · [Audit-Intake](feld/2026-10-14-audit-instrument-sources.md). GATE-01 ist die **Instrumenten**-Voraussetzung.
