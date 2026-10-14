# Gates

## HU-PROBE-GATE-01 — Measurement Integrity

**Status:** open (Schema v2 eingefroren; Read-Aggregate im Feld OK; Roh-BOT/Deskriptoren/`fw_sha` ausstehend)

Teilfortschritt (2026-10-14): monotonic `t_us`/`gap_us` (uint32), Session-`capture_health`, physische USB attach/detach — aber Seq-Lücke in erster Feldsession, Drain-`t_us` bis 0.2.2, kein Roh-CBW/CDB/CSW.

Vor echtem Feldbetrieb und vor der Bewertung von HU-Fakten müssen folgende Punkte erfüllt sein. Ohne dieses Gate ist die Probe ein vorbereitetes Gerüst, kein wissenschaftlich brauchbares Messgerät.

### Zeit & Sequenz

- [ ] Monotonic µs-Timestamp (`t_us` / `t_probe_us`) auf allen Events
- [ ] Global per-Session `seq` monoton steigend, lückenlos nachweisbar (oder Drop dokumentiert)
- [ ] `gap_us` aus µs-Uhr, nicht aus `millis()`-Quantisierung

### USB Enumeration / Control

- [ ] USB Reset / Connect / Disconnect sichtbar
- [ ] Control-Transfer-Visibility (bmRequestType, bRequest, wValue, wIndex, wLength)
- [ ] Descriptor-Capture (Device, Configuration, String)
- [ ] SET_ADDRESS / SET_CONFIGURATION in der Timeline
- [ ] Endpoint / Direction je Transfer
- [ ] STALL / CLEAR_HALT / Reset-Recovery

### BOT / SCSI Evidenz

- [ ] BOT CBW: raw hex + decoded (signature, tag, xfer_len, flags, lun, cdb_len, cdb[16])
- [ ] SCSI CDB: raw + decoded (opcode, LBA, blocks, control)
- [ ] DATA: expected / actual length + duration
- [ ] CSW: raw + decoded (tag, residue, status)
- [ ] Inter-command gap aus monotonic clock

### Capture Health / Forensik

- [ ] Event-Drop-Accounting (`dropped`, `queue_highwater`)
- [ ] Explizites `capture_health`-Event / Session-Attribut
- [ ] TCP-Disconnect-Accounting
- [ ] Firmware-Version + SHA (oder Build-ID) in `session.json`
- [ ] Probe-Identity (MAC, Serial, Volume-Label) in Session
- [ ] Rohdaten in `events.jsonl` unverändert erhalten (Derived nur unter `derived/`)

### Freigabe

Erst wenn alle Checkboxen erfüllt und ein Labor-Dry-Run (Sim + idealerweise Host-PC als USB-Host) die Capture-Health mit `dropped=0` zeigt, gilt GATE-01 als **closed**. Danach: BMW-Validierung, dann Mazda Discovery.

Verwandt: Technology-Audit HU-A\* (Verhaltens-Gates). GATE-01 ist die **Instrumenten**-Voraussetzung dafür.
