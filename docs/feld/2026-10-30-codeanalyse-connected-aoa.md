# Code-Analyse des Connected-Anteils

**Projekt:** hu-probe  
**Datum:** 2026-10-30  
**Repo-Basis:** `main` @ `849f30e`  
**Gegenstand:** Firmware-Analyse des Connected-/AOA-Observe-Pfads (Beobachtungsgrenze, nicht ein einzelner C++-Crash)  
**Multi-Review:** [`2026-10-30-gesamtbericht-aoa-zubehoer-multireview.md`](2026-10-30-gesamtbericht-aoa-zubehoer-multireview.md)  
**Problemfelder:** [`2026-10-30-aoa-problemfelder.md`](2026-10-30-aoa-problemfelder.md)  
**Gesamtbewertung:** [`2026-10-30-gesamtbewertung-aoa-fuenf-reports.md`](2026-10-30-gesamtbewertung-aoa-fuenf-reports.md)  
**Reports:** [1](2026-10-30-research-01-aoa-direct-accessory.md) · [2](2026-10-30-research-02-usb-sniffing-gadget-validation.md) · [3](2026-10-30-research-03-esp32-tinyusb-ep0-callbacks.md) · [4](2026-10-30-research-04-composite-vs-vendor-only.md) · [5](2026-10-30-research-05-bcl-etch-nbt.md)  
**Lab:** [`2026-10-30-lab-aoa-aaidrive-phone-host.md`](2026-10-30-lab-aoa-aaidrive-phone-host.md)  
**Epistemik:** [`../CONNECTED-APP.md`](../CONNECTED-APP.md) · [`../GATES-Connected.md`](../GATES-Connected.md)

Die eckigen Ziffern im Text sind die mitgelieferten Verweise. Zuordnung zu Dateien in diesem Repo, keine externe Bibliographie:

| Ziffer | Datei |
|---|---|
| [2] | `firmware/src/connected/ConnectedAppProbe.h` |
| [3] | `firmware/src/connected/ConnectedAppProbe.cpp` |
| [4] | `firmware/src/connected/bcl/CaC1LabBclStream.h` |
| [7] | `firmware/src/connected/bcl/BclTypes.h` |
| [8] | `firmware/src/connected/transport/SppTransport.h` |
| [9] | `firmware/src/connected/transport/AoaTransport.cpp` |
| [12] | `firmware/src/connected/aoa/AoaObserveGadget.h` |
| [13] | `firmware/src/connected/aoa/AoaControlTap.cpp` |
| [14] | `firmware/src/connected/aoa/AoaControlTap.h` |
| [15] | `firmware/src/connected/aoa/AoaObserveGadget.cpp` |
| [18][19] | `firmware/src/net/EventBus.cpp`, `EventBus.h` |

> **Ablagehinweis zum Stand `849f30e`:** Der Text ab der Kurzantwort ist die gelieferte Analyse. Die Architekturthese steht im Code. Drei Präzisierungen, die der Text nicht ausformuliert, stehen in Abschnitt 17: die gemessene Stage-Enum-Blindstelle, die Reihenfolge Zähler vor Queue-Drop, und dass `hypothesis_respond_` im Observe-Boot an ist. Diese Analyse wiederholt nicht die überholte Aussage, `p0_b_` werde bei jedem Sample gesetzt.

---

# Kurzantwort

## 1. Haben wir einen entscheidenden Fehler?

**Ja – aber eher einen Architektur-/Evidenzfehler als einen einzelnen offensichtlichen C++-Bug.**

Der entscheidende Punkt ist:

> Die Firmware behandelt einen `USBVendor.onRequest()`-Callback als ausreichend aussagekräftige Sicht auf AOA-/EP0-Verkehr. Der eigene Code dokumentiert jedoch ausdrücklich, dass `AoaControlTap` **kein vollständiger EP0-Sniffer** ist. [13][14]

Damit sind Aussagen wie:

- „Die HU hat keine Requests 51/52/53 gesendet“
- „Die HU öffnet keinen Control-Kanal“
- „Die HU hat keinen AOA-Handshake versucht“

mit dieser Firmware allein **nicht sicher beweisbar**.

## 2. Kann die Firmware die HU richtig analysieren?

**Teilweise.**

Die Firmware kann derzeit zuverlässig:

- die eigene USB-Identität präsentieren,
- feststellen, dass das eigene USB-Gadget gestartet beziehungsweise gemountet ist,
- die Control-Requests beobachten, die bis zum `USBVendor`-Callback gelangen,
- Bulk-OUT-Daten lesen,
- eigene Bulk-TX-Versuche protokollieren,
- Events an einen Collector senden.

Sie kann derzeit nicht zuverlässig:

