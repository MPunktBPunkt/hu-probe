# Unabhängige Review-Triage — BMW S0 (2026-10-14)

Quellen: Mistral-Handoff, GPT-Prüfbericht, Grok-Prüfbericht, Re-Audit von `events.jsonl` / Firmware.  
Repo-HEAD Feldauswertung: `fd5dff2`. Maßnahmenstand: FW **0.2.4** (Capture-`t_us`, TCP peek→pop, 512 KiB-Medium, Collector-Seq-Audit).

## Gesamturteil

**Instrumenten-Test bestanden.** Messkette NBT → USB → ESP → Ring → WiFi/TCP → Collector hat ~6 min und 11 physische Replugs mit `dropped=0` / `complete=true` gehalten.  
Erste belastbare Host-Signatur vorhanden; GATE-01 bleibt offen.

## Abgleich der drei Berichte

| Punkt | Mistral | GPT / Grok | Re-Audit |
|-------|---------|------------|----------|
| Capture-Integrität | korrekt | korrekt | bestätigt |
| 4 KiB / ~3,7 ms | „Golden bestätigt“ zu weit | FACT Session + INFERENCE PiDrive | GPT/Grok folgen |
| Identische Fenster | **7/8 falsch** | **6/8 korrekt** | bestätigt |
| Fenster 3 | unterbelichtet | wichtig (kein DATA) | bestätigt |
| Fenster 7 = Doppel-Enum | Hypothese als Fakt-nah | Hypothese korrekt markiert | **plus Drain-`t_us`-Bug** |
| Fenster 4–6 fehlen | „Fingerprint-Bug“ | klären | **kein Bug:** 0 Reads, weggelassen |
| Seq in `usb_phase` | als Risiko | „nur Derivatsprung“ | bestätigt; **aber** echte Lücke 249–250 in `events.jsonl` |
| Detach→Attach | ~2,4–6,9 s | ungenau | **2,294–6,011 s** (host) |
| Medium 256 KiB | korrekt limitierend | korrekt | bestätigt |
| GATE-01 offen | korrekt | korrekt | bestätigt |
| Nächste Prio | Medium früh | Validator zuerst | Validator + **t_us-Fix** zuerst |

**Mistral:** ~8/10, gute Struktur, 7/8 und zu lockere Golden-Formulierung korrigieren.  
**GPT/Grok:** methodisch strenger; P0-Validator-Priorität richtig. Beide haben den Drain-Stamp-Bug nicht gesehen.

## Neue FACTs aus dem Re-Audit

1. **Drain-Zeit-Stamp (FW ≤0.2.2):** `MscGadget::drainSamples()` → `onReadTimed()` → `stamp()` setzt `t_us = now` (Loop), obwohl `Sample` Capture-Zeit und `gap_us` schon hält. Dadurch kollabiert Fenster 7 in ~27 ms *nach* Detach; `gap_us` bleibt die echte Pace-Spur.
2. **Seq-Lücke:** `249`, `250` fehlen in `events.jsonl`; nicht in status/markers. `dropped=0` ⇒ Seq-Audit ≠ Capture-Health-Feld.
3. **Leere Plugs 4–6:** drei Host-Attaches ohne Read-Events im Stream (Firmware hat nichts emittiert oder HU hat nicht gelesen) — Fingerprint-Index-Lücken sind Filter, kein Off-by-one.
4. **Host detach→attach:** 2,294 / 2,373 / 2,414 / 2,930 / 3,328 / 3,379 / 3,884 / 4,442 / 4,998 / 6,011 s (plus 0,244 s direkt nach Soft-Cycle).

## Epistemische Wissensbasis (Übernahme)

