# GO — kurze Abendfahrt (~10 min) · 2026-11-12

**Ziel:** Feldplan-Job `s158` (`size_158`, 10 min, fine) — nächster Leiter-Schritt nach PASS Schritt-1 heute morgen.
**FW:** 0.3.41 · **Probe:** `.89` · **Plan:** `2026-11-13a` (geladen, `runs_done=0`, next=`s158`)
**Lab:** Collector `192.168.178.187:9100` läuft · `arm_at_home=false` → Messung startet **nicht** im Heimnetz.

## Vor dem Losfahren (jetzt)

- [x] Collector up
- [x] Plan `2026-11-13a` auf Sonde (`fetched`, active, next_job=s158)
- [x] `autoCapture=true`, Trips pending=0
- [ ] HU an, Stick sichtbar
- [ ] Soft-Cycle (BOOT doppelt), TRACK1 Autoplay

## Während der Fahrt

1. Zündung / RST **außerhalb Heimnetz** (oder WLAN weg) → Sonde arm’t `s158`.
2. LED **lila** = Offline-Capture · TRACK1 hörbar halten.
3. Fenster ≈ **10 min** (`measure_s=600`). Danach Soft-Detach / LED wechselt — OK.
4. Magenta blink → Soft-Cycle. Rot blink → notieren, weiterfahren ok.
5. **Kein RST** mitten in lila Phase (sonst fehlen ≤~90 s am Ende).

## Zurück

1. Heimnetz gewinnen lassen (**kein Hotspot**).
2. Upload idle abwarten · Session erwartet: `fp_2026-11-13a_s158_r1` bzw. `drive_home` + media `size_158`.
3. Lab wertet aus (`integrity PASS`, `replay_records≈offline_records`).

## LED

| LED | Bedeutung |
|-----|-----------|
| lila fest | Messung läuft |
| grün fest | HU liest |
| blau blink | Upload — nicht RST |
| magenta blink | Soft-Cycle |
| gelb/orange blink | Trip im Flash, RST ok |
