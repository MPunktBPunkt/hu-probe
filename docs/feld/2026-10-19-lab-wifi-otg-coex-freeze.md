# Lab: WiFi/OTG-Coex-Freeze (0.2.32–0.2.36) — 2026-10-19

**Gerät:** Lab-ESP `.88` (YD ESP32-S3 N8R2, 8 MiB Flash + 2 MiB Quad-PSRAM)  
**Host:** Proxmox CT `DebianCursor` (100) @ `192.168.178.187`, USB via Hub `2-3`  
**FW-Stand Review:** Branch mit 0.2.36 (Coex-Hold, PSRAM EventBus/Offline, Zombie-Assoc-Fix)  
**Status:** **offen / nicht gelöst** — Web friert nach kurzer Zeit ein; oft nur manueller RST hilft.

---

## 1. Symptom (aktuell)

| Beobachtung | Wert |
|---|---|
| LED | oft noch **dimmblau** / **cyan** / **gelb blinkend**, obwohl HTTP tot |
| WebUI / `GET /api/status` | Timeout (kein TCP zu `.88:80`) |
| ARP | `STALE` → `FAILED` |
| MSC (OTG) | optional: Host sieht `303a:1001` / `HU_PROBE`; WiFi stirbt auch **nach** Soft-Detach |
| Ohne OTG | Nach Coex-Episode kann WiFi **trotzdem** tot bleiben (Zombie), bis RST |
| Serial | `[WIFI] tcp-stall …`, `coex-hold`, `reconnect #n`, `coex waiting reattach … tcp=0`, selten `unrecoverable — ESP.restart()` |
| Collector | `.88` baut TCP zu `:9100` (oder versucht es), etablierte Sessions oft `0` |

**Kern:** Die Firmware wirkt „lebendig“ (Serial/LED), das **IP/WiFi-Datenebene** ist aber weg. Das fühlt sich wie Einfrieren der Netz-Stack-Seite an, nicht wie ein Hard-Hang der CPU.

---

## 2. Repro (Lab, FACT)

### A — OTG einstecken (Debian/Proxmox als MSC-Host)

1. RST, OTG **raus**, LED dimmblau, API `200`.
2. OTG (nativer USB) in Proxmox-Hub stecken.
3. Host: `2-3.2 Espressif`, `/dev/sda` = `HU_PROBE` (16 MiB SynthFat).
4. LED: blau → kurz grün (Enum/Read) → gelb / cyan.
5. API nach **~5–20 s** tot (`http=000`).

**Wichtig:** Kein Dauer-`dd` nötig. Schon **Enum/Mount** (wenige Dutzend Sektoren) reicht. `fuser`/`udisks` zeigten **keinen** Host-Stressprozess — nur Kernel-Scan.

### B — Soft-Detach greift, WiFi bleibt krank

Serial (typisch 0.2.34+):

```text
[WIFI] tcp-stall pending=… idle=… plugged=1
[MSC] soft detach hold=0ms
[WIFI] coex-hold (stall+usb) pending_was=… heap=… psram_free=…
[WIFI] reconnect #n (down … ms coex=1 …)
[WIFI] coex waiting reattach pending=0 tcp=0 hold=…ms
```

Host: Espressif oft weg oder ohne `/dev/sda`. Trotzdem: ARP fail, Web tot.

### C — OTG physisch raus, Web trotzdem offline

Nach Episode B: LED kann **dimmblau** sein (`IdleWifi` = `WL_CONNECTED`), API timeout.  
→ **Zombie-Assoc:** `WiFi.status()==WL_CONNECTED`, aber kein nutzbarer IP-Pfad.

### D — Heap-Bleed ohne OTG (Nebenbefund)

Unter Auto-Session `lab_debian_s0_plug_enum` sank freier Heap über ~80 s spürbar (Status/Health → EventBus), auch ohne MSC. Verstärker für Stall, nicht alleinige Ursache.

---

## 3. Ursachenmodell

### Primär (Silizium / Stack)

ESP32-S3 **USB-OTG Device (TinyUSB MSC) + WiFi STA** auf demselben Chip: unter Host-SOF/Enum stirbt der RF-/MAC-Pfad. Bekanntes Lab-Muster seit 0.2.11 (`docs/feld/2026-10-14-wifi-death-0.2.11.md`), damals vor allem unter sustained Reads; **heute schon bei Plug/Enum**.

### Firmware-Verstärker (eingegrenzt 2026-10-19)