| Status | Aussage |
|--------|---------|
| FACT | 0.2.2 capturt ~6 min im NBT ohne EventBus-Drops / TCP-Disconnect-Zähler |
| FACT | 11 host Unplug/Plug; 240 Reads; Mode 4096; Gap-p50 3724 µs |
| FACT | 6/8 nicht-leere Fenster = 22-Read-Muster |
| FACT | Fenster 3: 14 Reads ohne DATA |
| FACT | Fenster 7: 94 Reads / 282 KiB mit Mehrfach-BOOT/FAT/ROOT/DATA in `gap_us` |
| FACT | Read-`t_us` in dieser Session ist Drain-Zeit (Instrument) |
| FACT | Seq 249–250 fehlen |
| FACT | 256 KiB ⇒ kein Eager-Stream-Claim |
| INFERENCE | Pace konsistent mit früherer PiDrive-~4 ms-Beobachtung |
| HYPOTHESIS | Fenster 7 = Mehrfach-Scan während Plug; Emission nach Recovery |
| UNKNOWN | Ursache Fenster 3; Ursache Seq-Lücke; BOT/CBW/CDB/CSW; ≥512 KiB-Verhalten |

## Maßnahmen (priorisiert)

### P0 — Sofort

| ID | Maßnahme | Status |
|----|----------|--------|
| P0.1 | `tools/validate_session.py` | ✅ |
| P0.2 | `tools/fingerprint_session.py` | ✅ |
| P0.3 | FW **0.2.3**: Capture-`t_us` | ✅ Code/Bin; OTA offen |
| P0.4 | Feldauswertung korrigieren | ✅ |

### P1 — Labor-/Feldschleife

| ID | Maßnahme | Status |
|----|----------|--------|
| P1.1 | 0.2.4 auf `.88`/`.89` flashen | ⏳ Geräte offline (Hub: noch PiDrive-FW) |
| P1.2 | Seq-Lücke: TCP `pop` vor TX-Erfolg | ✅ Fix 0.2.4 + Collector `recv_seq_gaps` |
| P1.3 | Fenster 3/7 × Marker | ✅ `tools/correlate_windows.py` + Auswertung |
| P1.4 | `msc_sample_overflow` / `published` / `tcp_tx_*` in capture_health | ✅ |
| P1.5 | EventBus/Session seq thread-safe (`portMUX`) | ✅ |

### P2 — Medium & Szenarien (BMW)

| ID | Maßnahme | Status |
|----|----------|--------|
| P2.1 | ≥512 KiB Silence-MP3 Demo-FAT | ✅ `tools/build_demo_fat.py` (3×158 KiB) |
| P2.2 | S1 Eager-EOF, S2 Prefetch, S3 Cache-Hit | offen (nach OTA) |
| P2.3 | S5 Busy-Budget, S6 Identity-Matrix | offen |

### P3 — GATE-01 schließen

| ID | Maßnahme |
|----|----------|
| P3.1 | Roh BOT CBW/CDB/CSW + Decode im Stream |
| P3.2 | USB Control/Deskriptor-Capture |
| P3.3 | `fw_sha` / Build-ID; `probe_identity` (Serial, Volume) in `session.json` |
| P3.4 | VBUS-Tail-Ring: letzte Samples über Power-Loss puffern und nachreichen |

### P4 — Erst danach

Mazda Discovery — Profil bleibt null, bis eigene Sessions existieren. Keine NBT→Mazda-Übertragung.

## Context-Box (Copy/Paste für Folge-KI)

```
Projekt: github.com/MPunktBPunkt/hu-probe
Zweck: USB-MSC-Sonde ESP32-S3; BMW NBT=Golden; Mazda=Stub/null. FACT nur eigene Session.
Status: FW 0.2.4 Bin ready (t_us + TCP peek/pop + 512KiB FAT); Feld noch 0.2.2. GATE-01 OFFEN.
Session: sessions/20261014-081637_bmw_s0_plug_enum — dropped=0, seq fehlt 249/250
  (Ursache: pop-before-TX bis 0.2.3), 4KiB, gap_p50=3724us, 6/8=22-Read,
  Fenster3=kurze Replug-Dwell ohne DATA, Fenster7=94 Reads Drain-Artefakt.
Tools: validate_session, fingerprint_session, correlate_windows, build_demo_fat
Next: OTA 0.2.4 → Lab S0 → BMW S1/S2 → GATE-01 BOT → Mazda
Never: NBT→Mazda; events.jsonl überschreiben; Drain-t_us als HU-Pace lesen.
```

## Validator-Lauf (diese Session)

```bash
python3 tools/validate_session.py sessions/20261014-081637_bmw_s0_plug_enum
```

Erwartung: FAIL auf Seq-Lücke + Drain-Heuristik (historische 0.2.2-Session).
