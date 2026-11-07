# GO — Größenleiter Retest 2026-10-27

**Ziel:** P0 Größenleiter nach Fix **0.3.21** (gestern Magenta/`NEED_RESET` + HU „kein Medium“ auf 0.3.20).  
**Gerät:** Auto-ESP `.89` · **Bin:** `firmware/dist/hu-probe.0.3.21.esp32s3.bin`  
**Plan:** [`2026-10-24-feldplan-groessenleiter-p0.md`](2026-10-24-feldplan-groessenleiter-p0.md) · Bug: [`2026-10-26-bug-need-reset-magenta-no-media.md`](2026-10-26-bug-need-reset-magenta-no-media.md) · Issue #9

Lab startet OTA + Sequenz, sobald `.89` online ist. Du bedienst nur HU + BOOT.

---

## 0. Vor dem Stecken (Lab)

| # | Check |
|---|--------|
| 1 | Collector `192.168.178.187:9100` läuft |
| 2 | OTA **0.3.21** auf `.89` (nicht 0.3.20) |
| 3 | `GET /api/status` → `fw=0.3.21` |
| 4 | Presets `size_158` / `size_1m` / `size_4m` / `size_16m` sichtbar |
| 5 | `POST /api/sequence/start` `{"id":"seq_size_ladder"}` |

---

## 1. LED-Schnellkarte

| LED | Bedeutung | Aktion |
|-----|-----------|--------|
| **weiß blink** | Sequenz wartet auf dich (`wait_driver`) | BOOT kurz = Arm/Start Schritt |
| **lila fest** | Offline-Capture aktiv (WLAN absichtlich aus) | OK — Tracks spielen |
| **grün fest** | HU liest aktiv | OK |
| **cyan** | USB steckt, ruhig | OK |
| **magenta blink** | NEED_RESET — **sollte nach Soft-Cycle weg sein** | Soft-Cycle (BOOT doppelt); wenn bleibt → Bug #9 nicht geheilt, RST + melden |
| **blau blink** | Trip-Upload (Detach absichtlich) | warten |
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

---

## 3. Fix-Akzeptanz (heute mitprüfen)

- Soft-Cycle heilt Magenta (LED wechselt cyan/grün/lila).
- HU zeigt Medium / Tracks nach Cycle.
- Schritt 1: Capture läuft (lila), HU spielt TRACK1–3.
- Nach Capture: `.89` wieder per HTTP erreichbar (oder nach Soft-Cycle).

Wenn Magenta **nach Soft-Cycle bleibt** oder HU „kein Medium“: Soft-Cycle 1×, sonst RST; Sequenz neu; Lab melden — kein PiDrive-Flash.

---

## 4. Abbruch / Fahren

- Sequenz idle lassen oder BOOT lang = Skip.
- Bei Fahrt vor Capture-Ende: LED ggf. lila (WLAN aus) — normal; Upload daheim.
- Notizen: Magenta ja/nein, Medium sichtbar, hörbarer Start @16 MiB, Step-Nr. bei Abbruch.
