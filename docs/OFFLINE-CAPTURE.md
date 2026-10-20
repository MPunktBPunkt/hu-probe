# Offline-Capture 0.2.30

FW 0.2.26 zeichnet kurze Messfenster bei abgeschaltetem WLAN in einem
RAM-Puffer auf und spielt sie nach dem Messende über den normalen
TCP/JSONL-Pfad zum Collector aus. Es gibt **keine Flash-Capture-Partition**:
Abholen = RAM→TCP-Replay. Reset oder Spannungsverlust vor erfolgreichem Upload
verliert den Puffer.

**Reset-Frage (Lab 2026-10-18):** Das Auslesen/Replay aus dem Capture-RAM
löst **keinen** `ESP.restart()` aus. Beobachtete Software-Resets kamen von
**Hub-OTA** (MAC noch als PiDrive registriert → `otaUrl` nach WiFi-Reconnect).
Ab 0.2.26: OTA während Offline blockiert; PiDrive-Artifacts werden ignoriert.

Ab **0.2.25/0.2.26** liegt darüber eine Experiment-Schicht: ein WebUI-Button
startet einen vollständigen Messlauf (Arm → WiFi aus → Capture → Finalize →
Upload → Integritätsurteil) mit Szenario-Profil, Capture-Budget und
Offline-Markern.

## Ablauf und Bedienung

### Empfohlen: Experiment-Messlauf (WebUI)

1. WebUI öffnen → Abschnitt **Messlauf**.
2. Fahrzeug + Experiment wählen (z. B. `bmw` / `S2 Prefetch`), Dauer und Ende
   prüfen.
3. **▶ Offline-Messung starten** — die Probe antwortet mit `capture_budget`,
   schaltet WLAN ab und erfasst nur noch USB.
4. Während Capture: **BOOT kurz** = nächster Experiment-Marker
   (`play_start` → `track_next` → …); **BOOT lang** (≥1,5 s) = Stop.
5. Nach Timer / Unplug / BOOT: Finalize, WLAN wieder an, TCP-Replay.
6. UI zeigt `Integrity: PASS|FAIL` aus `offline.last_run`.

### API

```bash
# Experiment (Profil-Defaults + Marker-Sequenz)
curl -X POST http://PROBE/api/experiment/start \
  -H 'Content-Type: application/json' \
  -d '{"vehicle":"bmw","experiment":"free"}'
# WebUI-Default: bmw + free · 60 s · soft_usb=false · media_hide · Unmount beendet nicht

# Roh-Offline (wie bisher; Marker aus Szenario-Profil, falls bekannt)
curl -X POST http://PROBE/api/offline/start \
  -H 'Content-Type: application/json' \
  -d '{"vehicle":"lab_debian","scenario":"s0_plug_enum","profile_id":"lab_debian","soft_usb":true,"measure_ms":12000}'

# Verfügbare Experimente + Budget
curl http://PROBE/api/experiments
```

`auto_detach_ms` und `measure_ms` sind gleichbedeutend; `auto_detach_ms` hat
Vorrang. `POST /api/offline/stop` setzt `api_stop`, ist nach WLAN-OFF aber
normalerweise nicht mehr erreichbar. Für Feldläufe BOOT, Timer oder Unplug
verwenden.

## Experiment-Profile

| ID | Default-Dauer | Ende | Offline-Marker (BOOT-Zyklus) |
|----|---------------|------|------------------------------|
| `s0_plug_enum` | 12 s | media_hide | plug_ready → stop |
| `s1_eager_eof` | 45 s | media_hide | play_start → audible_change → stop |
| `s2_prefetch` | 60 s | media_hide | play_start → track_next → audible_change → stop |
| `s3_cache_hit` | 90 s | media_hide | play_start → audible → stop → replay → … |
| `s4_remount` | 90 s | soft_detach | inkl. replug |
| `s5`…`s7` | 60–120 s | media_hide | generische Operator-Marker |
| `free` (**WebUI-Default**) | 60 s (max 10 min) | media_hide, `soft_usb=false`, Unmount beendet nicht | BOOT kurz = mark_NN · BOOT 2× = Soft-Steckzyklus |