- den gesamten USB-Control-Verkehr auf EP0 beobachten,
- Standard- und Class-Requests vollständig erfassen,
- beweisen, dass ein nicht beobachteter Request nie gesendet wurde,
- physikalischen USB-Busverkehr messen,
- sicher feststellen, dass ein `write()` tatsächlich von der HU empfangen wurde,
- einen klassischen AOA-Moduswechsel vollständig implementieren,
- BCL-on-wire nachweisen.

Die Firmware ist damit aktuell ein **USB-Gadget mit begrenzter Beobachtungsfunktion**, aber kein vollständiger USB-Analyzer.

---

# 1. Wichtigster Befund: Die eigene Dokumentation bestätigt die Einschränkung

In `AoaControlTap.h` steht ausdrücklich:

> „Queue USB control SETUPs that reach USBVendor.onRequest“  
> „not a full EP0 sniffer“ [14]

Auch `AoaObserveGadget.cpp` routet die Control-Requests ausschließlich über:

```cpp
g_vendor.onRequest(onVendorControl);
```

und anschließend über:

```cpp
AoaControlTap::instance().onControlRequest(...)
```

[15]

Das bedeutet:

```text
USB-Controller
    ↓
TinyUSB / Arduino USB-Stack
    ↓
USBVendor-Funktionspfad
    ↓
AoaControlTap
```

Der Tap sitzt also **nicht am Roh-Eingang aller EP0-SETUP-Pakete**.

Das ist kein versteckter Fehler, sondern im Code sogar korrekt dokumentiert. Problematisch ist nur, wenn die daraus erzeugten Messwerte später wie ein vollständiger USB-Trace interpretiert werden.

---

# 2. Was der aktuelle Control-Tap tatsächlich misst

## 2.1 Gezählt werden nur Requests, die den Callback erreichen

In `AoaControlTap.cpp` wird bei einem SETUP zunächst:

```cpp
setup_count_++;
```

erhöht.

Wenn der Request als Vendor klassifiziert wird:

```cpp
const bool vendor = ((bmRequestType >> 5) & 0x3) == 2;
```

dann wird zusätzlich:

```cpp
vendor_setup_count_++;
```

gezählt. [13]

Die entscheidende Einschränkung:

> `setup_count_` bedeutet nicht „alle USB-SETUP-Pakete“, sondern nur „alle SETUP-Pakete, die diesen Callback erreicht haben“.

Das ist im Header ebenfalls korrekt beschrieben:

```cpp
/** @brief All REQUEST_STAGE_SETUP samples that reached this callback. */
uint32_t setupCount() const;
```

[14]

## 2.2 Nicht sichtbare Request-Kategorien

Mit der aktuellen Position können mindestens folgende Pakete unsichtbar bleiben:

- Standard Requests wie `GET_DESCRIPTOR`, `SET_ADDRESS`, `SET_CONFIGURATION`
- Class Requests anderer Interfaces
- Vendor Requests an andere Recipients
- Requests, die im TinyUSB-Core beantwortet werden
- Requests, die nicht an diese `USBVendor`-Funktion geroutet werden
- Requests aus einer anderen Enumeration oder Re-Enumeration, die nicht in der erwarteten Phase erfasst werden

Daher ist ein Wert:

```text
aoa51 = 0
aoa52 = 0
aoa53 = 0
```

nur der Beweis für:

> In diesem konkreten Callback wurden diese Requests nicht beobachtet.

Er ist nicht der Beweis für:

> Die HU hat diese Requests nie gesendet.

---

# 3. Der größte funktionale Mangel: Der klassische AOA-Wechsel ist nicht implementiert

Der `PhoneLike`-Pfad soll offenbar dazu dienen, den klassischen Ablauf:

```text
51 → 52 → 53 → Re-Enumeration als Accessory
```

zu untersuchen.

Der Code beantwortet zwar die Requests 51, 52, 53 und 58 hypothetisch. Bei Request 53 steht jedoch ausdrücklich:

```cpp
// START accepted; post-53 VID re-enum NOT IMPLEMENTED (Arduino lock).
```

[13]

Das ist entscheidend.

## Aktueller Zustand

Die Firmware kann:

1. sich als nicht-Accessory-Gerät präsentieren,
2. Requests 51/52/53 erkennen, falls sie im Callback ankommen,
3. hypothetische Antworten liefern,
4. Request 53 akzeptieren.

Sie kann aber nicht:

5. die USB-Deskriptoren beziehungsweise VID/PID auf Accessory umstellen,
6. eine korrekte USB-Disconnect-/Reset-/Re-Enumeration-Sequenz auslösen,
7. anschließend als `18D1:2D00` neu erscheinen.

Damit ist der `PhoneLike`-Pfad **kein vollständiger AOA-Implementierungstest**, sondern nur ein begrenzter Test der Control-Request-Erreichbarkeit.

Die Firmware selbst weist darauf hin, dass der Post-53-Wechsel nicht implementiert ist [13].

