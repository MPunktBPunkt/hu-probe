# Event- und Session-Schema

Version: **2** (Feld `v` in jedem Event)

Breaking vs v1: Primärzeit ist `t_us` (monotonic µs). `t_ms` bleibt optionales Komfortfeld (`t_us / 1000`). SCSI-Kurzform `ev=scsi` mit nur `op` ist **deprecated**; kanonisch sind `usb`, `bot_cbw`, `bot_data`, `bot_csw` (+ optionales `scsi_decoded`).

## Session-Ordner

```
sessions/YYYYMMDD-HHMMSS_<vehicle>_<scenario>/
  session.json      # Meta + probe identity + capture summary
  events.jsonl      # Rohstrom (Source of Truth) — nie überschreiben
  markers.jsonl     # Operator / Ohr / Ignition
  status.jsonl      # periodische Aggregate inkl. capture_health
  derived/          # nach Ingest: Parquet, Fingerprint-Inputs
  fingerprint.json  # berechnetes Profil dieser Session
  diff.json         # vs Golden / vs Vergleichsprofil
```

### session.json

```json
{
  "schema": 2,
  "session_id": "20261014-064500_bmw_s0_plug_enum",
  "started_at": "2026-10-14T06:45:00+02:00",
  "ended_at": null,
  "vehicle": "bmw",
  "profile_id": "bmw_nbt_evo",
  "usb_host_kind": "bmw_nbt_evo",
  "usb_host_role": "vehicle_hu",
  "fw_sha": "a1b2c3d4e5f6",
  "fw_build_id": "a1b2c3d4e5f6-20261014T120000Z",
  "probe_identity": {
    "mac": "68:b6:b3:2b:c5:68",
    "usb_serial": "HP0001",
    "volume_label": "HUPROBE",
    "board": "ESP32-S3",
    "hw_type": "esp32s3"
  },
  "scenario": "s0_plug_enum",
  "fw_version": "0.1.1",
  "fw_type": "hu-probe",
  "fw_sha": null,
  "mac": "aa:bb:cc:dd:ee:ff",
  "chip": "ESP32-S3",
  "probe_identity": {
    "usb_serial": "HUPROBE0001",
    "volume_label": "HUPROBE"
  },
    "clock": {
      "domain": "probe_monotonic_us",
      "source": "esp_timer_get_time()",
      "note": "Not wall clock; gaps are relative to this domain. Prefer over micros() (71-min wrap)."
    },
  "collector": {"host": "192.168.178.10", "port": 9100},
  "gates": ["HU-PROBE-GATE-01", "HU-A1"],
  "capture": {
    "dropped": 0,
    "queue_highwater": 0,
    "tcp_disconnects": 0,
    "complete": null
  },
  "notes": ""
}
```

## Gemeinsame Event-Felder

| Feld | Typ | Pflicht | Bedeutung |
|------|-----|---------|-----------|
| `v` | int | ja | Schema-Version (=2) |
| `sid` | string | ja | session_id |
| `seq` | int | ja | monoton steigend pro Session |
| `t_us` | int | ja | Probe-monotonic µs seit Boot (`micros()` / esp_timer) |
| `t_ms` | int | nein | Komfort: `t_us // 1000` |
| `ev` | string | ja | Event-Typ |

## Zeitregel

- **Primär:** `t_us` — einzige Quelle für Gap-/Duration-Analysen im µs-Bereich.
- `gap_us` und `dur_us` werden aus derselben Uhr abgeleitet, nie aus `millis()`.
- Ab FW **0.2.1:** `gap_us` / `dur_us` sind volle **uint32** (µs), kein 16-bit-Cap bei 65535.
- Wall-clock nur in `session.json` (`started_at` / Marker-Notizen), nicht für Host-Timing.

---

## USB Enumeration / Control

### ev = usb

```json
{
  "v": 2, "sid": "...", "seq": 10, "t_us": 12003456, "t_ms": 12003,
  "ev": "usb",
  "usb_ev": "reset",
  "dir": "out",
  "ep": 0,
  "bmRequestType": null,
  "bRequest": null,
  "wValue": null,
  "wIndex": null,
  "wLength": null,
  "data_len": 0,
  "data_hex": null,
  "dur_us": 120,
  "note": ""
}
```

`usb_ev`: `attach` | `detach` | `reset` | `get_descriptor` | `set_address` | `set_configuration` | `clear_halt` | `stall` | `control` | `bot_need_reset` | `other`

Descriptor-Payloads: `data_hex` (roh) + optional `desc_type` (`device`/`config`/`string`).

`bot_need_reset` (FW ≥0.2.9): TinyUSB `MSC_STAGE_NEED_RESET` nach invalidem CBW (beide Bulk-EPs gestallt). Device-Stack-Sicht, kein Wire-Capture. `bot_source=tinyusb`, `synthesized=false`, typisch `note=msc_stage_need_reset`.

---

## BOT / SCSI (kanonische Evidenz)

Ein SCSI-Befehl = typischerweise drei Events (CBW → DATA → CSW), verknüpft über `bot_tag`.

