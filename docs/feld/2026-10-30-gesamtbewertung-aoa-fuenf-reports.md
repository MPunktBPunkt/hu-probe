# Gesamtbewertung des HU-Probe-Projekts

**Projekt:** hu-probe  
**Datum:** 2026-10-30  
**Repo-Basis:** `main` @ `849f30e`  
**Gegenstand:** Bewertung des Multi-Reviews und der fünf Research-Reports  
**Multi-Review:** [`2026-10-30-gesamtbericht-aoa-zubehoer-multireview.md`](2026-10-30-gesamtbericht-aoa-zubehoer-multireview.md)  
**Problemfelder:** [`2026-10-30-aoa-problemfelder.md`](2026-10-30-aoa-problemfelder.md)  
**Reports:** [1](2026-10-30-research-01-aoa-direct-accessory.md) · [2](2026-10-30-research-02-usb-sniffing-gadget-validation.md) · [3](2026-10-30-research-03-esp32-tinyusb-ep0-callbacks.md) · [4](2026-10-30-research-04-composite-vs-vendor-only.md) · [5](2026-10-30-research-05-bcl-etch-nbt.md)  
**Code-Analyse Connected:** [`2026-10-30-codeanalyse-connected-aoa.md`](2026-10-30-codeanalyse-connected-aoa.md)  
**Verbesserungsplan C1b-P0:** [`2026-10-30-verbesserungsplan-c1b-p0.md`](2026-10-30-verbesserungsplan-c1b-p0.md)  
**Epistemik:** [`../CONNECTED-APP.md`](../CONNECTED-APP.md)

Die Ziffer `[1]` im Text unten meint den Repo-Codekontext (`ConnectedAppProbe`, `AoaControlTap`, `AoaObserveGadget`, `AoaTransport`, `BclFramer`), nicht eine externe Fußnote.

> **Ablagehinweis zum Stand `849f30e`:** Der Text ab dem Kurzfazit ist die gelieferte Bewertung. Drei Formulierungen beschreiben diesen Commit nicht mehr wörtlich. Der Abgleich steht in Abschnitt 12.
>
> 1. `p0_b_` wird nicht bei jedem Control-Sample gesetzt. Seit `4c1d6d2` (FW 0.3.28) nur bei `bRequest` 51, 52, 53 oder 58.
> 2. TX im Observe-Pfad ist der Default (`p0_tx_disabled`). Der B2b-Knock hat `wire_tx_enabled_` ausdrücklich gesetzt; die HU hat trotzdem nicht geantwortet.
> 3. Ein Host-Trace existiert für Debian als AOA-Host gegen das Phone. Offen bleibt derselbe Trace mit der HU als Host gegen die Probe.

---

## Kurzfazit

Der aktuelle Stand ist technisch **nicht gescheitert**, aber die bisherige Evidenz wurde teilweise auf unterschiedlichen Ebenen vermischt.

Gesichert ist:

- Der ESP32-S3 wird von der Head Unit als USB-Zubehör beziehungsweise Accessory erkannt.
- Der direkte AOA-Bootpfad mit `18D1:2D00` ist grundsätzlich plausibel und mit dem aktuellen Gadget-Design vereinbar.
- Die BMW-/NBT-ähnlichen Strings beeinflussen die UI bis hin zu „verbunden“.
- Der aktuelle `AoaControlTap` und der P0-Observe-Pfad sind **kein vollständiger USB-/EP0-Sniffer**.
- Ein echter USB-Bulk-/BCL-Datenkanal ist bisher **nicht nachgewiesen**.
- Frühere MSC+Vendor-Ergebnisse sind hinsichtlich ihrer Kausalität nur eingeschränkt verwertbar.

Die zentrale Projektentscheidung sollte daher lauten:

> **AOA-Erkennung und UI-Reaktion sind bestätigt. Der BMW-Connected-Datenkanal ist weiterhin offen. Der nächste Schwerpunkt muss nicht weiteres Blind-Knocking sein, sondern eine beweiskräftige Transport- und Hostanalyse.**

Der gegenwärtige technische Status lässt sich so zusammenfassen:

| Ebene | Bewertung |
|---|---|
| USB-Enumeration | **bestätigt** |
| AOA-/Accessory-Identität | **stark bestätigt** |
| Direkter Accessory-Boot ohne sichtbare 51/52/53 | **konsistent** |
| Vollständige EP0-Beobachtung | **nicht gegeben** |
| Android-/Accessory-Consumer | **nicht nachgewiesen, eher fraglich** |
| Head-Unit-UI-Reaktion | **bestätigt** |
| USB-Bulk-Datenkanal | **nicht nachgewiesen** |
| BCL über USB | **nicht nachgewiesen** |
| MSC als Ursache früherer Reaktionen | **plausibel und methodisch relevant** |
| Bluetooth/SPP oder anderer Transport | **offene Alternative** |

