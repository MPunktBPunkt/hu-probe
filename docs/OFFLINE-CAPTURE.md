# Offline-Capture 0.2.24

FW 0.2.24 zeichnet kurze Messfenster bei abgeschaltetem WLAN in einem
RAM-Puffer auf und spielt sie nach dem Messende über den normalen
TCP/JSONL-Pfad zum Collector aus. Es gibt keine persistente Capture-Datei auf
der Probe: Reset oder Spannungsverlust vor erfolgreichem Upload verliert den
Puffer.

## Ablauf und Bedienung

1. Solange WLAN verfügbar ist, `POST /api/offline/start` senden.
2. Die Probe beendet eine eventuell aktive Live-Session, reserviert 64 KiB RAM
   (48 KiB Fallback), merkt sich die Wallclock, stoppt WLAN und wartet 500 ms.
3. Optional erfolgt mit `soft_usb=true` ein Soft-Re-Enum vollständig im
   WLAN-freien Fenster. Danach werden TinyUSB-/BOT-Ereignisse mit ihrem
   Capture-`t_us` im RAM aufgezeichnet.
4. Die Messung endet durch Timer, langen BOOT-Druck (mindestens 1,5 s),
   Unplug/Umount plus 1,5 s Idle oder spätestens nach drei Minuten. Ein kurzer
   BOOT-Druck setzt den Marker `boot`.
5. Die Probe beendet den MSC-Zugriff, schaltet WLAN wieder ein und spielt die
   RAM-Records als Schema-v2-JSONL mit `transport=ram_offline` aus. Ein eigener
   Upload-Endpunkt existiert nicht; Upload und Abschluss laufen automatisch.

Beispiel:

```bash
curl -X POST http://PROBE/api/offline/start \
  -H 'Content-Type: application/json' \
  -d '{"vehicle":"lab_debian","scenario":"s0_plug_enum","profile_id":"lab_debian","soft_usb":true,"measure_ms":12000}'
```

`auto_detach_ms` und `measure_ms` sind gleichbedeutend; `auto_detach_ms` hat
Vorrang. `POST /api/offline/stop` setzt `api_stop`, ist nach WLAN-OFF aber
normalerweise nicht mehr erreichbar. Für Feldläufe BOOT, Timer oder Unplug
verwenden.

## Media-Hide und Soft-Detach

`end_strategy` ist ab 0.2.24 standardmäßig `media_hide` und wird in der
Konfiguration als `offlineEndStrategy` gespeichert.

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

`POST /api/offline/start` akzeptiert:

- `vehicle`, `scenario`, `profile_id`
- `soft_usb` (Default `false`)
- `auto_detach_ms` oder Alias `measure_ms` (Default `0`)
- `end_strategy`: `media_hide` oder `soft_detach`; ungültige Werte werden
  nicht übernommen

Die Startantwort enthält `ok`, `action=offline_arm`, `soft_usb`,
`auto_detach_ms`, `end_strategy` und `note=wifi_off_then_capture`.
`GET /api/status` enthält unter `offline`:

- `state`, `active`, `capturing`
- `records`, `bytes`, `cap`, `overflow`, `wallclock`
- `stop_reason`, `soft_usb`, `auto_detach_ms`, `end_strategy`
- `upload_status`, `media_hidden`, `fallback_detached`
- `transport_epoch_start`, `transport_epoch_end`

Mögliche Upload-Zustände sind unter anderem `wifi_connecting`, `wifi_retry`,
`replay_enqueued`, `delivered`, `queued` und `replay_failed`.

## Capture-Health und Panic-Tail

Beim Replay/Abschluss wird `ev=capture_health` mit mindestens
`transport=ram_offline`, `offline_records`, `offline_bytes`,
`offline_overflow`, `offline_wallclock`, `end_reason`, `end_strategy`,
Transport-Epochen, `upload_status`, `dropped`, `published` und `complete`
erzeugt. Eine Session ist nur dann uneingeschränkt verwertbar, wenn insbesondere
`offline_overflow=0`, `dropped=0`, `complete=true` und auf Collector-Seite keine
`recv_seq_gaps` vorliegen.

Der `panic_tail` hält bis zu acht grobe Lifecycle-/USB-Breadcrumbs in
RTC-Speicher. Nach einem Reset erscheinen der vorherige Tail und
`reset_reason` im nächsten Health-Event. Er dient der Reset-Diagnose, ersetzt
aber weder die verlorenen RAM-Records noch eine vollständige Timeline.

## Recovery bei WiFi-/Uploadfehlern

Schlägt der WLAN-Reconnect nach 25 s fehl, bleibt die State Machine in
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
RAM-Offline-Zyklus mit 96 Records, `overflow=0` und `complete=true`. Der einzige
BMW-Offline-Versuch desselben Abends lieferte dagegen keine finalisierte
`ram_offline`-Session; alle verwertbaren BMW-Daten waren `tcp_live`. Er ist ein
Fehlversuch, kein Beleg für oder gegen die 0.2.24-Media-Hide-Semantik. Ein neuer
BMW-Lauf muss anhand seines eigenen `capture_health` bewertet werden.

Alle Events stammen aus TinyUSB bzw. dem Device-Stack. Eine kontinuierliche
Device-Stack-Epoche ist **keine Wire-Aufzeichnung**; Bus-Timing und Vorgänge
unterhalb der Instrumentierung bleiben `Wire=UNKNOWN`.

## Hotpath-Regel

Im Messfenster gilt: Callback/Drain schreibt ausschließlich kompakte Records in
RAM. Es gibt keine Capture-, NVS- oder sonstigen Flash-Writes im USB-Hotpath.
Auch der Panic-Tail liegt in RTC-RAM. Konfigurationsspeicherung erfolgt vor dem
Arming; JSONL-Erzeugung, WLAN und TCP-Replay erst nach dem Messfenster.
