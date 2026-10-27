# Auswertung Schritt 1 — Größenleiter 2026-10-27

**Session:** `sessions/20261027-080529_bmw_seq_size_ladder_1r1`  
**FW:** hu-probe **0.3.21** · Sequenz `seq_size_ladder` Schritt 1 · Medium `size_158`  
**Operator:** Soft-Cycle + Arm Lab; Autoplay TRACK1–3 ~4 min; LED lila fest (kein Magenta-Blink)  
**Verdict:** **kein Mess-PASS** — Capture-Body fehlt auf dem Collector

---

## Was ankam

| Größe | Wert |
|-------|------|
| `events.jsonl` | ~697 KB / 1826 Zeilen (~63 % CBW, ~37 % CSW) |
| `bot_cbw` | 911 — **alle** `origin=post_finalize` |
| SCSI | 455× TUR + 455× REQUEST_SENSE + 1× PREVENT_ALLOW |
| Muster | ~2,24 s TUR→REQUEST_SENSE über ~16,7 min Hide-Poll |
| **READ10** | **0** |
| Read-Episoden / Fenster / EOF-Probe | leer |

Rohdaten getrackt: `sessions/20261027-080529_bmw_seq_size_ladder_1r1/` (+ `derived/`).

`analyze_session.py --kbps 128` → `episodes=0`, `read10=0`. Analyse unter `derived/analysis.md`.

Das ist der **Media-Hide-Nachlauf** (HU pollt nach `auto_media_hide` am Capture-Ende), nicht der Fresh-158k-Play.

---

## Was der ESP während Capture hatte

Aus Live-Status vor Abbruch:

- `records=535`, `bytes≈11832`, `fine_data=true`, `stop_reason=auto_media_hide`
- `read_count≈171` während Play (Lab-Poll)
- Upload `replaying`, `media_hidden=true`, Trips-Flash **`pending=0`** (RAM-only)

Beim Losfahren: WLAN weg → Live-Replay abgebrochen. Auf dem Collector landete praktisch nur der (lange) post_finalize-Strom; der **RAM-Capture-Body ist verloren**.

---

## Betriebsfazit

| Thema | Stand |
|-------|--------|
| Fix 0.3.21 Soft-Cycle/Magenta | **feld-OK** (lila fest, Medium spielte, kein sticky Magenta) |
| Größenleiter Schritt 1 Messdaten | **fehlgeschlagen** (Upload/Persistenz) |
| PiDrive Q16 / Play-Slot-MiB | **unverändert offen** |

**Abend:** Schritt 1 wiederholen; Upload **fertig** abwarten (oder Trip-Persist prüfen), erst dann fahren. Danach Schritt 2 CYCLE 158k.