Epistemik (Audit): ohne `play_start` + (`audible_change` | `track_next`) bleibt
S2 = **UNKNOWN**.

## Media-Hide und Soft-Detach

`end_strategy` ist standardmäßig `media_hide` und wird in der Konfiguration als
`offlineEndStrategy` gespeichert.

- `media_hide`: Das LUN wird zuerst als nicht vorhanden gemeldet. USB bleibt
  attached; damit bleibt die Device-Stack-/Transport-Epoche möglichst
  kontinuierlich.
- Schlägt Media-Hide fehl, folgt automatisch `softDetach(0)`.
- `end_strategy=soft_detach` wählt den bisherigen, disruptiven Abschluss
  explizit.
- Kommt WLAN nach erfolgreichem Media-Hide nicht zurück, wird Soft-Detach
  ebenfalls als Coexistenz-Fallback ausgeführt.

Nach dem Upload wird ein ausgeblendetes Medium wieder sichtbar gemacht und
ein soft-detachtes USB-Gerät wieder attached.

## API-Felder

`POST /api/experiment/start` akzeptiert:

- `vehicle`, `experiment` (oder `scenario`), `profile_id`
- `soft_usb`, `measure_ms` / `auto_detach_ms`, `end_strategy`
- fehlende Felder kommen aus dem Experiment-Profil

Die Startantwort enthält zusätzlich `capture_budget`
(`capacity_bytes`, `record_size`, `estimated_max_records`, `max_duration_ms`,
`measure_ms`) und die Marker-Liste.

`GET /api/status` enthält unter `offline`:

- `state`, `active`, `capturing`
- `records`, `bytes`, `cap`, `overflow`, `wallclock`
- `stop_reason`, `soft_usb`, `auto_detach_ms`, `end_strategy`
- `upload_status`, `media_hidden`, `fallback_detached`
- `transport_epoch_start`, `transport_epoch_end`
- `markers`, `last_marker`, `marker_index`, `capture_budget`
- `last_run` — nach Abschluss: Records, Overflow, Dropped, `integrity` PASS/FAIL

Mögliche Upload-Zustände sind unter anderem `wifi_connecting`, `wifi_retry`,
`replay_enqueued`, `delivered`, `queued` und `replay_failed`.

## Capture-Budget

Compact-Record-Größe × Kapazität begrenzt die Fensterlänge. Vor dem Start meldet
die Probe das Budget; Overflow markiert die Session als nicht quantitativ
verwertbar. Typisch ~64 KiB (Fallback 48 KiB); `record_size` steht in Status und
`capture_health`. Ab 0.2.31 gibt es weitere Fallback-Größen 32/24 KiB; im Feld lag bisher 32 KiB vor.

**Format `compact/1` (ab 0.2.31):** Datensätze haben eine variable Länge. Im RAM
steht nur, was sich nicht ableiten lässt; beim Upload baut `replayOneRec()` daraus
dieselben Events wie vorher (`bot_cbw` inkl. `raw_hex`, `bot_data` inkl. `tag`/`kind`,
`bot_csw`, `usb`, Marker).

| Datensatz | vorher | `compact/1` | weggelassen bzw. abgeleitet |
|---|---|---|---|
| Kopf (alle) | – | 7 B | Zeit als int32 µs relativ zum Scharfschalten statt int64 |
| CBW, TEST UNIT READY | 99 B | 19 B | Signatur `USBC` konstant, CDB nur bis zum letzten Byte ≠ 0 |
| CBW, READ10 | 99 B | ≈ 28 B | wie oben |
| DATA (READ10) | 99 B | 30 B | `tag`/`kind` als Regionscode (1 B), `dir` = `in` |
| CSW | 99 B | 16 B | Signatur `USBS` konstant |
| Control | 99 B | 15 B | – |
| Marker | 99 B | 7 B + Namenslänge | – |

