# USB-MSC-Verbesserungspotential

**Projekt:** hu-probe  
**Datum:** 2026-10-30  
**Repo-Basis:** `main` @ `849f30e`  
**Gegenstand:** Bewertung des MSC-/Cache-Instruments. Nicht implementiert.  
**AOA-Verbesserungsplan:** [`2026-10-30-verbesserungsplan-c1b-p0.md`](2026-10-30-verbesserungsplan-c1b-p0.md)  
**Code-Analyse Connected:** [`2026-10-30-codeanalyse-connected-aoa.md`](2026-10-30-codeanalyse-connected-aoa.md)  
**Multi-Review:** [`2026-10-30-gesamtbericht-aoa-zubehoer-multireview.md`](2026-10-30-gesamtbericht-aoa-zubehoer-multireview.md)  
**Problemfelder:** [`2026-10-30-aoa-problemfelder.md`](2026-10-30-aoa-problemfelder.md)  
**Gesamtbewertung:** [`2026-10-30-gesamtbewertung-aoa-fuenf-reports.md`](2026-10-30-gesamtbewertung-aoa-fuenf-reports.md)  
**Epistemik:** [`../CONNECTED-APP.md`](../CONNECTED-APP.md)

Die eckigen Ziffern im Text sind die mitgelieferten Verweise. Zuordnung zu Dateien in diesem Repo, keine externe Bibliographie:

| Ziffer | Datei |
|---|---|
| [18][19] | `firmware/src/net/EventBus.cpp`, `EventBus.h` |
| [25] | `firmware/src/capture/AutoMarker.h` |
| [26] | `firmware/src/capture/AutoMarker.cpp` |
| [27] | `firmware/src/capture/ExperimentProfiles.h` |
| [28] | `firmware/src/capture/OfflineCapture.cpp` |
| [29] | `firmware/src/capture/OfflineCapture.h` |
| [36] | `firmware/src/media/MediaLayout.h` |
| [37] | `firmware/src/media/MediaLayout.cpp` |
| [38] | `firmware/src/media/MediaPresets.h` |
| [39] | `firmware/src/media/Mp3Gen.h` |
| [40] | `firmware/src/media/ProbeMedia.cpp` |
| [41] | `firmware/src/media/ProbeMedia.h` |
| [61] | `firmware/src/web/WebUi.cpp` |
| [67] | `firmware/src/msc/ReadStall.h` |
| [73] | `firmware/src/msc/MscBotTap.cpp` |
| [74] | `firmware/src/msc/MscBotTap.h` |
| [75] | `firmware/src/msc/MscGadget.cpp` |
| [76] | `firmware/src/msc/MscGadget.h` |
| [77] | `firmware/src/msc/ReadInstrument.cpp` |
| [78] | `firmware/src/msc/ReadInstrument.h` |
| [79] | `firmware/src/msc/ReadStall.cpp` |

> **Ablagehinweis zum Stand `849f30e`:** Der Text ab der Gesamtbewertung ist die gelieferte Einschätzung. Sie ist nicht umgesetzt. Die Device-Stack-Grenze, die festen TinyUSB-Offsets, die Mutationsmischung und der Dateiname ohne Pfad stehen so im Code. Drei Stellen hat der Commit schon weiter, als der Text sie beschreibt. Der Abgleich steht am Ende.

---

# Gesamtbewertung

Ja, auch in der **USB-MSC-Analyse** gibt es relevantes Verbesserungspotenzial. Gleichzeitig ist der MSC-Teil des Projekts deutlich weiter entwickelt als der AOA-Teil:

- `MscBotTap` erfasst reale TinyUSB-MSC-/BOT-Ereignisse.
- CBW, DATA, CSW, BOT-Reset, MSC-Control-Requests, Mount/Unmount, STALL und Clear-Halt werden bereits instrumentiert.
- Die Rohzeiten stammen überwiegend aus dem USB-Task und werden in den Arduino-Loop weitergereicht.
- Die LBA-Zuordnung zu FAT-/Verzeichnis-/Dateibereichen ist dynamisch und nicht hartcodiert.

Damit kann `hu-probe` das **positive Leseverhalten** der HU bereits ziemlich gut analysieren. `MscBotTap` dokumentiert aber selbst die entscheidende Einschränkung:

> Die Daten sind Device-Stack-Evidenz und keine direkten USB-Wire-Pakete. [74]

Für die Frage:

> „Hat die HU tatsächlich gelesen oder aus dem Cache abgespielt?“

ist das Instrument daher brauchbar, aber noch nicht optimal. Insbesondere negative Aussagen wie:

> „Die HU hat nicht gelesen“

müssen vorsichtig formuliert werden. Belastbar ist aktuell eher:

> „Im TinyUSB-MSC-Read-Pfad wurde kein Read sichtbar.“

---

# 1. Was bereits gut gelöst ist

## 1.1 Reale BOT-Kette statt synthetischer READ10-Ereignisse

Im aktuellen MSC-Pfad werden CBW, DATA und CSW über TinyUSB-Hooks erfasst. `MscBotTap` verwendet dafür unter anderem einen Wrapper um `mscd_xfer_cb` und liest den internen MSC-Zustand aus `_mscd_itf` [73].

Die Kette ist:

```text
HU sendet CBW
  → TinyUSB MSC verarbeitet CBW
  → hu-probe liest CBW aus TinyUSB-Zustand
  → ProbeMedia liefert Daten
  → bot_data wird aufgezeichnet
  → CSW wird aufgezeichnet
```

Das ist wesentlich besser als ausschließlich aus `MscGadget::onRead()` synthetische READ10-Events abzuleiten.

Die `ReadInstrument`-Events unterscheiden außerdem zwischen:

```text
bot_cbw
bot_data
read
bot_csw
```

und markieren reale TinyUSB-Ereignisse mit:

```json
"bot_source": "tinyusb",
"synthesized": false
```

[77][78]

Das ist eine gute Grundlage für forensische Auswertung.

---

## 1.2 Zeitstempel entstehen am richtigen Ort

Die Capture-Zeit wird bereits im USB-Task gesetzt:

```cpp
p.tUs = ProbeClock::nowUs();
```

und nicht erst beim späteren Export im Arduino-Loop [73].

