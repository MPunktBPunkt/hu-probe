# Kontrollierte Medienvariation

Ziel: Der HU genau **eine** Variable pro Vergleich präsentieren, damit Cache-, Prefetch- und Identity-Hypothesen (HU-A2, M3, tail_then_head) trennbar werden.

Bis FW 0.2.30 gab es nur das Flash-Image `demo_v1` mit drei byte-identischen Tracks gleicher Größe. Damit sind Cache-Hit und „kein Prefetch“ nicht unterscheidbar (siehe [unabhängige Auswertung 2026-10-19](feld/2026-10-19-unabhaengige-auswertung.md), K4). Ab **FW 0.2.31** erzeugt die Sonde das Medium zur Laufzeit (`firmware/src/media/SynthFat`).

## Wie das synthetische Medium funktioniert

- **FAT16 zur Laufzeit:** Bootsektor, beide FATs, Root- und Unterverzeichnisse sowie die MP3-Daten werden pro gelesenem LBA erzeugt. Es wird kein Flash belegt; Volumes bis 2 GiB sind möglich, Standard ist „so klein wie für FAT16 nötig“ (≥ 16 MiB).
- **MP3-Generator** (`Mp3Gen.h`): MPEG-1 Layer III, 44,1 kHz, Joint Stereo, stille Frames. Die Bitrate ist wählbar (32–320 kbit/s), optional VBR (4-Frame-Muster), Xing/Info-Header und ID3v1 am Dateiende. Dazu kommt ein ID3v2.3-Tag mit Titel, Interpret und Album (TIT2/TPE1/TALB) sowie optional einem APIC-Cover: entweder ein echtes JPEG oder ein Platzhalter beliebiger Größe.
- **ID3-Texte mit fester Slotgröße:** Jeder Textframe hat immer 30 Zeichen Nutzlast, mit NUL aufgefüllt. Der ID3v2-Tag ist dadurch immer 133 Byte groß (157 Byte mit APIC-Kopf). Texte lassen sich so live ändern, ohne dass sich Audio-Offsets, Dateigröße oder Cluster verschieben. Ohne Angabe gelten `HP <name> v<seed>`, `hu-probe` und `HUPROBE`.
- **Echte Cover** (`CoverJpegs.cpp`, erzeugt mit `firmware/scripts/gen_covers.ps1`): vier Baseline-JPEGs mit 320×320 Pixeln und 5,2–6,4 KiB. Das ist das Format, das bei PiDrive auf dem NBT Evo ein Cover zeigt (PiDrive-Fakt M2). Die Farben sind 1 rot, 2 grün, 3 blau, 4 gelb, dazu jeweils eine große Ziffer und „HU-PROBE COVER“. Der APIC-Slot ist immer mindestens so groß wie das größte Cover (6656 Byte, auf 512 aufgerundet); danach folgen Nullbytes. Ein Cover lässt sich deshalb live tauschen.
- **Stempel pro Frame** (Bytes 36–47 jedes Frames): `'H','P', node, seed, frameIdx (BE32), 'F','R'`. Jede Datei und jede Inhaltsversion (`seed`) ist damit byte-verschieden. Bei Bedarf lässt sich in einem USB-Mitschnitt jeder gelesene Block einer Datei und einem Frame zuordnen.
- **Platzierung:** Verzeichnisse liegen zuerst, danach die Dateien in Spec-Reihenfolge. `gap_clusters` lässt eine Lücke vor der Datei, `frag:true` verschränkt alle markierten Dateien im Raster `fragment_kib` (Round-Robin). `reserved` verschiebt den ganzen Datenbereich (z. B. damit LBA 24 nicht in TRACK1 liegt).
- **Klassifikator (B21):** Jedes `bot_data`/`read`-Event trägt jetzt `tag` (BOOT/RSVD/FAT/ROOT/DIR/DATA/FREE/OOR), `file`, `file_idx` und `file_blk` aus dem aktiven Medium. Zu Beginn jeder Session meldet die Sonde `session.media` (Variante, Hash, Geometrie) und ein `media_layout`-Event pro Datei mit den LBA-Extents.

## Dimensionen

