# Stored-Trip-Upload: Hang in `trip_replay` — Diagnose & Fixes (2026-10-21)

**Gerät:** Auto-ESP `.89` (ESP32-S3 N8R2)  
**Collector:** `192.168.178.187:9100`  
**Firmware-Spanne:** 0.3.3 → **0.3.15**  
**Aktuelles Bin:** `firmware/dist/hu-probe.0.3.15.esp32s3.bin`  
**Status:** 0.3.15 erstmals `tcp_ok=true` während #17-Replay, dann wieder WLAN-Zombie; #17/#18 noch pending.

---

## 1. Kurzfassung

Beim Hochladen ausstehender Offline-Fahrten (Trip-Journal → Collector) hängt die Firmware systematisch in der Phase **`trip_replay`** (Replay der gespeicherten Events über TCP). Typische Begleiterscheinungen:

| Symptom | Bedeutung |
|---------|-----------|
| LED Magenta (bis 0.3.7) | TinyUSB `NEED_RESET` nach absichtlichem Soft-Detach |
| LED blau hell/dunkel (ab 0.3.8) | Upload noch aktiv (`TripUpload`), USB absichtlich detached |
| LED Dimm-Blau + Web „STATUS unreachable“ | WLAN-Assoziation „zombie“: ESP meint verbunden, LAN erreicht `.89` nicht |
| Web/HTTP tot während Upload | Radio/Stack unter Replay-Last, TCP oft `connected=0` |

Die **Datenmengen sind klein** (ca. 13–85 KiB pro Trip). Die Dauer entsteht nicht durch Volumen, sondern durch **TCP-Tod + Reconnect-Schleifen + Stall-Timeouts** und früher durch **falsches NeedReset-Magenta** nach Soft-Detach.

---

## 2. Ausgangslage

Nach Heimfahrten lagen mehrere **pending** Trips im Flash-Journal (`TripStore`). Manuelles „Ausstehende hochladen“ sollte sie nacheinander an den Collector streamen (`OfflineCapture::replayStored` → EventBus → `TcpStreamer`).

Beobachtete Pending-Trips (BMW `drive_home`, FW 0.3.0 Capture):

| Trip | Bytes (ca.) | Verlauf 21.10. |
|------|-------------|----------------|
| #13 | 12 KiB | früher `delivered` |
| #15 | 13 KiB | `delivered` (nach Retries) |
| #16 | 38 KiB | `delivered` unter 0.3.8 nach ~3 min TCP-Kampf |
| #17 | 85 KiB | Hang: `last_crash = trip_replay / 17` |
| #18 | 51 KiB | noch pending |

---

## 3. Was schiefging (Root Cause)

### 3.1 Primärer Hang-Ort: `trip_replay`

PhaseGate (ab 0.3.5/0.3.7) und DiagLog belegen wiederholt:

```text
last_crash: { "phase": "trip_replay", "arg": <trip_id>, "depth": 2 }
```

Stack typisch: `trip_batch` → `trip_replay`. Der Absturz/Hang ist **nicht** erst Drain/Batch-Ende, sondern mitten im Replay.

### 3.2 TCP stirbt bei „WLAN verbunden“

Beispiel Replay #16 unter 0.3.8 (Diag):

```text
83759  replay hb trip=#16 ... wifi=1 tcp=1 lastOkAge=175ms
96867  replay tcp force-reconnect ... pending=1 lastOkAge=10015ms tcp=0
...    (weitere force-reconnect alle ~12 s, lastOkAge wächst)
186946 ENTER stored_oc_drain arg=16
308105 PASS stored_oc_drain  → später delivered
```

Also: **Assoziation bleibt**, Collector-Socket liefert kein `tcp_tx_ok` mehr. Ohne Force-Reconnect und Fortschritts-Stall (vor 0.3.8) konnte Replay weiter Events enqueuen und den Stall-Timer falsch zurücksetzen — Web/HTTP und Radio gingen unter.

### 3.3 Soft-Detach → NeedReset → Magenta

Trip-Upload macht absichtlich `tud_disconnect()` / Soft-Detach, damit OTG die Antenne nicht killt. TinyUSB latched dabei oft **`NEED_RESET`**. Die Status-LED priorisierte Magenta und suggerierte einen USB-Fehler, obwohl der Detach **gewollt** war. Zusätzlich blieb der Latch nach dem Upload stehen.

### 3.4 Früher: `drain_timeout` / Orphan-Ring

Unter 0.3.3 endete ein Trip als `queued` mit `pending=6` im EventBus; WebUi wartete danach bis zu 300 s auf Drain und brach den **ganzen Batch** ab (`drain_timeout`). Fix: Ring bei stored give-up clearen, kürzerer Drain, Abandon ohne Batch-Kill.

### 3.5 PhaseGate / `last_crash` und `power_on`