Das ist für die Cacheanalyse wichtig, weil die Reihenfolge und Abstände zwischen:

- CBW,
- erstem READ10,
- weiteren READ10,
- CSW,
- UNIT ATTENTION,
- Mount/Unmount

erhalten bleiben.

Die spätere Ausgabe verwendet den Capture-Zeitpunkt und nicht ausschließlich den Zeitpunkt der JSON-Erzeugung [77].

---

## 1.3 Dynamische LBA-/Dateizuordnung

Die Zuordnung der LBA-Bereiche kommt aus dem tatsächlich aktiven FAT-Layout:

```cpp
ProbeMedia::instance().classify(lba, ...)
```

[40][41]

Dadurch ist die Analyse nicht fest an bestimmte Demo-LBAs gebunden. Das ist für die unterschiedlichen Medienvarianten sehr wertvoll:

- andere Clustergrößen,
- andere Dateigrößen,
- andere Verzeichnisstrukturen,
- Fragmentierung,
- Headervarianten,
- unterschiedliche Dateipositionen.

Die Daten können dadurch als:

```text
BOOT
FAT
ROOT
DIR
DATA
FREE
OOR
```

klassifiziert werden [36][37].

Das ist für die Cachefrage deutlich aussagekräftiger als eine reine LBA-Liste.

---

## 1.4 Gute Unterstützung für Cacheexperimente

Die Medienmodelle erlauben gezielte Variationen:

- identischer Inhalt,
- geänderter Inhalt,
- geänderte Seriennummer,
- geändertes Volume Label,
- geänderte Volume-ID,
- andere Dateigröße,
- andere Platzierung,
- andere Fragmentierung,
- andere ID3-/APIC-/Xing-Strukturen.

Beispiele dafür sind `cache_a`, `cache_b`, `cache_a_pin`, `ident_serial`, `ident_label`, `ident_vol`, `size_1m`, `size_4m`, `size_16m`, `hdr_variants` und `frag_mix` [38].

Auch die Sequenzen sind bereits sinnvoll strukturiert:

- Inhalt versus Serial
- gleicher Inhalt versus geänderter Inhalt
- Fresh versus Cycle
- identische Seriennummer versus neue Seriennummer
- verschiedene Größen
- Cache-/Replay-Szenarien [27].

Das ist konzeptionell bereits ein guter Versuchsaufbau.

---

# 2. Wichtigste Schwächen der aktuellen MSC-Analyse

## Priorität 1: Cacheantwort und echtes Lesen werden noch nicht explizit getrennt

Aktuell wird ein READ10-Ereignis aufgezeichnet, wenn die HU den Probe-Device-Stack tatsächlich über MSC liest. Das ist ein guter positiver Nachweis.

Aber die Auswertung unterscheidet noch nicht vollständig zwischen:

```text
HU liest Datei erneut
HU liest nur FAT/DIR erneut
HU liest Dateikopf erneut
HU liest Audiodaten erneut
HU spielt vollständig aus Cache
HU liest nur wenige Prüfbytes
HU verwirft den Cache und liest danach
```

Diese Fälle sollten als eigene Kategorien in der Analyse erscheinen.

### Empfohlene Read-Kategorien

```text
metadata_read
file_head_read
file_body_read
file_tail_read
full_file_read
partial_file_probe
readahead_read
cache_validation_read
```

Die bestehende `ReadStall`-Klassifikation unterscheidet bereits `meta`, `head` und `body` [67][79]. Diese Logik sollte in die normale Auswertung übernommen werden, nicht nur für Stallversuche.

---

## Priorität 2: Ein READ10 muss als Transaktion zusammengefasst werden

Die Software erzeugt bewusst mehrere Events für einen Lesevorgang:

```text
bot_cbw
bot_data
read
bot_csw
```

[77]

Das ist korrekt, führt aber bei Auswertungen leicht zu einer falschen Interpretation:

```text
1 READ10 = 2 Reads
```

weil sowohl `bot_data` als auch `read` als „Leseereignis“ erscheinen können.

### Empfehlung

Zusätzlich zu den Rohereignissen ein abgeleitetes Transaktionsobjekt erzeugen:

```json
{
  "ev": "msc_read_transaction",
  "bot_tag": 123,
  "lba_first": 2048,
  "lba_last": 2055,
  "bytes_requested": 4096,
  "bytes_received": 4096,
  "region": "DATA",
  "file": "TRACK1.MP3",
  "file_block_first": 0,
  "file_block_last": 7,
  "cbw_seen": true,
  "data_seen": true,
  "csw_seen": true,
  "status": 0,
  "dur_us": 3800,
  "gap_us": 120000
}
```

Die Rohereignisse bleiben erhalten. Die Transaktion ist lediglich eine normalisierte Auswertungsebene.

---

## Priorität 3: `bot_data` beschreibt nicht zwingend den vollständigen Hosttransfer

In `MscGadget::onRead()` wird je Callback ein Datenstück gemeldet:

```cpp
MscBotTap::emitDataChunk(t0, lba, bufsize, dur, gap, ...)
```

[75]

Bei größeren READ10-Kommandos können deshalb mehrere `bot_data`-Events zu einem einzigen CBW gehören. Die Software coalesct solche Daten im Offlineformat teilweise wieder [28][29].

Für die Liveanalyse sollte ebenfalls zuverlässig rekonstruiert werden:

```text
CBW-Tag 4711:
  DATA LBA 1000, 4096 B
  DATA LBA 1008, 4096 B
  DATA LBA 1016, 2048 B
  CSW passed
```

statt diese Datenstücke als drei unabhängige HU-Leseaktionen zu behandeln.

### Empfohlene Transaktionsregeln

Ein READ10-Transaktionsobjekt sollte enthalten:

- CBW-Tag
- CBW-Zeit
- angeforderte Bytezahl
- Start-LBA
- Blockzahl
- alle DATA-Chunks
- tatsächliche Bytezahl
- erster und letzter Zeitstempel
- CSW-Status
- Residue
- STALL-/Reset-Zustand
- Datei und Dateiblock
- Region
- `mutation_gen` des Mediums

---

## Priorität 4: Queue-Drops invalidieren derzeit nicht automatisch den Lauf