## Konsequenz

Ein fehlender Re-Enumeration-Schritt kann nicht ohne Weiteres der HU angelastet werden. Wenn die Firmware nach Request 53 nicht wirklich in einen neuen USB-Zustand wechselt, ist ein vollständiger AOA-Test vorzeitig beendet.

---

# 4. Der Direct-Accessory-Pfad ist kein klassischer AOA-Handshake-Test

Im Accessory-Profil wird bereits beim Booten gesetzt:

```cpp
vid_ = 0x18D1;
pid_ = 0x2D00;
```

und anschließend:

```cpp
USB.VID(vid_);
USB.PID(pid_);
```

[15]

Der Code beschreibt dieses Profil selbst als:

```cpp
hypothesis_google_accessory_0x18D1_0x2D00
```

[15]

Das ist ein sinnvoller Testpfad, aber er beantwortet eine andere Frage:

> Wie reagiert die HU auf ein Gerät, das bereits mit einer AOA-ähnlichen Accessory-Identität erscheint?

Er beantwortet nicht vollständig:

> Führt die HU mit diesem Gerät den klassischen AOA-Handshake 51/52/53 durch?

Denn der klassische Handshake ist ein **Übergangspfad**. Der Direct-Accessory-Modus ist ein **bereits präsentierter Zustand**.

Das Fehlen von 51/52/53 im Direct-Accessory-Test ist deshalb nicht überraschend. Es bedeutet zunächst nur:

- Die HU hat die Requests nicht über diesen Callback geschickt,
- oder sie akzeptiert den bereits präsentierten Zustand,
- oder sie behandelt das Gerät nicht als klassischen AOA-Partner,
- oder die Requests waren außerhalb der aktuellen Observability.

---

# 5. Problem bei der Interpretation von „connected“

Ein weiterer wichtiger Punkt ist die Bedeutung von `connected_`.

In `AoaTransport::poll()` wird:

```cpp
const bool mounted = g.vendorMounted();
```

ermittelt.

Wenn `vendorMounted()` wahr ist, wird:

```cpp
connected_ = true;
last_event_ = TransportEvent::Connected;
```

gesetzt. [9]

`vendorMounted()` bedeutet aber im Wesentlichen, dass die `USBVendor`-Funktion gemountet beziehungsweise konfiguriert ist:

```cpp
return ready_ && g_vendor.mounted();
```

[15]

Das beweist nicht, dass:

- eine BMW-App aktiv ist,
- ein Accessory-Consumer den Kanal geöffnet hat,
- die HU Nutzdaten austauscht,
- BCL läuft,
- die HU die Bulk-Endpoints tatsächlich verwendet.

Es bedeutet nur ungefähr:

> Der lokale USB-Vendor-Endpunkt ist im Device-Stack verfügbar.

Die Statusmeldung:

```text
CONNECTED
```

ist daher aktuell zu stark benannt. Präziser wären beispielsweise:

```text
USB_FUNCTION_MOUNTED
USB_CONFIGURED
VENDOR_INTERFACE_READY
```

und separat:

```text
PEER_BULK_RX_SEEN
PEER_BULK_TX_CONFIRMED
APPLICATION_CONSUMER_PROVEN
```

## Empfehlung

`connected_` sollte nicht mehr als Nachweis eines Connected-App-Links verwendet werden.

Besser wäre eine Trennung:

```cpp
bool usb_configured_;
bool vendor_interface_mounted_;
bool bulk_rx_seen_;
bool bulk_tx_queued_;
bool application_peer_proven_;
```

---

# 6. Bulk-Empfang ist brauchbar – aber nur für eine Richtung

Der Bulk-Pfad ist grundsätzlich sinnvoll implementiert:

```cpp
g.drainVendorRx(tmp, sizeof(tmp))
```

Danach werden die Daten in einen Ringpuffer gelegt und später über `read()` verarbeitet [9].

Wenn tatsächlich Daten aus der HU eintreffen, kann die Firmware diese grundsätzlich:

- erkennen,
- zählen,
- hexadezimal protokollieren,
- als `bulk_rx` publizieren.

Das ist ein echter positiver Befund.

## Aber: kein Bulk ist kein sicherer Negativbefund

Wenn `drainVendorRx()` nichts liefert, kann das bedeuten:

1. Die HU hat keinen Bulk gesendet.
2. Die HU hat den falschen Endpoint verwendet.
3. Das Interface wurde nicht geöffnet.
4. Der USB-Stack hat die Daten nicht bis `USBVendor` weitergegeben.
5. Der Host hat nur Control-Verkehr betrieben.
6. Die Funktion wurde zwar konfiguriert, aber nicht als Anwendungskanal verwendet.
7. Die Daten gingen über einen anderen Transport.

Die Firmware kann deshalb zuverlässig sagen:

> „Ich habe an diesem Bulk-RX-Pfad keine Daten erhalten.“

Sie kann nicht allein sagen:

> „Auf dem USB-Bus gab es definitiv keinen Bulk-Verkehr.“

Dafür ist ein Host-Capture oder Hardware-Analyzer nötig.

---

# 7. Bulk-TX bestätigt nicht, dass die HU empfangen hat

`sendKnock()` führt schließlich aus:

```cpp
const size_t n = aoa_.write(buf, len);
```

und meldet bei `n > 0`:

```cpp
tx_ok
```

[3]

Auf der USB-Seite bedeutet das jedoch zunächst nur:

> Die lokale `USBVendor`-/TinyUSB-Schicht hat die Bytes akzeptiert beziehungsweise zur Übertragung angenommen.

Es bedeutet nicht sicher:

- dass die HU die Bytes auf dem Bus gesehen hat,
- dass die HU sie gelesen hat,
- dass ein HU-Application-Handler sie verarbeitet hat,
- dass eine Antwort hätte kommen müssen.

Der Eventname `tx_ok` sollte deshalb präziser sein:

```text
tx_accepted_by_device_stack
```

oder:

```text
tx_queued_to_usb_stack
```

Zusätzliche Zustände wären sinnvoll:

```text
tx_queued
tx_bus_completed
tx_host_acknowledged
tx_application_response_seen
```

Der aktuelle Code unterscheidet diese Ebenen nicht.

---

# 8. Die BCL-Auswertung ist korrekt isoliert, aber keine Live-Protokollanalyse

Die Trennung des BCL-Framers vom Live-P0-Pfad ist eine Stärke.

In `ConnectedAppProbe.h` steht:

```cpp
No SAS/Etch/RHMI. BCL-on-wire is C1b-P1 (not auto-fed in P0).
```

[2]

Und im Live-P0-Pfad wird ausdrücklich verwendet:

```cpp
drainTransportRaw();
```

nicht:

```cpp
drainTransportToFramer();
```

[3]

Das ist methodisch richtig. Die Firmware behauptet dadurch nicht fälschlich, dass Rohdaten bereits BCL-Frames seien.

Auch die BCL-Typen sind korrekt als Desk-/Audit-Hypothese gekennzeichnet:

```cpp
Header layout from BimmerGestalt desk evidence (R0), not own wire captures
```

[7]

und:

```cpp
synthetic host tests prove parser mechanics only — not NBT Evo wire FACT.
```

[7]

Das ist sehr gut.

## Aber

Der Parser beweist ausschließlich:

- dass ein 8-Byte-Header nach dieser angenommenen Struktur geparst werden kann,
- dass Fragmentierung funktioniert,
- dass synthetische Testdaten korrekt verarbeitet werden.

Der Lab-Stream ist ausdrücklich synthetisch:

```cpp
source=synthetic
evidence_level=TEST
direct_observation=false
```

[4]

Das ist kein Fehler. Es muss nur verhindert werden, dass ein erfolgreicher Lab-Test später als Beweis für das NBT-Evo-Wireformat interpretiert wird.

---

# 9. Konkreter Bug beziehungsweise Messfehler bei `req52`

Bei Request 52 soll der String aus der DATA-Phase aufgezeichnet werden.

Der Ablauf ist:

1. Bei SETUP von Request 52 wird `string_scratch_len_` gesetzt.
2. Der Puffer wird an `USBVendor::sendResponse()` übergeben.
3. Bei DATA wird der Inhalt aus dem Scratch-Puffer in den Sample kopiert.

[13]

Das funktioniert nur, wenn:

- der Request den Callback erreicht,
- `hypothesis_respond_` aktiviert ist,
- der Request als Vendor klassifiziert wird,
- der Callback auch die DATA-Phase erhält,
- `sendResponse()` tatsächlich den vorgesehenen Puffer verwendet.

Wenn die Firmware nur beobachten, aber nicht antworten soll, ist die String-Erfassung nicht unabhängig vom Hypothesen-Response-Modus. Der Code schreibt den Response-Puffer nur in diesem Block:

```cpp
if (hypothesis_respond_ && v && vendor && stage == REQUEST_STAGE_SETUP)
```

[13]

Das bedeutet:

> Der „Observe“-Pfad ist bei req52 teilweise mit aktivem Protokollverhalten gekoppelt.

Das ist methodisch problematisch. Eine reine Beobachtungsfirmware sollte nicht davon abhängen, dass sie selbst eine hypothetische AOA-Antwort aktiviert.

## Empfehlung

String-Logging und Response-Handling trennen:

```cpp
capture_request_metadata();
capture_data_phase_if_available();

if (hypothesis_respond_) {
    send_hypothesis_response();
}
```

