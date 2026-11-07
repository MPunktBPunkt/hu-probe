# BUG: NEED_RESET/Magenta dauerhaft + HU „kein Medium“ (0.3.20, seq_size_ladder)

**Datum:** 2026-10-26 · **Gerät:** Auto-ESP `.89` (ESP32-S3 N8R2) · **FW:** **0.3.20**  
**Kontext:** P0 Größenleiter [`2026-10-24-feldplan-groessenleiter-p0.md`](2026-10-24-feldplan-groessenleiter-p0.md) · Sequenz `seq_size_ladder`  
**Status:** Fix in **0.3.21** (Soft-Detach heilt Latch + TinyUSB-Stage) — **Feld 27.10. Soft-Cycle/lila OK** (kein sticky Magenta); Größenleiter-Messung separat fail (Upload/RAM, siehe [`2026-10-27-schritt1-auswertung.md`](2026-10-27-schritt1-auswertung.md))

---

## 1. Kurzfassung

Nach OTA **0.3.20** und Start von `seq_size_ladder` (Schritt 1 / `size_158`) blieb die Status-LED **dauerhaft magenta** (`MSC_STAGE_NEED_RESET`). Operator-**Soft-Cycle** änderte die LED **nicht**. Später meldete die BMW-HU **kein Medium**, obwohl der Stick steckte. HTTP von Lab aus war über längere Strecken tot (`.89` unreachable). Messung abgebrochen.

---

## 2. Repro (Feld)

| # | Aktion | Beobachtung |
|---|--------|-------------|
| 1 | OTA `hu-probe.0.3.20.esp32s3.bin` → `.89` (vorher 0.3.19) | OK, Presets `size_158`/`size_1m`/`size_4m`/`size_16m` sichtbar |
| 2 | `POST /api/sequence/start` `{"id":"seq_size_ladder"}` | Runner `prepare` → `wait_driver`, Prompt Fresh 158k |
| 3 | Lab: `POST /api/usb/cycle`, dann `POST /api/sequence/next` | Cycle OK; **`next` ließ Runner in `wait_driver`** (Arm unklar) |
| 4 | HU: Medium neu gelesen, Autoplay TRACK1→2→3 Loop | Operator hörte/spielte Tracks |
| 5 | LED | **magenta** (nicht lila-fest / nicht grün) |
| 6 | RST durch Operator | Sequenz idle; neu gestartet + erneut Soft-Cycle |
| 7 | Erneut Autoplay-Loops | wieder **magenta** |
| 8 | Soft-Cycle (Operator, vor Abbruch) | **keine LED-Änderung** |
| 9 | Weiterfahrt | Magenta blieb; HU: **kein Medium** |
| 10 | Lab-Polls | `http://192.168.178.89/api/status` → Timeout / HTTP 000 |

Zeitfenster ca. 08:09–08:18 UTC+1 (Stand/kurze Messung), Magenta danach unterwegs bis Abbruch ~08:18+.

---

## 3. Erwartet vs. Ist

| | Erwartet | Ist |
|--|----------|-----|
| Nach Soft-Cycle vor Fresh-Arm | Enum OK, LED cyan/grün bei Reads bzw. **purple solid** bei Offline-Capture | **Magenta blink** (NeedReset) |
| Soft-Cycle während/nach NeedReset | Latch heilen oder Capture sauber beenden | LED unverändert |
| HU während Sequenz-Schritt | Stick mit `size_158` Tracks sichtbar | zeitweise Autoplay OK, später **kein Medium** |
| Lab-Erreichbarkeit | nach Capture-Ende WLAN wieder da | lange unreachable; Abort/`offline/stop` von Lab unmöglich |

---

## 4. Verdacht (für Fix-Agent)

1. **`NEED_RESET`-Latch** nach absichtlichem Soft-Cycle / Sequenz-Medienwechsel wird nicht geheilt; LED priorisiert Magenta über Capture-State (`StatusLed`: magenta > purple).
2. Gadget bleibt für die HU **ohne gültiges Medium** (soft-detached / stalled BOT / Media-Hide hängen), obwohl Operator Soft-Cycle versucht.
3. **`/api/sequence/next` armte nicht** zuverlässig aus `wait_driver` nach Lab-Cycle — möglicher zweiter Bug (Sequenz vs. OfflineCapture idle / Prepare).
4. Coex: Soft-Cycle + Offline-Capture → WLAN weg, Lab blind; ohne heilenden Cycle/RST kein Upload/Abort.

Verwandt (nicht identisch): Trip-Upload-NeedReset-Verwechslung bis 0.3.7/0.3.8 ([`2026-10-21-trip-upload-hang-phasegate.md`](2026-10-21-trip-upload-hang-phasegate.md)); hier **kein** Trip-Upload, sondern Sequenz + Soft-Cycle + anschließend **kein Medium**.

---

## 5. Artefakte / offene Checks daheim

- Trips auf ESP prüfen: `GET /api/trips` (nach RST/WLAN) — RAM-Capture des Fresh-158k-Laufs vermutlich verloren.
- `GET /api/diag/log`, `panic_tail` / `phase_gate`, `bot_need_reset` Events falls Trip doch da.
- Collector `192.168.178.187:9100` — Session `20261026-081123_bmw_free` war kurz live vor erstem Cycle; Nachlauf offline.

---

## 6. Akzeptanz Fix

- Soft-Cycle (API + BOOT doppelt) heilt NeedReset bzw. remountet Medium so, dass HU wieder Tracks sieht.
- Während Offline-Capture: LED purple (nicht permanent NeedReset-Magenta), oder NeedReset nur transient + Auto-Heal.
- `seq_size_ladder` Schritt 1: nach Cycle+Arm Capture läuft, HU Medium sichtbar, Lab nach Capture-Ende wieder HTTP-OK.
- Regression: absichtlicher Soft-Detach bei Trip-Upload zeigt weiter Upload-Blau, nicht falsches Magenta.

---

## 7. Fix (0.3.21)

- `MscGadget::softDetach` / Media-`usbCycle` rufen `MscBotTap::clearNeedReset()` (vorher nur Trip-Upload).
- `clearNeedReset` setzt bei stuck `MSC_STAGE_NEED_RESET` auch `_mscd_itf.stage` → `CMD` (+ Len-Felder), damit `poll()` nicht sofort neu latcht.
- Soft-Cycle (API + BOOT 2×) soll Magenta heilen und BOT wieder für die HU nutzbar machen.

## 8. Nicht tun / Scope

- Kein PiDrive-Flash auf `.89` für diesen Bug.
- Größenleiter-Messung erst nach 0.3.21-OTA wiederholen.
- Identity/Serial-Experimente irrelevant.
- `wait_driver` nach `next` vor 3 s Settle ist erwartetes Verhalten (kein Arm-Bug, sofern danach `running`).