Der `MscBotTap`-Ring ist 256 Einträge groß. Bei Überlauf wird der älteste Eintrag verworfen:

```cpp
pendingHead_ = (pendingHead_ + 1) % kPendingRing;
pendingCount_--;
pendingOverflow_++;
```

[73][74]

Das ist robust für den Betrieb, aber forensisch kritisch. Ein Lauf mit:

```text
pendingOverflow > 0
```

darf nicht mehr als vollständige Lesechronologie gelten.

### Aktuelles Problem

Der Counter ist vorhanden, aber die Auswertung muss daraus explizit ableiten:

```text
capture_valid = false
```

Der aktuelle `capture_health`-Mechanismus berücksichtigt primär EventBus-Drops und andere allgemeine Verluste [18][19]. Für MSC müssen mindestens zusammengeführt werden:

```text
msc_pending_overflow
msc_sample_overflow
eventbus_dropped
eventbus_stream_lost
tcp_tx_fail
offline_overflow
offline_time_range_dropped
```

### Empfohlener Status

```json
{
  "msc_capture_valid": false,
  "msc_capture_invalid_reasons": [
    "pending_ring_overflow"
  ],
  "msc_pending_overflow": 2,
  "msc_sample_overflow": 0,
  "eventbus_dropped": 0,
  "eventbus_stream_lost": 0
}
```

Ohne diese Regel kann ein scheinbar vollständiger Lauf einzelne entscheidende READ10s verloren haben.

---

# 3. Kritischer technischer Punkt: Der aktuelle MscBotTap ist nicht vollständig wire-nah

## 3.1 Positiv: Er ist näher am Stack als reine App-Logs

Die Verwendung von Linker-Wrappers für:

- `mscd_xfer_cb`
- `mscd_control_xfer_cb`
- `dcd_set_address`
- `usbd_edpt_stall`
- `usbd_edpt_clear_stall`
- `usbd_edpt_xfer`

ist sehr leistungsfähig [73].

Damit sieht der Code deutlich mehr als nur `USBMSC.onRead()`.

## 3.2 Einschränkung: Private TinyUSB-Strukturen

Der Code greift auf:

```cpp
extern "C" uint8_t _mscd_itf[];
```

und feste Offsets zu:

```cpp
kOffCbw = 0
kOffCsw = 32
kOffStage = 48
```

[73]

Das ist stark versionsabhängig. Der Code kommentiert selbst TinyUSB 0.15 und feste Offsets. Das bedeutet:

- bei Stackänderungen kann die Analyse still falsche Bytes lesen,
- CBW-/CSW-Daten können falsch interpretiert werden,
- Stagewerte können sich ändern,
- ein Build gegen eine andere TinyUSB-Version ist nicht automatisch kompatibel.

### Empfehlung

Beim Boot einen Stack-/Layout-Selbsttest ausgeben:

```json
{
  "msc_tap_layout": {
    "tinyusb_expected": "0.15",
    "cbw_offset": 0,
    "csw_offset": 32,
    "stage_offset": 48,
    "layout_verified": true
  }
}
```

Noch besser:

- TinyUSB-Version aus Buildmetadaten aufnehmen.
- Compile-Time-Assertions verwenden, falls Strukturen zugänglich sind.
- Bei unbekannter Version den Tap als `unsupported_layout` markieren.
- Niemals stillschweigend weiter analysieren.

---

## 3.3 Standard-Control-Sicht ist weiterhin nur teilweise vorhanden

`MscBotTap` fängt MSC-Control-Requests über:

```cpp
__wrap_mscd_control_xfer_cb
```

[73]

Das erfasst insbesondere MSC-Class-Requests wie:

- `GET_MAX_LUN`
- BOT Reset

und einige Standardereignisse indirekt über:

- `dcd_set_address`
- Mount
- Unmount
- Endpoint Stall/Clear-Halt.

Es ist aber kein vollständiger generischer EP0-Trace. Insbesondere kann aktuell nicht sicher rekonstruiert werden:

- jede `GET_DESCRIPTOR`-Anfrage,
- jedes `SET_CONFIGURATION`,
- jedes `SET_INTERFACE`,
- alle Standard-Requests,
- der vollständige DATA-/STATUS-Ablauf.

Die erzeugten Identity-Events in `ReadInstrument::emitIdentityDescriptorsAt()` sind ausdrücklich synthetisiert:

```json
"bot_source": "gadget_identity",
"synthesized": true,
"note": "local gadget identity (not host control sniff)"
```

[77]

Das ist korrekt markiert, darf aber nicht als Nachweis gewertet werden, dass die HU diese Descriptoren tatsächlich gelesen hat.

---

# 4. Wichtigster inhaltlicher Punkt: Das Cacheproblem ist ein Versuchsdesignproblem

Die Firmware kann feststellen:

> Die HU hat zu einem bestimmten Zeitpunkt wieder einen READ10 an das Gerät gesendet.

Sie kann aber nicht aus dem Ausbleiben eines READ10 allein beweisen:

> Die HU spielt aus einem Cache.

Das Ausbleiben kann auch bedeuten:

- die HU hält die Datei für bereits geladen,
- die HU hat sie aus einem anderen internen Cache,
- die HU hat nur Metadaten aktualisiert,
- die HU hat die Datei verworfen,
- die HU wartet auf einen anderen Trigger,
- die HU liest über einen anderen Pfad,
- der Test hat keinen echten Cacheinvalidierungsreiz ausgelöst.

Deshalb muss der Versuch explizit zwischen drei Hypothesen unterscheiden:

```text
H1: Datei wird aus Cache abgespielt
H2: Datei wird erneut gelesen, aber nur teilweise validiert
H3: Datei wird vollständig neu gelesen
```

Die aktuelle Firmware liefert dafür bereits viele Bausteine, bildet diese Hypothesen aber noch nicht als abgeleitete Auswertung ab.

---

# 5. Der wichtigste konkrete Mangel bei Live-Mutationen

Die größte technische Schwachstelle für Cacheexperimente sehe ich in `ProbeMedia::applyMutation()`.

Die Methode verändert das aktive Layout direkt:

```cpp
if (m.seed >= 0) n.seed = ...
if (m.cover >= 0) n.cover = ...
if (m.tagMask & ...) memcpy(...)
mutationGen_++;
```

