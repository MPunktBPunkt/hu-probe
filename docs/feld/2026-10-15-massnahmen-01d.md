# Maßnahmen — 2026-10-15 (nach BMW 0.2.17-Review + S1–S3-Audit)

**Quellen:** [bmw-s0-0.2.17-review](2026-10-15-bmw-s0-0.2.17-review.md) (Cursor/Mistral/GPT/Grok) · [auswertung](2026-10-15-bmw-s0-0.2.17-auswertung.md) · Desk-Audit `msc-host-read-semantics-s1s2s3-2026` (technology-audit PR #9, gemerged) · Vorgänger: [massnahmen-01c](2026-10-14-massnahmen-01c.md)

**Kernurteil (Konsens):** S0-**Transportschicht** NBT ist hu-probe-FACT (4 KiB, Gap-p50 ~3739 µs, tinyusb-BOT, SCSI-Mix, dropped=0). Semantik (Eager/Prefetch/Cache/Busy/Identity) bleibt **PiDrive-Erbe** — S1–S3/S5/S6 **nie gefahren**. GATE-01 bleibt **OPEN**. WiFi-Root-Cause parallel, **kein Blocker** mehr für Burst-Feld.

## Korrekturen (nicht als Maßnahmen-Ziel übernehmen)

| Claim aus Vorberichten | Entscheidung |
|------------------------|--------------|
| „~39 File-Reads/Burst“ = FACT | **REJECTED als FACT** — inkonsistent mit Sessiontabelle; erst Burst-Definition + H-Quant offline |
| MODE_SENSE6→STALL→CLEAR_HALT = HU-Recovery FACT | **Kandidat / Artefakt-Verdacht** — Probe antwortet MODE_SENSE6 vermutlich unsauber; vor S5 klären |
| `enum_s≈1 s` überholt / „~12 ms Mount→READ“ | **nicht gesichert** — anderes Intervall als PiDrive-Enum; Zeitkette offline neu |
| PiDrive Eager/Prefetch/Cache = HU FACT | **KILL** — bis S1/S2/S3 nur `origin: pidrive_field` |
| GATE-01 closed / lange Timeline nötig für S2/S3 | **REJECTED** — Burst-Epistemik (Blau→Grün→Blau) freigeben |
| Lab-Gap ≈ BMW-Gap als eine Signatur | **KILL** — Lab ≠ BMW |

## Priorisierte Maßnahmen

| Prio | Maßnahme | Status | Hinweis |
|------|----------|--------|---------|
| **P0a** | Offline-Auswertung der 8 BMW-0.2.17-Sessions | **done** | [offline-p0](2026-10-15-offline-p0.md): Burst via `gap_us`>500 ms; H-Quant **39×4096=158208**; Pause p50 ~10 s |
| **P0b** | STALL vs MODE_SENSE6 Artefakt-Test (Rohdaten) | **done** | 70/84 Stalls same-`t_us` wie ms6; ms6/cap=2,0 → Instrument-Hypothese erhärtet |
| **P0c** | Zeitkette Mount→INQUIRY→TUR→READ10 + TUR-Rate | **done** | Mount→INQUIRY p50 ~997 ms; Session-TUR-Hz p50 ~2,1 (nicht idle) |
| **P0d** | QD aus BOT-Tag-Folgen (offline) | **done** | max outstanding Tags p50=1 (INFERENCE) |
| **P1a** | `t_us`-Regression in langen Sessions (`2176`/`2199`) | **offen** | Pace weiter über `gap_us`; Fix vor S1-Längenstreams |
| **P1b** | MODE_SENSE6 korrekt beantworten (oder A/B: Stall vs Fix) | **done (0.2.18)** | TinyUSB Case-5: builtin lieferte 4 B, BMW CBW 192 B → ep_in-STALL; `usbd_edpt_xfer`-Wrap padet auf CBW-Länge (+ Caching-Page). Feld-A/B noch offen |
| **P1c** | Profil `bmw_nbt_evo.json`: nur S0-FACTs mit `origin: hu_probe_field`, `fw: 0.2.17` | **offen** | Gap µs, read 4096, writes=false, BOT-Hinweis; PiDrive-Semantik `unverified` belassen; Burst nur INFERENCE |
| **P1d** | README auf 0.2.17; Short-SID-Mapping dokumentieren | **offen** | Hygiene |
| **P1e** | GATE-01: Burst-Epistemik formal dokumentieren | **offen** | Kriterien: Burst-Def, min. Bursts, dropped=0, Pace=`gap_us`; Gate bleibt OPEN |
| **P2b** | Lab WiFi-Last-Schwelle (optional, ≤2 h) | **offen** | Burst 1/3/10 s oder KB/s-Stufen → Zeit bis Gelb; **nicht** Root-Cause |
| **P2c** | USB↔WiFi Root-Cause (Power 5 V + `WIFI_OFF`-Diskriminierung) | **offen** | parallel; Soft-Detach bleibt Workaround |
| **P1f** | RAM Offline-Capture (WLAN aus, Stufe 1) | **done (0.2.19–0.2.23)** | Lab E2E `ram_offline` 0.2.23 (`181642`); Soft-USB+TCP-Pump. **Auto Offline noch fail** — siehe [s2-auswertung](2026-10-15-bmw-s2-0.2.23-auswertung.md) |
| **P2** | Feld **S2/S3** (höchster Nutzen/Minute) | **teilweise (S2 Abend)** | S2 Live 0.2.23: 8 brauchbare Sessions, File-Gap-p50 ~3783 µs; Marker Autoplay/Tip/RST; S3 noch offen |
| **P3** | S1 Eager-EOF | **offen** | Medium: Mess-Slots **≥4 MiB** (Audit); 512 KiB nur Kontrolle |
| **P4** | S5 Fehler-Recovery (STALL/CHECK/BOT-Reset-Injektion) | **offen** | nach P1b; natürliche STALL-Baseline aus P0b |
| **P5** | S6 Identity-Matrix · S7 Lifecycle | **offen** | nach Semantik-Kern |
| **P6** | Mazda | zuletzt | Stub bleibt |
| — | Desk-Audit S1/S2/S3 Messplan | **done** | technology-audit PR #9 / Method `msc-host-read-semantics` — keine NBT-Zahlen |

## Sofort-Reihenfolge (pragmatisch)

1. **P0a–P0d** Offline auf vorhandenen Sessions (kein Auto nötig)  
2. **P1c** Profil-FACTs nachziehen (nach P0, damit keine falschen Burst-FACTs einwandern)  
3. **P1a + P1b** Instrument (t_us, MODE_SENSE6) → FW 0.2.18-Kandidat  
4. **P2** Feld S2/S3 mit Audit-Markern (Burst-Fenster)  
5. **P3→P5** S1 / S5 / S6·S7 · parallel **P2c** WiFi  

## Audit-Deliverables für Feld (Pins)

Aus `msc-host-read-semantics-s1s2s3-2026`:

| Deliverable | Kern |
|-------------|------|
| Marker S2/S3 | `play_start`, `track_next`, `audible_change`, `replay`, `stop` |
| Auswerte-Regeln | LBA-Monotonie, Bytes-bis-EOF, Reads vor hörbarem Wechsel, Replay ohne Replug |
| Medium S1 | Slots ≥4 MiB; 512 KiB trennt Eager vs. Batch nicht |
| Claim-Status | `eager_eof`/`cache_hit` → needs_field; `prefetch≈15 s` → hypothesis |

## Bewusst unverändert

- Wire / NBT-Sense = UNKNOWN  
- Lab-Zahlen ≠ BMW-Profil  
- Soft-Detach-Coex (0.2.16/17) = Lab-Workaround, nicht BMW-Duty-Cycle  
- massnahmen-01c P2 „lange unbroken BMW-Timeline“ bleibt **blockiert** — ersetzt durch Burst-Strategie  

## FW-Linie (Vorschlag)

`0.2.18` = MODE_SENSE6-Pad · `0.2.19` = RAM Offline-Capture (Messkonzept Stufe 1) · t_us (P1a) noch offen · Flash-Offline = Stufe 2 later.
