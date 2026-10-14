# Gates

## HU-PROBE-GATE-01 — Measurement Integrity

**Status:** open (Stufe A ≥0.2.6; Stufe B BOT/SCSI ab **0.2.8**; Control/STALL ab **0.2.13**; Lab Kill 1–3 **PASS** — lange BMW-Timeline **blockiert** durch OTG/WiFi-Death, siehe [wifi-otg-0.2.15](feld/2026-10-14-wifi-otg-0.2.15.md))

### Zeit & Sequenz

- [x] Monotonic µs-Timestamp (`t_us`) auf Events (Capture-Zeit ab 0.2.3)
- [x] Global per-Session `seq` (thread-safe); Lücken über Collector `recv_seq_gaps` + TCP peek→pop (0.2.4)
- [x] `gap_us` aus µs-Uhr (uint32)
- [x] Event-Stream `t_us` monoton über BOT+read (0.2.12: Capture-`tUs` im Pending-Ring + comfort read mit `bot_data`)

### USB Enumeration / Control

- [x] USB attach/detach/suspend/resume sichtbar (Host + Soft)
- [x] Control-Transfer-Visibility: **MSC-Class** (GET_MAX_LUN / BOT_RESET) ab 0.2.8; Standard via `dcd_set_address` + Mount/Umount (0.2.13)
- [~] Descriptor-Capture: **gadget_identity**-Events bei Attach (synthetisch) + ggf. Class-Control
- [x] SET_ADDRESS (`dcd_set_address` wrap) + Mount/Umount (`tud_mount_cb`/`umount_cb`, ≡ SET_CONFIGURATION n/0) — [control-stall intake](feld/2026-10-14-audit-control-stall-recovery.md)
- [~] Endpoint / Direction auf USB-Events (teilweise; `ep_stall`/`ep_clear_halt` mit ep_addr ab 0.2.13)
- [x] STALL / CLEAR_HALT / Reset-Recovery — `bot_need_reset` + `ep_stall`/`ep_clear_halt` + `msc_bot_reset`; Clear-Halt heilt NEED_RESET **nicht** (note `need_reset_active_not_healed`); Mount clear BOT-Epoch

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

**Closed** erst wenn Stufe B vollständig (BOT ok ab 0.2.8/0.2.9 + Standard-Control/STALL **implementiert und Lab-Dry-Run**) + Event-`t_us` monoton + `dropped=0` / keine `recv_seq_gaps` + formale Freigabe.  
Lab 0.2.13: Kill 1–3 PASS, Gap-p50 ~4,0–4,4 ms, `publishUs`-Clamp Smoke `t_us` OK — siehe [lab-debian-s0-0.2.13](feld/2026-10-14-lab-debian-s0-0.2.13.md). Wire-Paketebene bleibt UNKNOWN.

Verwandt: [BOT instrument-sources](feld/2026-10-14-audit-instrument-sources.md) · [Control/STALL recovery](feld/2026-10-14-audit-control-stall-recovery.md). GATE-01 ist die **Instrumenten**-Voraussetzung.