Damit passen etwa 4- bis 5-mal so viele Datensätze in denselben Puffer
(32 KiB: ≈ 1500 statt 331). `capture_budget.record_size` ist ein typischer Wert
(22 B), `capture_health.record_size` der tatsächliche Mittelwert des Laufs;
`record_format` nennt das Format. Unbekannte `tag`/`kind`-Kombinationen werden als
kurze Strings im Datensatz mitgespeichert, damit nichts verloren geht.

**Schutz gegen falsche Daten:**

- Zeit außerhalb ±35 min vom Scharfschalten wird nicht gekappt gespeichert, sondern
  verworfen und als `offline_time_range_dropped` gezählt (Integrity FAIL).
- Jede Datensatzart hat eine feste oder selbstbeschreibende Länge (`recShapeOk`).
  Passt ein Datensatz nicht, stoppt das Replay mit `replay_error`
  (`bad_length`, `bad_shape`, `truncated_header`, `count_mismatch`), statt verschobene
  Bytes als Events auszugeben.
- `replay_complete` ist nur wahr, wenn alle Bytes und genau `offline_records` +
  `offline_post_finalize_records` Datensätze abgespielt wurden. `complete` /
  `integrity_pass` / `last_run.integrity` verlangen das zusätzlich (auch ein
  abgebrochener Upload `replay_stalled` ist damit FAIL).
- Zeitanker: `offline_anchor_unix_us` und `offline_anchor_capture_t_us` wurden im
  selben Moment beim Scharfschalten gelesen. UTC eines Events =
  `anchor_unix_us + (capture_t_us − anchor_capture_t_us)`; Genauigkeit wie die NTP-Zeit
  der Sonde. Ohne NTP fehlen beide Felder.

## Freie Aufnahme (Profil `free`, ab 0.2.31)

WebUI → Karte „Messlauf“ → Experiment „Freie Aufnahme“, Dauer 5–600 s.

- **BOOT kurz:** Marker `mark_01`, `mark_02`, … Der Zeitstempel ist der Moment des
  Drückens; geschrieben wird der Marker erst, wenn 0,5 s kein zweiter Druck folgt.
- **BOOT zweimal schnell (≤ 0,5 s zwischen dem Loslassen):** Soft-Steckzyklus. Marker
  `soft_replug`, dann D+ für 2 s getrennt und wieder verbunden. Die Versorgung bleibt,
  die HU sieht Abmelden und neues Anmelden. Die Aufnahme läuft weiter.
- **BOOT lang (≥ 1,5 s):** Stop.
- **Checkbox „Unmount/Remount der HU beendet Aufnahme“** (API `end_on_unmount`): bei
  `free` standardmäßig aus, bei allen anderen Profilen an. Aus = ein Unmount der HU oder
  ein Abziehen mit externer Versorgung beendet die Aufnahme nicht; Ende nur über Zeit
  oder BOOT lang. Der Soft-Steckzyklus beendet die Aufnahme in keinem Fall.
- **Echtes Abziehen** schaltet die Sonde ab (Strom nur von der HU) — die Daten im RAM
  sind dann verloren.
- Status/Health: `end_on_unmount`, `free_markers`, `soft_replugs`.
- Höchstdauer aller Offline-Läufe jetzt 600 s (`max_duration_ms`), vorher 180 s.

## Capture-Health und Integritäts-Gate

Beim Replay/Abschluss wird `ev=capture_health` mit mindestens
`transport=ram_offline`, `offline_records`, `offline_bytes`,
`offline_overflow`, `offline_wallclock`, `end_reason`, `end_strategy`,
Transport-Epochen, `upload_status`, `dropped`, `published`, `complete` und ab
0.2.25 `integrity_pass` / `record_size` / `capacity_bytes` erzeugt.