Operator-RST erscheint oft als `reset_reason=power_on` → RTC-NOINIT weg. Ohne NVS-Spiegel war `last_crash` leer. Ab 0.3.7: NVS + sticky + CRC-validiertes RTC; Diag-Snapshots enthalten `# phase open=…`.

---

## 4. Zeitlinie der Firmware-Fixes

| Version | Inhalt |
|---------|--------|
| **0.3.3** | Soft-Detach für stored Replay; Drain/Reconnect während Trip-Upload; kein `ESP.restart` mitten im Batch |
| **0.3.4** | Stored hard-timeout: Ring clear + Diag; WebUi-Drain mit Heartbeat, TCP force-reconnect, Abandon 60 s / Cap 90 s |
| **0.3.5** | `PhaseGate` ENTER/PASS (RTC) um Trip-Upload |
| **0.3.6** | NVS-Spiegel für offene Phase (erster Versuch) |
| **0.3.7** | NVS robust (`power_on` zuerst, sticky, CRC); Phase-Zeile in jedem Diag-Snapshot |
| **0.3.8** | NeedReset nach Intentional-Detach clearen; LED `TripUpload` (blau blink); Replay-Stall an `tcp_tx_ok`; TCP force-reconnect 12 s; kleinere Bursts/Delays; Stall 60 s / Hard 180 s; Ring max 24 |
| **0.3.9** | **Single-Owner TcpStreamer:** `wifiCoexTask` ruft weder `tcp_.loop()` noch `superviseWifi()` während Trip-Upload/Replay (Race Mainloop↔Coex auf `WiFiClient`); `drain_abandon` ≠ Batch-Erfolg |
| **0.3.10** | Stored pumps kooperativ (kein `delay`-Busywait); Health ohne 30 s `drainUpload`; `web_.loop()` vor/nach OC-Poll |
| **0.3.11** | Kurzer TCP-Connect-Timeout (800 ms) + kein `client_.flush()` im Offline-Drain; kein zweites `tcp_.loop()` während OC-Upload |
| **0.3.12** | Soft-Detach settle 500 ms vor WiFi/TX; stored zombie-STA bounce auf Mainloop; `softWifiKeepalive` (nur Assoc-lost) während Trip-Upload |
| **0.3.13** | Stall auch bei wifi=1/tcp=1 ohne TX; skip Soft-Detach ohne Host; sanftere Batches; TX-Stack-Buf (zu klein → 0.3.15) |
| **0.3.14** | `lastOkAge`: nie-TX ≠ uptime (kein Früh-Zombie-Bounce); Connect-Timeout 2 s |
| **0.3.15** | TX-Buf 1536 + String-Fallback; kein `setTimeout(1)` beim Write (partial → tx_fail-Sturm) |

### Self-Heal (Feld 0.3.10 / 0.3.11)

Upload #17 unter 0.3.10: Web nach ~4 s unreachable, **>6 min** tot. **0.3.11** (kurze Connect-Timeouts, kein `flush`, kein doppeltes `tcp_.loop`): OTA ok, Replay #17 startet (`wifi_up` → `trip_replay`), dann wieder sofort DOWN (~4 min Monitor, 1/80 Samples ok). **Kommt nicht von selbst zurück → RST.** Ursache liegt tiefer als Pump/Flush (vermutlich Soft-Detach/`wifiOn` oder Radio-Zombie ohne Coex-Supervise).

### Concurrency-Hypothese (Audit nach 558d5d3)

`TcpStreamer`/`WiFiClient` wurde parallel aus Arduino-Loop, `OfflineCapture::pumpTcp*` und `wifiCoexTask` bedient — ohne Mutex. Gleichzeitig konnte `enterCoexHold()` → `esp_wifi_stop()` laufen. Das erklärt TCP-Tod bei assoziiertem WLAN besser als reines RF. 0.3.9 ist der A/B-Test: Ownership klar, sonst keine Replay-Tuning-Änderungen.

---

## 5. Was 0.3.8 bereits verbessert

- **Kein Magenta** mehr während Upload (NeedReset-Clear + eigene LED-Mode).
- **`last_crash` nach Power-RST** zuverlässig (z. B. `trip_replay/16`, später `trip_replay/17`).
- Trip **#16** kam trotz mehrerer Minuten TCP-Ausfall noch **`delivered`**.
- Diag zeigt klar: force-reconnect, Heartbeats, Phase-Stack, `# phase open=…`.

Offen: TCP/WLAN-Pfad stirbt weiterhin früh im Replay; Web wird unreachable; Batch hängt bis RST. #17/#18 noch pending.

---

## 6. LED-Lexikon (relevant hier)

| LED | Mode |
|-----|------|
| Magenta blink | NeedReset (USB BOT) — bei Upload ab 0.3.8 unerwünscht/unterdrückt |
| Blau hell↔dunkel | **TripUpload** — Job läuft, Soft-Detach aktiv |
| Dimm-Blau fest | IdleWifi — WLAN ok laut Firmware, Stick nicht gesteckt |
| Gelb blink | WLAN verbindet / Portal |
| Gelb↔Cyan | Coex-Hold |

