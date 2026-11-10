# Feld 2026-11-10 — I-303A Reval + MSC (F20 NBT Evo)

| | |
|--|--|
| **Anlass** | 07.11. ggf. Unterversorgung (Serial-USB fehlte); heute beide USBs wie sonst |
| **FW gemessen** | **0.3.36** · DUT `.89` |
| **FW Code** | **0.3.37** gebaut (USB-`iProduct`-Fix; OTA heute nicht mehr) |
| **HB I-303A** | `artifacts/i303a-20261110T064024Z-hb.jsonl` |
| **Bezug 07.11.** | [I303A-FELD-2026-11-07.md](I303A-FELD-2026-11-07.md) |

---

## Brauchbar?

**Ja — epistemisch, nicht als neuer Connected-Durchbruch.**

| Messung | Nutzen |
|---------|--------|
| I-303A Reval mit beiden USBs | schließt „Versorgungsartefakt“ aus → G1-Negativ **belastbar** |
| Soft-Detach `lab_debian` | Ops-Falle erklärt (RST → HU „kein Gerät“) |
| `cache_place` vs `cache_a` | Medium-Wahl erklärt „keine Dateien“ |
| Espressif-`iProduct` | Anzeigename ≠ SCSI; Fix in 0.3.37 |

**Nicht gemessen heute:** Zubehör-Pfad (`aoa_observe` / `18D1:2D00`), Phone×HU (P1), Bulk/BCL/SAS.

---

## FACT

### I-303A Reval (`aoa_id_303a`, Vendor-only `303A:1001`)

- Mode sticky, Observing, beide USBs verbunden.
- HU: **„USB-Gerät wird nicht unterstützt“**; LED dimmblau (WiFi ok, kein MSC).
- Probe: `attach` (inkl. OTG-Replug `1→2`), **`aoa_51/52/53 = 0`**, `vendor_setup = 0`, kein Bulk.
- **REJECTED:** „I-303A-Negativ nur wegen fehlendem Serial-USB gestern.“

### MSC-Kontrolle (anschließend)

- Nach Mode-Switch/`gadget/reboot` bzw. RST: oft **`msc_soft_attached=false`** — Diag: `lab_debian: soft-detached by default (WiFi first)`. OTG allein reicht nicht; **`POST /api/usb/cycle`** → soft-attach.
- Mit Soft-Attach + Medium: HU sah Gerät, SCSI-Reads liefen.
- Aktiv zuerst **`cache_place`** → Espressif-Langname, **keine Dateien**, „unbekannter Name/Interpret“.
- Switch **`cache_a`** + Cycle → **Dateien wieder sichtbar**.
- USB-Name HU: **„Espressif ESP32-S3 DevKitC-1 …“** (Arduino-Default-`iProduct`). SCSI Inquiry `HUPROBE`/`HU_PROBE`; Volume `HUPROBE`.

### Vergleich: wann war „Zubehör“? (nicht heute)

| Mode | Identity | HU (belegt) |
|------|----------|-------------|
| `aoa_observe` | `18D1:2D00` | Zubehör wählbar + Reconnect-Loop (2026-10-30, FW 0.3.34) — [AOA-C1B-P0-NBT](AOA-C1B-P0-NBT.md) |
| `aoa_phone_like` | `04E8:6860` | „nicht unterstützt“ |
| `aoa_id_303a` | `303A:1001` | „nicht unterstützt“ (07.11. + **heute**) |
| `msc` | Arduino-VID + MSC | Stick/Dateien, kein Zubehör-Pfad |

---

## FIX (0.3.37, Code + Dist)

MSC-Boot: `USB.manufacturerName("hu-probe")` / `USB.productName("hu-probe")` vor `USB.begin()`.  
Bin: `firmware/dist/hu-probe.0.3.37.esp32s3.bin`.

---

## INFERENCE

- I-303A bleibt G1-Abschluss: Class/Config ohne MSC ≠ nutzbares Medium / kein AOA-Callback.
- `cache_place` ist kein BMW-Alltagsmedium für „Dateien sichtbar“.
- `vehicleId=lab_debian` im Auto → Soft-Detach-by-default verwirrt Feld-Ops.

---

## NEXT

- OTA **0.3.37** am DUT (Anzeigename prüfen).
- Optional Auto-Profil `bmw` / `bmw_nbt_evo` (weniger Soft-Detach-Falle).
- **P1:** [FIELD-CONNECTED-REAL-01](FIELD-CONNECTED-REAL-01.md) — keine weitere Identity-Matrix.
