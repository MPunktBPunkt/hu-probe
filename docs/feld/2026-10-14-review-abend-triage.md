# Unabhängige Review-Triage — Abend 2026-10-14 (HEAD `87b5db0`)

**Quellen geprüft:** GitHub `origin/main` @ `87b5db0`, Sessions BMW `171815`/`172103` + Lab `181901`/`182045`/`182234`, `GATES.md`, `SCHEMA.md`, FW `MscBotTap`/`MscGadget`/`ReadInstrument`, Berichte Cursor/Grok/Mistral/GPT.

**Zweck:** Folge-KI ohne Reverse-Engineering; Epistemik erzwingen; Maßnahmen priorisieren.

---

## 1. Gesamturteil

Die drei Berichte (Cursor-Tabelle, Grok, Mistral) sind **zahlenmäßig weitgehend korrekt** und epistemisch sauber bei Lab≠BMW. GPT ergänzt den **wichtigsten übersehenen Befund**: `seq`-steigend bei fallendem `t_us` in Lab **und** BMW.

**Aktueller Stand ist reifer als „kann die Probe messen?“ — aber nicht so sauber, wie Mistral/Grok für eine sofortige Lab-0.2.11-Baseline nahelegen.**

| Claim | Verdict |
|-------|---------|
| BOT tinyusb unter NBT-Playback erreicht | **FACT** — beibehalten |
| GATE-01 closed / GATE-01B „fertig“ | **REJECTED** — BOT-Kern ja; Control/STALL/Dry-Run nein (`GATES.md` open) |
| Lab Gap 4,1–4,3 ms vs BMW 3,6–3,7 ms = Host-Diff | **REJECTED** — unterschiedliche FW-Generationen |
| WiFi-Death: Soft unschuldig, `dd` killt | **FACT** (Lab-Repro) |
| Root-Cause = RF/WiFi-Task | **HYPOTHESIS** — nur Korrelation belegt (GPT richtig strenger) |
| README noch auf 0.2.9 | **REJECTED** (Grok) — README zeigt 0.2.11 |
| Gap-p50 aus `read.gap_us` weiterhin brauchbar | **FACT mit Vorbehalt** — siehe §3 |
| Lab/BMW 0.2.11 Sessions als Timing-Baseline | **FAIL bis Fix** — `t_us`-Regressionen systematisch |

---

## 2. Was an den Berichten stimmt

- Nebeneinander-Tabelle (Medium, Mode 4096, Gap-Bänder, WiFi, Sessions) deckt sich mit Docs + Rohdaten.
- Priorität Lab-Homogenität auf 0.2.11 + lange unbroken BMW-Timeline + Restart ~25 s + S1/S2/S3 später: sinnvoll **nach** Timing-Fix.
- SCSI-Begleitverkehr (TUR ≫ READ10 + prevent/mode/capacity/inquiry): bestätigt.
- Wire-Ebene bewusst UNKNOWN: korrekt.
- Mazda hinten: korrekt.
- Short-SID-Hygiene (`2218`/`2296`/`2334`): echte Audit-Schwäche, aber **nicht** P0.

---

## 3. Kritischer Neufund: `t_us`-Regression (GPT bestätigt + ausgeweitet)

### Messung

| Session | `t_us`-Regressionen | davon `bot_data`→`read` | `bot_data.t > read.t` (same tag) |
|---------|---------------------|-------------------------|----------------------------------|
| Lab `181901` | 5 | (dominant) | **138** |
| Lab `182045` | 8 | — | 132 |
| BMW `171815` | **51** | **49** | 105 |
| BMW `172103` | 6 | — | 46 |

Beispiel (GPT, verifiziert) Lab `181901`:

```
seq 12 bot_data t_us=5704612
seq 13 read     t_us=5700997   ← 3615 µs früher, seq später
```

`seq` lückenlos; Schema verlangt primär monotonisches `t_us` für Gap-/Duration-Analysen aus der Event-Timeline.

### Ursache (FACT, Code)

Zwei Emit-Pfade mit unterschiedlicher Zeitquelle:

1. **`MscBotTap` Pending-Ring** (0.2.11): `emitDataChunk` speichert **kein** Capture-`tUs`. Beim `drain()` ruft `emitBotDataTinyusb()` → `stamp()` = **Loop-/Drain-Jetztzeit**.
2. **`MscGadget` Sample-Ring**: speichert `s.tUs = t0` (MSC-Callback). `emitReadAggregate(s.tUs, …)` stempelt **Capture-Zeit**.

Drain-Reihenfolge: oft zuerst BOT-JSON (`bot_data` mit späterem Stamp), danach Comfort-`read` mit früherem Stamp → **seq↑ / t_us↓**.

Der alte Synth-Pfad (`emitBotRead10Synth`) stempelte bewusst `stampAt(tUs)` für DATA+read „so t_us stays non-decreasing“ — TinyUSB-Pfad hat das beim Deferral verloren.

### Was trotzdem hält