Außerdem sollte der Response-Puffer nicht als verlässlicher Rohdatenmitschnitt betrachtet werden. Für eine echte DATA-Phasenbeobachtung braucht ihr einen Hook, der die empfangenen Bytes unabhängig von der Antwortlogik erfasst.

---

# 10. Die Ringpuffer können Negativbefunde verfälschen

`AoaControlTap` nutzt eine Queue mit:

```cpp
static constexpr size_t kQueueDepth = 32;
```

Wenn sie voll ist:

```cpp
drops_++;
return false;
```

[13][14]

Das ist gut, weil Drops sichtbar gemacht werden. Aber für die Interpretation gilt:

> Sobald `drops_ > 0` ist, sind negative Aussagen über fehlende Requests nicht mehr belastbar.

Ein Request könnte:

- tatsächlich empfangen,
- in die Queue geschrieben,
- aber wegen Queue-Überlauf verworfen worden sein.

Deshalb sollte jeder Testlauf folgende harte Gültigkeitsregel haben:

```text
control_trace_valid =
    queue_drops == 0
    && eventbus_drops == 0
    && session_capture_complete
```

Auch der EventBus kann bei Überlauf alte Events verwerfen:

```cpp
if (count_ >= ringSize_) {
    ...
    dropped_++;
}
```

[18][19]

Damit gilt zusätzlich:

- `AoaControlTap::dropCount() == 0`
- `EventBus::dropped() == 0`
- `EventBus::streamLost() == 0`
- TCP-Collector vollständig synchronisiert

Erst dann sind die exportierten Logs vollständig genug, um Negativbefunde zu bewerten.

---

# 11. Threading-/Concurrency-Risiko

`AoaControlTap` wird offenbar aus einem USB-Callback beziehungsweise USB-Task beschrieben und im Arduino-Loop gelesen.

Die Queue ist als:

```cpp
lock-free-ish ring
```

beschrieben [14].

Die Indizes sind `volatile`, aber nicht durch eine Critical Section oder atomare Operationen geschützt:

```cpp
volatile uint32_t head_ = 0;
volatile uint32_t tail_ = 0;
```

[14]

Das kann funktionieren, wenn:

- genau ein Producer,
- genau ein Consumer,
- geeignete native Schreib-/Lesegrößen,
- keine problematischen Memory-Ordering-Effekte

vorliegen.

Für forensische USB-Analyse ist das aber nicht ideal. Ein beschädigter oder inkonsistenter Sample-Eintrag könnte wie ein ungewöhnlicher Request oder ein fehlender Request aussehen.

## Empfehlung

Mindestens:

- Producer-/Consumer-Annahmen explizit dokumentieren,
- Queue-Overflow und korrupt wirkende Samples markieren,
- eine Sequenznummer pro Sample ergänzen,
- Critical Section oder atomare Indizes verwenden,
- maximale Callback-Rate testen.

Für P0 sollte zusätzlich ein kleiner Rohtrace-Counter existieren, der nicht von der EventBus-Übertragung abhängt.

---

# 12. Ein weiterer wichtiger Punkt: Die Firmware misst ihre eigene USB-Identität, nicht die Hostsicht

`P0-C` wird beim Attach erzeugt mit:

```cpp
extra["vid"] = g.vid();
extra["pid"] = g.pid();
```

und:

```cpp
"presented_at_boot_not_post_53_reenum"
```

[3]

Das ist korrekt als Hypothese markiert:

```cpp
direct_observation = false
```

Aber die Firmware liest hier nur die Werte, die sie selbst gesetzt hat. Sie beweist nicht, dass:

- der Host diese Deskriptoren korrekt gelesen hat,
- der Host sie akzeptiert hat,
- keine Deskriptorfehler aufgetreten sind,
- tatsächlich eine Re-Enumeration stattgefunden hat.

Für die Hostsicht braucht ihr:

- `usbmon`/USBPcap,
- oder einen Hardware-Analyzer,
- oder zumindest Host-seitige Enumeration-Logs.

---

# 13. Was die aktuelle Firmware bereits gut macht

Trotz der Einschränkungen ist die Architektur in mehreren Punkten sauber.

## Positiv

### A. MSC und AOA werden isoliert

Der AOA-Observe-Gadget ist ausdrücklich ohne MSC:

```cpp
bool mscInterfacePresent() const { return false; }
```

[12]

und der Startpfad publiziert:

```cpp
msc_interface_present = false
vendor_interface_present = true
```

[3]

Damit wird die frühere MSC-Composite-Verunreinigung im neuen Pfad vermieden.

### B. P0 sendet standardmäßig nichts

`AoaTransport::write()` blockiert TX im Observe-Modus:

```cpp
p0_tx_disabled
```

[9]

Das ist für eine saubere Erstbeobachtung sehr gut.

### C. BCL wird nicht ungeprüft aus Live-Daten decodiert