[40][41]

Gleichzeitig können USB-Lesezugriffe im USB-Task parallel stattfinden. Der Code kommentiert selbst:

> Ein Sektor kann während eines Updates alte und neue Bytes mischen. Größe und Cluster bleiben zwar konsistent, der Inhalt aber nicht zwingend. [41]

Das ist für Cachetests problematisch.

## Warum?

Wenn die HU einen Datensatz liest, während gerade:

- Seed,
- ID3-Titel,
- Cover,
- Audioinhalt

geändert wird, kann ein einzelner Testlauf enthalten:

```text
alte ID3-Daten
neue Audiodaten
altes Cover
neuer Seed
```

Dann ist später nicht sicher erkennbar, ob:

- der Host aus dem Cache gelesen hat,
- der Host neue Daten gelesen hat,
- oder die Firmware während des Lesens gemischte Daten erzeugt hat.

## Empfehlung

Live-Mutationen müssen an eine definierte Lesegrenze gekoppelt werden.

### Variante A: Mutation nur zwischen USB-Transaktionen

Mutation erst anwenden, wenn:

```text
kein READ10 aktiv
kein pending CBW
kein DATA-Callback aktiv
kein CSW ausstehend
```

### Variante B: Atomarer Generation-Snapshot

Vor jeder Leseoperation:

```cpp
uint32_t gen = mutationGen_;
```

Die generierten Daten werden aus einem konsistenten Snapshot abgeleitet. Wenn die Mutation während der Erzeugung wechselt, wird der Zugriff verworfen und wiederholt.

### Variante C: Zwei vollständige Mediengenerationen

Für besonders wichtige Cachetests:

```text
Generation A vollständig
→ Hostaktion
→ Media-Hide / UA / Re-Enum
→ Generation B vollständig
```

Keine In-Place-Mutation während einer laufenden Dateiübertragung.

Das ist methodisch die sauberste Variante.

---

# 6. Die aktuelle Cache-Versuchsmatrix ist gut, aber sollte präziser werden

Die vorhandenen Presets und Sequenzen sind eine gute Basis [27][38]. Für die Cachefrage würde ich daraus eine explizite Matrix machen.

## Testgruppe A: Kein Inhalt geändert

```text
A1: gleiche Datei, gleiche Serial, kein Re-Enum
A2: gleiche Datei, gleiche Serial, Soft-Cycle
A3: gleiche Datei, neue Serial, Soft-Cycle
A4: gleiche Datei, gleiche Serial, Media-Hide + UA
```

Zweck:

- Baseline für normales HU-Verhalten
- Erkennen, ob ein USB-Cycle grundsätzlich zu erneutem Lesen führt
- Prüfen, ob Serial Teil des Cachekeys ist

---

## Testgruppe B: Metadaten geändert

```text
B1: nur Titel geändert
B2: nur Artist geändert
B3: nur Album geändert
B4: nur ID3v1 geändert
B5: nur Cover geändert
B6: nur Volume Label geändert
B7: nur Volume ID geändert
```

Wichtig: Je Variante darf nur **eine Dimension** geändert werden. Das machen die bestehenden `ident_*`-Varianten bereits teilweise [38].

Auswertung:

```text
Metadaten-Read
Header-Read
Audio-Read
UI-Update
```

Ein erneutes Lesen der FAT-/Directory-Struktur beweist noch nicht, dass die Audiodaten neu gelesen wurden.

---

## Testgruppe C: Audioinhalt geändert

```text
C1: Seed ändern, Dateigröße gleich
C2: Audioframes ändern, ID3 gleich
C3: Dateiinhalte vollständig neu, gleiche Serial
C4: Dateiinhalte neu, neue Serial
```

Hier sollte zusätzlich ein unmissverständlicher Inhaltstest eingebaut werden.

### Empfehlung: sichtbarer Generation-Token

Bei jeder Generation sollte die Datei einen eindeutigen Marker erhalten:

```text
Generation A: HPGEN-A-...
Generation B: HPGEN-B-...
```

Der Marker sollte an mindestens drei Stellen liegen:

- ID3-Titel,
- Audioframe-Ancillary-Bereich,
- optional Cover/APIC.

So kann man unterscheiden:

```text
HU zeigt alte Metadaten
HU liest neue Audiodaten
HU liest nur Dateikopf
HU liest neue Datei vollständig
```

Die vorhandene MP3-Stamp-Logik in `Mp3Gen` ist dafür schon geeignet [39].

---

## Testgruppe D: Cacheinvalidierung erzwingen

```text
D1: UNIT ATTENTION
D2: Media-Hide / NOT READY
D3: Soft USB Disconnect/Reconnect
D4: neue Serial
D5: neue Volume-ID
D6: neue Datei-ID / Pfad
D7: neue Dateigröße
D8: vollständige neue FAT-Struktur
```

Dabei muss exakt protokolliert werden, welche Aktion die HU sieht.

Die Firmware unterstützt bereits:

- UNIT ATTENTION `06/28/00`,
- Remount,
- USB Cycle,
- Serialänderung,
- Media-Hide [73][75][76].

Der wichtigste fehlende Schritt ist eine **auswertbare Kausalitätskette**:

```text
mutation_applied
→ notify_sent
→ HU response
→ first metadata read
→ first data read
→ full data read
→ playback marker
```

---

# 7. UNIT ATTENTION ist vorhanden, aber die Auswertung sollte verbessert werden

Die Implementierung ist grundsätzlich sinnvoll:

```cpp
MscBotTap::requestUnitAttention()
```

und beim nächsten erfolgreichen TUR:

```cpp
06/28/00
```

[73][75].

Das ist ein sauberer standardnaher Reiz.

## Aber aktuell fehlen wichtige Details

Der Lauf sollte explizit ausgeben:

```json
{
  "media_change": {
    "method": "unit_attention",
    "requested_t_us": 123,
    "sent_t_us": 456,
    "trigger_tur_tag": 789,
    "sense_key": 6,
    "asc": 40,
    "ascq": 0
  }
}
```

Zusätzlich:

