# Event- und Session-Schema

Version: **1** (Feld `v` in jedem Event)

## Session-Ordner

```
sessions/YYYYMMDD-HHMMSS_<vehicle>_<scenario>/
  session.json      # Meta
  events.jsonl      # Rohstrom (Source of Truth)
  markers.jsonl     # Operator / Ohr / Ignition (auch in events als ev=marker)
  status.jsonl      # periodische Aggregate
  derived/          # nach Ingest (später)
  fingerprint.json  # berechnetes Profil dieser Session
  diff.json         # vs Golden / vs Vergleichsprofil
```

### session.json

```json
{
  "schema": 1,
  "session_id": "20261014-064500_bmw_s0_plug_enum",
  "started_at": "2026-10-14T06:45:00+02:00",
  "ended_at": null,
  "vehicle": "bmw",
  "profile_id": "bmw_nbt_evo",
  "scenario": "s0_plug_enum",
  "fw_version": "0.1.0",
  "fw_type": "hu-probe",
  "mac": "aa:bb:cc:dd:ee:ff",
  "chip": "ESP32-S3",
  "collector": {"host": "192.168.178.10", "port": 9100},
  "gates": ["HU-A1"],
  "notes": ""
}
```

`vehicle`: `bmw` | `mazda_cx30` | frei wählbar.  
`profile_id`: Referenzprofil für Diff (meist `bmw_nbt_evo` oder `mazda_cx30`).

## Event-Framing

Newline-delimited JSON (JSONL). Gemeinsame Felder:

| Feld | Typ | Bedeutung |
|------|-----|-----------|
| `v` | int | Schema-Version (=1) |
| `sid` | string | session_id |
| `seq` | int | monoton steigend pro Session |
| `t_ms` | int | Probe-Uptime ms (Wanduhr relativ) |
| `ev` | string | Event-Typ |

### ev = read

```json
{"v":1,"sid":"...","seq":42,"t_ms":123456,"ev":"read","lba":1953,"n":4096,"dur_us":3800,"gap_us":4000,"tag":"fav0","kind":"file"}
```

| Feld | Bedeutung |
|------|-----------|
| `lba` | Logical Block Address |
| `n` | Bytes gelesen |
| `dur_us` | Callback-Dauer |
| `gap_us` | Zeit seit vorherigem Read |
| `tag` | Slot/Dateiname-Kurzlabel |
| `kind` | `file` \| `boot` \| `fat` \| `dir` \| `cold_body` \| `other` |

### ev = scsi

```json
{"v":1,"sid":"...","seq":43,"t_ms":123500,"ev":"scsi","op":"tur"}
```

`op`: `inquiry` | `capacity` | `tur` | `read10` | `write10` | `sense` | `other`

### ev = phase

```json
{"v":1,"sid":"...","seq":44,"t_ms":130000,"ev":"phase","phase":"remount","note":"restored_p"}
```

`phase`: `idle` | `enum` | `armed` | `streaming` | `stall` | `busy` | `remount` | `sleep` | `wake` | …

### ev = marker

```json
{"v":1,"sid":"...","seq":45,"t_ms":131000,"ev":"marker","name":"ear_tone_start","note":""}
```

Operator- oder UI-Marker (`ignition_on`, `door_open`, `ear_tone_start`, …).

### ev = status

```json
{"v":1,"sid":"...","seq":100,"t_ms":140000,"ev":"status","read_rate_kb_s":120.5,"tur_hz":1.0,"probe_state":"streaming","scenario":"s0_plug_enum","vehicle":"bmw"}
```

### ev = session

```json
{"v":1,"sid":"...","seq":1,"t_ms":0,"ev":"session","action":"start","vehicle":"bmw","scenario":"s0_plug_enum","profile_id":"bmw_nbt_evo"}
```

`action`: `start` | `stop` | `meta`

## Ableitbare Kennzahlen

Aus `events.jsonl` müssen u. a. berechenbar sein: Gap-p50, Eager-EOF, Prefetch-Lead, Resume-Fenster, Busy-Dauer, TUR-Hz, Identity-Wechsel, Write-Count, Xfer-Histogramm.
