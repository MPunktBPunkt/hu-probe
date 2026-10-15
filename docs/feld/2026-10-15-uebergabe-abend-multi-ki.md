# hu-probe — Übergabebericht Abend 2026-10-15 (Multi-KI-Konsens + Rohdaten-Check)

**Zweck:** Folge-KIs arbeiten ohne Reverse-Engineering.  
**HEAD:** `0dd7042` (`0dd7042183e0521157d87f1898e7c6724261b606`) · **FW:** 0.2.23  
**Quellen dieses Dokuments:** Cursor-Auswertung S2 · Maßnahmen 01d · Offline-P0 · Berichte Mistral/GPT/Claude · **eigene Verifikation** an `events.jsonl` (2026-10-15 Abend).

**Epistemik (Pflicht):** FACT nur aus eigener Session; Lab↔BMW nur nebeneinander; kein Mazda-Übertrag; Wire=UNKNOWN; nachgezogene Marker ≠ Operator-Zeitstempel; verbindliche Korrekturen in [massnahmen-01d](2026-10-15-massnahmen-01d.md).

---

## 1. Gesamturteil (Konsens Mistral/GPT/Claude + Check)

Drei echte Fortschritte an einem Tag:

1. **Lab Offline-Capture E2E bewiesen** (`ram_offline`, Session `181642`).
2. **S2 erstmals am BMW gefahren** — Transport stabil, Marker-Dichte hoch, Semantik noch INFERENCE.
3. **Offline-P0 vor Feld** hat MODE_SENSE6-STALLs als Instrument-Artefakt entlarvt (Fix 0.2.18); Burst-39 als FACT abgelehnt.

**Engpass:** BMW Offline (WiFi OFF) **fail** — ein Versuch, Capture leer. Live-S2 ist brauchbar; unbroken BMW-Timeline weiter offen. GATE-01 bleibt **OPEN**; Burst-Epistemik ist gültige Messform.

---

## 2. Repo / FW-Linie (6f655a8 → 0dd7042)

| FW | Inhalt |
|----|--------|
| 0.2.18 | MODE_SENSE6-Pad: TinyUSB 4 B vs BMW-CBW 192 B → ep_in-STALL; Fix padet auf CBW-Länge |
| 0.2.19–0.2.20 | RAM OfflineCapture: WIFI_OFF → Heap-Ring → Unplug/BOOT-Stop → Replay Schema-v2 `transport=ram_offline`; OOM-Härtung |
| 0.2.21–0.2.22 | Soft-USB im Offline (measure_ms), Grace gegen Soft-Re-Enum-umount |
| 0.2.23 | TCP-Pump im Finalize (Collector erreicht) |

Offline-Puffer (Quellcode): Default **64 KiB**, Fallback **48 KiB**, `Rec` ≈ **91 B** (Lab 8736/96), `kMaxCaptureMs` **180 s**. ≈720 Records bei 64 KiB — für kurze Fenster ok; Worst-Case `overflow=0` prüfen.

---

## 3. Lab Offline — FACT

| | Session `20261015-181642_lab_debian_s0_plug_enum` |
|--|--|
| FW / transport | 0.2.23 / **ram_offline** |
| Records / bytes | 96 / 8736 · overflow=0 · complete=true |
| Stop | `auto_soft_detach` (measure_ms=12000) |
| validate | seq/t_us OK, dropped=0 |
| Replay | umount → set_address → mount → descriptors |

**Nicht schließen:** Lab-PASS ⇒ BMW-PASS.

---

## 4. BMW Offline — FAIL (P0)

Ein Versuch ~18:26: Start-OK vom Client, danach idle/leer, **keine** `ram_offline`-Session. Alle Auto-Abenddaten = **`tcp_live`**.

Kandidaten (ungeordnet): Soft-Re-Enum vs HU-Re-Detect; measure_ms=12 s zu kurz wg. ~1 s Mount→INQUIRY; WiFi-Stop/Power; OOM-Reboot. **Nächster Schritt:** reproduzieren + UART/LED/NVS-Reset-Ursache, Variante physischer Replug, längeres measure_ms.

---

## 5. BMW S2 Live — FACT (Transport)

8 Kern-Sessions: `2280`, `2276`, `200036`, `200319`, `195417`, `195128`, `195657`, `2210`.

| Aggregat | Wert |
|----------|------|
| File-Reads | 642 (Gap n=632) |
| File-Gap-p50 / p90 | **3783 / 3913 µs** |
| read-Event n=4096 | 626/642 file (gesamt n-modes: 4096 dominant) |
| dropped / tcp_tx_fail / seq-gaps | 0 / 0 / 0 |
| bot_source | tinyusb |
| SCSI (Kern) | tur 479 · read10 148 · prevent 70 · ms6 60 · cap 30 · inq 10 |
| ep_stall (S2, verifiziert) | **0** in Stichprobe 2280/2276/200036/2210 |

