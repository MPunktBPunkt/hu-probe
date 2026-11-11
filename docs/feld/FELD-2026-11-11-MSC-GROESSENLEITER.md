# Feld 2026-11-11 — MSC Größenleiter (0.3.38)

| | |
|--|--|
| **Anlass** | Retest `seq_size_ladder` nach Soft-Detach-Fix (0.3.38) |
| **DUT** | Auto-ESP `.89` · FW **0.3.38** (`c5c4123ed625-dirty`) |
| **Collector** | `192.168.178.187:9100` |
| **GO** | [MSC-GROESSENLEITER-GO.md](MSC-GROESSENLEITER-GO.md) |
| **Roh** | [artifacts/20261111-msc-groessenleiter/](artifacts/20261111-msc-groessenleiter/) · Sessions unten |

Abbruch nach Schritt 2 (Fahrt). Schritte 3–8 nicht gelaufen.

---

## Brauchbar?

**Teilweise — Ops ja, Body-Upload nein.**

| Messung | Nutzen |
|---------|--------|
| Soft-Detach am Measure-Ende | **PASS** — HU „USB nicht verbunden“, `end_reason=auto_soft_detach` |
| Capture / Autoplay `size_158` | **PASS** — lila, TRACK1–3 hörbar (Fresh + Cycle) |
| RAM-Replay → Collector | **FAIL** — `replay_records=0`, `upload_status=replay_incomplete`, integrity FAIL |
| Live-TCP vor Sequenz | **PASS** — HU las Medium (`307` reads) |

**0.3.38 hat die gestrige TUR/`media_hide`-Ops-Falle entschärft, den Replay-Body aber nicht geliefert.** Gleicher Kernbefund wie 2026-11-10 (dort noch `media_hide` / falsches `delivered`).

---

## FACT

### Ablauf

1. Lab: Sequenz `seq_size_ladder` gestartet; Medium `size_158`.
2. Schritt 1 Fresh: Soft-Cycle → Autoplay → Arm (Lab `sequence/next` + Operator); Capture lila ~4 min.
3. Soft-Detach → Upload `replaying` → Session `…_1r1` ohne Body → `wait_driver` Schritt 2.
4. Schritt 2 Cycle: Capture lila; Soft-Cycle während Capture; erneut Soft-Detach + Upload.
5. Weiß blink (Schritt 3 bereit) → Operator-Fahrt; Sequenz nicht zu Ende.

### Sessions

| Session | Transport | offline | replay | upload | integrity |
|---------|-----------|--------:|-------:|--------|-----------|
| `20261111-072308_bmw_free` | `tcp_live` | — | — | streaming | PASS (live) |
| `20261111-072943_bmw_seq_size_ladder_1r1` | `ram_offline` | 534 | **0** | `replay_incomplete` | FAIL |
| `20261111-073629_bmw_seq_size_ladder_2r1` | `ram_offline` | 535 | **0** | `replay_incomplete` | FAIL |

Beide Offline-Sessions: nur `session` + `media_layout` + `capture_health` (7 Events). Health: `end_strategy=soft_detach`, `persisted=false`, `fine_data=true`, kein `replay_error`.

`published` stieg live≈1856 → offline≈1874/1881 — Delta ≈ Meta/Health, **kein** Capture-Body.

### Operator

- Magenta: nein (in den zwei Schritten).
- OTG-Fallback: nicht nötig; Soft-Detach ausreichend für HU-Disconnect.
- Soft-Cycle vor/während Arm: Medium wieder sichtbar, Autoplay ok.

---

## Deutung (kurz)

- Measure-Pfad **zählt** Records im RAM (`offline_records≈535`).
- Upload meldet `replaying`, landet aber ohne Body → `replay_incomplete` (Drain: Ring leer, `replayComplete=false`).
- Wahrscheinlich: Replay-Phase bricht ab **bevor** `replaySeq_` hochzählt (Stall / early `replayOneRec()==false` ohne `replay_error`), nicht Collector-Parse der Body-Events (`parse_errors=0` pro Session).
- `beginFinalize` kann zusätzlich `media_hide` setzen (`media_hidden=true` trotz Soft-Detach-Ende) — Nebenbefund, nicht der Hauptblocker.

---

## NEXT

1. **FW:** Live-Offline-Replay diagnostizieren (DiagLog: `replayOff/used/pending/tx_ok` in Replay; silent `return false` in `replayOneRec` wenn `!buf_/!sessionStarted_`).
2. Lab-Repro: `seq_size_ladder` Schritt 1 mit Host am Bus vs. OTG gezogen während Upload.
3. Erst nach `replay_records>0` + `integrity PASS` Größenleiter 1→16 MiB fortsetzen.
4. Connected P1 unverändert parallel: [FIELD-CONNECTED-REAL-01.md](FIELD-CONNECTED-REAL-01.md).