| Dimension | Werte | Frage | Preset |
|-----------|-------|-------|--------|
| Dauer | 10 / 26 / 60 / 180 s bei 48 kbit/s | Prefetch-Regel: Vorlauf vor Ende oder Verzögerung nach Start? | `dur_mix` |
| Bitrate | 32 / 128 / 320 kbit/s bei 60 s | puffert die HU zeit- oder bytebasiert? | `bitrate_mix` |
| Größe | 1 / 4 / 8 MiB bei 128 kbit/s | Streaming gegen Read-All, Refill-Schwelle, Chunking | `size_1_4_8` |
| PiDrive-Layout | FAVORITE/ 8 MiB + 2 × 512 KiB, MENUE/ | Verhalten auf echter PiDrive-Geometrie (FAT16 16 MiB, spc 8) | `pidrive_l3` |
| Struktur | 3 Ordner, 30 Dateien, verschachtelt | Scan oder On-Demand, Traversierung, Ordnergrenzen | `folders_30` |
| Header | plain, ID3v1, APIC 256 KiB, Xing, VBR | entsteht tail_then_head durch Tag-Suche? | `hdr_variants` |
| Platzierung (Reserved) | 40 reservierte Sektoren | ist LBA 24/8 ein festes Offset oder datei-relativ? | `tail_probe` |
| Fragmentierung | 64-KiB-Fragmente verschränkt | 240-Block-Grenze, Folgen von Cluster-Ketten | `frag_mix` |
| Inhalt | seed 1/2/3 gegen 11/12/13 | Content-Cache (M3) | `cache_a` / `cache_b` |
| Platzierung (Lücke) | +100 Cluster vor TRACK1 | LBA-Cache | `cache_place` |
| Identität | Serial `HP0B01`, Label `HUPROBE_B`, Volume-ID `0x48500B01` | Identity-Cache (S6) | `ident_b` |
| ID3-Texte und Cover | eigene Texte, Cover 1–3, eine Datei mit leerem Cover-Slot, ID3v1 auf COVER2 | zeigt die HU Texte/Cover aus ID3v2 oder ID3v1? Wann liest sie sie neu? | `id3_cover` |
| Referenz | Flash-Image FAT12, 3 identische Tracks | Vergleich mit Feldläufen bis 0.2.30 | `demo_v1` |

Die Presets `cache_a`, `cache_b`, `cache_place` und `ident_b` unterscheiden sich jeweils in genau einer Dimension von `cache_a`.

## Auswahl

- **WebUI:** Karte „Medium (SynthFat)“: Preset wählen, Wechselart wählen, „Aktivieren“. Eine eigene JSON-Spezifikation kann im aufklappbaren Feld geladen werden.
- **API:**
  - `GET /api/media` liefert die aktive Variante inklusive Dateiliste und alle Presets.
  - `GET /api/media/presets` liefert nur die Presets.
  - `POST /api/media/variant` mit `{"preset":"dur_mix","method":"cycle"}` oder `{"spec":{…},"method":"remount"}`.
    - `cycle` (Standard): LUN aus, Medium neu aufbauen, USB-Re-Enumeration. Die HU liest Kapazität und Dateisystem neu.
    - `remount`: nur ein NOT-READY-Fenster von 1,5 s ohne Re-Enumeration.
  - Während einer Offline-Messung oder Sequenz wird der Wechsel mit 409 abgelehnt.
- Die gewählte Variante wird im NVS (`hpmedia`) gespeichert und überlebt einen Neustart. Sequenzen schalten nur temporär um.
- Läuft gerade eine Live-Session, beendet der Wechsel sie und startet eine neue. Eine Session gehört damit immer zu genau einem Medium.

## Spezifikationsformat

```json
{
  "id": "custom", "label": "frei", "volume_mib": 0, "spc": 8, "reserved": 1,
  "fragment_kib": 64, "kbps": 128, "stamp": true,
  "fs_label": "HUPROBE", "volume_id": "0x48500131", "usb_serial": "",
  "files": [
    {"path": "ALBUM/T01.MP3", "dur_s": 26.5, "kbps": 128, "seed": 1},
    {"path": "ALBUM/T02.MP3", "size_kib": 4096, "vbr": true, "xing": true,
     "id3v1": true, "apic_kib": 64, "gap_clusters": 10, "frag": true},
    {"path": "ALBUM/T03.MP3", "dur_s": 26.5, "title": "Mein Titel", "artist": "Künstler",
     "album": "Album", "cover": 2},
    {"path": "ALBUM/T04.MP3", "dur_s": 26.5, "cover_slot": true}
  ]
}
```