S0 0.2.17 Gap-p50 ~3739 µs → S2 ~3783 µs (**~1,2 %**). Transportsignatur stabil. WiFi: Blau→langes Grün→Gelb weiterhin.

Read-Kind Gap-p50: file 3783 · fat 1218 · dir 3813 · boot 20863 µs.

---

## 6. Rohdaten-Check der Multi-KI-Claims (verbindlich für Folgearbeit)

### 6.1 Claude: „4096 / 3,74 ms = Probe-/Link?“ — **teilweise bestätigt**

Verifiziert S2:

| Beobachtung | Wert |
|-------------|------|
| read-Events / READ10-CBW | **783 / 148 ≈ 5,29** |
| READ10 `blocks` (häufigste) | **8** (=4096 B), **240** (=122880 B), auch 1/2/3/5/69 |
| blocks=8 → read-Chunks | typisch **1×4096** (CBW-Größe = Event-Größe) |
| blocks=240 → Chunks | **30×4096** = 122880 (Stack splittet große CBWs) |
| gap_p50(n=4096) / FS-Erwartung (~3741 µs) | **ratio ≈ 1,00** (S2 und S0) |
| gap_p50(n=512) | ~42 ms (≫ Link — echte Pause/Host) |

**Konsequenz:**

- `read`-Event-`n=4096` ist **Device-Stack-Chunk und/oder** häufige CBW-Größe (blocks=8) — nicht blind „HU liest immer 4 KiB Anfragen“.
- File-Gap-p50 ~3,7–3,8 ms bei n=4096 ist **sehr gut mit FS-Bulk-Sättigung** erklärbar → als **Messgröße** FACT, als **HU-Scheduler-Pace** nur INFERENCE bis LBA/CBW-Auswertung.
- Profil: `read_size_bytes_mode` / Gap **nicht** als reine HU-Parameter festschreiben, bevor READ10-`blocks`-Verteilung + LBA-Folgen dokumentiert sind. Zähl-FACTs (BOT tinyusb, Writes=false, SCSI-Mix, ms6/cap=2) sind sicherer.

### 6.2 Burst-39 — INFERENCE (Maßnahmen bestätigt)

Offline-P0: Split `gap_us>500 ms`; File-Reads/Burst p50=39; reine Cluster 39×4096=158208 B; Pause p50~10,2 s. Histogramm hat auch 62–78. **REJECTED:** „immer 39“. H-Mount / H-Audio-Rate (~128 kbps) bleiben Hypothesen.

### 6.3 STALL / MODE_SENSE6

- S0: 84 stalls, 70 same-`t_us` wie ms6; ms6/cap=2,0 → Instrument-Artefakt, Fix 0.2.18.
- S2 (nach Fix): **ep_stall=0** (Stichprobe) — Feldbestätigung positiv.
- ms6/cap **weiter 2,0** (60/30) → **HU-Verhalten**, kein Stall-Indiz.

### 6.4 Enum / TUR / QD

- Mount→INQUIRY p50 ~997 ms (n=12) — GPT „12 ms“ verwerfen.
- TUR session-weit p50 ~2,06 Hz — „~1 Hz erhärtet“ verwerfen; Fenster-Raten nötig.
- QD max 2 nur in Sessions mit t_us-Drain-Verdacht → eher Artefakt.

### 6.5 hu_aus

Marker nachgezogen. Claude: zuerst **Standby/Zündung** (~20 min) ausschließen, bevor Power-Hypothese. P2c bleibt sinnvoll, aber nicht vorschnell kausal.

### 6.6 Marker

Viele nachgezogen (ESP offline). Erzählreihenfolge brauchbar; Mikro-Timing nicht. Prefetch/Cache/Eager = INFERENCE bis LBA-Formalauswertung.

---

## 7. Was FACT / INFERENCE / REJECTED ist

**FACT (hu-probe, Device-Stack):**  
tinyusb BOT; dropped=0 in Kern-Sessions; keine Writes gesehen; SCSI-Mix; S2 642 File-Reads; Gap-p50 file ~3783 µs (Messgröße); S0↔S2 Gap ~stabil; Lab ram_offline E2E; BMW Offline fail; S2 ep_stall≈0 nach 0.2.18; ms6/cap=2,0 HU; WiFi stirbt unter langem Grün bei weiterlaufendem USB; Read-Events splittet große READ10.

**INFERENCE:** Burst-39-Cluster; Prefetch~15 s; Eager/Cache; QD=1 als HU; Gap als HU-Scheduler; Ursache WiFi/HU-Aus; Auto-Offline-Ursache.