- welches Medium war vor der UA aktiv,
- welche `mutation_gen` galt,
- welche Datei geändert wurde,
- wie viele TURs bis UA kamen,
- wie lange bis zum nächsten READ verging,
- ob nur Metadaten oder auch Audiodaten neu gelesen wurden.

Aktuell werden `ua_request` und `ua_sent` als Marker aufgezeichnet, was gut ist [75]. Für die Cacheauswertung sollten diese Marker aber mit der folgenden Read-Transaktion verbunden werden.

---

# 8. Fehlende Inhaltsvalidierung auf Read-Ebene

Die Firmware weiß derzeit aus dem Read-Trace:

```text
LBA X wurde gelesen
```

Sie weiß aber nicht automatisch:

```text
Welche Inhaltsgeneration wurde tatsächlich geliefert?
```

Für Cacheexperimente ist das ein wesentlicher Unterschied.

## Empfohlene Ergänzung

Bei ausgewählten Reads, nicht bei jedem 4-KiB-Chunk, berechnen:

```text
content_sample_hash
```

Zum Beispiel:

- erste 32 Bytes eines READ10,
- letzte 32 Bytes,
- MP3-Frame-Header,
- ID3-Titelbereich,
- Audio-Stamp,
- APIC-Anfang,
- deterministischer Hash über die gelieferten Bytes.

### Eventbeispiel

```json
{
  "ev": "msc_content_sample",
  "bot_tag": 123,
  "lba": 2048,
  "file": "TRACK1.MP3",
  "file_block": 0,
  "generation": 7,
  "sample_offset": 0,
  "sample_len": 32,
  "sample_hash": "sha256:...",
  "mutation_gen": 3
}
```

Das sollte selektiv erfolgen, um den USB-Task nicht unnötig zu belasten.

### Minimalvariante

Für jede Datei und Generation nur:

```text
ID3 header hash
audio head hash
audio body sample hash
audio tail hash
```

Damit lässt sich sehr zuverlässig erkennen, ob tatsächlich neue Inhalte geliefert wurden.

---

# 9. Der Dateipfad ist derzeit zu grob

`ReadInstrument::addFileFields()` ergänzt:

```json
file
file_idx
file_blk
```

[77][78]

Das ist gut. Für Cacheanalyse sollte zusätzlich erfasst werden:

```text
file_path
file_generation
file_size
file_offset
file_region
file_block
```

Derzeit wird primär der Node-Name exportiert:

```cpp
doc["file"] = media.layout().nodes[node].name;
```

[77]

Bei mehreren Verzeichnissen oder identischen Dateinamen reicht das nicht. `MediaLayout::nodePath()` existiert bereits [36][37] und sollte für jedes reale Read-Event verwendet werden.

### Statt

```json
"file": "TRACK1.MP3"
```

besser:

```json
"file": "ALBUM1/TRACK1.MP3",
"file_idx": 4,
"file_blk": 128
```

---

# 10. Cache-Auswertung braucht abgeleitete Kennzahlen

Die Rohdaten sind vorhanden, aber die entscheidenden Kennzahlen müssen automatisch berechnet werden.

## Pro Datei

```json
{
  "file": "TRACK1.MP3",
  "first_read_us": 123456,
  "last_read_us": 456789,
  "read_transactions": 42,
  "read_bytes": 1048576,
  "metadata_bytes": 16384,
  "body_bytes": 1032192,
  "unique_blocks": 2048,
  "repeated_blocks": 128,
  "coverage_percent": 100.0,
  "sequentiality": 0.98,
  "generation": 2
}
```

## Wichtige Definitionen

### `read_bytes`

Summe tatsächlich gelieferter DATA-Bytes.

### `unique_blocks`

Anzahl unterschiedlicher 512-Byte-Blöcke oder Read-Intervalle.

### `coverage_percent`

```text
gelesene eindeutige Dateiblöcke / Dateiblöcke insgesamt
```

### `repeated_blocks`

Blöcke, die innerhalb eines Laufes mehrfach gelesen wurden.

### `revalidation_only`

Nur FAT/Directory/Header gelesen, kein Audio-Body.

### `full_reload`

Mindestens 95–100 % der Datei erneut gelesen.

Diese Klassifikation beantwortet die eigentliche Nutzerfrage wesentlich besser als nur:

```text
read_count = 0 / 1 / 2
```

---

# 11. Read-Pattern-Erkennung verbessern

Die HU kann eine Datei auf verschiedene Arten erneut lesen:

```text
sequentiell von Offset 0
nur Header
kleine Stichproben
Tail/ID3v1
Fenster aus dem Audio
große Readahead-Blöcke
mehrfach kleine Blöcke
```

Dafür sollte pro Dateigeneration ein Read-Pattern klassifiziert werden:

```text
NONE
METADATA_ONLY
HEAD_ONLY
SPARSE_PROBE
SEQUENTIAL_PARTIAL
SEQUENTIAL_FULL
RANDOM_PARTIAL
FULL_RELOAD
```

## Beispiel

```text
TRACK1.MP3 Generation 2:
- FAT/DIR reread: yes
- ID3 head reread: yes
- first audio body read: no
- full body reread: no
- classification: METADATA_ONLY / CACHE_PLAYBACK_LIKELY
```

Wichtig: Das Ergebnis sollte als Inferenz markiert werden:

```text
cache_interpretation = "likely"
```

nicht als absolute Tatsache.

---

# 12. Besonders wertvoll: Host- und Probe-Zeitlinie verbinden

Die HU-UI-Aktion muss mit den Reads verbunden werden.

Die Firmware besitzt bereits:

- Session-Marker,
- automatische Marker,
- BOOT-Marker,
- `media_mutate`,
- `ua_request`,
- `ua_sent`,
- `soft_replug`,
- `mount_done`,
- `first_read`,
- `next_track_read`.

[25][26][28]

Das ist gut. Es fehlt aber ein standardisiertes Ereignismodell für die Cacheaktion:

```text
cache_test_start
cache_invalidation_request
cache_invalidation_sent
operator_play
operator_stop
operator_track_next
first_host_read_after_invalidation
first_body_read_after_invalidation
full_reload_detected
cache_result
```

### Empfohlener Ablauf

```text
1. baseline_run_start
2. Datei abspielen
3. cache_invalidation_request
4. Mutation Generation B anwenden
5. UA / Remount / Cycle
6. Track erneut auswählen
7. Reads erfassen
8. cache_result erzeugen
```