Der Live-P0-Pfad bleibt Raw-Bulk-only [3]. Das verhindert, dass zufällige Bytes als BCL-Frames klassifiziert werden.

### D. Hypothesen sind teilweise sauber markiert

Beispielsweise:

```cpp
hypothesis_google_accessory_0x18D1_0x2D00
```

und:

```cpp
not_claimed = "nbt_wire_bcl,sas,rhmi,cds,av,aoa_equals_bcl"
```

[3][15]

Das ist epistemisch sehr gut.

---

# 14. Fähigkeitseinstufung der aktuellen Firmware

| Fähigkeit | Status | Bewertung |
|---|---:|---|
| Eigenes USB-Gadget starten | Ja | gut |
| VID/PID und Strings setzen | Ja | gut |
| MSC-freies AOA-Testgerät präsentieren | Ja | gut |
| USB-Interface-Mount erkennen | Ja | eingeschränkt |
| Vendor-Control-Callbacks beobachten | Ja | gut |
| Vollständige EP0-SETUP-Beobachtung | Nein | fehlt |
| Standard Requests beobachten | Nicht zuverlässig | fehlt |
| Class Requests beobachten | Nicht zuverlässig | fehlt |
| AOA 51/52/53 innerhalb des Vendor-Callbacks sehen | Ja, falls geroutet | eingeschränkt |
| Klassischen AOA-Wechsel vollständig ausführen | Nein | Re-Enumeration fehlt |
| Bulk-OUT von HU beobachten | Ja | brauchbar |
| Beweisen, dass kein Bulk auf dem Bus lief | Nein | Host-/Hardware-Trace nötig |
| Bulk-TX an HU senden | Ja, gated | nur Stack-Akzeptanz nachgewiesen |
| HU-TX-Verarbeitung bestätigen | Nein | Response/Host-Capture nötig |
| BCL-Live-Framing | Absichtlich nein | korrekt isoliert |
| BCL-Syntax synthetisch testen | Ja | gut |
| BCL-on-wire beweisen | Nein | nicht vorhanden |
| Bluetooth/SPP analysieren | Nein | `SppTransport` ist nur Stub [8] |
| Eventverluste erkennen | Teilweise | Counters vorhanden |
| Forensisch vollständige Capture-Garantie | Nein | noch nicht |

---

# 15. Empfohlene Änderungen

## Priorität 1: Begriffe und Statussemantik korrigieren

### Aktuell

```text
CONNECTED
```

wenn `vendorMounted()` wahr ist.

### Besser

```text
usb_configured
vendor_interface_mounted
```

Zusätzliche Zustände:

```text
bulk_rx_seen
bulk_tx_queued
bulk_tx_bus_completed
application_peer_proven
```

Damit wird verhindert, dass eine lokale Stack-Konfiguration als BMW-App-Verbindung interpretiert wird.

---

## Priorität 2: Vollständiges EP0-Logging ergänzen

Der beste nächste Punkt liegt **unterhalb von `USBVendor.onRequest()`**, möglichst im TinyUSB-Control-Dispatch.

Minimal zu erfassen:

```text
timestamp
rhport
stage
bmRequestType
bRequest
wValue
wIndex
wLength
recipient
direction
request_type
dispatch_result
data_length
status_result
stall
```

Zusätzlich:

```text
USB reset
SET_ADDRESS
SET_CONFIGURATION
SET_INTERFACE
disconnect
reconnect
```

Wichtig ist die Unterscheidung:

```text
ep0_setup_all
vendor_callback_seen
vendor_callback_handled
data_phase_seen
status_completed
```

---

## Priorität 3: Control-Trace nicht über EventBus als primäre Wahrheit führen

Für die USB-Forensik sollte ein kompakter binärer oder statischer Ringpuffer unabhängig vom JSON-/TCP-System existieren.

Beispiel:

```cpp
struct Ep0TraceRecord {
    uint32_t seq;
    uint64_t t_us;
    uint8_t  rhport;
    uint8_t  stage;
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
    uint8_t  dispatch;
    uint8_t  result;
    uint16_t data_len;
};
```

Der EventBus sollte nur eine exportierte Kopie sein.

So bleibt der Rohtrace erhalten, auch wenn:

- WiFi ausfällt,
- TCP abbricht,
- der EventBus überläuft,
- die Arduino-Hauptschleife verzögert wird.

---

## Priorität 4: AOA-Testpfade klar trennen

Es sollten drei explizite Modi existieren:

### Modus A: Direct Accessory Identity

```text
Boot als 18D1:2D00
Kein klassischer 51/52/53-Test
Ziel: Reaktion auf bereits aktiven Accessory-Zustand
```

### Modus B: Phone-like handshake probe

```text
Boot als Nicht-Accessory
51/52/53 beobachten
Ziel: Prüfen, ob HU den klassischen AOA-Einstieg versucht
```

### Modus C: vollständiger AOA-Responder

