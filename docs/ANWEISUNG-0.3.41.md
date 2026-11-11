# Anweisung an die Cursor-Instanz zuhause — hu-probe 0.3.41 bauen

Stand 2026-11-11, geschrieben von der Instanz ohne Compiler. Firmware ist **nicht** kompiliert. Host-Test `tools/field_plan` lief hier mit Clang (`zig c++` 0.17.1, `-Wall -Wextra -Werror`, `-O0` und `-O2`) und bestand. Mit GCC (CI) lief er nicht. Kein Hardware-Lauf, kein PASS.

## Auftrag

0.3.41 bauen und **per USB-Kabel** flashen. Nicht per OTA ausliefern. `field/plan.json` bleibt `"enabled": false`, bis die Abnahme unten durch ist.

## Dateien übernehmen

Alle Dateien aus `update/hu-probe-main/` 1:1 ins Repo legen (Pfade identisch). Zeilenenden beibehalten: `DriveMode.h`, `DriveMode.cpp`, `GithubOta.h`, `GithubOta.cpp` sind CRLF, der Rest LF. Was sich geändert hat, steht in `update/update.md` (Abschnitte „hu-probe 0.3.41“ und die Nachträge). Logik nicht umbauen, nur Compile-Fehler beheben.

## Bauen und prüfen

1. `pio run -e huprobe-s3` im Verzeichnis `firmware`. Warnungen und Fehler beheben, ohne das Verhalten zu ändern. `FW_VERSION` bleibt `0.3.41`.
2. `make -C tools/field_plan test`
3. `python tools/validate_field_plan.py` — die Vorlage ist absichtlich `enabled: false`, `online_only: true`, `arm_at_home: false`.
4. Committen. `field/plan.json` nicht auf `enabled: true` setzen.
5. Per Kabel flashen: `pio run -e huprobe-s3 -t upload`. Die Partitionstabelle ist seit 0.3.0 unverändert, ein Tabellen-Flash ist nicht nötig, solange die Sonde schon 0.3.x mit `partitions_huprobe_8mb.csv` hat (Fahrzeugstand 0.3.38).

## Abnahme, bevor der Plan scharf geht

Kein PASS behaupten. Ergebnisse notieren.

1. **Upload (Fix 0.3.39, ungetestet).** Kurze Fahrt aufnehmen, nächster Start im Heimnetz. Erwartet: `replay_records = offline_records`, die Session kommt vollständig am Collector an. Das ist der Weg, über den später alle Pendelfahrten laufen.
2. **Feldplan.** `enabled: true` pushen, Sonde im Normalbetrieb mit WLAN starten. WebUI: `drive.field_plan.result` = `new id=…`, „beim Start automatisch messen“ geht von selbst an.
3. **Hotspot aus.** Start ohne Hotspot und ohne Heimnetz: Zustand `car_idle`, Status „nur online, nicht geladen“, keine neue Fahrt im Journal, Stick bleibt sichtbar.
4. **Hotspot an, HU läuft, RST.** Session `fp_<plan>_<job>_r1`, `runs_done` = 1.
5. **LED.** Lila mit weißem Blitz oder grün = Messung läuft (RST schneidet bis ~90 s ab). Gelb oder orange blinkend = Fahrt liegt im Flash, RST ist unbedenklich. Folgemessung: BOOT lang, Farbwechsel abwarten (bis ~25 s), dann RST.
6. **Gleicher Plan, Job ergänzt.** `plan`-ID beibehalten, `repeat` erhöhen oder Job anhängen. Raw-URL erst abrufen, bis die neue Fassung da ist (GitHub-Cache bis ~5 min), dann BOOT lang und RST. Der Zähler läuft weiter (`r2`, `r3`, …). Neue `plan`-ID setzt nur den Planzähler auf 0; die Fahrten im Flash bleiben und werden im Heimnetz hochgeladen.

7. **Bremse über den Feldplan (Job `pace`).** Erst im Stand: Job mit `"stall":{"regions":["body"],"pace":"realtime","count":0}`, HU spielt `drive_mix`. Erwartet: `drive.error` leer, WebUI „mit Bremse“ vor dem Lauf, in der hochgeladenen Session Marker `stall_on` und `read_stall`, `analyze_session.py` → `read_stalls` mit Haltezeiten passend zur Bitrate (240 Blöcke ≈ 7,7 s bei 128 kbit/s). Notieren: Ton durchgehend oder Lücken, Menü träge, Fehlermeldung der HU. Eine absichtlich falsche `stall`-Konfiguration muss `stall_config:…` melden und den Lauf **nicht** zählen.

## Danach, nur auf Zuruf

- Tag `v0.3.41` pushen. `firmware.yml` schreibt `ota/manifest.json` (`enabled` fasst es nicht an) und hängt `hu-probe-0.3.41.bin` ans Release. Tag muss exakt `FW_VERSION` sein.
- Erstes OTA am Schreibtisch mit Kabel in Reichweite: in `ota/manifest.json` von Hand `"enabled": true`. Die Sonde prüft das Manifest nur im Heimfahrt-Modus (`autoCapture`) bei `autoUpdate` und WLAN. Im Fahrzeug ungetestet. Danach `enabled` wieder `false`, sonst installiert die Sonde im Auto jedes spätere Release vor der Messung (bis 3 min, die HU sieht den Stick erst danach).

## Verhalten, das schon so gewollt ist

- `online_only`: gemessen wird nur, wenn `field/plan.json` bei diesem Start geladen wurde. Hotspot an = messen, Hotspot aus = nicht. Gilt auch, wenn die Läufe aufgebraucht sind (dann WebUI-Standardwerte, nur online).
- Gezählt wird beim Host-Mount. 20 s ohne HU: Lauf verworfen, nichts gespeichert, kein Planzähler.
- Speicher: 32 Fahrten oder 1,5 MiB. `fine: true` ≈ 0,9 MB / 30 min. Voll = LED rot/lila, keine neue Messung.
- Zu Hause den Hotspot aus, sonst kann er statt des Heimnetzes gewinnen und der Upload bleibt aus.
- `arm_at_home: true` würde auch am Schreibtisch einen Lauf verbrauchen, sobald ein PC die Sonde einbindet. Vorlage lässt es aus.