**Falle:** Dimm-Blau oder Upload-Blau + UI „STATUS unreachable“ = Zombie-Assoziation; RST hilft, PhaseGate liest den Hang-Ort.

---

## 7. Aktuelles Problem (Stand 0.3.15, Abend 21.10.)

**Ziel:** Pending Trips #17/#18 von Auto-ESP `.89` an Collector `:9100` streamen.  
**Ist:** Unter 0.3.15 startet Replay, `tcp_ok` wird kurz `true`, dann stirbt die WLAN-Assoziation (Web unreachable, LED dimm-blau / Coex gelb↔cyan). Self-Heal kommt oft **nicht** — RST nötig. Trips bleiben `uploaded=false`.

### Was wir inzwischen wissen (Diag + Feld)

| Befund | Konsequenz |
|--------|------------|
| Hang-Ort `trip_replay` / später `stored_oc_drain` | PhaseGate + Diag lokalisiert; kein „mystery reboot“ mehr |
| `lastOkAge == millis()` wenn noch nie `tcp_tx_ok` | Früh-Zombie-Bounce (0.3.13) — gefixt in **0.3.14** |
| TX-Stack-Buf 768 B + `setTimeout(1)` | `tcp=1` aber `tx_ok=0` / `tx_fail≫0` — gefixt in **0.3.15** |
| Soft-Detach ohne Host + live STA | Race; skip ohne Host in 0.3.13 — RF-Coex ggf. wieder nötig, aber mit Settle |
| Blocking `drainUpload(30s)` / doppeltes `tcp_.loop` | Loop-Starvation — 0.3.10/0.3.11 |
| Lab `.88` Live-Offline (winzig) | `delivered` unter 0.3.12+ — Pfad grundsätzlich ok |
| Collector `:9100` | erreichbar vom Dev-Host; Problem liegt am ESP-Pfad unter Replay-Last |

**Kernhypothese jetzt:** Unter Stored-Replay (viele kurze JSONL-Writes) kippt der ESP32-S3-STA in einen **Zombie** (Firmware: `WL_CONNECTED`, LAN: tot). Entweder RF/Coex (USB soft noch attached), TX-Burst ohne Atmung für den WiFi-Stack, oder beides.

---

## 8. Plan — wie es weitergeht

1. **0.3.16 — TX drosseln + Atmung:** 1 Event/Tick, festes Yield/`web_.loop`-Fenster alle N ms; Write-Timeout moderat (z. B. 3 s), nicht 1 s und nicht unbegrenzt. Ziel: `tcp_tx_ok` steigt kontinuierlich, HTTP bleibt pollbar.
2. **Soft-Detach mit Settle, ohne Früh-Kill:** Detach nur wenn nötig (Host am Bus **oder** RSSI/TX-Probe schlecht), immer ≥500 ms Settle **vor** erstem Collector-Connect; nie Zombie-STA-Bounce bevor `tcpEverOk`.
3. **Upload-Watchdog:** Wenn `tripUploadActive` und (kein `tcp_tx_ok` **oder** Web tot) > N s → Ring clear, Phase PASS, Batch sauber beenden (nicht ewig blau / dimm-blau).
4. **Feld-Gate auf `.89`:** OTA 0.3.16 → POST `/api/trips/upload` → Erfolg = `#17` dann `#18` `uploaded=true`, HTTP durchgängig erreichbar. Bei Fail: `/api/diag/saved` + LED-Notiz.
5. **Optional später:** Mutex um `WiFiClient` (falls Coex wieder parallel darf); RSSI-Gate vor Batch; Collector-Keepalive.

Nicht parallel: große Refactors am TripStore-Format oder Collectorschema — erst den TX/WLAN-Pfad stabilisieren.

---

## 9. Operator-Checkliste nach Hang

1. LED notieren (Magenta / blau blink / dimm blau / gelb↔cyan).  
2. RST.  
3. `GET http://192.168.178.89/api/status` → `phase_gate` / `fw`.  
4. `GET /api/diag/saved` → `PHASE`, `replay hb`, `tcp force`, `tx_ok`/`tx_fail`.  
5. `GET /api/trips` → `pending`, `upload_error`.

---

## 10. Fazit

Kein Volumen-Problem (85 KiB), sondern **Stored-Replay killt den WLAN/TCP-Pfad**. 0.3.9–0.3.15 haben Ownership, Loop-Starvation, false Stall-Timer und Write-Timeouts abgeräumt; 0.3.15 zeigt erstmals kurz `tcp_ok` während #17. **Offen:** stabiler TX ohne Zombie bis `delivered` für #17/#18 — nächster Slice **0.3.16** (Drossel + Settle + Watchdog).
