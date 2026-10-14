# Gates

## HU-PROBE-GATE-01 — Measurement Integrity

**Status:** open (Stufe A teilweise erfüllt in FW ≥0.2.6; Roh-Host-Control & echter TinyUSB-CBW noch offen)

### Zeit & Sequenz

- [x] Monotonic µs-Timestamp (`t_us`) auf Events (Capture-Zeit ab 0.2.3)
- [x] Global per-Session `seq` (thread-safe); Lücken über Collector `recv_seq_gaps` + TCP peek→pop (0.2.4)
- [x] `gap_us` aus µs-Uhr (uint32)

### USB Enumeration / Control

- [x] USB attach/detach/suspend/resume sichtbar (Host + Soft)
- [ ] Control-Transfer-Visibility (bmRequestType, …) — Host-Sniff fehlt
- [~] Descriptor-Capture: **gadget_identity**-Events bei Attach (synthetisch aus lokaler Identity, kein Host-Control)
- [ ] SET_ADDRESS / SET_CONFIGURATION in der Timeline
- [~] Endpoint / Direction auf USB-Events (teilweise)
- [ ] STALL / CLEAR_HALT / Reset-Recovery

### BOT / SCSI Evidenz

- [~] BOT CBW: `raw_hex` + Decode — **`bot_source=msc_synth`** aus MSC-Read (kein TinyUSB-Roh-CBW)
- [~] SCSI CDB: READ(10) synthetisiert aus LBA/Länge
- [x] DATA: expected/actual/dur/gap
- [~] CSW: synthetisiert (status=passed) parallel zum Read
- [x] Inter-command gap (`gap_us`) aus Capture-Zeit
- [ ] Echte CBW/CSW-Bytes vom Bus (TinyUSB-Hook) — Stufe B

### Capture Health / Forensik

- [x] Event-Drop-Accounting (`dropped`, `queue_highwater`)
- [x] `capture_health` (+ `published`, `tcp_tx_*`, `msc_sample_overflow`)
- [x] TCP-Disconnect-Accounting; silent TX-loss behoben (0.2.4)
- [x] Firmware-Version + **Git-SHA / Build-ID** in Session (`fw_sha`, `fw_build_id`)
- [x] Probe-Identity (MAC, USB-Serial, Volume-Label) in Session
- [x] Rohdaten `events.jsonl` unverändert; Derived unter `derived/`
- [~] VBUS-Tail: last `seq`/`sid` in NVS bei capture_health (kein voller Event-Ring über Power-Loss)

### Freigabe

**Closed** erst wenn Stufe B (echter Bus-CBW + Control-Sniff) + Lab-Dry-Run `dropped=0` / keine `recv_seq_gaps`.  
Stufe A (0.2.6) erlaubt instrumentierte Lab-/Feldläufe mit klar markierter Synthese (`synthesized: true`).

Verwandt: Technology-Audit HU-A\*. GATE-01 ist die **Instrumenten**-Voraussetzung.