---

# 1. Bewertung des Gesamtberichts und der fünf Reports

## 1.1 Was die Reports gemeinsam richtig erkannt haben

Die fünf Reports ergeben zusammen ein im Wesentlichen konsistentes Bild:

1. **AOA und Connected-Nutzdaten sind verschiedene Ebenen.**
2. **`18D1:2D00` ist stärker als ein fehlender Handshake-Trace**, wenn das Gerät direkt in diesem Zustand startet.
3. **Ein vendor-spezifischer Callback ist kein vollständiger EP0-Sniffer.**
4. **Hostseitige USB-Captures sind für Negativbefunde deutlich belastbarer.**
5. **MSC+Vendor kann frühere Automotive-Beobachtungen verfälscht haben.**
6. **BCL/Etch/`0x0FA4`/`0x1389` sind bislang Arbeitshypothesen, keine bestätigte NBT-Evo-Wire-Spezifikation.**

Diese Trennung passt auch zum vorhandenen Repository-Aufbau: P0 ist ausdrücklich als Observe-only-Phase angelegt, liest Raw-Bulk und führt den Datenverkehr nicht live durch `BclFramer`. TX ist im Observe-Pfad der Default und kann für einen gezielten Knock freigegeben werden [1]. Siehe Abschnitt 12.

Das ist methodisch richtig, weil dadurch bisher keine unbelegten BCL-Frames als Normalpfad in die Head Unit eingespeist wurden. Der eine Knock-Lauf (hex4) war eine explizite Ausnahme und lieferte `rx=0`.

---

## 1.2 Die wichtigste Schwäche der bisherigen Untersuchung

Die Reports zeigen, dass bisher teilweise drei verschiedene Aussagen gleichgesetzt wurden:

### Aussage A

> Die Head Unit hat keine Requests 51/52/53 gesendet.

### Aussage B

> Die aktuelle Device-Instrumentierung hat keine Requests 51/52/53 gesehen.

### Aussage C

> Es gibt keinen AOA-Übergang.

Diese Aussagen sind **nicht gleichwertig**.

Bei diesem Setup ist vor allem Aussage B belegt. Der `AoaControlTap` hängt an `USBVendor.onRequest` und ist kein generischer EP0-Monitor [1]. Daher kann aus `51/52/53 = 0` allein kein belastbarer Negativbeweis abgeleitet werden.

Zusätzlich ist das Gerät im P0-Bootmodus `aoa_observe` bereits als USBVendor-only-Accessory mit `VID 0x18D1` und `PID 0x2D00` konfiguriert. Ein klassischer Phone-to-Accessory-Umschaltvorgang ist in diesem Bootpfad daher nicht zwingend zu erwarten [1].

Die Reports 1 bis 3 bewerten diesen Punkt grundsätzlich richtig. Er muss künftig in allen Statusberichten berücksichtigt werden.

Im Lab gegen die Probe ist Aussage B sogar enger: Host-51 wurde gesendet und endete in STALL, der Tap zählte 0. Das ist Instrument-Blindheit, nicht Absenz. [`2026-10-30-lab-aoa-aaidrive-phone-host.md`](2026-10-30-lab-aoa-aaidrive-phone-host.md)

---

# 2. Report 1: AOA, Direct-Accessory und Requests 51/52/53/58

## Bewertung: technisch stark, aber teilweise noch hypothesenbehaftet

### Was belastbar ist

Die Einordnung von 51, 52 und 53 als klassischer AOA-Eintrittspfad ist fachlich plausibel:

- 51: Protocol-Abfrage
- 52: Übertragung der Accessory-Strings
- 53: Start des Accessory-Modus

Ebenso ist die Unterscheidung zwischen:

- **Transition Evidence:** Requests 51/52/53
- **State Evidence:** Enumeration als `18D1:2D00`

methodisch sehr hilfreich.

Für dieses Projekt ist besonders wichtig:

> Wenn die Firmware selbst bereits als `18D1:2D00` enumeriert, ist das Fehlen von 51/52/53 nicht automatisch ein Gegenbeweis gegen AOA.

Das passt direkt zu `AoaObserveGadget`, das als USBVendor-only-Gadget mit Google-VID und AOA-PID aufgebaut wird [1].

### Was vorsichtiger formuliert werden sollte

Die Aussage zu Request 58 ist im Report selbst korrekt als „sehr wahrscheinlich, live zu verifizieren“ gekennzeichnet. Sie sollte derzeit nicht als gesicherte Grundlage für die Projektarchitektur verwendet werden. Im Code ist 58 als AOAv2 `SET_AUDIO_MODE` kommentiert (Desk-Evidenz), nicht als NBT-Wire-FACT.