```text
51/52/53 beantworten
USB neu initialisieren
als 18D1:2D00 re-enumerieren
Bulk-Endpunkte bereitstellen
```

Der aktuelle Code implementiert A und Teile von B, aber nicht C.

---

## Priorität 5: Host-Trace als Pflichtbestandteil definieren

Ein Firmware-Lauf sollte künftig nur dann als gültiger AOA-/USB-Befund gelten, wenn parallel ein Host-Trace vorhanden ist.

Mindestens:

```text
Device firmware trace
+
Host usbmon/USBPcap trace
```

Für besonders wichtige Negativbefunde:

```text
Device trace
+
Host trace
+
optional Hardware analyzer
```

---

# 16. Konkrete Antworten auf die beiden Fragen

## Haben wir im Code einen entscheidenden Fehler?

### Ja, der entscheidende Fehler ist die Interpretationsebene:

Die Firmware misst nicht den USB-Bus vollständig, sondern einen begrenzten `USBVendor`-Callback-Pfad. Das ist im Code selbst dokumentiert [13][14].

Zusätzlich gibt es drei konkrete funktionale Einschränkungen:

1. **Post-53-Re-Enumeration ist nicht implementiert.**  
   Der klassische AOA-Wechsel kann daher nicht vollständig getestet werden [13].

2. **`connected` bedeutet nur Vendor-Mount beziehungsweise lokale USB-Bereitschaft.**  
   Es beweist keinen Connected-App-Peer [9][15].

3. **`tx_ok` bedeutet nur Annahme durch den lokalen USB-Stack.**  
   Es beweist keine Verarbeitung durch die HU [3][9].

Das sind keine zwingenden Firmware-Crash-Bugs, aber sie begrenzen die Aussagekraft der gesamten Untersuchung erheblich.

## Haben wir auf der FW-Seite überhaupt die Fähigkeit, die HU richtig zu analysieren?

### Für positive Ereignisse: teilweise ja

Die Firmware kann belastbar feststellen:

- dass ein beobachtbarer Vendor-Control-Request angekommen ist,
- dass Bulk-Daten im eigenen RX-Pfad eingetroffen sind,
- dass die eigene USB-Funktion gemountet wurde,
- dass der eigene Stack einen TX-Versuch akzeptiert hat.

### Für negative Aussagen: derzeit nein

Sie kann nicht belastbar feststellen:

- dass die HU keinen bestimmten EP0-Request gesendet hat,
- dass kein Standard-/Class-Control-Verkehr stattgefunden hat,
- dass auf dem USB-Bus kein Bulk-Verkehr existierte,
- dass die HU keinen Consumer geöffnet hat,
- dass ein `18D1:2D00`-Zustand von der HU tatsächlich als AOA akzeptiert wurde,
- dass BCL nicht über USB lief,
- dass BCL über einen anderen Transport lief.

---

# Schlussbewertung

Der aktuelle FW-Stand ist als **P0-Observe-Gadget** sinnvoll und relativ sauber aufgebaut. Besonders positiv sind:

- MSC-Isolation,
- Raw-Bulk-only im P0-Pfad,
- TX-Gating,
- Trennung von synthetischem BCL und Live-Transport,
- Kennzeichnung von Hypothesen.

Aber:

> **Die Firmware ist derzeit kein vollständiges Analyseinstrument für die HU. Sie ist ein beobachtendes USB-Accessory-Gadget mit Vendor-Control- und Bulk-Sicht.**

Der wichtigste nächste Schritt ist deshalb nicht ein weiterer BCL-Knock, sondern:

1. vollständiges EP0-Setup-Logging unterhalb von `USBVendor.onRequest()`,
2. klare Trennung zwischen „USB konfiguriert“ und „Anwendung verbunden“,
3. Implementierung oder explizite Aufgabe der echten Post-53-Re-Enumeration,
4. paralleler Host-USB-Capture,
5. Validierung aller Negativbefunde gegen `dropCount`, EventBus-Verluste und Host-Trace.

Die zentrale Statusaussage sollte aktuell lauten:

> **Die Firmware kann beobachtbare Vendor-Control- und Bulk-Ereignisse erfassen, aber sie kann aus ihrem eigenen Trace noch nicht beweisen, dass bestimmte USB-Requests oder Bulk-Transfers auf dem Bus nicht stattgefunden haben.**

---

# 17. Repo-Abgleich (nicht Teil der gelieferten Analyse)

Geprüft gegen `main` @ `849f30e`. Die Architekturthese und die drei Einschränkungen aus Abschnitt 16 stehen so im Code. Keine Firmware-Änderung in dieser Ablage.

