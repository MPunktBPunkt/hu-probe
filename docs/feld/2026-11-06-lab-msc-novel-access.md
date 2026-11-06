# Lab MEASURED — MSC Offline, Sequenz, Media-Wechsel, Novel Host-Zugriff

| | |
|--|--|
| **Datum** | 2026-11-06 |
| **DUT** | `.89` hu-probe **0.3.36** `usb_gadget=msc` |
| **Host** | `.88` usb-analyzer 0.0.52 |
| **Artefakte** | `artifacts/20261106-lab-msc-novel.json` · `…-part2.json` |

**Not claimed:** BMW-HU wire. Novel = Lab-Host-Stimulus gegen Feld-Grammatik.

---

## FACT

### Media / Attach
- Host sieht DUT `303a:1001` (Espressif MSC).
- Preset `size_1_4_8`: Dateien `S1M.MP3@65`, `S4M.MP3@2113`, `S8M.MP3@10305`.
- Wechsel `cache_b` (remount) → Host remount + `bmw_mount` ok (`modeSenseOk=6`).

### Offline
- Erster Lauf 20 s `s0_plug_enum` / fine_data: **177 Records**, Stop `auto_media_hide` (DUT detached → Folge-Zugriffe `no_dut`).
- Zweiter kurzer Lauf: Capture lief; Host-Read während Capture **ok**; Status danach `uploading` / records≈65.

### BMW-like Baseline
- `bmw_mount_scan` / `bmw_mount_deep`: MODE_SENSE6×6, PREVENT×7 MEASURED auf Host; DUT `msc_reads` stieg (z. B. 0→65 im Novel-Lauf).

### Novel Host-Zugriff (an HU so nicht beobachtet)
| Muster | Ergebnis |
|--------|----------|
| Reverse-LBA-Treppe 500→10 | **8/8 ok** |
| File-LBA Reads S1M/S4M/S8M | **3/3 ok** |
| Odd sizes 7 / 17 / 63 blocks | **ok** |
| Sequential probe 128 blocks | **ok** |
| WRITE probe LBA200 | **write_fail** (DUT RO — erwartet) |

DUT `scsi_opcode_histogram` (Novel-Lauf):  
`tur=50`, `inquiry=4`, `mode_sense6=6`, `prevent_allow=7`, `read_capacity10=8`, `read10=58`, `write10=0`, `other=2`.

### Sequenz
- `seq_size` start → state `prepare` / Prompt „S1M wählen…“ → `next` → `wait_driver` → skip/abort API bedient.

---

## INFERENCE
Lab-Host kann Datei-LBAs und Reverse/Odd-READ10 erzeugen, die von der NBT-P-MOUNT/P-MREAD-Grammatik abweichen; die Probe zählt sie auf MSC-Messschicht.

## UNKNOWN
Ob die HU jemals Odd-Blockcounts oder Reverse-LBA nutzt — bisherige Feld-Captures zeigen das nicht.

## REJECTED
„Media-Wechsel während aktiver Offline-Capture“ — API **409** `capture_or_sequence_active` (by design).