Damit lässt sich später automatisch sagen:

```text
Nach UNIT ATTENTION:
  - HU las FAT neu
  - HU las Directory neu
  - HU las ID3 neu
  - HU las Audio-Body nicht neu
  - Ergebnis: Cache weiterhin aktiv oder Alternate-Cache wahrscheinlich
```

---

# 13. Instrumentierungsfehler und konkrete Verbesserungen

## 13.1 `MscBotTap::pendingOverflow()` muss in `/api/status`

Der Status von `ConnectedAppProbe` zeigt Drop-Zähler nur für den AOA-Tap [3]. Der allgemeine MSC-Status zeigt `msc_sample_overflow`, aber nicht klar genug den MscBotTap-Pending-Ring [61].

Empfehlung:

```cpp
doc["msc_pending_overflow"] = MscBotTap::pendingOverflow();
```

und zusätzlich:

```cpp
doc["msc_capture_valid"] =
    MscBotTap::pendingOverflow() == 0 &&
    msc_.sampleOverflow() == 0 &&
    EventBus::instance().dropped() == 0;
```

---

## 13.2 `MscBotTap::drain()` sollte Push-/Persistfehler zählen

Im Offlinepfad wird der Rückgabewert von:

```cpp
OfflineCapture::instance().appendPending(...)
```

nicht ausgewertet [73].

Das bedeutet, dass ein Record verloren gehen kann, ohne dass der konkrete `MscBotTap`-Status weiß:

```text
which kind failed
which timestamp failed
which bot_tag failed
```

Empfehlung:

```cpp
const bool stored = OfflineCapture::instance().appendPending(...);
if (!stored) {
    offline_append_failures_++;
    offline_append_last_kind_ = p.kind;
    offline_append_last_t_us_ = p.tUs;
}
```

Das ist insbesondere bei:

- Buffer overflow,
- Zeitbereichsüberschreitung,
- unbekanntem Recordtyp

wichtig.

---

## 13.3 `pendingOverflow_` zählt nur Ringverluste, nicht EventBus-Verluste

Derzeit existieren mehrere unabhängige Drop-Ebenen:

```text
MscBotTap pending ring
MscGadget sample ring
OfflineCapture RAM buffer
EventBus ring
TCP/Collector
```

[18][19][28][29][73][76]

Die Auswertung sollte eine Verlustkette liefern:

```json
{
  "loss": {
    "msc_pending_ring": 0,
    "msc_sample_ring": 0,
    "offline_buffer": 0,
    "offline_time_range": 0,
    "eventbus_ring": 0,
    "eventbus_stream": 0,
    "tcp_tx_fail": 0
  },
  "analysis_valid": true
}
```

---

## 13.4 `EventBus::publish()` kann bei Ringüberlauf alte Events löschen

Der EventBus überschreibt bei voller Queue das älteste Event [18][19].

Das ist für Laufzeitrobustheit richtig, aber für Cachetests darf ein Lauf mit `dropped > 0` nicht automatisch als vollständig gelten.

Die bestehende Integritätslogik berücksichtigt Drops in mehreren Bereichen bereits [28]. Für C1b-P0/P0.5 sollte dieselbe Logik explizit in die USB-/MSC-Gesamtvalidität übernommen werden.

---

## 13.5 Concurrency der kleinen Ringe

Mehrere Ringe werden aus dem USB-Task geschrieben und im Arduino-Loop gelesen:

- `MscBotTap::pending_`
- `MscGadget::ring_`
- `ReadStall::done_`

Die Indizes sind teilweise `volatile`, aber nicht überall durch denselben Lockmechanismus geschützt [73][76][79].

Das kann funktionieren, ist für ein Messinstrument aber riskant. Ergänzen:

- Producer-/Consumer-Sequenznummer,
- Overflow-/Corruption-Counter,
- klare Single-Producer-/Single-Consumer-Dokumentation,
- Critical Sections oder atomare Indizes.

---

# 14. Ein sehr wichtiger Punkt: USB-Cycle ist nicht gleich Cacheinvalidierung

`MscGadget::softCycle()` trennt nur die D+-Pullup-Verbindung und verbindet sie wieder [75][76].

Das bedeutet:

```text
USB disconnect/reconnect
```

aber nicht automatisch:

```text
HU-interner Mediencache gelöscht
HU-Anwendungscache gelöscht
Datei aus internem Audiopuffer gelöscht
```

Ein USB-Cycle kann sogar dazu führen, dass die HU:

- dieselbe Seriennummer sieht,
- denselben Volume-Hash erkennt,
- dieselbe Dateiidentität wiederherstellt,
- den bereits geladenen Inhalt weiterverwendet.

Deshalb sollte die Testmatrix klar zwischen diesen Aktionen unterscheiden:

```text
USB transport reset
MSC LUN not-ready
UNIT ATTENTION
Serial changed
Volume ID changed
File content changed
File size changed
Path changed
Full medium identity changed
```

Nur weil ein USB-Cycle stattfindet, ist nicht bewiesen, dass der Cache verworfen wird.

---

# 15. Was die bestehende Firmware für Cacheanalyse zuverlässig kann

## Stark belastbar

Mit der aktuellen Firmware kann man positiv feststellen:

- HU hat ein MSC-Gerät gemountet.
- HU hat MSC-Control-Requests wie BOT Reset oder GET_MAX_LUN gesendet, sofern sie im MSC-Class-Wrapper sichtbar sind.
- HU hat ein CBW mit bestimmtem SCSI-Befehl verarbeitet.
- HU hat im Device-Stack Daten für eine bestimmte LBA angefordert.
- HU hat einen bestimmten Dateibereich gelesen.
- HU hat einen bestimmten Datenumfang gelesen.
- HU hat einen neuen READ10-Transaktionszyklus begonnen.
- HU hat nach einer Aktion erneut FAT/DIR/Header/Body gelesen.
- HU hat bestimmte STALL-/Reset-/Clear-Halt-Zustände ausgelöst.

## Nur eingeschränkt belastbar