### ev = bot_cbw

`bot_source`: `tinyusb` (FW ≥0.2.8, Host-CBW aus Device-Stack) | `msc_synth` (Stufe A, rekonstruiert).  
`synthesized`: `false` bei `tinyusb`, `true` bei `msc_synth`.

```json
{
  "v": 2, "sid": "...", "seq": 40, "t_us": 13000000,
  "ev": "bot_cbw",
  "bot_tag": 7,
  "bot_source": "tinyusb",
  "synthesized": false,
  "raw_hex": "55534243...",
  "signature": "USBC",
  "xfer_len": 4096,
  "flags": 128,
  "lun": 0,
  "cdb_len": 10,
  "cdb_hex": "2800000007a10000000800",
  "scsi": {
    "opcode": "read10",
    "opcode_byte": 40,
    "lba": 1953,
    "blocks": 8,
    "control": 0
  }
}
```

### ev = bot_data

```json
{
  "v": 2, "sid": "...", "seq": 41, "t_us": 13003800,
  "ev": "bot_data",
  "bot_tag": 7,
  "dir": "in",
  "expected": 4096,
  "actual": 4096,
  "dur_us": 3800,
  "gap_us": 4000,
  "tag": "fav0",
  "kind": "file",
  "lba": 1953
}
```

`gap_us`: Zeit seit Ende des vorherigen DATA-Transfers (Host-Pacing).

### ev = bot_csw

```json
{
  "v": 2, "sid": "...", "seq": 42, "t_us": 13004050,
  "ev": "bot_csw",
  "bot_tag": 7,
  "raw_hex": "55534253...",
  "residue": 0,
  "status": 0
}
```

`status`: 0=passed, 1=failed, 2=phase error (BOT).

### ev = read (Komfort / Aggregat)

Abgeleitet oder parallel zu `bot_data` für schnelle Histograms. **Nicht** alleinige Evidenz für „HU hat READ10 benutzt“ — dafür `bot_cbw.scsi.opcode` + `cdb_hex`.

```json
{"v":2,"sid":"...","seq":43,"t_us":13003800,"t_ms":13003,"ev":"read","lba":1953,"n":4096,"dur_us":3800,"gap_us":4000,"tag":"fav0","kind":"file","bot_tag":7}
```

### ev = scsi (deprecated Kurzform)

Nur noch für Stub/Sim. Feld `legacy: true` setzen. Migration: durch `bot_cbw` ersetzen.

---

## Session / Phasen / Marker / Status

### ev = phase

```json
{"v":2,"sid":"...","seq":44,"t_us":14000000,"ev":"phase","phase":"remount","note":"restored_p"}
```

`phase`: `idle` | `enum` | `armed` | `streaming` | `stall` | `busy` | `remount` | `sleep` | `wake` | …

### ev = marker

```json
{"v":2,"sid":"...","seq":45,"t_us":14100000,"ev":"marker","name":"play_start","note":"offline"}
```

Operator-/Audit-Marker (FW ≥0.2.25, Offline per BOOT-Zyklus): `play_start`,
`track_next`, `audible_change`, `replay`, `stop`, plus szenariospezifisch
`plug_ready` / `replug`. Ohne `play_start` + (`audible_change`|`track_next`)
bleibt S2-Prefetch epistemisch **UNKNOWN**.

### ev = status

```json
{
  "v": 2, "sid": "...", "seq": 100, "t_us": 15000000,
  "ev": "status",
  "read_rate_kb_s": 120.5,
  "tur_hz": 1.0,
  "probe_state": "streaming",
  "scenario": "s0_plug_enum",
  "vehicle": "bmw"
}
```

### ev = session

```json
{
  "v": 2, "sid": "...", "seq": 1, "t_us": 0,
  "ev": "session",
  "action": "start",
  "vehicle": "bmw",
  "scenario": "s0_plug_enum",
  "profile_id": "bmw_nbt_evo"
}
```

### ev = capture_health

Periodisch und bei Session-Ende. Ohne dieses Event ist die Session forensisch nur eingeschränkt verwertbar.

```json
{
  "v": 2, "sid": "...", "seq": 101, "t_us": 15001000,
  "ev": "capture_health",
  "event_seq_first": 1,
  "event_seq_last": 10042,
  "dropped": 0,
  "published": 10042,
  "stream_lost": 0,
  "tcp_tx_ok": 10000,
  "tcp_tx_fail": 0,
  "queue_pending": 12,
  "queue_highwater": 312,
  "tcp_connected": true,
  "tcp_disconnects": 0,
  "ring_size": 2048,
  "msc_sample_overflow": 0,
  "msc_reads": 240,
  "reset_reason": "power_on",
  "transport": "ram_offline",
  "transport_epoch_start": 2,
  "transport_epoch_end": 2,
  "end_reason": "auto_media_hide",
  "end_strategy": "media_hide",
  "upload_status": "delivered",
  "offline_overflow": 0,
  "offline_records": 412,
  "record_size": 99,
  "capacity_bytes": 65536,
  "complete": true,
  "integrity_pass": true
}
```

