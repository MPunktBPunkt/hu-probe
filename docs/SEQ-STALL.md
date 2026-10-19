# SEQ-STALL: READ10 bremsen (BUSY-Pacing) am NBT Evo

Stand FW 0.2.31, nicht kompiliert, nicht im Labor oder Fahrzeug getestet.

## Warum

PiDrive hat gezeigt, **dass** das NBT Evo gebremste READ10 toleriert: Mit Bremse spielte es etwa 65 s bis zum Dateiende, ohne Bremse brach es nach etwa 6 s ab (HU-Technical-Facts E7, R29). Unter Bremse reagierten Menü und Dateiliste etwa 20 s träge (R35). Offen sind laut Parameter-Matrix (Abschnitt 2.2):

| Frage | Stand | Schritt in `seq_stall` |
|---|---|---|
| Timeout pro READ10 | ≥ ~20 s, abgeleitet (A4, P0) | Leiter 2/5/10/20/30/60/120 s |
| Reaktion bei Überschreitung (Reset? Skip? Remount?) | unbekannt | Leiter ab dem ersten abgebrochenen Schritt |
| Ursache der Menülatenz | These A3: wartet auf genau einen 240-Block-Read | Menü-Test und Live-Tempo |
| Benötigter Vorlauf ohne UI-Freeze | ≥ 1 CBW = 120 KiB, abgeleitet (P0) | Live-Tempo B032/B128/B320 |

Die HU-Probe sieht dabei jedes Kommando der HU mit Zeitstempel, auch offline. PiDrive sieht nur Zähler und das Ohr.

„Stall“ meint hier **BUSY**: Der Read-Callback liefert 0, TinyUSB versucht denselben Chunk erneut. Ein USB-Endpoint-STALL (Fehler) wird nie erzeugt; der war in früheren HU-Probe-Läufen ein Messartefakt (`EVID-HUPROBE-2026-10-15-006`).

## Wie die Bremse arbeitet (`msc/ReadStall.*`)

- Ein passendes READ10 wird vor seinem ersten Datenbyte für `delay_ms` gehalten. Danach läuft es ungebremst bis zum Ende.
- Pro Retry wartet der Callback 2 ms (`vTaskDelay`), damit WLAN und Loop weiterlaufen, wie in esp32.pidrive.
- Ein neues Kommando wird am **CBW-Tag** erkannt, nicht an `offset == 0`. Bei TinyUSB ist `offset` in fast jedem 4-KiB-Chunk 0; ein Zurücksetzen des Timers bei jedem Retry war der PiDrive-Fehler in 0.4.47. Die HU vergibt die Tags monoton ([unabhängige Auswertung 2026-10-19](feld/2026-10-19-unabhaengige-auswertung.md)).
- Das Kommando, das beim Aktivieren gerade läuft, wird nie gehalten.
- BOT hat Warteschlangentiefe 1: Während ein READ10 gehalten wird, kommt keine TEST UNIT READY und kein anderes Kommando (E6).
- **Abgebrochen** heißt: Die HU schickt ein neues Kommando, setzt den Bus zurück oder wird abgesteckt, bevor die Haltezeit vorbei ist.
- Lief die Bremse während einer Offline-Messung, schaltet sie sich am Messende ab. Finalize und Upload laufen also nie gegen ein gehaltenes Kommando. Ein Medienwechsel und ein Sequenzabbruch schalten sie ebenfalls ab.

## API und WebUI

`POST /api/stall`:

```json
{"regions": ["body"], "delay_ms": 10000, "count": 1, "arm_delay_s": 20, "window_s": 0}
{"regions": ["head", "body"], "pace": "realtime", "pace_factor": 1.0, "count": 0, "window_s": 100}
{"enable": false}
```

- `regions`:
  - `meta`: Boot, Reserved, FAT, Root, Verzeichnisse;
  - `head`: die ersten `head_blocks` (Standard 16 = 8 KiB, also ID3 und Cover) einer Datei;
  - `body`: der Rest der Datei;
  - `all`: alle drei.
- `file` / `idx`: nur diese Datei (nicht mit `meta` kombinierbar).
- `delay_ms`: Haltezeit pro Kommando, 1–120 000.
- `pace: "realtime"`: Haltezeit = angeforderte Bytes bei der Bitrate der Datei × `pace_factor`. Das entspricht einem Live-Stream ohne Vorlauf, also PiDrive ohne Ring. Ein 240-Block-Read dauert so bei 32 kbit/s 30,7 s, bei 128 kbit/s 7,7 s und bei 320 kbit/s 3,1 s. Für Meta-Reads gilt `delay_ms`.
- `count`: Zahl der gehaltenen Kommandos (0 = unbegrenzt).
- `arm_delay_s`: Start nach dieser Zeit. Während einer Offline-Messung ist das WLAN aus; die Bremse wird deshalb vorher geplant.
- `window_s`: Ende nach dieser Zeit (0 = offen).