- vollständige Host-Control-Sequenz,
- alle Descriptorzugriffe,
- echte physikalische USB-Transfers,
- „kein Read“ als Beweis für Cache,
- „keine weiteren Reads“ nach einem bestimmten Zeitpunkt ohne Host-Trace,
- tatsächliche Bus-Übertragung von jedem Read-Byte.

---

# 16. Empfohlene Prioritäten

## Priorität 1: Read-Transaktionen und Capture-Gültigkeit

Unbedingt zuerst umsetzen:

1. CBW/DATA/CSW zu einer `msc_read_transaction` zusammenfassen.
2. `pendingOverflow`, `sampleOverflow`, Offline-Drops und EventBus-Drops als Gültigkeit ausweisen.
3. Rohereignisse beibehalten.
4. `bot_data` nicht als zweiten unabhängigen Read zählen.
5. Eindeutige `file_path` statt nur `file.name` ausgeben.

Das verbessert die Analyse sofort, ohne den USB-Stack zu verändern.

---

## Priorität 2: Mutation atomar und generationssicher machen

Vor weiteren Cacheexperimenten:

1. Keine In-Place-Mutation während einer laufenden READ10-Transaktion.
2. `mutation_gen` in jedes Read-Event aufnehmen.
3. Pro Read-Transaction eine Generation speichern.
4. Nach Mutation erst ab klarer USB-/BOT-Grenze neue Daten liefern.
5. Für kritische Tests vollständige Generation A/B verwenden.

Ohne diese Korrektur bleiben die Cacheexperimente potenziell mehrdeutig.

---

## Priorität 3: Inhaltsproben integrieren

Für ausgewählte Leseoperationen:

- Hash vom ID3-Bereich,
- Hash vom Audioanfang,
- Hash aus Audio-Mitte,
- Hash vom Dateiende.

Damit wird aus:

```text
Datei erneut gelesen
```

die präzisere Aussage:

```text
Datei Generation B wurde tatsächlich vom Host angefordert und geliefert.
```

---

## Priorität 4: Automatische Cacheklassifikation

Pro Datei und Generation berechnen:

- Read-Transactions,
- Bytes,
- Coverage,
- Metadata-only,
- Body-read,
- Full-reload,
- Wiederholungsblöcke,
- erstes Lesen nach Invalidierung,
- Zeit bis zum ersten Read.

Das kann zunächst im Collector erfolgen. Es muss nicht zwingend auf dem ESP32 laufen.

---

## Priorität 5: EP0-/Host-Trace ergänzen

Für MSC nicht zwingend als erster Schritt, aber notwendig für maximale Sicherheit:

- vollständiger generischer EP0-Hook im TinyUSB-Core,
- Host-seitiger `usbmon`-/Wireshark-Trace,
- bei kritischen Fällen Hardware-Analyzer.

Der aktuelle `MscBotTap` ist für MSC-BOT sehr leistungsfähig, aber kein vollständiger USB-Bus-Analyzer.

---

# 17. Konkrete Zielstruktur für Cachetest-Events

Ein sinnvoller Minimaldatensatz wäre:

```json
{
  "ev": "msc_read_transaction",
  "sid": "session-id",
  "seq": 4711,
  "bot_tag": 1234,
  "t_first_us": 1000000,
  "t_last_us": 1004200,
  "lba_first": 2048,
  "lba_last": 2055,
  "bytes_requested": 4096,
  "bytes_received": 4096,
  "file_path": "ALBUM1/TRACK1.MP3",
  "file_idx": 7,
  "file_block_first": 0,
  "file_block_last": 7,
  "region": "DATA",
  "read_class": "file_body",
  "mutation_gen": 3,
  "cbw_seen": true,
  "data_seen": true,
  "csw_seen": true,
  "csw_status": 0,
  "residue": 0,
  "stall": false,
  "reset": false,
  "capture_valid": true
}
```

Für eine Datei-Zusammenfassung:

```json
{
  "ev": "cache_analysis",
  "file_path": "ALBUM1/TRACK1.MP3",
  "generation_before": 1,
  "generation_after": 2,
  "invalidation": "unit_attention",
  "metadata_reread": true,
  "body_reread": false,
  "full_reload": false,
  "unique_bytes_read": 16384,
  "coverage_percent": 1.2,
  "classification": "CACHE_PLAYBACK_LIKELY",
  "confidence": "medium"
}
```

Wichtig ist die Formulierung:

```text
CACHE_PLAYBACK_LIKELY
```

nicht:

```text
CACHE_PROVEN
```

Denn eine andere interne HU-/Medienstrategie kann ein identisches Read-Muster erzeugen.

---

# Schlussbewertung

## Antwort auf deine Frage

**Ja, in der USB-MSC-Analyse gibt es Verbesserungspotenzial.** Der aktuelle Stand ist aber grundsätzlich solide und für positive Leseereignisse bereits leistungsfähig.

Die wichtigste Aussage lautet:

> **Das Instrument kann heute recht gut zeigen, wann und welche Bereiche die HU über MSC anfordert. Es kann aber noch nicht optimal bewerten, ob ein fehlendes Lesen tatsächlich Cacheverhalten oder nur eine andere interne HU-Strategie bedeutet.**

## Die drei wichtigsten Verbesserungen sind

### 1. Read-Transaktionen statt Einzelereignisse

CBW, DATA und CSW müssen automatisch zu einer vollständigen READ10-Transaktion zusammengeführt werden.

### 2. Konsistente Mediengenerationen

`applyMutation()` darf nicht parallel zu laufenden USB-Reads den sichtbaren Dateiinhalt verändern. Die aktuelle Möglichkeit, alte und neue Daten in einem Read zu mischen, ist für Cachetests ein echter methodischer Schwachpunkt [40][41].

### 3. Inhaltsnachweis

Die Firmware sollte selektiv nachweisen, welche Inhaltsgeneration tatsächlich gelesen wurde. Die bestehende Seed-/Stamp-/ID3-Infrastruktur ist dafür bereits geeignet [39].

## Priorisierte Projektentscheidung

Ich würde die weitere Arbeit nicht zuerst in einen tieferen USB-Sniffer investieren. Für das konkrete HU-Cacheproblem ist diese Reihenfolge effizienter:

```text
1. Read-Transaktionsmodell
2. Drop-/Capture-Validität
3. Mutation-Atomizität
4. Generation-/Content-Hash
5. Cacheklassifikation
6. Host-Trace nur für widersprüchliche oder negative Befunde
```

Das Ziel sollte nicht sein, noch mehr Rohdaten zu sammeln, sondern aus den vorhandenen Rohdaten eindeutigere Aussagen zu machen:

> **Hat die HU nur Metadaten geprüft, Audiodaten teilweise gelesen, die Datei vollständig neu geladen oder überhaupt keinen neuen USB-Read ausgeführt?**

---

# Repo-Abgleich (nicht Teil der gelieferten Bewertung)

Geprüft gegen `main` @ `849f30e`. Keine der empfohlenen Änderungen ist umgesetzt. Die Device-Stack-Grenze, die Offsets, die Mutationsmischung und der Dateiname ohne Pfad stehen so im Code.

| Stelle in der Bewertung | Stand `849f30e` |
|---|---|
| Device-Stack, nicht Wire | `MscBotTap.h`: „Epistemic: device-stack bytes, not wire packets.“ |
| BOT-Kette und Offsets | `__wrap_mscd_xfer_cb` und `__wrap_mscd_control_xfer_cb`. Kommentar: TinyUSB 0.15, `kOffCbw=0`, `kOffCsw=32`, `kOffStage=48`. Kein `layout_verified`, keine Compile-Time-Assertion. Plattformpin: `espressif32@6.4.0`. Der MSC-Control-Wrapper vergleicht `CONTROL_STAGE_SETUP`. |
| Zeitstempel | `MscBotTap::queueControl` setzt `p.tUs = ProbeClock::nowUs()` im Callback. `queue()` ruft `ProbeClock::observeEventTUs` auf, bevor der Ring publiziert. |
| Regionen | `MediaLayout::Region` ist Boot, Reserved, Fat, Root, Dir, File, Free, Beyond. Die Namen sind `BOOT`, `RSVD`, `FAT`, `ROOT`, `DIR`, `DATA`, `FREE`, `OOR`. `RSVD` fehlt in der Liste im Text. |
| Presets und Sequenzen | `cache_a`, `cache_b`, `cache_a_pin`, `ident_serial`, `ident_label`, `ident_vol`, `size_1m`, `size_4m`, `size_16m`, `hdr_variants`, `frag_mix` stehen in `MediaPresets.h`. Sequenzen in `ExperimentProfiles.h`. |
| `ReadStall` meta/head/body | Vorhanden, nur für Stallversuche. `ReadStall` schreibt den Pfad bereits über `nodePath()`. Die normalen Read-Events tun das nicht. |
| Dateiname | `ReadInstrument::addFileFields()` setzt `file` auf `nodes[node].name`, plus `file_idx` und bei File-Region `file_blk`. `nodePath()` existiert und wird in `ProbeMedia::toJson` benutzt, nicht in den Read-Events. |
| Offline-Coalesce | `OfflineCapture::appendPending` fasst aufeinanderfolgende DATA desselben BOT-Tags zusammen, sofern nicht `fineData`. Live-JSON bleibt chunkweise. |
| `appendPending`-Rückgabe | `MscBotTap::drain()` ignoriert den `bool`. Ein fehlgeschlagener Offline-Append wird nicht als eigener Tap-Fehler gezählt. `OfflineCapture` zählt `overflow_` und `timeRangeDropped_` selbst. |
| `capture_health` | Der periodische Event aus `App::emitCaptureHealth()` enthält bereits `msc_sample_overflow` und `bot_pending_overflow`, dazu EventBus-`dropped`, `stream_lost` und `tcp_tx_fail`. Es gibt kein `msc_capture_valid`. `/api/status` in `WebUi.cpp` schreibt `msc_sample_overflow`, nicht `MscBotTap::pendingOverflow()`. Der Offline-Health (`transport=ram_offline`) hat `offline_overflow` und `offline_time_range_dropped`. Ein beendeter Offline-Lauf setzt `integrityPass` nur, wenn Overflow und Drops null sind und der Lauf vollständig ist. Die Live-MSC-Auswertung faltet das nicht zu einem Flag. |
| Ringüberlauf | `kPendingRing=256`. Bei Vollstand wird der älteste Eintrag verworfen und `pendingOverflow_++`. Der neue Eintrag wird danach geschrieben. `MscGadget::kSampleRing=32` gilt seit 0.2.12 nur noch für Write-Notizen, nicht für Reads. |
| Concurrency | `pendingHead_`/`pendingTail_`/`pendingCount_` und der Sample-Ring sind `volatile`, ohne Critical Section. `ReadStall::doneHead_` und `doneCount_` sind keine `volatile`; `doneDropped_` existiert. |
| `applyMutation` | Schreibt Seed, Cover und Tags in das aktive Layout und erhöht `mutationGen_`. Kommentar in `ProbeMedia.h`: Reads laufen parallel auf dem USB-Task. `mutation_gen` steht in `ProbeMedia::toJson`, nicht in den Read-Events. |
| Stamp | `Mp3Gen` schreibt bei `kStamp` „HP“ plus Node, Seed und Frame-Index in Ancillary-Bytes. Ein `HPGEN-A`-Token ist nicht implementiert. |
| UNIT ATTENTION | `requestUnitAttention()` setzt `uaPending_`. Der nächste bereite TUR bekommt `tud_msc_set_sense(..., 0x28, 0x00)`. Marker `ua_request` und `ua_sent` existieren. Kein zusammengefasstes `media_change`-Objekt und keine Bindung an die folgende Read-Transaktion. |
| Soft-Cycle | `MscGadget.h`: Soft-Cycle ist `tud_disconnect`/`tud_connect` am D+-Pull-up, nicht VBUS. `softCycle()` ruft `softDetach()` auf. |
| Identity-Events | `emitIdentityDescriptorsAt()` setzt `bot_source=gadget_identity`, `synthesized=true`, Note `local gadget identity (not host control sniff)`. |
| Ziffer [3] in Abschnitt 13.1 | Im AOA-Status meint dieselbe Ziffer `ConnectedAppProbe.cpp`. Dort stehen die AOA-Drop-Zähler. Der MSC-HTTP-Status ist `WebUi.cpp`. |