Ebenso sollte `18D1:2D00` nicht überinterpretiert werden. Es beweist:

- eine AOA-kompatible Geräteidentität beziehungsweise einen AOA-Accessory-Zustand,

aber nicht automatisch:

- dass die BMW-HU einen AOA-Nutzkanal öffnet,
- dass eine passende Android-App aktiv ist,
- dass Bulk-Daten fließen,
- dass BCL darüber transportiert wird.

### Bewertung

| Punkt | Bewertung |
|---|---|
| Bedeutung 51/52/53 | hoch belastbar |
| Interpretation `18D1:2D00` | hoch belastbar |
| Aussage „fehlende 51/52/53 widerlegen AOA nicht“ | hoch belastbar |
| Request 58 | plausibel, aber verifizieren |
| AOA gleich BMW-Connected-Kanal | nicht zulässig |

### Konsequenz

Der AOA-Teil sollte im Projektstatus als **PASS** geführt werden. Aber nur die Accessory-/Identitätsebene ist PASS, nicht die BMW-Datenkommunikation.

---

# 3. Report 2: USB-Sniffing und Validierungsstrategie

## Bewertung: der methodisch wichtigste Report

Dieser Report enthält die wichtigste Konsequenz für die nächsten Schritte:

> Für die Frage „Wurde Request X wirklich gesendet?“ ist ein Host-Trace deutlich belastbarer als ein Device-Callback.

Das ist für diese Situation zentral. Der aktuelle `AoaControlTap` ist kein vollständiger Busmonitor [1].

## 3.1 Was der Report besonders gut macht

Er trennt sauber zwischen:

- Host-Sicht,
- Device-Stack-Sicht,
- Hardware-Analyzer-Sicht.

Diese Rangfolge ist richtig:

| Fragestellung | Beste Evidenzquelle |
|---|---|
| Hat der Host den Request initiiert? | Host-Trace |
| Hat der Device-Callback den Request erhalten? | Device-Trace |
| Was passierte physikalisch auf dem USB-Bus? | Hardware-Analyzer |
| Gab es Bulk-URBs beziehungsweise Transfers? | Host-Trace, besser zusätzlich Hardware |
| Wurde ein bestimmter Callback ausgeführt? | Device-Trace |

Besonders wichtig ist die Unterscheidung:

> „Im Device-Log wurde kein Bulk gesehen“ ist nicht dasselbe wie „auf dem USB-Bus gab es keinen Bulk-Verkehr“.

Für Debian-als-Host gegen das Phone ist der Host-Negativbefund schon da: Bulk absent. Für die HU gegen die Probe fehlt er.

## 3.2 Bewertung der vorgeschlagenen Strategie

Die empfohlene Reihenfolge ist sinnvoll:

1. Host-Capture
2. verbesserte Device-Instrumentierung
3. Known-good-/Problemvergleich
4. bei verbleibender Unsicherheit Hardware-Analyzer

### Minimalstrategie

- Linux-Host verwenden, wenn verfügbar.
- `usbmon` und Wireshark/tshark einsetzen. Im Repo: `tools/lab_usbmon_capture.sh`.
- Vollständigen Trace ab physischem Attach aufzeichnen.
- Re-Enumeration separat markieren.
- Nach `bRequest = 51`, `52`, `53`, `58` suchen.
- Alle Bulk-Transfers nach Interface und Endpoint auswerten.

### Best-Practice

- Host-Capture plus Device-EP0-Logging.
- Einen bekannten funktionierenden AOA-Lauf mit echtem Android-Telefon als Referenz aufzeichnen. Den Lab-Lauf vom 2026-10-30 gibt es bereits.
- Einen Problem-Lauf mit der HU unter dokumentierter Gerätekonfiguration aufzeichnen. Der fehlt.
- Erst danach entscheiden, ob ein Hardware-Analyzer erforderlich ist.

Ein Hardware-Analyzer ist für die erste Iteration wahrscheinlich nicht zwingend notwendig. Ein sauberer Host-Trace mit der HU dürfte bereits klären, ob sie:

- überhaupt 51/52/53 sendet,
- nur Standard-Enumeration durchführt,
- Bulk-Transfers startet,
- MSC-Kommandos sendet,
- oder nach der Enumeration passiv bleibt.

Bei `aoa_observe` ist Absenz von 51/52/53 zusätzlich erwartbar (Report 1). Der Host-Trace bleibt trotzdem der Weg, das von Instrument-Blindheit zu trennen.

---

