# Heimfahrt-Modus, GitHub-Auto-Update, Medienmix `drive_mix` (FW 0.3.0)

Stand: Implementierung, **nicht** auf Hardware oder im Fahrzeug geprüft. Kein Feld-PASS.

## Ziel

Auf der täglichen Heimfahrt (~12 min) ohne Bedienung messen: Sonde einstecken, sie misst
ab dem ersten Enumerieren der HU, daheim im WLAN lädt sie die Fahrt zum Collector hoch.
Unterwegs (Handy-Hotspot, kein Collector) holt sie bei Bedarf die neueste Firmware von GitHub.

## Einmalig: Kabel-Flash (neue Partitionstabelle)

0.3.0 bringt `firmware/partitions_huprobe_8mb.csv` (2 × 3 MiB App, 1,5 MiB `trips`,
SPIFFS, Coredump). Eine geänderte Partitionstabelle kommt **nicht** per OTA an: einmal per
USB-Kabel flashen (`pio run -e huprobe-s3 -t upload`). Danach funktionieren Hub-, WebUI- und
GitHub-OTA wieder wie gewohnt. Ohne neue Tabelle meldet `/api/trips` „keine trips-Partition“;
der Heimfahrt-Modus misst dann ohne Flash-Sicherung.

## Einrichten (WebUI → Karte „Heimfahrt-Modus“)

| Feld | Bedeutung |
|---|---|
| beim Start automatisch messen | `autoCapture`; ab dem nächsten Start aktiv |
| Fein-Modus | `autoFine`, ein DATA-Record je 4 KiB (Chunk-Takt) |
| Medium | `autoMedia`, Standard `drive_mix` |
| Hotspot-SSID / -Passwort | zweites WLAN (`wifi2Ssid`/`wifi2Pass`); das Heimnetz bleibt im WiFiManager-Speicher |
| Auto-Update | `autoUpdate`; prüft das Manifest nur, wenn ein WLAN da ist |
| Update-Manifest | Standard `https://raw.githubusercontent.com/MPunktBPunkt/hu-probe/main/ota/manifest.json` |

Das Passwort wird nie zurückgegeben (`wifi2PassSet`); ein leeres Feld behält das gespeicherte.

## Ablauf bei jedem Start (Heimfahrt-Modus an)

1. MSC startet **abgemeldet** (die HU sieht noch keinen Stick). LED dim weiß.
2. WLAN-Versuch 15 s über WiFiMulti (Heimnetz und Hotspot, das stärkere gewinnt). Kein Portal.
   Portal nur, wenn noch gar keine Zugangsdaten existieren oder **BOOT beim Einstecken gedrückt** ist.
3. Collector erreichbar → gespeicherte Fahrten hochladen (LED blau), danach den Journal-Bereich
   löschen, wenn alles hochgeladen ist.
4. Auto-Update an und WLAN da → Manifest prüfen (LED hell weiß). Bei neuer Version: Download,
   sha256-Prüfung, Neustart. Läuft das neue Image dreimal nicht bis zum Abschluss der WLAN-Phase,
   bootet die Sonde wieder das vorherige Image (`restart_cause=ota_rollback`, WebUI zeigt „ROLLBACK“).
5. **Heimnetz** (SSID = WiFiManager-Heimnetz, nicht Hotspot): keine neue Messung. Wenn der
   Collector gesehen wurde und nichts mehr aussteht → `autoCapture=false` speichern (einmaliger
   Heimfahrt-Durchlauf), weiter im normalen Sondenbetrieb. Pending übrig oder Collector weg →
   Flag bleibt an (nächster Boot versucht Upload erneut).
6. **Kein Heimnetz** (kein WLAN oder nur Hotspot): 256 KiB Flash vorlöschen (≤ 8 s), Medium
   setzen, Offline-Aufnahme scharf, Soft-Re-Enum → die HU enumeriert zum ersten Mal. LED lila
   mit weißem Blitz alle 2 s.
7. Kein Host innerhalb 20 s (z. B. Ladegerät ohne Heimnetz-Erkennung) → Lauf verworfen, normaler
   Sondenbetrieb; `autoCapture` bleibt an.

## Während der Fahrt

- Experiment `drive_home`: bis 30 min, Ende per Media-Hide, Unmount beendet **nicht**.
  BOOT kurz = `mark_NN`, BOOT 2× = Soft-Steckzyklus, BOOT lang = Stop.
- Flash-Checkpoints: in USB-Ruhephasen (≥ 500 ms ohne CBW) spätestens alle 60 s, erzwungen
  nach 90 s; je Durchlauf 8 KiB. Jeder Checkpoint hinterlässt einen Marker `ckpt:<ms>:<KiB>`.