**Quantitative Auswertung nur wenn:**

| Bedingung | Pflicht |
|-----------|---------|
| `offline_overflow` | `= 0` |
| `dropped` | `= 0` |
| `complete` | `true` |
| Collector `recv_seq_gaps` | `0` |

Sonst: Session archivieren, nur diagnostisch nutzen (`validate_session.py`
meldet FAIL). Upload bei Overflow bleibt erwünscht — Verwertung nicht.

Der `panic_tail` hält bis zu acht grobe Lifecycle-/USB-Breadcrumbs in
RTC-Speicher. Nach einem Reset erscheinen der vorherige Tail und
`reset_reason` im nächsten Health-Event. Er dient der Reset-Diagnose, ersetzt
aber weder die verlorenen RAM-Records noch eine vollständige Timeline.

## Recovery bei WiFi-/Uploadfehlern

Schlägt der WLAN-Reconnect nach 25 s fehl, bleibt die State Machine in
`uploading`, der RAM-Puffer wird nicht freigegeben und `poll()` versucht den
Abschluss alle drei Sekunden erneut. Bei Media-Hide wird vor weiteren Versuchen
einmalig Soft-Detach als Fallback ausgelöst. Den Probe-Strom nicht trennen:
Reset oder Power-Loss löscht die noch nicht übertragenen Records. Nach
WLAN-Erfolg wird TCP aufgebaut, Replay eingereiht und bis zu einem begrenzten
Zeitfenster gepumpt; `upload_status=queued` bedeutet daher noch nicht
`delivered`.

## BMW-Stand und Evidenzgrenze

Der Lab-Lauf mit FW 0.2.23
(`20261015-181642_lab_debian_s0_plug_enum`) belegte den vollständigen
RAM-Offline-Zyklus mit 96 Records, `overflow=0` und `complete=true`.

Lab 0.2.26 (2026-10-18): Experiment-Start → Offline → Probe-`last_run`
`integrity=PASS` (z. B. 190 Rec, overflow=0, `transport=ram_offline`).
Event-Format brauchbar (`lba`, `gap_us`, `dur_us`, `bot_source=tinyusb`).
Collector-Flush nach Burst-Replay kann noch Seq-Lücken zeigen (`upload_status=queued`);
TCP-Framing (atomare Zeilen + Collector `raw_decode`) ist gehärtet, vollständiges
`capture_health` auf Collector-Seite bleibt Lab-Nacharbeit. Hub während Lab-Offline
`enableHub=false` empfohlen.

Der einzige BMW-Offline-Versuch (0.2.23) lieferte keine finalisierte
`ram_offline`-Session; verwertbare BMW-Daten waren `tcp_live`. Ein neuer
BMW-Lauf mit 0.2.26 muss anhand seines eigenen `capture_health` /
`integrity_pass` bewertet werden.

Alle Events stammen aus TinyUSB bzw. dem Device-Stack. Eine kontinuierliche
Device-Stack-Epoche ist **keine Wire-Aufzeichnung**; Bus-Timing und Vorgänge
unterhalb der Instrumentierung bleiben `Wire=UNKNOWN`.

Offline-Capture eliminiert WiFi als Messvariable — das beweist nicht, dass WiFi
die Ursache früherer Störungen war (Hardware-Audit 2026-10-18).

## Hotpath-Regel

Im Messfenster gilt: Callback/Drain schreibt ausschließlich kompakte Records in
RAM. Es gibt keine Capture-, NVS- oder sonstigen Flash-Writes im USB-Hotpath.
Auch der Panic-Tail liegt in RTC-RAM. Konfigurationsspeicherung erfolgt vor dem
Arming; JSONL-Erzeugung, WLAN und TCP-Replay erst nach dem Messfenster.