# 4. Report 3: ESP32-S3, TinyUSB und `USBVendor.onRequest()`

## Bewertung: sehr relevant, aber die exakte Stackversion muss noch geprüft werden

Der Report zieht die wahrscheinlich richtige Schlussfolgerung:

> `USBVendor.onRequest()` ist kein vollständiger EP0-Eingang.

Das entspricht der Codearchitektur. `AoaControlTap.h` sagt: jedes SETUP, das diesen Callback erreicht, kein vollständiger EP0-Sniffer [1].

## 4.1 Konsequenz für die aktuelle Messung

Der bisherige Tap kann positive Aussagen stützen, zum Beispiel:

> Ein Control-Request hat diesen Instrumentierungspunkt erreicht.

Er kann negative Aussagen nur eingeschränkt stützen:

> Ein bestimmter Request wurde vom Host nicht gesendet.

Der zweite Satz wäre erst belastbar, wenn nachgewiesen ist, dass der Hook:

- jedes EP0-SETUP-Paket sieht,
- unabhängig von Standard/Class/Vendor,
- unabhängig vom Recipient,
- vor interner TinyUSB-Verarbeitung,
- inklusive Re-Enumeration-Phasen.

Das ist derzeit nicht nachgewiesen. Zusätzlich filtert der Tap auf `stage == REQUEST_STAGE_SETUP`. Der MSC-Pfad vergleicht `CONTROL_STAGE_SETUP`. Der Lab-Befund hält einen Stage-Enum-Mismatch für hochwahrscheinlich.

## 4.2 Die beste Firmware-Verbesserung

Der Report empfiehlt zu Recht einen Loggingpunkt unterhalb des Arduino-Callbacks, idealerweise im TinyUSB-Control-Dispatch.

Minimal sollten erfasst werden:

```text
bmRequestType
bRequest
wValue
wIndex
wLength
```

Zusätzlich:

```text
request type: standard/class/vendor
recipient: device/interface/endpoint/other
direction: host-to-device/device-to-host
dispatch target
data phase length
status/completion result
stall/error
USB reset
set configuration
set interface
disconnect/reconnect
```

Wichtig ist außerdem, die Messgrößen semantisch zu trennen.

> **Ablage:** Die gelieferte Bewertung sagt, `p0_b_` werde bei jedem ControlTap-Sample gesetzt. Auf `849f30e` gilt das nicht. `drainControlTap()` setzt `p0_b_` nur bei 51/52/53/58. `setup_count_` zählt jedes SETUP, das den Callback erreicht. Ein Zähler für alle EP0-SETUPs vor dem Callback fehlt weiter.

Die vorgeschlagenen Namen sind nicht implementiert. Nah dran sind:

```text
p0_b_          # nur 51/52/53/58, schon so
setup_count_   # SETUP am Callback, nicht alle EP0
p0_e_          # erstes Bulk-RX, schon so
```

Offen bleiben `p0_b_ep0_setup_all` und ein Completion-/Stall-Zähler unterhalb von `USBVendor`.

Der aktuelle `AoaControlTap` sollte nicht ersetzt, sondern ergänzt werden:

- `AoaControlTap`: semantischer AOA-/Vendor-View
- `Ep0SetupTrace`: vollständigerer Low-Level-View

## 4.3 Bewertung der Reportaussagen

Die Architekturannahmen des Reports sind sehr plausibel. Die exakte Aussage darüber, welche Requests in `espressif32@6.4.0` an `USBVendor.onRequest()` gelangen, ist im Git-Baum nicht lesbar: `USBVendor.cpp` ist nicht eingecheckt.

> Die Messung ist bereits jetzt als unvollständig zu behandeln. Der Lab-STALL bei Host-51 mit `setup_count=0` ist die konkrete Blindstelle, die schon gemessen ist. Die Dispatch-Quelle im Arduino-Core ist noch nicht in diesem Repo nachgelesen.

---

# 5. Report 4: MSC, Composite und Vendor-only

## Bewertung: sehr wichtig für die historische Datenbereinigung

Dieser Report liefert einen der wichtigsten methodischen Warnhinweise:

> Frühere Ergebnisse aus einem MSC+Vendor-Composite-Setup dürfen nicht ohne Weiteres als Beweis für eine Vendor- oder AOA-Reaktion interpretiert werden.

## 5.1 Warum MSC die Interpretation verändert

Ein MSC-Interface eröffnet einen bekannten Standardpfad:

- Enumeration,
- Mass-Storage-Erkennung,
- `GET_MAX_LUN`,
- Mass-Storage-Reset,
- SCSI Inquiry,
- Test Unit Ready,
- Read Capacity,
- gegebenenfalls Dateisystem- und Medienlogik.