- `read.gap_us` / `dur_us` kommen aus dem MSC-Callback (`t0`/`lastReadEndUs_`) und sind **nicht** aus der Event-Reihenfolge abgeleitet.
- Gap-p50 ~3,6–3,7 ms (BMW) bleibt als **Pace aus Sample-Feldern** belastbar.
- BOT-Bytes (`raw_hex`, `bot_source=tinyusb`, `synthesized=false`) bleiben FACT.

### Was nicht hält

- Jede Analyse, die „Timeline = sortiert nach `seq` und monoton in `t_us`“ annimmt.
- BOT-Kette CBW→DATA→read→CSW als **zeitlich geordnete** Spur ohne Korrektur.
- Neue Lab-0.2.11-Baseline / Validator „t_us monoton“ ohne Fix.

---

## 4. GATE-01 (unabhängige Checkbox)

| Block | Stand |
|-------|-------|
| Zeit & Sequenz (Capture-`t_us` auf Samples) | ✅ Samples; ⚠️ Event-Stream nicht monoton |
| BOT/SCSI tinyusb + Class-Driver | ✅ Feld bestätigt |
| MSC-Class-Control | ~ |
| SET_ADDRESS / SET_CONFIGURATION | ❌ |
| STALL / CLEAR_HALT / Recovery | ❌ (`bot_need_reset` Event da) |
| Wire | UNKNOWN |
| Lab-Dry-Run sauber | ❌ — WiFi-Repro ja; Timing-Baseline nein |

**GATE-01 bleibt open.** Formulierung „GATE-01B im Feld erreicht“ nur für **BOT-Kern**, nicht für Gate-Close.

---

## 5. WiFi-Death

| Aussage | Epistemik |
|---------|-----------|
| Sustained MSC-Reads → HTTP tot (Lab ~3–5 s; Auto unter Playback) | FACT |
| Soft-Cycle allein nicht der Killer | FACT (Lab Phase B) |
| MSC weiter nutzbar während WiFi tot | FACT |
| Keine Ring-Overflows bis zum Cut | FACT |
| Root-Cause = RF/Coexistence/Task-Starvation/Power | **UNKNOWN** (Hypothesen) |
| 0.2.11 Auto-Restart rettet ohne RST | FACT (~30–110 s) |

---

## 6. Maßnahmen (verbindliche Reihenfolge)

### P0 — Timing-Semantik reparieren (Blocker vor Baseline)

1. **Pending.Data um Capture-`tUs` erweitern** in `emitDataChunk` (wie `Sample.tUs = t0`).
2. **`emitBotDataTinyusb` → `stampAt(tUs)`** statt `stamp()`.
3. Ideal: CBW/CSW ebenfalls Capture-Zeit queueen (oder dokumentierte Emit-Zeit + separates `capture_t_us`); Mindestziel: **keine `bot_data`→`read` Regression** bei gleichem `bot_tag`.
4. Optional: Comfort-`read` erst **nach** zugehörigem `bot_data` im selben Drain mit `t_us >= bot_data.t_us` (oder read weglassen wenn BOT-Kette kanonisch).
5. **Validator:** fail bei `t_us[i] < t_us[i-1]` in `events.jsonl`.
6. Lab-Smoke auf `.88` nach Fix: Soft+`dd` kurz, Regression-Count = 0, dann erst Triple.

### P1 — Lab 0.2.11 homogenisieren

Nach P0: Power-Cycle → Triple Soft+Remount+`dd` → Gap-p50 + BOT-Counts + Health → `profiles/lab_debian.json` + Write-up. Lab≠BMW.

### P2 — Quick Win WiFi-Supervise

`WIFI_RESTART_AFTER_MS` 90000 → ~25000 (weniger tote Gelb-Zeit). Kein Ersatz für Root-Cause.

### P3 — Lange unbroken BMW-S0-Timeline

Stabiles WLAN, wenig Replug, Gap-Histogram + SCSI-Stats; WiFi-Drops als Marker, nicht als Pace.

### P4 — S1 / S2 / S3

Eager-EOF, Prefetch, Cache/Replug — Instrument reicht; Timing muss vorher sauber sein.

### P5 — GATE-01 Control/STALL (Desk fertig → Code)

Audit `usb-control-stall-recovery-2026` (`5fab7f5`) liefert die Hook-Karte. **Kein weiteres Fremd-Audit.**  
Umsetzung: [audit-control-stall-recovery](2026-10-14-audit-control-stall-recovery.md) · Maßnahmen [01c](2026-10-14-massnahmen-01c.md).  
Parallel zu P2/P3 ok **nach** P0; formaler Gate-Close erst nach Lab-Dry-Run inkl. Kill-Kriterien.

### Nicht jetzt

- Mazda
- Weiteres Control/STALL-Fremd-Audit
- Voller WiFi-Root-Cause vor jeder HU-Messung
- 8 MiB Medium bevor S1 an 512 KiB scheitert
- Lab-Zahlen als BMW-Signatur

---