- `title`, `artist`, `album`: höchstens 30 Zeichen in ISO-8859-1. Umlaute aus UTF-8 werden umgewandelt, andere Zeichen werden zu `?`. Längere Texte lehnt die Sonde mit `tag_le_30` ab.
- `cover`: 0 bis 4. Mit 1–4 wird das echte JPEG eingebettet. 0 bedeutet Platzhalter, aber nur, wenn ein Slot existiert.
- `cover_slot: true` reserviert einen Cover-Slot ohne echtes Bild (Platzhalter). So kann später live ein Cover eingesetzt werden.
- `apic_kib` vergrößert den Slot. Bei einem echten Cover folgen danach Nullbytes (Frage aus der Parameter-Matrix: maximale ID3-/Covergröße, 16/32/64 KiB).
- `no_id3v2` zusammen mit einem Cover wird abgelehnt (`apic_needs_id3v2`).

- Pfade in 8.3-Großschreibung, Ordner werden aus den Pfaden angelegt. Maximal 48 Dateien und 64 Knoten (Dateien plus Ordner).
- Pro Datei entweder `dur_s` oder `size_kib`. Ohne beide Angaben gilt 26 s. Standard-Bitrate ist 48 kbit/s (global über `kbps` änderbar).
- `usb_serial` leer: Die Sonde vergibt pro Re-Enumeration eine neue Serial (`HPnnnn`), wie bisher. Ist eine Serial gesetzt, bleibt sie fest.

## Live-Änderung ohne Remount und weicher Medienwechsel

Ein USB-Stick ist passiv: Die Sonde kann die HU nicht zum Lesen zwingen. Sie kann aber (a) den Inhalt einer Datei ändern, ohne dass sich das Dateisystem ändert, und (b) der HU auf verschiedenen Wegen mitteilen, dass sich das Medium geändert haben könnte. Ob und was die HU daraufhin neu liest, ist genau die Messfrage.

### Datei ändern: `POST /api/media/mutate`

```json
{"file": "COVER1.MP3", "title": "Neu", "artist": "X", "album": "Y",
 "cover": 2, "seed": 9, "notify": "unit_attention", "delay_s": 40}
```

- Die Datei wird per `file` (Pfad, Groß-/Kleinschreibung egal) oder `idx` (Knotenindex aus `GET /api/media`) gewählt. Alle anderen Felder sind optional; ein leerer Text setzt den Standardtext zurück.
- Änderbar sind `seed` (ändert den Audio-Stempel in jedem Frame und den Standardtitel), `title`, `artist`, `album` (ID3v2 und, falls vorhanden, ID3v1) und `cover` (0–4, nur mit Slot).
- Dateigröße, Cluster, FAT und Verzeichniseinträge bleiben immer gleich. Wer auch Größe oder Platzierung ändern will, braucht einen Variantenwechsel.
- `notify` sagt, wie der Host informiert wird:
  - `none`: still. Die HU sieht die Änderung nur, wenn sie die Bytes ohnehin neu liest.
  - `unit_attention`: siehe unten.
  - `remount`: NOT-READY-Fenster von 600 ms mit gleicher Serial.
  - `cycle`: USB-Re-Enumeration (Soft-Detach, 600 ms).
- `delay_s` (bis 600): Die Änderung wird erst nach dieser Zeit ausgeführt. Während einer Offline-Messung ist das WLAN aus und die API nicht erreichbar. Die Änderung wird deshalb vor dem Start der Messung geplant und landet dann mitten in der Messung. Eine neue Anfrage ersetzt eine noch wartende.
- Die Änderung ist **nicht persistent**: Ein Variantenwechsel oder Neustart stellt die Spezifikation wieder her. Ein Variantenwechsel verwirft auch eine wartende Änderung und eine noch nicht zugestellte UNIT ATTENTION.
- `GET /api/media` zeigt pro Datei die aktuell ausgelieferten Texte, `cover` und `apic_bytes` sowie global `mutation_gen`, `ua_pending` und `ua_sent`.

### Nur informieren: `POST /api/media/notify`

`{"kind": "unit_attention" | "remount" | "cycle"}` sendet die Benachrichtigung ohne Inhaltsänderung. Das ist die Kontrollmessung: Liest die HU nach einer UNIT ATTENTION auch dann neu, wenn sich nichts geändert hat?

### UNIT ATTENTION 06/28/00