- **Stromverlust (Zündung aus) ist das normale Ende.** Gesichert ist alles bis zum letzten
  Checkpoint; der Rest (≤ ~90 s) fehlt. Der nächste Start lädt die Fahrt mit
  `end_reason=power_loss` hoch.
- Nach 30 min blendet Media-Hide das Medium aus. Längere Fahrten: der Rest ist ungemessen.
- Journal voll (1,5 MiB oder 32 Fahrten) → keine neue Aufnahme, LED rot/lila, Medium wird
  normal angeboten. WebUI → „Ausstehende hochladen“ bzw. „Alle löschen“.

## Ende der Aufnahme mit Strom (Timer / BOOT lang)

- Heimnetz + Collector erreichbar → Upload sofort, danach normaler Betrieb.
- Kein WLAN/Collector → `upload_status=stored_in_flash`, WLAN bleibt aus (kein Neustart-Zyklus
  im Auto), Upload beim nächsten Start daheim.

## Gespeicherte Fahrten (WebUI / API)

- `GET /api/trips`: Partition, freie Bytes, je Fahrt `trip_id`, Bytes, Commits, `end_reason`,
  hochgeladen ja/nein, Medium, Wallclock, FW.
- `POST /api/trips/upload`: alle ausstehenden nacheinander hochladen (WLAN + Collector nötig).
- `POST /api/trips/erase` `{"all":false}` löscht nur, wenn nichts mehr aussteht;
  `{"all":true}` löscht alles.
- Ein Upload kann doppelt ankommen (Abbruch nach dem Senden, vor der Markierung). Der Collector
  legt dann zwei Sessions an; `trip_id` in `session.json → capture` erkennt das Duplikat.

## Collector / Analyse

`capture_health` trägt zusätzlich `trip_id`, `persisted`, `checkpoints`, `persist_full`,
`stored_replay`, `capture_fw`, `media_variant`; `session_store.py` übernimmt sie nach
`session.json → capture`. `tools/analyze_session.py` zeigt einen Abschnitt
„Flash-Checkpoints (Heimfahrt)“ (Anzahl, Abstände, Schreibdauer, Hinweis bei Stromverlust)
und zählt `ckpt:`-Marker als automatische Marker.

## GitHub-Release und Freigabe

1. `FW_VERSION` in `firmware/platformio.ini` erhöhen, committen, Tag `v<FW_VERSION>` pushen.
2. `.github/workflows/firmware.yml` baut mit `HUPROBE_RELEASE=1` (Tag muss passen, Baum sauber),
   hängt `hu-probe-<ver>.bin` an das Release und schreibt `version/url/sha256/size` nach
   `ota/manifest.json` auf `main`.
3. **`enabled` bleibt unverändert.** Erst wenn `enabled: true` von Hand gesetzt ist, installieren
   die Sonden das Update. `enabled: false` stoppt das Verteilen sofort (keine Rücknahme
   installierter Images).

Das Image prüft den GitHub-Server über ein eingebettetes Mozilla-CA-Bundle
(`scripts/gen_ca_bundle.py`, beim Build von curl.se geladen). Fehlt es (offline gebaut), meldet
die Sonde `no_ca_bundle` und aktualisiert nicht; der Workflow bricht in diesem Fall ab.

## Medienmix `drive_mix`

40 Dateien in 12 Ordnern, bis 4 Ebenen (`ROCK/CLASSIC/LIVE/1977/`), Cluster 16 KiB, Spielzeit
~2,7 h. Keine zwei Dateien gleich: eigener Seed, Länge 15–450 s, 96–320 kbit/s, CBR/VBR,
Xing/Info, ID3v1, ohne ID3v2, Titel/Interpret/Album/Track (TRCK + ID3v1.1), Cover 1–4,
ein APIC-Slot 512 KiB, zwei verschränkt fragmentierte Dateien (256 KiB). Hintergrund: Mit drei
identischen Dateien wäre im Feldtest 2026-10-20 fast ein Lesemuster erkannt worden, das nur
aus der Gleichheit der Dateien kam.

## Offene Punkte / nicht geprüft

- Flash-Schreiben während laufender USB-Übertragung (Cache-Sperre) auf der Hardware messen:
  `write_ms_max` im Analyzer, Lücken im Chunk-Takt um `ckpt:`-Marker.
- Verhalten der HU, wenn die Sonde erst nach ~20–30 s Bootphase enumeriert (WLAN + Upload + OTA).
- WiFiMulti-Auswahl Heimnetz vs. Hotspot in der Garage.
- Rollback-Pfad (absichtlich defektes Image) im Labor.