## 7. Nachtrag: Control/STALL-Audit (PR #6, `5fab7f5`)

Intake: [2026-10-14-audit-control-stall-recovery.md](2026-10-14-audit-control-stall-recovery.md).

| Audit-Ergebnis | Wirkung auf hu-probe |
|----------------|----------------------|
| SET_ADDRESS / SET_CONFIGURATION → Mount erst bei cfg n | Implementierungsziel; Checkbox bleibt offen bis Code+Lab |
| Clear-Halt heilt NEED_RESET **nicht** | Logger-Regel; Epoch-Trennung |
| Replug ohne VBUS oft ohne Umount | State bei Mount clearen |
| USB↔WiFi nur HYPOTHESIS | bestätigt unsere WiFi-Epistemik |
| Timing/S1–S3 out of scope | P0 Timing bleibt eigene Arbeit |

---

## 8. Context-Box für die nächste KI

```
Projekt: github.com/MPunktBPunkt/hu-probe HEAD 87b5db0 (2026-10-14 Abend)
  + lokale Docs: review-abend-triage, audit-control-stall, massnahmen-01c, GATES update
Zweck: USB-MSC-Sonde ESP32-S3; BMW NBT=Golden; Mazda=Stub; lab_debian=Instrument-Ref.
  Lab<->BMW nur nebeneinander; nie Mazda-Uebertrag; Wire=UNKNOWN.

Instrument: FW 0.2.11 Head. 0.2.8 tinyusb CBW/CDB/CSW; 0.2.9 mscd_xfer_cb+need_reset;
  0.2.10 RGB-LED; 0.2.11 deferred BOT ring + WiFi supervise (~90s restart).

KRITISCH: Event-Stream t_us nicht monoton. Ursache: bot_data stamp()=drain-now,
  read stampAt(sample.tUs=callback). Lab 181901 + BMW 171815 betroffen.
  gap_us auf read bleibt Sample-Pace (p50 BMW ~3.6-3.7ms OK als Feld).
  P0: Pending.Data + stampAt Capture-Zeit; Validator t_us regression.

AUDIT Control/STALL (technology-audit 5fab7f5, Kampagne closed): Hook-Karte fertig —
  SET_ADDR/SET_CONFIG/mount_cb; Clear-Halt != NEED_RESET Heilung; Mount-Epoch bei
  Replug ohne VBUS; WiFi Issues = HYPOTHESIS only. Intake:
  docs/feld/2026-10-14-audit-control-stall-recovery.md. KEIN weiteres Fremd-Audit.
  Naechste Arbeit: Code (P0 Timing + P1 Control-Hooks) -> Lab-Dry-Run.

GATE-01: OPEN. BOT-Kern Feld-OK; Control/STALL Desk-Karte da, Implementation fehlt;
  t_us Event-Monotonie fehlt; Dry-Run fehlt. Nicht als closed formulieren.

BMW-S0 FACT (0.2.11): Mode 4096; Gap-p50 3586-3738 us (aus read.gap_us);
  tur>>read10 + prevent/mode/capacity/inquiry; OTG 2296 Serial HP0001->HP0002;
  dropped=0. Sessions: 165304(0.2.10), 171815/172103/2334, OTG 2218/2296; hist 081637.

Lab FACT: WiFi-Death Repro 181901/182045/182234 — dd killt HTTP ~3-5s, Soft allein OK;
  Peak ~443 KB/s; keine Overflows bis Cut. Gap-p50 4136-4336 us nur 0.2.6/0.2.7 Docs
  — NICHT mit BMW 0.2.11 vergleichen bis nach P0+Lab-Triple.

Geraete: Lab .88, Auto .89, Collector .187:9100 / Web :8088.
Docs: bmw-s0-0.2.11-auswertung, wifi-death-0.2.11, review-abend-triage,
  audit-instrument-sources, audit-control-stall-recovery, massnahmen-01c, GATES.md.

Next: P0 t_us-Fix (+ P0b Restart~25s) -> P1 Control/Mount/STALL-Epochs (0.2.12)
  -> P2 Lab-Triple + BMW lange Timeline -> P3 S1/S2/S3 -> GATE-01 Close. Mazda spaeter.

Never: Lab=BMW Signatur; events.jsonl ueberschreiben; synth als Bus; Wire-Claims;
  Gap-Ausreisser (s/min) als Host-Pace; Root-Cause-WiFi als FACT; Baseline vor P0;
  Clear-Halt als NEED_RESET-Heilung; weiteres Control-Fremd-Audit.
```

---

## 9. Quellen

- Repo HEAD `87b5db0` · Sessions wie oben · `firmware/src/msc/{MscBotTap,MscGadget,ReadInstrument}.*`
- Fremdberichte: Cursor-Tabelle, Grok, Mistral, GPT (Abend 2026-10-14)
- technology-audit: `usb-control-stall-recovery-2026` @ `5fab7f5`
- Vorläufer: `2026-10-14-review-triage.md` (0.2.2-Ära)