Offline-Integrity (quantitativ): `offline_overflow=0`, `dropped=0`,
`complete=true`, Collector ohne `recv_seq_gaps`. Sonst nur diagnostisch.

Collector spiegelt zusätzlich in `session.json` → `capture`:

- `recv_seq_gaps` — Empfänger-Sicht (nach `seq`)
- `recv_events` / `parse_errors`
- `reset_reason`, Transport-Epoch(en), `end_reason` / `end_strategy` und
  `upload_status` — Abschluss- und Transportdiagnose (keine Wire-Evidenz)
- `panic_tail` — optionaler, nach Reset aus RTC-RAM gelesener Session-/Breadcrumb-Tail;
  wird ohne Flash-Schreibzugriff im USB-Hotpath geführt

Analyse-Regel:

- Probe: `dropped == 0`, `stream_lost == 0`, `tcp_disconnects == 0`
- Collector: `recv_seq_gaps` leer
- sonst → Claims nur mit Einschränkung / Re-Run

Hinweis: Bis FW 0.2.3 konnte TCP bei Write-Fehler Events bereits aus dem Ring nehmen
(`pop` vor erfolgreichem TX) — stille Seq-Lücken bei `dropped=0`. Ab 0.2.4: peek→TX→pop.

---

## Connected App / Control Plane (C1a Host)

Host-Adapter (`tools/bcl_c1a`) und Device (`ConnectedAppProbe` / C1a.5). Getrennt von `bot_*` / `read`.

### ev = ca_bcl

```json
{
  "v": 2, "sid": "LAB-ca_c1_lab_bcl", "seq": 3, "t_us": 3000,
  "ev": "ca_bcl",
  "ca_ev": "frame",
  "cmd": 2,
  "cmd_desk_name": "DATA",
  "src": 1,
  "dest": 4004,
  "dest_desk_label": "Etch",
  "len": 7,
  "payload_hex": "53594e54483031",
  "source": "synthetic",
  "evidence_level": "TEST",
  "direct_observation": false
}
```

`ca_ev`: `frame` | `error`.  
`dest_desk_label` optional (Desk-R0, z. B. `"Etch"` für `0x0FA4`) — **kein** Live-Etch-Claim.  
Synthetic Lab-Events: `source=synthetic` / `evidence_level=TEST` / `direct_observation=false`.  
Transport-Skeleton-Inject: `source=transport_aoa_skeleton` — weiterhin **kein** NBT-Wire.

### ev = ca_transport (C1b-0 / C1b-P0)

```json
{
  "v": 2, "sid": "...", "seq": 2, "t_us": 2000,
  "ev": "ca_transport",
  "ca_ev": "DISCONNECTED",
  "transport": "aoa",
  "impl": "observe",
  "usb_role_intent": "device_in_accessory_mode",
  "usb_stack_bound": true,
  "armed": true,
  "connected": false,
  "rx_bytes": 0,
  "tx_bytes": 0,
  "gate": "C1b-P0",
  "direct_observation": true
}
```

`impl`: `skeleton` (C1b-0 / MSC-Boot) | `observe` (C1b-P0 / `aoa_observe`).  
`ca_ev`: `CONNECTED` | `DISCONNECTED` | `RX_BYTES` | `TX_BYTES` | `TRANSPORT_ERROR`.

### ev = ca_usb (C1b-P0)

```json
{
  "v": 2, "sid": "...", "seq": 3, "t_us": 3000,
  "ev": "ca_usb",
  "ca_ev": "aoa_control",
  "p0_level": "P0-B",
  "transport": "aoa",
  "impl": "observe",
  "usb_stack_bound": true,
  "b_request": 51,
  "aoa_desk_name": "GET_PROTOCOL",
  "hypothesis": true,
  "hypothesis_handled": true,
  "direct_observation": true,
  "evidence_level": "FIELD",
  "gate": "C1b-P0",
  "probe_plane": "control"
}
```

`p0_level`: `P0-0` | `P0-A` | `P0-B` | `P0-C` | `P0-D` | `P0-E`.  
`ca_ev`: `gadget_isolation` | `attach` | `aoa_control` | `vid_pid` | `ep` | `bulk_rx`.  
P0-E `bulk_rx` carries `payload_hex` (truncated) and **must not** be treated as BCL until C1b-P1.

Status `connected_app`: `usb_gadget`, `msc_interface_present`, `vendor_interface_present`, `p0{P0_0…P0_E}`.

Siehe [CONNECTED-APP.md](CONNECTED-APP.md), [GATES-Connected.md](GATES-Connected.md).

---

## Ableitbare Kennzahlen

Aus `events.jsonl` (v2) müssen berechenbar sein: USB-Enum-Timeline, Gap-p50 (µs), Eager-EOF, Prefetch-Lead, Resume-Fenster, Busy-Dauer, TUR-Hz, Identity-Wechsel, Write-Count, Xfer-Histogramm, CBW/CSW-Vollständigkeit, Capture-Completeness.