Eine Head Unit kann dadurch bereits eine USB-Quelle aktivieren, einen Medienstatus anzeigen, „verbunden“ signalisieren, eine Dateiansicht öffnen oder „keine Dateien“ beziehungsweise „nicht unterstützt“ anzeigen.

Diese Reaktionen sind nicht automatisch Belege für AOA, Vendor-Control, Connected-App-Kommunikation, BCL oder RHMI.

## 5.2 Keine zu starke MSC-Theorie

Der Report formuliert gleichzeitig korrekt, dass es keine allgemeine USB-Regel gibt, wonach MSC bei jedem Composite Device automatisch alle Vendor-Interfaces verdrängt.

Die richtige Aussage lautet nicht „MSC gewinnt immer“, sondern:

> MSC bietet dem Host einen bekannten und möglicherweise unabhängig aktivierbaren Standardpfad. Die Head Unit kann MSC nutzen, Vendor ignorieren, beide getrennt behandeln oder das Composite-Layout insgesamt nicht unterstützen.

## 5.3 Bedeutung für frühere Ergebnisse

### Stark abzuwerten, wenn sie sich auf das Composite-Feld stützen

- „Die Head Unit unterstützt beliebige Vendor-only-Geräte.“
- „Die UI beweist, dass das Vendor-Interface akzeptiert wurde.“, bezogen auf FW 0.3.23.
- „Connected wurde durch Vendor-Kommunikation ausgelöst“, obwohl parallel MSC vorhanden war.

### Weniger stark abzuwerten

- ein tatsächlich aufgezeichneter AOA-Control-Handshake (Lab-Host gegen Phone),
- eine reproduzierbare Reaktion im vendor-only-Setup (Abendfeld FW 0.3.32: Zubehör-UI, Bulk `rx=0`),
- eine Reaktion, die nach Entfernung von MSC bei derselben Boot-Identität bestehen bleibt. Die 0.3.32-UI ist genau das, aber ohne Host-Trace der HU.

## 5.4 Konsequenz

Die P0-Session FW **0.3.23** ist im Feldbericht bereits so geführt:

```text
composite-contaminated
causal attribution unresolved
not suitable as vendor-only proof
```

[`2026-10-29-c1b-p0-aoa-observe-feld.md`](2026-10-29-c1b-p0-aoa-observe-feld.md)

Der Leak ist seit FW 0.3.27 behoben. Die Daten bleiben Vergleichsfall, nicht Beweis für den BMW-Connected-Pfad.

---

# 6. Report 5: BMW Connected, BCL, Etch und NBT Evo

## Bewertung: wichtig für die Erwartungssteuerung, aber der schwächste Evidenzbereich

Der Report kommt zu einer nüchternen Bewertung:

> BCL über USB-Bulk ist eine plausible, aber unbestätigte Arbeitshypothese.

Diese Einschätzung passt zu [`../CONNECTED-APP.md`](../CONNECTED-APP.md): Header und Destinationen sind Desk-R0, die eigene F20 bleibt UNKNOWN.

## 6.1 Was gestützt wird

Plausibel beziehungsweise als Desk-R0 geführt:

- BMW-App-/Headunit-Integrationen,
- AAIdrive-nahe Projekte,
- UI-/Remote-HMI-artige Schichten,
- ConnectedDrive-Cloud getrennt vom lokalen Kanal,
- mehrere mögliche lokale Transporte, im Desk-Code vor allem BT RFCOMM.

Nicht als NBT-Wire belegt:

- dass BCL über USB-Bulk auf dieser NBT Evo läuft,
- dass das 8-Byte-Framing dem realen NBT-Wireformat entspricht,
- dass `0x0FA4` auf dem USB-Wire die Etch-Destination ist,
- dass `0x1389` auf dem USB-Wire der Watchdog ist.

Im Code sind die beiden Werte Desk-Konstanten (`kDestEtchDesk`, `kDestWatchdogDesk`), Layout vier `u16` big-endian.

## 6.2 Risiko einer Selbstbestätigung

Der `BclFramer` bleibt nützlich und birgt Confirmation-Bias, wenn Captures nur nach diesem Muster durchsucht werden. P0 füttert ihn nicht. Der Status im Baum ist bereits:

```text
BCL framing:
- source: desk/audit evidence (R0, BE)
- independent NBT Evo wire proof: absent
- NBT Evo USB validation: absent
- use: hypothesis decoder only
- live P0: framer not fed
```

## 6.3 Die richtige technische Suchrichtung

Nicht weiter primär nach dem String „BCL“ suchen, sondern die Transportkette ausgehend von einer realen Aktion verfolgen:

```text
BMW-App-Aktion
  → Android-Service oder Broadcast
  → Session-/Capability-Aufbau
  → Transportadapter
  → konkretes Gerät, Socket oder Accessory
  → Write/Transfer
  → HU-Reaktion
```

Parallel: USB, Bluetooth/RFCOMM, lokale IP-/Socket-Aktivität, Android-Logcat, App-Systemaufrufe, USB-Accessory-Handles.

Der BT-Pfad ist im Desk-Code genannt und in der Firmware nur `SppTransport` mit `not_implemented`.

---

# 7. Konsolidierte Bewertung der fünf Themen

| Thema | Ergebnis | Vertrauensniveau | Konsequenz |
|---|---|---:|---|
| AOA / Direct Accessory | AOA-Zustand und Identity plausibel bestätigt | hoch | Nicht weiter grundsätzlich beweisen; Datenpfad untersuchen |
| USB-Sniffing | Device-Logs reichen für Negativbefunde nicht | sehr hoch | Host-Capture der HU priorisieren; Phone-Seite ist im Lab schon da |
| TinyUSB-Callbacks | aktueller Tap ist nicht vollständig | hoch | EP0-Logging unterhalb `USBVendor`; Stage-Enum zuerst prüfen |
| MSC/Composite | alte Reaktionen können kontaminiert sein | hoch | 0.3.23 caveated; 0.3.32 vendor-only nicht darunter subsumieren |
| BMW/BCL/NBT | Desk-R0 stark, BCL-on-USB offen | niedrig bis mittel für USB-Wire | BCL nur als Hypothese weiterführen |

---

# 8. Bewertung des aktuellen Projektstatus

## 8.1 Was erreicht ist

Die **Identitäts- und Umschaltebene** ist erreicht:

- Die Head Unit erkennt das Gerät als USB-Zubehör.
- Das Gerät präsentiert eine AOA-kompatible Identität.
- Strings beziehungsweise Accessory-Metadaten wirken bis in die UI.
- Ein „verbunden“-Zustand kann im Lab gegen das Phone reproduziert werden.
- Dieselbe Zubehör-UI gibt es vendor-only an der HU (FW 0.3.32).

Das ist ein realer technischer Erfolg.

## 8.2 Was nicht erreicht ist

Nicht bewiesen ist:

- dass die Head Unit einen passenden BMW-Accessory-Consumer öffnet,
- dass sie einen relevanten USB-Bulk-Endpoint nutzt,
- dass sie BCL-Frames sendet oder empfängt,
- dass `BclFramer` das echte NBT-Evo-Wireformat beschreibt,
- dass die UI-Reaktion aus einem Datenkanal und nicht nur aus Identity-/State-Erkennung entsteht.

Drei offene Erklärungen:

### Hypothese A: Kein passender Consumer

Die HU beziehungsweise das Android-System erkennt das Accessory, aber kein passender Handler öffnet den Datenkanal. Gestützt durch den Android-Dialog „Keine installierten Apps für dieses USB-Zubehör“ und durch Bulk-OUT-Timeout im Lab.

### Hypothese B: Falscher Transport

AOA-/USB-Erkennung funktioniert, der BMW-Nutztransport läuft nicht über diesen USB-Bulk-Weg. Der Desk-Code nennt BT RFCOMM. Die Firmware misst den Pfad noch nicht.

### Hypothese C: USB-Bulk wird erst nach einer weiteren Bedingung aktiviert

Möglich bleiben exakte Descriptoren, andere Strings, anderes Interface-Layout, ein Accessory-Filter, weitere Control Requests, oder echtes NBT-Verhalten statt Lab-Host.

Keine der drei ist bewiesen.

---

# 9. Empfohlener Lösungsplan

## Priorität 1: Vollständigen Host-Trace der HU erstellen

Der Phone-Lauf ist erledigt. Offen ist die HU als Host.

### Ziel

Klären, ob die HU tatsächlich 51/52/53 sendet, nur normal enumeriert, MSC initialisiert, Bulk-Transfers startet, oder nach der Enumeration keine Nutzdaten öffnet.

### Vorgehen

1. Capture vor dem physischen Attach starten.
2. Gesamten Lauf bis mindestens 10–30 Sekunden nach Enumeration aufzeichnen.
3. USB-Reset, Disconnect und Re-Enumeration markieren.
4. Control Requests nach `bRequest` und `bmRequestType` auswerten.
5. Alle Interfaces und Endpoints getrennt betrachten.
6. Bulk-IN und Bulk-OUT nach Endpoint auswerten.
7. MSC-Kommandos von Vendor-/AOA-Verkehr trennen.

