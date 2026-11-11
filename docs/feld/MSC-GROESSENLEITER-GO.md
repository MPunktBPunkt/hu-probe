# GO — Größenleiter Retest

**Ziel:** P0 Größenleiter (`seq_size_ladder`) — Play-Slot / Prefetch über 158 KiB→16 MiB.  
**Gerät:** Auto-ESP `.89` · **Bin:** `firmware/dist/hu-probe.0.3.38.esp32s3.bin`  
**Plan:** [`archiv/2026-10-22-msc-kampagne/2026-10-24-feldplan-groessenleiter-p0.md`](archiv/2026-10-22-msc-kampagne/2026-10-24-feldplan-groessenleiter-p0.md)

**Feld 2026-11-10:** Capture ok (lila, Play), aber RAM-Upload bei `media_hide` → `replay_records=0` / integrity FAIL (Host-TUR-Flut). Ab **0.3.38:** `free` endet mit **`soft_detach`**; Upload detacht bei noch gestecktem Host. Operator-Fallback: OTG ziehen sobald Capture-LED wechselt.

**Feld 2026-11-11:** FW 0.3.38, Schritt 1–2 (`size_158` Fresh+Cycle). Soft-Detach **ops PASS** (HU disconnect), Capture/Play ok — Body-Upload weiterhin **FAIL** (`replay_incomplete`, `replay_records=0`). Bericht: [FELD-2026-11-11-MSC-GROESSENLEITER.md](FELD-2026-11-11-MSC-GROESSENLEITER.md). **Kein weiterer Leiter-Lauf bis Replay-Fix.**

Lab startet OTA + Sequenz, sobald `.89` online ist. Du bedienst HU + BOOT (+ optional OTG).

---

## 0. Vor dem Stecken (Lab)

| # | Check |
|---|--------|
| 1 | Collector `192.168.178.187:9100` läuft |
| 2 | OTA **0.3.38** auf `.89` |
| 3 | `GET /api/status` → `fw=0.3.38` |
| 4 | Presets `size_158` / `size_1m` / `size_4m` / `size_16m` sichtbar |
| 5 | `POST /api/sequence/start` `{"id":"seq_size_ladder"}` |

---

## 1. LED-Schnellkarte

| LED | Bedeutung | Aktion |
|-----|-----------|--------|
| **weiß blink** | Sequenz wartet auf dich (`wait_driver`) | BOOT kurz = Arm/Start Schritt |
| **lila fest** | Offline-Capture aktiv (WLAN absichtlich aus) | OK — Tracks spielen |
| **lila → weg** (cyan/blau/…) | Measure vorbei, Upload startet | Soft-Detach sollte greifen; **Fallback: OTG ziehen**, Serial-USB am ESP dran lassen |
| **grün fest** | HU liest aktiv | OK |
| **cyan** | USB steckt, ruhig | OK |
| **magenta blink** | NEED_RESET | Soft-Cycle (BOOT doppelt); wenn bleibt → RST + melden |
| **blau blink** | Trip-/Upload-Phase | warten — **nicht RST** bis Upload idle |
| **rot blink** | Capture unhealthy | notieren, weiter oder Abbruch |

---

## 2. Operator-Ablauf (Stand, ~40–50 min)

BOOT: kurz = Marke / Arm · doppelt = Soft-Cycle · lang = Schritt skip.

| Schritt | Medium | Fenster | Du |
|--------:|--------|--------:|----|
| 1 | 158 KiB Fresh | 4 min | Soft-Cycle → TRACK1 Autoplay; **kein** weiterer Cycle |
| 2 | 158 KiB Cycle | 4 min | TRACK1; nach ~30 s Soft-Cycle; Resume prüfen |
| 3 | 1 MiB Fresh | 4 min | Soft-Cycle → TRACK1 |
| 4 | 1 MiB Cycle | 4 min | Cycle+Resume |
| 5 | 4 MiB Fresh | 4 min | Soft-Cycle → TRACK1 |
| 6 | 4 MiB Cycle | 4 min | Cycle+Resume |
| 7 | 16 MiB Fresh | 5 min | Soft-Cycle → TRACK1; **BOOT kurz bei Wahl + bei hörbarem Start** |
| 8 | 16 MiB Cycle | 5 min | Cycle nach ~60 s; Resume |

Zwischen Stufen wechselt der Runner das Medium (Soft-Remount) → vor jedem **Fresh** immer Soft-Cycle.

**Nach jedem Schritt:** Upload `idle` / Session mit `origin=offline` + READ10 abwarten, **erst dann** fahren oder nächsten Arm.

---

## 3. Fix-Akzeptanz (0.3.38)

- Soft-Cycle heilt Magenta; HU zeigt Medium nach Cycle.
- Capture: lila, TRACK1–3 hörbar.
- Nach Capture: Soft-Detach (kein dauerhaftes Host-Polling); Upload liefert Capture-Body (`replay` / `integrity PASS`), nicht nur `post_finalize`.
- Falls Host trotzdem pollt: OTG ziehen bis Upload idle, dann wieder stecken für nächsten Schritt.

**Stand 2026-11-11:** Punkte Capture + Soft-Detach-Ops erfüllt; **Body/`integrity PASS` offen** (beide gelaufenen Schritte).

---

## 4. Abbruch / Fahren

- Sequenz idle lassen oder BOOT lang = Skip.
- Bei Fahrt vor Capture-Ende: LED ggf. lila (WLAN aus) — normal; Upload daheim nur wenn Body in Flash/Trip — RAM-only nicht.
- Notizen: Magenta ja/nein, OTG-Zug ja/nein, hörbarer Start @16 MiB, Step-Nr. bei Abbruch.