| Stelle in der Analyse | Stand `849f30e` |
|---|---|
| Tap ist kein EP0-Sniffer | `AoaControlTap.h`: „Publishes every SETUP that arrives on this callback (not a full EP0 sniffer).“ Routing nur über `g_vendor.onRequest(onVendorControl)`. |
| `setup_count_` nur Callback-SETUPs | Zähler laufen nur bei `stage == REQUEST_STAGE_SETUP`. Headertext dazu stimmt. |
| Stage-Enum, im Text nicht ausformuliert | Lab gegen `aoa_phone_like`: Host-51 → STALL, `setup_count=0`. Hochwahrscheinlich TinyUSB `CONTROL_STAGE_SETUP=1` gegen Arduino `REQUEST_STAGE_SETUP=0`. Der MSC-Tap in `MscBotTap.cpp` vergleicht bereits `CONTROL_STAGE_SETUP`. Ein Request kann den Callback erreichen und trotzdem nicht gezählt und nicht beantwortet werden. [`2026-10-30-lab-aoa-aaidrive-phone-host.md`](2026-10-30-lab-aoa-aaidrive-phone-host.md) |
| Post-53-Re-Enumeration fehlt | Kommentar in `AoaControlTap.cpp` bei Request 53: `post-53 VID re-enum NOT IMPLEMENTED (Arduino lock).` |
| Direct `18D1:2D00` | Accessory-Zweig in `AoaObserveGadget::begin()` setzt `vid_=0x18D1`, `pid_=0x2D00`, Note `hypothesis_google_accessory_0x18D1_0x2D00`. Phone-like ist `0x04E8:0x6860`. |
| `connected_` | `AoaTransport::poll()` setzt `connected_` und `TransportEvent::Connected` aus `vendorMounted()` (`ready_ && g_vendor.mounted()`). Status-JSON hat daneben `transport.vendor_mounted`. Der Eventname bleibt `Connected`. |
| `tx_ok` | JSON-Feld ist die von `write()` zurückgegebene Bytezahl `n`. `sendKnock()` setzt `enableWireTx(true)` nur um diesen `write()` herum und danach wieder aus. Default ohne Knock: `p0_tx_disabled`. |
| req52 an Hypothesenantwort gekoppelt | Scratch-Länge wird nur im Block `hypothesis_respond_ && vendor && REQUEST_STAGE_SETUP` gesetzt. Default des Members ist `true`. `AoaObserveGadget::begin()` ruft `setHypothesisRespond(true)` auf. Observe-Boots antworten also. |
| Queue-Drop | Bei voller Queue (`kQueueDepth=32`) wird das neue Sample nicht gespeichert, `drops_++`, `return false`. Die Zähler `setup_count_` / `aoa51_` / `aoa52_` / `aoa53_` werden davor erhöht. Ein verworfener 51 bleibt in `aoa51_` sichtbar und erhöht `drops_`. `aoa51=0` wird durch Drops anderer Samples nicht gelöscht. Unbelastbar bleibt der exportierte Event-Strom, sobald `drops_>0`. |
| EventBus | `publish()` läuft in `portENTER_CRITICAL` und überschreibt bei vollem Ring das älteste Event (`dropped_++`). `streamLost()` existiert. |
| Bulk-RX-Ring | Zusätzlich zu Tap und EventBus: `AoaTransport::pushRing()` zählt `rx_overflow_`, wenn der RX-Ring voll ist. Ein Überlauf ist ein positiver Empfang mit unvollständigem Mitschnitt. |
| P0-C | `publishCaUsb("P0-C", "vid_pid", true, false, …)` mit Note `presented_at_boot_not_post_53_reenum`. Viertes Argument ist `direct_observation=false`. Werte sind `g.vid()` / `g.pid()`, also die selbst gesetzten IDs. |
| Live-P0 und BCL | `ConnectedAppProbe::loop()` im Observe-Boot ruft `drainTransportRaw()` auf und kehrt zurück. Kommentar: „P0: raw bulk only — no BclFramer“. `drainTransportToFramer()` liegt auf dem Skeleton-/MSC-Boot-Pfad und optional beim Lab-Inject. `BclTypes.h` und `CaC1LabBclStream.h` markieren Desk-R0 beziehungsweise `source=synthetic`. |
| MSC-Isolation auf dem neuen Pfad | `AoaObserveGadget::mscInterfacePresent()` liefert `false`. Status-JSON setzt `msc_interface_present = !aoa_observe_boot_`. Der historische Composite-Leak gilt für Sessions vor FW 0.3.27, nicht für diesen Observe-Gadget. |
| `SppTransport` | `implLevel()` ist `"stub"`, `arm()` setzt `not_implemented`. |
| `p0_b_` | Diese Analyse behauptet nicht, dass jedes Sample `p0_b_` setzt. `drainControlTap()` setzt es seit `4c1d6d2` (FW 0.3.28) nur bei `bRequest` 51, 52, 53 oder 58. `stage` und `request_type` werden mitpubliziert. |