Bei `aoa_observe` 51/52/53-Absenz nicht als AOA-FAIL werten. Der Trace zeigt trotzdem, ob die HU etwas anderes auf EP0 oder Bulk tut, das der Tap nicht sieht.

## Priorität 2: EP0-Instrumentierung im TinyUSB-Core

Den `AoaControlTap` ergänzen, nicht ersetzen. Zuerst den Stage-Vergleich gegen `CONTROL_STAGE_SETUP` prüfen, analog zu `MscBotTap`. Danach ein Logging vor der funktionsspezifischen Dispatch-Logik.

`p0_b_` nicht wieder auf „jedes Sample“ aufweiten. Den fehlenden Zähler als eigenen Namen führen (`ep0_setup_all`), nicht als `P0_B`.

## Priorität 3: Saubere A/B-Testmatrix

1. vendor-only, neutrale Identität — nach 0.3.27 teilweise gelaufen (`aoa_observe`)
2. vendor-only, phone-like Identity — Boot existiert, HU hat den Fingerprint abgelehnt
3. MSC-only
4. MSC+Vendor — der alte Leak, nicht als sauberer A/B-Lauf
5. AOA-Accessory ohne MSC — Abend 0.3.32
6. AOA mit dokumentierter Stringkombination — Lab-Host gegen Phone, nicht HU gegen Probe

Für jeden neuen Lauf: VID/PID, Device-Class, Configuration, Interface-Reihenfolge, IADs, Strings, Endpoint-Adressen, USB-Speed, Kabel.

## Priorität 4: Android-/Consumer-Seite prüfen

- `UsbAccessory`-Intent,
- Accessory-Filter-XML,
- Manufacturer, Model, Description, Version, URI, Serial,
- `dumpsys usb`,
- `logcat`,
- ob eine App den Accessory-File-Descriptor öffnet.

> Wird der Accessory-FD von irgendeinem Consumer geöffnet?

Wenn nein, ist ein leerer Bulk-Kanal nicht überraschend. Das Lab deutet schon darauf.

## Priorität 5: BCL nicht als Primärstrategie

Nicht empfehlenswert:

- weitere Blind-Knock-Pattern zu raten,
- BCL-Frames ohne Host-/Wire-Evidenz zu senden,
- aus dem UI-Status auf BCL zu schließen,
- die Destinationswerte als NBT-Wire zu behandeln.

Der hex4-Knock ist gelaufen und negativ. Eine weitere Muster-Suite ohne neuen Host-Trace wiederholt das.

## Priorität 6: Bluetooth/SPP parallel untersuchen

Nicht als bewiesen annehmen. Prüfen: Profile, SDP, RFCOMM, SPP-UUIDs, Android-Bluetooth-Sockets, Keepalive, Korrelation mit der HU-UI. Im Baum ist dafür noch kein Transport armiert.

---

# 10. Formale Neubewertung der bisherigen Claims

## Weiterhin zulässige Aussagen

- „Die HU erkennt den ESP32-S3 als USB-Zubehör.“
- „Der ESP32-S3 kann direkt als AOA-ähnliches Accessory mit `18D1:2D00` auftreten.“
- „Die Accessory-Strings beeinflussen die HU-/Android-UI.“
- „Die aktuelle P0-Instrumentierung beobachtet den Callback `USBVendor.onRequest`, nicht den ganzen EP0.“
- „Ein USB-Bulk-/BCL-Kanal ist bisher nicht nachgewiesen.“
- „Die FW-0.3.23-Session ist kausal nicht sauber isoliert.“
- „Die FW-0.3.32-Session ist vendor-only und zeigt die Zubehör-UI ohne Bulk-RX.“

## Derzeit nicht zulässige Aussagen

- „Die HU sendet definitiv keine 51/52/53.“
- „AOA ist widerlegt, weil 51/52/53 fehlen.“
- „Die UI-Anzeige beweist einen offenen Connected-Datenkanal.“
- „Die HU spricht BCL über USB.“
- „`0x0FA4` ist die bestätigte Etch-Destination auf dem NBT-Wire.“ Sie ist die Desk-Konstante.
- „`0x1389` ist der bestätigte NBT-Watchdog auf dem Wire.“
- „Die 0.3.23-Reaktion wurde durch das Vendor-Interface verursacht.“
- „BMW Connected nutzt auf dieser NBT Evo generell USB-Bulk.“
- „`p0_b_` springt bei jedem Control-Sample.“ Das ist seit FW 0.3.28 falsch.

---

# 11. Endbewertung

## Technische Bewertung

Das Projekt hat einen Teilpfad demonstriert:

> ESP32-S3 → USB-Enumeration → AOA-/Accessory-Identität → sichtbare HU-/Android-UI-Reaktion