`GET /api/stall` und `/api/status` → `stall` zeigen Zustand und Zähler: `held`, `abandoned`, `retries`, `last_wait_ms`, `max_wait_ms`, `last_off`. Die WebUI hat dafür die Karte „Bremse (READ10 BUSY)“.

## Marker

| Marker | Bedeutung |
|---|---|
| `stall_on` | Bremse ist scharf (nach `arm_delay_s`) |
| `read_stall` | ein Kommando wurde gehalten und dann bedient (Zeitpunkt = Beginn des Haltens) |
| `stall_abandon` | ein gehaltenes Kommando hat die HU aufgegeben |
| `stall_off` | Bremse aus (Notiz: `count`, `window`, `capture_end`, `api`, `media_switch` …) |

Live tragen die Marker eine Notiz mit LBA, Bytes, Region, Datei, Soll- und Ist-Haltezeit. Offline steht nur der Name im Ringpuffer. Die Haltezeit misst der Analyzer aber ohnehin aus den Kommandos selbst. Pro Konfiguration werden höchstens 64 `read_stall`-Marker geschrieben.

## Die Sequenz `seq_stall`

Experiment `s5_busy_budget` mit den BOOT-Markern `play_start` → `menu_open` → `menu_reaction` → `audio_gap` → `stop`.

| # | Medium | Bremse | Messdauer | Frage |
|---|---|---|---|---|
| 1–4 | `cache_a` | 1 Audio-Read, 2 / 5 / 10 / 20 s, ab 20 s nach Start | 75–100 s | Wird toleriert? Tonaussetzer? |
| 5–7 | `cache_a` | 1 Audio-Read, 30 / 60 / 120 s | 120–180 s | Wo liegt der Timeout, was tut die HU dann, erholt sie sich? |
| 8 | `cache_a` | alle FAT/DIR-Reads je 5 s, 60 s lang ab 10 s | 90 s | Wie stark hängt das Menü direkt an Meta-Reads? |
| 9–11 | `bitrate_mix` | Kopf und Audio im Live-Tempo, 100 s lang | 120 s | Skaliert die Menülatenz mit der Dauer eines 240-Block-Reads (30,7 / 7,7 / 3,1 s)? |

Die Schritte 5–7 überschreiten den vermuteten Timeout absichtlich. **Nur im Stand.** Erst fahren, wenn 1–4 sauber waren.

## Auswertung (`tools/analyze_session.py` → `read_stalls`)

- Pro READ10 mit mindestens 0,5 s Haltezeit (CBW bis erstes `bot_data` desselben Tags):
  - Datei/Region, Blöcke, `hold_s`;
  - `outcome` (`served` oder `abandoned`);
  - die nächsten Kommandos;
  - ob ein Reset oder Remount folgte (`set_address`, `mount`, `msc_bot_reset`);
  - Zeit bis zur ersten TUR danach.
- `timeout_lower_s` ist die längste tolerierte Haltezeit, `timeout_upper_s` die kürzeste abgebrochene. Über die Leiter hinweg grenzt das den Timeout pro READ10 ein; `--compare` stellt die Sessions nebeneinander.
- `menu_latency_s` ist der Abstand der Bediener-Marker `menu_open` → `menu_reaction`. Er ist nur so genau wie der Daumen auf BOOT (Klasse `INFERRED`).
- Voraussetzung: Zeitbasis `capture_t_us` oder `t_us`. Mit dem TUR-Raster ist die Haltezeit nicht messbar, weil während des Haltens keine TUR kommt.

## Was PiDrive davon hat

- **Timeout-Grenze:** Daraus folgt, wie lange PiDrive bei hängendem Stream höchstens BUSY senden darf, bevor es auf Stille umschalten muss (heute `stallMaxMs` ≤ 15 s als Firmware-Grenze).
- **Reaktion bei Überschreitung:** Daraus folgt, ob ein überzogener Stall den Stick kostet (Reset/Remount) oder nur einen Track.
- **Menü-Test und Live-Tempo:** Sie zeigen, ob die Abhilfe gegen den UI-Freeze „Vorlauf ≥ 1 CBW“ oder „höhere Bitrate“ heißt, und wie groß Ring und PSRAM dafür sein müssen.

Keine dieser Fragen ist durch die HU-Probe bisher gemessen. Die PiDrive-Befunde E7/R29/R35 bleiben die einzige Feldevidenz.