- Die nächste TEST UNIT READY bei präsentem Medium schlägt mit Sense Key 06 (UNIT ATTENTION), ASC/ASCQ 28/00 fehl: „not ready to ready change, medium may have changed“. Umgesetzt im TUR-Wrapper (`MscBotTap.cpp`).
- Die HU erfährt es beim nächsten TUR-Poll, im NBT-Evo-Leerlauf also nach höchstens etwa 2,24 s (PiDrive E3). Ist das Medium gerade versteckt, bleibt die Anforderung stehen, bis es wieder präsent ist.
- Anders als `remount` und `cycle` gibt es kein NOT-READY-Fenster und keine neue Enumeration. Das ist der weichste der drei Wege.

### Marker in der Session

- `media_mutate` mit Notiz `n<idx> seed title … notify=… gen=…` (live). Offline steht nur der Markername im Ringpuffer, weil Offline-Marker höchstens 15 Zeichen haben.
- `ua_request`, wenn die UNIT ATTENTION angefordert wird.
- `ua_sent` mit dem Capture-Zeitpunkt der TUR, die die UNIT ATTENTION tatsächlich gemeldet hat.
- `tools/analyze_session.py` wertet für jeden dieser Marker ein Fenster von 30 s aus (`mutation_reactions`): Anzahl CBW, die ersten Opcodes (z. B. `request_sense`, `read_capacity10`), den ersten READ10 mit Region (FAT/ROOT/DATA) und Kopf-Reads von Dateien (Block 0, also ID3v2 und Cover).

### Versuchsideen für PiDrive (nicht verifiziert)

1. **Cover-Cache:** Mit `id3_cover` COVER1 spielen und warten, bis das Cover angezeigt wird. Dann `cover:2` ohne Benachrichtigung setzen, zum nächsten Track springen und zurück. Erscheint Cover 2, liest die HU den Kopf neu; bleibt Cover 1, cacht sie.
2. **Gleiche Änderung mit `unit_attention`:** Prüft, ob die HU nach 06/28 den Inhalt verwirft (FAT/ROOT neu lesen) oder die UNIT ATTENTION nur quittiert.
3. **Kontrolle:** `POST /api/media/notify` ohne Änderung. Die Reaktion zeigt, was 06/28 allein auslöst.
4. **Text gegen Cover:** Nur `title` ändern. Liest die HU für den Text dieselben Blöcke wie für das Cover (Block 0) neu?
5. **Laufender Track:** `seed` der gerade spielenden Datei mit `delay_s` während einer Offline-Messung ändern. Der Stempel in den später gelesenen Blöcken zeigt im Mitschnitt, welche Blöcke vor und welche nach der Änderung gelesen wurden.
6. **Vergleich der drei Wege:** dieselbe Änderung mit `unit_attention`, `remount` und `cycle`, jeweils als eigene Session. Ergebnis ist die Kosten-/Nutzen-Ordnung für PiDrive: Welcher Weg löst eine Neuanzeige mit den wenigsten Reads aus?

Keine dieser Reaktionen ist am Fahrzeug gemessen. Insbesondere ist offen, ob das NBT Evo UNIT ATTENTION überhaupt auswertet.

## Matrix (Kampagne ab 0.2.31)

Die Reihenfolge und die Abnahmekriterien stehen in der [Feldsequenz GATE-01 bis S6](feld/2026-10-19-feldsequenz-gate01-s6.md). Die automatisierten Abläufe (`SEQ-DUR`, `SEQ-SIZE`, `SEQ-CACHE`, `SEQ-STRUCT`) sind in `firmware/src/capture/ExperimentProfiles.h` definiert.

1. **Content-only:** `cache_a` → `cache_b` bei festgehaltener Serial (SEQ-CACHE Schritt 2/3).
2. **Replug same:** `cache_a` → Re-Enum → `cache_a` mit gleicher Serial.
3. **ID-only:** `cache_a` → `ident_b` (Serial, Label, Volume-ID gemeinsam). Einzelne Identitätsmerkmale lassen sich per eigener Spec trennen.
4. **Placement-only:** `cache_a` → `cache_place`.
5. **Dauer, Bitrate, Größe, Struktur:** je ein Preset pro Dimension.

Jede Zelle ist eine eigene Session. `session.json` enthält `media` (Variante, Hash, Geometrie) und `media_layout` (Extents pro Datei). Ausgewertet wird mit `tools/analyze_session.py`.

Erwartungen aus BMW NBT Evo werden nicht auf Mazda übertragen. Bis eine Feldmessung mit 0.2.31 vorliegt, ist keine dieser Varianten im Fahrzeug verifiziert.