1. **Zombie-Assoc:** Stall-Pfad und LED nutzen `WL_CONNECTED`; HTTP/ARP sind schon tot.  
2. **EventBus voll (2048):** TCP flush unter MSC-Last → `tcp-stall`; Status/Health füllen nach.  
3. **Zu späte / zu weiche Recovery:** `WiFi.reconnect()` reicht nach RF-Tod nicht; Soft-Detach allein heilt WiFi nicht.  
4. **Reattach zu früh (älter):** Timer-Reattach mit vollem Ring → erneuter Tod (in 0.2.32+ entschärft).  
5. **PSRAM war ungenutzt** bis 0.2.35 → DRAM-Druck (Ring-Slots + Capture).

Nicht die Hauptursache: Host-Userland-USB-Stress (kein `dd`-Loop, udisks inaktiv).

---

## 4. Was in 0.2.32–0.2.36 versucht wurde

| Version | Änderung | Ergebnis |
|---|---|---|
| 0.2.32 | Stall → Soft-Detach; Reattach nur bei leerem Ring | Soft-Detach ok; WiFi oft weiter tot |
| 0.2.33 | + STA-Bounce nach Soft-Detach | Bounce feuert; Recovery unzuverlässig |
| 0.2.34 | `enterCoexHold`: Soft-Detach + EventBus `clear` + `esp_wifi_stop/start` + `WiFi.begin`; kein Status-Spam im Hold | Hold greift; Zombie-Assoc blockiert echte Recovery |
| 0.2.35 | PSRAM: EventBus-Slots + Offline-Capture bis ~1 MiB; früherer Stall bei `plugged` | Boot-Heap besser (`[BUS] in_psram=1`); Coex-Tod **bleibt** |
| 0.2.36 | Zombie-Assoc zählt nicht als Recovery; STA-Restart im Hold ohne TCP | Noch nicht als PASS verifiziert; Symptom „Web tot / LED blau“ weiterhin beobachtet |

**PSRAM-Nutzung (0.2.35+):**

- `HuMem::allocPreferPsram` / `callocPreferPsram`
- EventBus: `String`-Slot-Array in PSRAM (`ring_in_psram`)
- OfflineCapture: große Puffer bevorzugt in PSRAM  
- JSONL-**Payloads** der `String`s liegen weiterhin im internen Heap (Arduino-`String`)

---

## 5. LED-Map (zur Einordnung)

| LED | Mode | Bedeutung |
|---|---|---|
| dimmblau | `IdleWifi` | `WL_CONNECTED`, kein Host-Plug |
| cyan | `UsbQuiet` | Host gesteckt, ruhig |
| grün | `Measuring` | Host liest |
| gelb blink | `WifiPortal` | nicht connected / reconnect |
| orange blink | `TcpDown` | plugged, TCP Collector down |

**Falle:** dimmblau/cyan ⇒ nicht „Web ok“.

---

## 6. Dual-USB-Lab-Setup (Arbeitshilfe)

- **COM** `2-3.1` CH343 → `/dev/ttyACM0` (LXC: `pct set 100 -dev2 path=/dev/ttyACM0,mode=0666,gid=20`)
- **OTG** `2-3.2` Espressif → `/dev/sda` `HU_PROBE`
- Mitlauf: Serial 115200 + Host-USB-Tree + `curl` API

---

## 7. Offene Fragen / nächste FW-Schritte

1. **Lab-Policy:** Bei `lab_debian` MSC dauerhaft soft-detached halten, solange Collector/HTTP gebraucht wird (nur explizit „MSC arm“ reattach).  
2. **Link-Liveness:** nicht nur `WL_CONNECTED` / TCP-Connect-Versuch — z. B. Gateway-Ping oder erfolgreicher Collector-`tcp_tx_ok` als Gesund.  
3. **EventBus-Payloads** in PSRAM (nicht nur Slots) oder kleiner Ring / weniger Auto-Session-Spam.  
4. Espressif-Coex / USB-PHY: prüfen ob `tud_disconnect` den PHY wirklich ruft; ggf. längerer Hold bevor `WiFi.begin`.  
5. Feld-Trennung beibehalten: COM=Versorgung/Log, USB=HU — Lab parallel beides am PC ist der Stressfall.

---

## 8. Review-Checkliste

- [ ] `firmware/src/app/App.cpp` — `enterCoexHold`, Stall-Schwellen, Zombie-Pfad  
- [ ] `firmware/src/net/EventBus.*` — PSRAM-Ring  
- [ ] `firmware/src/core/HuMem.h` — Allokator  
- [ ] `firmware/src/capture/OfflineCapture.cpp` — PSRAM-Capture  
- [ ] `firmware/platformio.ini` — `BOARD_HAS_PSRAM`, `qio_qspi`, `FW_VERSION`  
- [ ] Repro A/B/C gegen geflashte `.88` nach OTA/COM-Flash wiederholen  

**Aktueller Lab-Ist (Berichtzeit):** OTG raus, ARP/`api` oft tot bis RST — Fehler reproduzierbar im Alltag der Session.
