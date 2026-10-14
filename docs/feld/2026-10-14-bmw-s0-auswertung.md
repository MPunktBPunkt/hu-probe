# Feldauswertung — BMW S0 (2026-10-14)

**Session:** `sessions/20261014-081637_bmw_s0_plug_enum/`  
**FW:** hu-probe `0.2.2` · ESP `192.168.178.89` · MAC `68:b6:b3:2b:c5:68`  
**Zeit:** 08:16:37–08:22:30 (+02:00) · ~6 min  
**Capture:** `dropped=0`, `tcp_disconnects=0`, `complete=true`, highwater=96

> Peer-Review (Mistral / GPT / Grok) + unabhängige Re-Prüfung: siehe
> [2026-10-14-review-triage.md](2026-10-14-review-triage.md).

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
| Physische Plug/Unplug (host) | 11 / 11 |
| Read-Größe Mode | **4096** (132×; auch 512/2048/2560) |
| Gap-p50 (Pace, &lt;10 s) | **3724 µs** |
| File-Reads | 55 (~175 KiB), LBA 51–83 |
| Eager großer Body-Stream | **nicht prüfbar** (Medium zu klein) |
| Host detach→attach | **2,294–6,011 s** (11 Paare; exkl. Soft-Cycle) |

### Pro Replug-Fenster (Host-Attach-Index)

| Fenster | Reads | Bytes | Muster |
|---------|-------|-------|--------|
| 0, 1, 2, 8, 9, 10 | 22 | 65 536 | BOOT+FAT+ROOT+DATA |
| 3 | 14 | 41 472 | BOOT+FAT+ROOT, **kein DATA** |
| 4, 5, 6 | 0 | 0 | Plug ohne Read-Emission (im Fingerprint weggelassen) |
| 7 | 94 | 282 112 | dichter Mehrfach-Scan — siehe unten |

**6 von 8** nicht-leeren Fenstern zeigen das 22-Read-Grundmuster (nicht 7/8).

### Fenster 3 (FACT + Korrelation)

14 Reads in ~3,8 ms Span, kein DATA. Attach→Detach nur **~3,76 s** — mitten in der physischen Replug-Serie nach Marker `before_physical_replug`.  
**INFERENCE:** zu kurze Verweildauer für DATA/Playback-Pfad. Ursache im Detail: **UNKNOWN**.

### Fenster 4–6 (FACT)

Jeweils Plug ~4,7–7,5 s **ohne** Read-Events. Ebenfalls schnelle Replug-Serie — HU hat in diesen Fenstern kein MSC-Read (oder Samples gingen verloren; `msc_sample_overflow` war in 0.2.2 nicht in `session.json`).

### Fenster 7 (FACT + Instrumenten-Artefakt)

94 Reads / 282 KiB, alle als Orphan **nach** Detach gestempelt (Drain-`t_us`). `gap_us` zeigt ~4× BOOT/FAT/ROOT/DATA. Plug-Dauer ~7,3 s.  
**HYPOTHESIS:** Mehrfach-Scan während Plug; Emission nach VBUS/Loop-Recovery.  
**Fix:** FW ≥0.2.3 Capture-`t_us`; FW ≥0.2.4 TCP peek→TX→pop (stille Seq-Verluste).

### Capture-Hygiene

- `t_us` session-weit nicht rückwärts
- **seq-Lücke:** 249, 250 fehlen in `events.jsonl` trotz `dropped=0` → **FAIL** für lückenlosen Seq-Nachweis (GATE-01)
- Seq-Sprünge in `derived/usb_phase.jsonl` sind erwartbar (gefilterter Strom)

## Abgleich Golden `bmw_nbt_evo`

| Erwartung (PiDrive-Feld) | Diese Session |
|--------------------------|---------------|
| 4 KiB dominant | **FACT** (diese Session) |
| Gap idle ≈ 4 ms | **FACT** Gap-p50 3724 µs; **INFERENCE** konsistent mit PiDrive ~4 ms |
| Eager-EOF / Prefetch-Strom | **nicht prüfbar** — 256 KiB-Demo |

Epistemik: Claims gelten nur für dieses Medium + FW 0.2.2. Keine Übertragung auf Mazda.  
Kein universelles „NBT liest immer 4 KiB / 4 ms“ ableiten.

## Limitationen / nächste Bau

1. ~~Validator + Fingerprint~~ → `tools/validate_session.py`, `fingerprint_session.py`, `correlate_windows.py`
2. ~~Capture-`t_us`~~ → FW 0.2.3+
3. ~~TCP silent loss / Seq~~ → FW 0.2.4 peek→TX→pop + Collector `recv_seq_gaps` (Feld-OTA ausstehend)
4. ~~512 KiB Silence-MP3-Medium~~ → Demo-FAT ab 0.2.4 (Flash ~80 %)
5. GATE-01: Roh-BOT CBW/CDB/CSW, `fw_sha`, Deskriptoren
6. VBUS-Verlust → lokaler Tail-Ring
7. OTA 0.2.4 auf Lab `.88` / Auto `.89` sobald online

## Artefakte

- `events.jsonl` — Rohstrom  
- `fingerprint.json` — verdichtetes Profil (`tools/fingerprint_session.py`)  
- `derived/reads.jsonl`, `derived/usb_phase.jsonl`  
- Review: [2026-10-14-review-triage.md](2026-10-14-review-triage.md)