Offen bleibt:

> Accessory → BMW-App-Consumer → Nutztransport → BCL/RHMI/Etch

Wahrscheinlicher als ein Fehler in der AOA-Identität ist derzeit eine **Beobachtungs- und Zuordnungsunsicherheit**:

- der Control-Tap ist nicht vollständig, und der Lab-STALL zeigt das schon an Vendor-51,
- der Consumer ist auf dem Phone möglicherweise nicht aktiv,
- die 0.3.23-Ergebnisse sind composite-kontaminiert, die 0.3.32-UI nicht,
- der erwartete BMW-Transport ist als USB-Bulk nicht belegt,
- der BCL-Framer ist Desk-R0, nicht ein eigener Wire-Capture.

## Gesamturteil

| Kategorie | Urteil |
|---|---|
| Technische Qualität der Reports | gut bis sehr gut |
| Qualität der AOA-Analyse | gut |
| Qualität der Sniffing-/Evidenzanalyse | sehr gut |
| Qualität der TinyUSB-Schlussfolgerung | gut; Stage-Enum ist der gemessene nächste Fix, `USBVendor.cpp` noch nicht im Baum gelesen |
| Qualität der MSC-Warnung | sehr wichtig; gilt für 0.3.23, nicht pauschal für 0.3.32 |
| Qualität der BCL-Recherche | ehrlich; Deckungsleich mit dem R0-Blatt |
| Aktuelle Projektbeweislage | AOA stark, Datenkanal schwach |
| Wahrscheinlichkeit eines Instrumentierungsproblems | hoch |
| Wahrscheinlichkeit eines fehlenden Consumers | mittel bis hoch |
| Wahrscheinlichkeit eines falschen USB-Transportmodells | mittel |
| Sinn weiterer Blind-Knock-Versuche | niedrig |

## Abschließende Bewertung

> **Die Head Unit akzeptiert den ESP32-S3 als USB-/AOA-Accessory, und die Accessory-Identität löst UI-Reaktionen aus. Ein BMW-Connected-Datenkanal über USB ist nicht gezeigt. Die Bulk- und 51/52/53-Negativbefunde der Probe sind wegen der Device-Instrumentierung nicht endgültig. Die MSC+Vendor-Session FW 0.3.23 bleibt kausal offen. Die vendor-only-Session FW 0.3.32 zeigt dieselbe Zubehör-UI ohne Bulk-RX.**

Die nächste Maßnahme:

> **Host-USB-Trace mit der HU als Host, EP0-Logging unterhalb von `USBVendor.onRequest()` inklusive Stage-Enum, und ein kontrollierter Vergleich von vendor-only, MSC-only, MSC+Vendor und AOA-only.**

Erst danach entscheiden, ob der USB-Bulk-Weg weitergeht, die Android-Consumer-Seite untersucht wird, das Identity-Layout angepasst wird, oder Bluetooth/SPP vorgezogen wird.

---

# 12. Repo-Abgleich (nicht Teil der gelieferten Bewertung)

| Stelle in der Bewertung | Stand `849f30e` |
|---|---|
| `p0_b_` bei jedem Sample | Behoben in `4c1d6d2`, FW 0.3.28. Nur 51/52/53/58. |
| TX im Observe-Pfad blockiert | Default `p0_tx_disabled` in `AoaTransport::write`. B2b setzt `wire_tx_enabled_` für hex4. TX ging raus, `rx=0`. [`2026-10-29-c1b-b2b-knock-feld.md`](2026-10-29-c1b-b2b-knock-feld.md) |
| Host-Trace fehlt | Phone-Seite: `usbmon2` im Lab 2026-10-30, Handshake PASS, Bulk absent. HU-Seite: fehlt. |
| Composite-Session | FW 0.3.23 caveated. Fix `105c791` / FW 0.3.27: ein Interface `class=ff`. FW 0.3.32 vendor-only, Zubehör-UI bleibt. |
| Request 58 | Im Tap als AOAv2 `SET_AUDIO_MODE` akzeptiert und geloggt. Nicht als NBT-FACT führen. |
| Stack prüfen | `firmware/platformio.ini`: `espressif32@6.4.0`. `USBVendor.cpp` nicht im Git. |
| Stage-Blindstelle | Lab: Host-51 → STALL, `setup_count=0`. Hochwahrscheinlich TinyUSB `CONTROL_STAGE_SETUP=1` gegen Arduino `REQUEST_STAGE_SETUP=0`. MSC-Tap nutzt bereits die TinyUSB-Konstante. |
| BCL | `BclTypes.h`: Desk-R0, nicht eigener Wire-Capture. P0: `drainTransportRaw()`. `SppTransport`: Stub. |