**REJECTED:** Burst-39 als Konstante; MODE_SENSE6-Stalls als HU-Recovery; PiDrive-Semantik als BMW-FACT; Lab=BMW-Signatur; nachgezogene Marker als exakte Zeit; Wire-Claims; GATE-01 closed.

---

## 8. Priorisierte nächste Schritte (geschärft)

| Prio | Was | Warum |
|------|-----|--------|
| **P0a** | Offline-Skript: READ10-`blocks`, gap vs n, Bytes/s im Burst, LBA-Folgen | Claude 5.1 entscheiden — Profil hängt davon ab |
| **P0b** | S2 LBA/Marker-Formalauswertung (Audit-Regeln), Marker-Vorbehalt | Semantik aus vorhandenem Material |
| **P0c** | Auto-Offline reproduzieren + diagnostizieren | Schlüssel zu unbroken Timeline / S1 |
| **P1c** | `profiles/bmw_nbt_evo.json` updaten | Älteste Schuld; nur gesicherte FACTs + `origin: hu_probe_field`; Gap/4096 als Messgröße kennzeichnen |
| **P1a** | t_us-Regression (2176/2199/2280 Drain) | vor langen S1-Streams |
| **P1** | Marker ohne WiFi (Queue/BOOT) | S2-Handicap |
| **P2** | S3 Cache/Replay → S1 ≥4 MiB → MODE_SENSE6 Feld-A/B (teils erledigt) → P2c Power → GATE-01 → Mazda |

**Nicht:** weitere WiFi-Live-Optimierung als Primärpfad; Flash-Logger vor overflow-Bedarf; 39/Prefetch als FACT; alte Stalls als HU-Recovery.

---

## 9. Context-Box (Copy/Paste)

```
Projekt: github.com/MPunktBPunkt/hu-probe HEAD 0dd7042 (2026-10-15 Abend)
FW 0.2.23. BMW NBT Evo=Golden (.89); Lab Debian (.88)=Instrument; Mazda=Stub.
Wire=UNKNOWN. Lab!=BMW. Verbindlich: massnahmen-01d, offline-p0,
bmw-s2-0.2.23-auswertung, dieser uebergabe-abend-multi-ki.

Lab ram_offline E2E PASS: 20261015-181642 (96 rec, complete, auto_soft_detach).
BMW Offline FAIL (1 Versuch, leer). Alle Auto-Abenddaten tcp_live.

S2 8 Sessions: 642 file-reads, gap_p50 3783 us, dropped=0, ep_stall=0 (nach 0.2.18).
READ10 blocks oft 8 (=4096 B) und 240 (gesplittet in 30x4096 read-Events);
read_ev/read10 ~5.3. gap_p50(n=4096)/FS-Erwartung ~1.00 -> Gap als Messgroesse FACT,
als HU-Scheduler nur INFERENCE. ms6/cap=2.0 bleibt nach Stall-Fix (=HU).

Burst: gap>500ms, p50=39 file-reads/burst = INFERENCE nicht FACT.
Prefetch/Cache/Eager = INFERENCE. Marker oft nachgezogen.

Offline RAM: 64 KiB / Rec~91 B / max 3 min. Flash nicht als Naechstes.

Prio: (1) offline blocks/gap/LBA-Check (2) S2 Formalauswertung (3) Auto-Offline-Diagnose
(4) Profil P1c (5) t_us P1a (6) S3->S1>=4MiB (7) Power P2c (8) GATE-01.

Never: 39 als FACT; ms6-stalls als HU-Recovery; PiDrive-Semantik als BMW-FACT;
Lab=BMW; nachgezogene Marker als exakte Zeit; Gap/4096 blind als HU-Pace ins Profil;
Wire-Claims.
```

---

## 10. Artefakte

| Doc | Rolle |
|-----|--------|
| [bmw-s2-0.2.23-auswertung](2026-10-15-bmw-s2-0.2.23-auswertung.md) | Abend-Feld Zahlen |
| [massnahmen-01d](2026-10-15-massnahmen-01d.md) | Konsens-Korrekturen |
| [offline-p0](2026-10-15-offline-p0.md) | Burst/STALL/Enum aus 0.2.17 |
| [Messkonzept-Offline](2026-10-15-Messkonzept-Offline-Capture.md) | Design Stufe 1 |
| Sessions | Lab `181642`; S2 Core whitelisted in `.gitignore` |

---

*Erstellt aus Mistral-/GPT-/Claude-Berichten + Rohdaten-Verifikation. Claude-Cache-Vorbehalt zu HEAD entfällt: `0dd7042` lokal verifiziert.*
