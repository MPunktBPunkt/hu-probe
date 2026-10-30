# Code-Verbesserungsplan für C1b-P0/P0.5

**Projekt:** hu-probe  
**Datum:** 2026-10-30  
**Repo-Basis:** `main` @ `849f30e`  
**Gegenstand:** Umsetzungsplan zur Messsemantik und Capture-Gültigkeit des AOA-Observe-Pfads. Nicht implementiert.  
**Code-Analyse:** [`2026-10-30-codeanalyse-connected-aoa.md`](2026-10-30-codeanalyse-connected-aoa.md)  
**MSC-Verbesserungspotential:** [`2026-10-30-msc-verbesserungspotential.md`](2026-10-30-msc-verbesserungspotential.md)  
**Multi-Review:** [`2026-10-30-gesamtbericht-aoa-zubehoer-multireview.md`](2026-10-30-gesamtbericht-aoa-zubehoer-multireview.md)  
**Problemfelder:** [`2026-10-30-aoa-problemfelder.md`](2026-10-30-aoa-problemfelder.md)  
**Gesamtbewertung:** [`2026-10-30-gesamtbewertung-aoa-fuenf-reports.md`](2026-10-30-gesamtbewertung-aoa-fuenf-reports.md)  
**Reports:** [1](2026-10-30-research-01-aoa-direct-accessory.md) · [2](2026-10-30-research-02-usb-sniffing-gadget-validation.md) · [3](2026-10-30-research-03-esp32-tinyusb-ep0-callbacks.md) · [4](2026-10-30-research-04-composite-vs-vendor-only.md) · [5](2026-10-30-research-05-bcl-etch-nbt.md)  
**Lab:** [`2026-10-30-lab-aoa-aaidrive-phone-host.md`](2026-10-30-lab-aoa-aaidrive-phone-host.md)  
**Epistemik:** [`../CONNECTED-APP.md`](../CONNECTED-APP.md) · [`../GATES-Connected.md`](../GATES-Connected.md)

Die eckigen Ziffern im Text sind die mitgelieferten Verweise. Zuordnung zu Dateien in diesem Repo, keine externe Bibliographie:

| Ziffer | Datei |
|---|---|
| [1] | `firmware/src/connected/ConnectedAppProbe.h` |
| [3] | `firmware/src/connected/ConnectedAppProbe.cpp` |
| [4] | `firmware/src/connected/bcl/CaC1LabBclStream.h` |
| [6] | `firmware/src/connected/bcl/BclFramer.h` |
| [7] | `firmware/src/connected/bcl/BclTypes.h` |
| [8] | `firmware/src/connected/transport/SppTransport.h` |
| [9] | `firmware/src/connected/transport/AoaTransport.cpp` |
| [10] | `firmware/src/connected/transport/AoaTransport.h` |
| [12] | `firmware/src/connected/aoa/AoaObserveGadget.h` |
| [13] | `firmware/src/connected/aoa/AoaControlTap.cpp` |
| [14] | `firmware/src/connected/aoa/AoaControlTap.h` |
| [15] | `firmware/src/connected/aoa/AoaObserveGadget.cpp` |
| [18][19] | `firmware/src/net/EventBus.cpp`, `EventBus.h` |

> **Ablagehinweis zum Stand `849f30e`:** Der Text ab dem Gesamturteil ist der gelieferte Plan. Er ist nicht umgesetzt. Die drei Begrenzungen stimmen mit dem Code überein. Drei Stellen beschreibt der Plan nicht so, wie der Commit sie schon hat. Der Abgleich steht am Ende.

---

## Gesamturteil

Der aktuelle Stand ist als **beobachtendes USB-Gadget** brauchbar, aber die Aussagekraft wird derzeit durch drei zentrale Punkte begrenzt:

1. **`connected_` bedeutet nicht „HU-/App-Peer verbunden“, sondern praktisch „USBVendor gemountet“.**
2. **`USBVendor.onRequest()` ist ausdrücklich kein vollständiger EP0-Sniffer.**
3. **P0 ist trotz Observe-Bezeichnung nicht vollständig passiv**, weil `hypothesis_respond_` beim Gadget-Start immer auf `true` gesetzt wird [13][15].

Der wichtigste Fehler ist damit nicht die BCL-Implementierung, sondern die Gefahr, dass lokale Stackzustände, Callback-Sichtbarkeit und echte Hostaktivität in denselben Statusbegriffen zusammenlaufen.

Die Priorität sollte deshalb lauten:

> **Erst Messsemantik und Capture-Gültigkeit korrigieren, dann die EP0-Instrumentierung vertiefen. Keine weitere Connected-App-Funktionalität implementieren.**

---

# 1. Quick Wins

## Priorität 1: `connected_` umbenennen beziehungsweise semantisch korrigieren

### Aktueller Zustand

In `AoaTransport::arm()` und `poll()` wird `connected_` bereits dann gesetzt, wenn:

```cpp
AoaObserveGadget::instance().vendorMounted()
```

wahr ist [9].

`vendorMounted()` bedeutet im aktuellen Gadget:

```cpp
ready_ && g_vendor.mounted()
```

also im Wesentlichen:

> Die lokale USBVendor-Funktion ist verfügbar beziehungsweise konfiguriert.

Das beweist nicht:

- dass die HU einen Connected-App-Consumer geöffnet hat,
- dass Bulk-Daten fließen,
- dass ein Anwendungspartner existiert,
- dass BCL aktiv ist.

### Empfohlene Änderung

Kurzfristig den bestehenden Zustand umbenennen:

```cpp
bool usb_function_mounted_;
```

statt:

```cpp
bool connected_;
```

Oder, falls größere Codeänderungen vermieden werden sollen:

```cpp
bool connected() const {
    return vendor_mounted_;
}
```

und im Status explizit:

```json
{
  "usb_function_mounted": true,
  "application_peer_proven": false
}
```

### Empfohlene neue Semantik

```cpp
enum class LinkState {
    UsbNotReady,
    UsbFunctionMounted,
    PeerBulkSeen,
    ApplicationPeerProven
};
```

Für P0 reicht zunächst:

```text
usb_stack_ready
usb_function_mounted
host_bulk_rx_seen
host_bulk_tx_observed
application_peer_proven
```

`application_peer_proven` sollte im aktuellen Code immer `false` bleiben, solange kein belastbarer Consumer-Nachweis existiert.

**Bewertung:** sehr hoher Nutzen, geringes Risiko.

---

## Priorität 2: P0 standardmäßig wirklich passiv machen

### Aktueller Fehler

In `AoaObserveGadget::begin()` wird immer gesetzt:

```cpp
AoaControlTap::instance().setHypothesisRespond(true);
```

[15]

Dadurch antwortet der Code bei passenden Vendor-Requests hypothetisch auf:

- 51
- 52
- 53
- 58

[13].

Das widerspricht der konzeptionellen Bezeichnung:

```text
P0 observe-only
```

und der Beschreibung, dass P0 keinen aktiven Protokollverkehr erzeugen soll [1][3].

### Empfohlene Änderung

Standardwert:

```cpp
AoaControlTap::instance().setHypothesisRespond(false);
```

Aktivierung nur über einen expliziten aktiven Testmodus:

```cpp
AoaControlTap::instance().setHypothesisRespond(
    profile == AoaIdentityProfile::PhoneLike &&
    active_aoa_responder_mode_);
```

Noch besser: drei klar getrennte Modi:

```cpp
enum class AoaMode {
    DirectAccessoryObserve,
    PhoneLikeObserve,
    PhoneLikeResponder
};
```

Dabei gilt:

| Modus | VID/PID | Antworten | Re-Enumeration |
|---|---|---:|---:|
| DirectAccessoryObserve | `18D1:2D00` | nein | nein |
| PhoneLikeObserve | nicht-AOA | nein | nein |
| PhoneLikeResponder | nicht-AOA | ja, hypothetisch | derzeit nicht implementiert |

Der aktuelle Code implementiert den dritten Modus nur teilweise, weil die Re-Enumeration nach Request 53 ausdrücklich fehlt [13].

**Bewertung:** sehr hoher Nutzen, geringe Änderung.

---

## Priorität 3: `aoa_control_count` eindeutig benennen

### Aktueller Zustand

Im Status wird ausgegeben:

```cpp
tr["setup_count"] = tap.setupCount();
tr["vendor_setup_count"] = tap.vendorSetupCount();
tr["aoa_control_count"] = tap.vendorSetupCount();
```

[3]

`aoa_control_count` klingt jedoch nach:

> Anzahl aller AOA-Control-Requests

tatsächlich bedeutet es:

> Anzahl der Vendor-SETUPs, die den `USBVendor`-Callback erreicht haben.

Das ist eine erhebliche semantische Unschärfe.

### Empfohlene Änderung

Legacy-Feld beibehalten, aber neue eindeutige Felder ergänzen:

```json
{
  "callback_setup_count": 12,
  "callback_vendor_setup_count": 5,
  "callback_aoa_51_count": 0,
  "callback_aoa_52_count": 0,
  "callback_aoa_53_count": 0,
  "callback_aoa_58_count": 0,
  "aoa_control_count_legacy": 5,
  "visibility_scope": "USBVendor.onRequest_only"
}
```

Das alte Feld sollte nicht sofort entfernt werden, damit bestehende Auswertungen nicht brechen. Es sollte aber als Legacy markiert werden.

### Zusätzliche Pflichtmetadaten

```json
{
  "is_full_ep0_trace": false,
  "includes_standard_requests": false,
  "includes_class_requests": false,
  "includes_all_vendor_recipients": false,
  "includes_data_stage": "partial",
  "includes_status_stage": false
}
```

**Bewertung:** sehr hoher Nutzen, minimale Änderung.

---

## Priorität 4: `P0_B` explizit aufteilen

### Aktueller Zustand

Der Code setzt:

```cpp
const bool aoa_req =
    s.b_request == 51 ||
    s.b_request == 52 ||
    s.b_request == 53;

const bool aoa_or_audio = aoa_req || s.b_request == 58;

if (!p0_b_ && aoa_or_audio) p0_b_ = true;
```

[3]

Das ist im aktuellen Code besser als die frühere Variante „jedes Callback-Sample setzt P0_B“. Trotzdem bleibt die Bedeutung unklar:

- `P0_B` bedeutet nicht „AOA vollständig bestätigt“.
- Es bedeutet nur:
  - ein passender Request wurde
  - in diesem `USBVendor`-Callback
  - beobachtet.

Zusätzlich wird Request 58 als `aoa_or_audio` aufgenommen, obwohl er semantisch nicht zum klassischen AOA-Entry-Pfad 51/52/53 gehört.

### Empfohlene Aufteilung

```cpp
bool p0_b_callback_any_;
bool p0_b_vendor_request_seen_;
bool p0_b_aoa_entry_seen_;       // 51/52/53
bool p0_b_aoa_audio_seen_;       // 58
bool p0_b_ep0_setup_seen_;       // zukünftiger Vollhook
bool p0_b_reenum_observed_;      // Host-/Device-Indikator
```

Im JSON:

```json
{
  "P0_B": false,
  "P0_B_vendor_callback": true,
  "P0_B_aoa_entry_callback": false,
  "P0_B_audio_callback": false,
  "P0_B_scope": "callback_visible_only"
}
```

### Empfohlene Definition

```text
P0_B_aoa_entry_callback =
    mindestens ein Request 51, 52 oder 53
    im USBVendor-Callback beobachtet
```

Nicht:

```text
P0_B = AOA bestätigt
```

Denn `P0_B` beweist weder den vollständigen Handshake noch eine Re-Enumeration.

**Bewertung:** sehr hoher Nutzen, minimale Änderung.

---

## Priorität 5: `P0_C` und `P0_D` sprachlich entschärfen

### `P0_C`

Der Code markiert die präsentierte VID/PID als:

```text
presented_at_boot_not_post_53_reenum
```

und setzt `direct_observation = false` [3].

Das ist bereits gut. Im Status sollte zusätzlich klar stehen:

```json
{
  "identity_configured_locally": true,
  "identity_seen_by_host": false,
  "post_53_reenumeration": false
}
```

### `P0_D`

Aktuell bedeutet `P0_D`:

```text
USBVendor bulk pair mounted
```

Das ist kein Beweis für:

- Host-Endpoint-Nutzung,
- Bulk-Transfer,
- App-Consumer.

Besser:

```text
P0_D_vendor_function_mounted
```

und separat:

```text
P0_E_host_bulk_rx_seen
P0_F_host_bulk_tx_observed
```

Der bestehende `P0_E` wird momentan bei lokal empfangenen Bytes gesetzt [3]. Auch das sollte als:

```text
device_stack_bulk_rx_seen
```

bezeichnet werden.

---

# 2. Messmethodik- und Status-Fixes

## Priorität 1: Capture-Gültigkeit formal einführen

Der aktuelle Code besitzt bereits mehrere Zähler:

- `AoaControlTap::dropCount()` [14]
- `AoaTransport::rxOverflow()` [9][10]
- EventBus `dropped()`
- EventBus `streamLost()` [18][19]
- TCP-Übertragungszähler [18][19]

Diese Werte müssen künftig in eine formale Capture-Gültigkeit einfließen.

### Neue Statusstruktur

```json
{
  "capture_valid": false,
  "capture_invalid_reasons": [
    "control_queue_drops",
    "eventbus_drops"
  ],
  "control_tap_drops": 0,
  "transport_rx_overflow": 0,
  "eventbus_dropped": 0,
  "eventbus_stream_lost": 0,
  "tcp_tx_fail": 0,
  "session_complete": true
}
```

### Empfohlene Gültigkeitsregel für Negativbefunde

Ein Device-seitiger Negativbefund darf nur als:

```text
device_observation_negative
```

bezeichnet werden, wenn:

```text
control_tap_drops == 0
transport_rx_overflow == 0
eventbus_dropped == 0
eventbus_stream_lost == 0
tcp_tx_fail == 0
session_complete == true
```

Aber auch dann ist er **kein Bus-Negativbeweis**, weil der Hook unvollständig ist.

Daher zwei Ebenen:

```text
device_capture_complete = true/false
host_trace_present = true/false
negative_claim_valid = host_trace_present && device_capture_complete
```

Für einen reinen Device-Lauf sollte gelten:

```text
negative_claim_scope = "callback_scope_only"
```

---

## Priorität 2: EventBus-Verluste nicht nur sammeln, sondern auswerten

Der EventBus überschreibt bei voller Queue den ältesten Eintrag und erhöht `dropped_` [18][19].

Das ist korrekt für robuste Laufzeit, aber für forensische Analyse muss ein Lauf mit Drops als eingeschränkt markiert werden.

### Neue Felder

```json
{
  "eventbus": {
    "published": 123,
    "dropped": 0,
    "stream_lost": 0,
    "queue_highwater": 83,
    "queue_pending": 0,
    "tcp_tx_fail": 0
  }
}
```

### Mindestmaßnahme

Am Ende jeder Session ein verpflichtendes:

```text
capture_health
```

Event publizieren, bevor die Session geschlossen wird.

Darin:

- erster Event-Sequence-Wert,
- letzter Event-Sequence-Wert,
- erwartete Eventzahl,
- veröffentlichte Eventzahl,
- Drops,
- TCP-Fehler,
- Control-Tap-Drops,
- Transport-Overflow,
- Testdauer,
- Attach-/Re-Enumeration-Zähler.

---

## Priorität 3: Queue-Überlauf im `AoaControlTap` härter behandeln

Die Queue hat nur:

```cpp
static constexpr size_t kQueueDepth = 32;
```

[14].

Das ist für einen kurzen Kontrolltrace möglicherweise ausreichend, aber ein Drop macht negative Aussagen unzuverlässig. Problematisch ist außerdem, dass die Zähler erhöht werden, bevor geprüft wird, ob das Sample tatsächlich in die Queue gelangt:

```cpp
setup_count_++;
vendor_setup_count_++;
...
push(s);
```

[13]

Wenn `push(s)` wegen voller Queue fehlschlägt:

- die globalen Counter zählen den Request,
- das Event existiert aber nicht im exportierten Trace.

Das erzeugt eine mögliche Diskrepanz zwischen:

```text
aoa51Count()
```

und tatsächlich sichtbaren JSON-Events.

### Quick Fix

`push()`-Ergebnis auswerten:

```cpp
if (!push(s)) {
    capture_incomplete_ = true;
}
```

Zusätzlich separate Counter:

```cpp
uint32_t setup_seen_callback_;
uint32_t setup_enqueued_;
uint32_t setup_dropped_;
```

Dann:

```text
callback_count = enqueued_count + dropped_count
```

Wenn das nicht aufgeht, liegt ein Instrumentierungsfehler vor.

### Empfehlung

Queue für P0 zunächst auf mindestens 128 oder 256 Samples erhöhen. Das ist kein Ersatz für eine bessere Architektur, reduziert aber unnötige Drops.

**Wichtig:** Eine größere Queue löst keine fehlende EP0-Abdeckung.

---

## Priorität 4: Zeitbasis und Sequenzierung verbessern

`AoaControlSample` besitzt bereits `t_us` [14]. EventBus-Ereignisse besitzen Session-Sequenzen [3].

Für die Korrelation zwischen USB-Callback, Transport, EventBus und Host-Trace sollten alle Ebenen zusätzlich eine lokale Monotonie-ID besitzen:

```cpp
uint32_t control_seq;
uint32_t transport_seq;
uint32_t usb_epoch;
```

Beispiel:

```json
{
  "control_seq": 17,
  "t_us": 123456789,
  "usb_epoch": 2,
  "stage": "SETUP",
  "callback_scope": "USBVendor.onRequest"
}
```

Das hilft, wenn:

- Events verzögert publiziert werden,
- Re-Enumeration stattfindet,
- TCP-Streaming zeitlich nicht synchron ist,
- mehrere USB-Epochen innerhalb einer Session auftreten.

---

## Priorität 5: `hypothesis_handled` nicht mit „Host akzeptiert“ verwechseln

Der Code setzt `hypothesis_handled`, wenn:

```cpp
v->sendResponse(...)
```

erfolgreich aufgerufen wurde [13].

Das bedeutet nur:

> Der lokale Callback hat eine Antwort an den USB-Stack übergeben.

Es bedeutet nicht:

- dass der Host die Antwort akzeptiert hat,
- dass der Transfer erfolgreich abgeschlossen wurde,
- dass der Host re-enumeriert hat,
- dass ein Accessory-Consumer aktiv wurde.

Bessere Feldnamen:

```json
{
  "device_response_queued": true,
  "host_acknowledged": false,
  "reenumeration_observed": false
}
```

Das bestehende Feld kann als Legacy bestehen bleiben, sollte aber durch `device_response_queued` ergänzt werden.

---

## Priorität 6: Response- und Observe-Daten strikt trennen

Request-52-Logging ist derzeit mit dem hypothetischen Response-Pfad gekoppelt:

```cpp
if (hypothesis_respond_ && v && vendor && stage == REQUEST_STAGE_SETUP)
```

werden Scratch-Puffer und Response vorbereitet [13].

Damit ist der Observe-Pfad nicht vollständig unabhängig von eigener Protokollaktivität.

### Empfohlene Trennung

```cpp
capture_setup_metadata();
capture_data_phase_if_visible();

if (active_responder_mode_) {
    send_hypothesis_response();
}
```

Zusätzliche Metadaten:

```json
{
  "observation_mode": "passive",
  "response_mode": "disabled",
  "data_capture_mode": "callback_response_buffer"
}
```

Wenn die Data-Phase ohne aktive Antwort nicht sichtbar ist, muss das ausdrücklich gemeldet werden:

```text
req52_data_visibility = response_coupled
```

---

# 3. Mittelfristige Instrumentierungsänderungen

## Priorität 1: Vollständigerer EP0-Hook unterhalb von `USBVendor.onRequest()`

Das ist die wichtigste technische Verbesserung.

### Aktuelle Grenze

`AoaControlTap` dokumentiert ausdrücklich:

```text
not a full EP0 sniffer
```

[14].

`AoaObserveGadget` registriert den Callback ausschließlich über:

```cpp
g_vendor.onRequest(onVendorControl);
```

[15].

Daraus folgt:

- Standard-Requests sind nicht zuverlässig sichtbar.
- Class-Requests sind nicht zuverlässig sichtbar.
- Vendor-Requests außerhalb dieses Pfads sind nicht garantiert sichtbar.
- DATA-/STATUS-Phasen sind nicht vollständig instrumentiert.

### Ziel

Ein zusätzlicher Tracepunkt möglichst im TinyUSB-Control-Dispatch:

```text
EP0 SETUP received
    ↓
request classified
    ↓
standard/class/vendor dispatch
    ↓
handler result
    ↓
data/status completion
```

### Minimaldatensatz

```cpp
struct Ep0SetupRecord {
    uint32_t seq;
    uint32_t usb_epoch;
    uint64_t t_us;
    uint8_t rhport;
    uint8_t stage;
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
    uint8_t request_type;
    uint8_t recipient;
    uint8_t direction;
    uint8_t dispatch_path;
    uint8_t result;
};
```

### Dispatch-Werte

```text
CORE_STANDARD
CLASS_HANDLER
VENDOR_HANDLER
UNHANDLED
STALL
DATA_STAGE
STATUS_STAGE
```

Der neue Hook sollte den vorhandenen `AoaControlTap` nicht ersetzen. Beide Ebenen sollten parallel bestehen:

| Trace | Bedeutung |
|---|---|
| `ep0_setup_trace` | möglichst vollständige EP0-Sicht |
| `vendor_callback_trace` | was `USBVendor.onRequest()` tatsächlich sah |
| `transport_trace` | Bulk-/Byte-Pfad im Device-Stack |
| `eventbus_trace` | Export-/Collector-Sicht |

Damit kann erstmals unterschieden werden:

```text
EP0 gesehen, Vendor-Callback nicht gesehen
```

Das wäre für die aktuelle Diagnose besonders wertvoll.

---

## Priorität 2: Dispatch- und Completion-Logging ergänzen

Nur das SETUP-Paket reicht für eine vollständige Diagnose nicht immer.

Zusätzlich sollte erfasst werden:

```text
response prepared
data phase received
data phase sent
status completed
stall returned
timeout/error
```

Beispiel:

```json
{
  "b_request": 53,
  "stage": "SETUP",
  "dispatch": "VENDOR_CALLBACK",
  "device_response_queued": true,
  "transfer_completed": true,
  "stall": false
}
```

Für P0 ist besonders wichtig, nicht nur zu loggen, dass Request 53 ankam, sondern ob:

- die Antwort tatsächlich akzeptiert wurde,
- der Control-Transfer abgeschlossen wurde,
- ein USB-Reset oder Disconnect folgte.

---

## Priorität 3: USB-Epoch und Re-Enumeration-Detektor

Der aktuelle Code zählt USB-Starts über:

```cpp
attach_count_
transport_epoch_
```

[12][15].

Das ist nützlich, aber für eine Re-Enumeration muss zusätzlich differenziert werden:

```text
USB started
USB stopped
USB reset
SET_ADDRESS
SET_CONFIGURATION
descriptor identity
interface layout
```

### Empfohlene Zustandsfolge

```text
epoch 1:
  started
  configured
  descriptor_seen

epoch 2:
  stopped
  started
  configured
  descriptor_seen
```

Pro Epoch speichern:

```json
{
  "usb_epoch": 2,
  "vid_configured": 6353,
  "pid_configured": 11520,
  "identity_source": "local_configuration",
  "host_confirmed": false
}
```

Damit wird Direct-Accessory klar von einem echten Post-53-Mode-Switch getrennt.

---

## Priorität 4: Deskriptor- und Interface-Snapshot auf Device-Seite

Die Firmware kennt ihre eigenen Deskriptorparameter, aber sie kann damit nicht beweisen, was der Host gelesen hat.

Trotzdem sollte jeder Bootlauf einen vollständigen lokalen Snapshot publizieren:

```text
device_vid
device_pid
device_class
device_subclass
device_protocol
configuration_count
interface_count
interface_class
interface_subclass
interface_protocol
endpoint_addresses
endpoint_types
```

Dieser Snapshot ist nur:

```text
device_configured_identity
```

und nicht:

```text
host_seen_identity
```

Er ist trotzdem wertvoll für A/B-Vergleiche zwischen:

- Direct-Accessory,
- Phone-like,
- Vendor-only,
- Composite.

---

## Priorität 5: Rohdatenpfad für Control und Bulk

Der aktuelle EventBus-/JSON-Pfad ist für Statusinformationen gut, aber nicht ideal als Primärspeicher für forensische Daten.

Empfehlung:

```text
USB callback
   ↓
kleiner binärer Ringpuffer in RAM
   ↓
spätere JSONL-Exportkopie
```

Der Rohpuffer sollte unabhängig bleiben von:

- WiFi,
- TCP,
- Arduino-Loop-Latenz,
- EventBus-Auslastung.

Für jedes Record:

```text
sequence
timestamp
epoch
record type
payload length
payload
```

Die JSON-Ausgabe ist dann eine abgeleitete Darstellung.

---

## Priorität 6: Concurrency des `AoaControlTap` absichern

Der Header bezeichnet die Queue als:

```text
lock-free-ish ring
```

[14].

Die Queue verwendet `volatile` Indizes, aber keine Critical Section oder explizite atomare Synchronisierung [14].

Das kann bei einem einzelnen Producer und einem einzelnen Consumer funktionieren, ist aber für forensische Instrumentierung unnötig unsicher.

### Empfehlung

Mindestens eine der folgenden Varianten:

1. ESP32-Critical-Section um `push()` und `pop()`
2. atomare Indizes mit klarer Memory-Ordering
3. ISR-/USB-Task-sichere Ringbuffer-Implementierung
4. eine bewusste Dokumentation:
   - genau ein Producer,
   - genau ein Consumer,
   - Producer-Kontext,
   - maximale Recordgröße,
   - Overflow-Verhalten

Zusätzlich:

```cpp
uint32_t producer_seq;
uint32_t consumer_seq;
uint32_t corruption_count;
```

---

## Priorität 7: SPP nicht implementieren, aber Transportbeobachtung vorbereiten

`SppTransport` ist derzeit ausdrücklich nur ein Stub [8].

Das ist für den jetzigen Auftrag kein Problem. Es sollte kein neuer Connected-Transport implementiert werden.

Sinnvoll wäre lediglich, die Architektur so vorzubereiten, dass spätere Vergleichsläufe dieselben Metadaten liefern:

```text
transport_id
transport_scope
transport_observation_level
peer_proven
rx_seen
tx_queued
```

Nicht sinnvoll wäre aktuell:

- BCL über SPP zu implementieren,
- Watchdog-/Etch-Frames zu senden,
- aus USB-Negativbefunden direkt auf SPP umzuschalten.

---

# 4. Punkte, die ohne Host-Trace oder Stack-Patch nicht lösbar sind

## 4.1 Ob 51/52/53 tatsächlich vom Host gesendet wurden

Mit dem aktuellen `AoaControlTap` kann nur festgestellt werden:

> Diese Requests wurden im `USBVendor.onRequest()`-Callback beobachtet oder nicht beobachtet.

Nicht lösbar auf aktueller Ebene:

```text
Host hat 51/52/53 nie gesendet
```

Dafür notwendig:

- Host-Trace oder
- TinyUSB-Core-Trace oder
- Hardware-Analyzer.

Ein verbesserter Device-EP0-Hook würde die Aussagekraft erhöhen, wäre aber weiterhin eine Device-Sicht. Für einen maximal belastbaren Negativbefund ist ein Host-Trace erforderlich.

---

## 4.2 Ob Standard- und Class-Requests stattfanden

`USBVendor.onRequest()` ist dafür nicht ausreichend.

Ohne Stack-Patch ist nicht zuverlässig feststellbar:

- ob `GET_DESCRIPTOR` gesendet wurde,
- wann `SET_CONFIGURATION` stattfand,
- ob `SET_INTERFACE` stattfand,
- ob Class Requests an andere Handler gingen,
- ob ein Request im TinyUSB-Core beantwortet wurde.

---

## 4.3 Ob eine Antwort physikalisch auf dem USB-Bus war

`hypothesis_handled` beziehungsweise `sendResponse()` beweist nur, dass der Device-Code eine Antwort angefordert hat [13].

Nicht lösbar ohne Host- oder Hardware-Trace:

```text
Die HU hat diese Antwort tatsächlich auf dem Bus erhalten.
```

Ebenso gilt für `writeVendor()`:

```text
n > 0
```

belegt nur die Annahme durch den lokalen USB-Stack, nicht die Verarbeitung durch die HU [9][15].

---

## 4.4 Ob Bulk auf dem Bus leer war

`drainVendorRx()` zeigt nur:

> Im lokalen Vendor-RX-Pfad wurden keine Bytes sichtbar.

Nicht beweisbar ist damit:

> Auf dem USB-Bus gab es keinen Bulk-Verkehr.

Dafür erforderlich:

- Host-Trace,
- besser zusätzlich Hardware-Analyzer.

Der Device-Code sollte deshalb künftig unterscheiden:

```text
device_bulk_rx_zero
host_bulk_transfer_zero
bus_bulk_transfer_zero
```

Diese drei Aussagen sind nicht gleichwertig.

---

## 4.5 Ob der Host die lokal gesetzte VID/PID wirklich gesehen hat

`g.vid()` und `g.pid()` geben die lokal konfigurierte Identität zurück [3][12].

Das beweist nicht:

- dass der Host den Descriptor gelesen hat,
- dass keine Descriptorfehler auftraten,
- dass die Identität akzeptiert wurde,
- dass eine Re-Enumeration als diese Identität erfolgte.

Dafür ist Host- oder Hardware-Sicht notwendig.

---

## 4.6 Ob ein Accessory-Consumer geöffnet wurde

Der aktuelle Code hat keinen Nachweis dafür, dass:

- eine Android-App,
- ein HU-Anwendungsdienst,
- ein USB-Accessory-FD,
- oder ein BMW-spezifischer Consumer

den Kanal geöffnet hat.

`vendorMounted()` ist dafür nicht ausreichend.

Das lässt sich auf der ESP32-Geräteseite allein nicht sicher lösen. Notwendig wären:

- Android-/HU-Logs,
- Host-Stack-Diagnostik,
- App-/FD-Beobachtung,
- oder ein nachweisbarer Bulk-Datenaustausch.

---

# Modi sauber trennen

## A. Direct-Accessory-Observe

Der aktuelle Accessory-Modus startet direkt mit:

```text
VID 18D1
PID 2D00
```

[12][15].

Er testet:

> Wie reagiert die HU auf ein bereits als Accessory präsentiertes Gerät?

Er testet nicht:

> Führt die HU den klassischen 51→52→53-AOA-Übergang aus?

Status:

```text
direct_identity_presented = true
classic_aoa_transition_tested = false
response_mode = passive
post_53_reenum = not_applicable
```

---

## B. Phone-like-Observe

Der aktuelle Phone-like-Modus verwendet:

```text
VID 04E8
PID 6860
Manufacturer Android
Product AOA Phone-like
```

[15].

Das ist eine Hypothese zur Messung der Erreichbarkeit. Es ist keine bewiesene BMW- oder Android-Referenzidentität.

Status:

```text
phone_like_identity_hypothesis = true
aoa_responder_enabled = false
post_53_reenum = not_implemented
```

---

## C. Vollständiger AOA-Responder

Dafür wären notwendig:

1. 51 beantworten,
2. 52 empfangen und speichern,
3. 53 beantworten,
4. USB-Disconnect/Reset auslösen,
5. Deskriptoren beziehungsweise VID/PID neu setzen,
6. als Accessory neu enumerieren,
7. Bulk-Endpunkte für den neuen Zustand bereitstellen.

Der aktuelle Code kann Punkt 1 bis 3 hypothetisch unterstützen, aber Punkt 4 bis 7 nicht vollständig. Die fehlende Post-53-Re-Enumeration ist im Code ausdrücklich dokumentiert [13].

Dieser Modus sollte daher nicht als implementiert bezeichnet werden:

```text
aoa_responder_status = partial_control_reply_only
```

---

# Empfohlene Statusstruktur für P0

Eine mögliche neue Statusstruktur:

```json
{
  "probe_mode": "direct_accessory_observe",
  "observation_scope": {
    "vendor_callback": true,
    "generic_ep0": false,
    "host_trace": false,
    "bus_trace": false
  },
  "usb_state": {
    "stack_ready": true,
    "function_mounted": true,
    "local_vid": 6353,
    "local_pid": 11520,
    "usb_epoch": 1,
    "host_identity_confirmed": false
  },
  "peer_state": {
    "application_peer_proven": false,
    "bulk_rx_seen_device": false,
    "bulk_tx_queued_device": false,
    "host_bulk_seen": false
  },
  "aoa_callback_observation": {
    "setup_seen": 0,
    "vendor_setup_seen": 0,
    "request_51_seen": 0,
    "request_52_seen": 0,
    "request_53_seen": 0,
    "request_58_seen": 0,
    "scope": "USBVendor.onRequest_only"
  },
  "capture_health": {
    "control_queue_drops": 0,
    "transport_rx_overflow": 0,
    "eventbus_dropped": 0,
    "eventbus_stream_lost": 0,
    "tcp_tx_fail": 0,
    "device_capture_complete": true,
    "negative_claim_valid": false
  },
  "claims": {
    "direct_accessory_identity_configured": true,
    "classic_aoa_handshake_proven": false,
    "host_bulk_absent_proven": false,
    "bcl_on_usb_proven": false
  }
}
```

Das wäre deutlich belastbarer als ein einzelnes:

```json
"connected": true
```

---

# Priorisierte Umsetzungsreihenfolge

## Phase 1: Sofort umsetzbar

1. `connected_` in `vendor_mounted_` oder `usb_function_mounted_` umbenennen.
2. `application_peer_proven = false` ergänzen.
3. `hypothesis_respond_` in P0 standardmäßig auf `false` setzen.
4. `aoa_control_count` als Legacy markieren.
5. `P0_B` in `P0_B_aoa_entry_callback` und `P0_B_audio_callback` aufteilen.
6. `P0_C`/`P0_D` sprachlich auf lokale Beobachtung begrenzen.
7. `push()`-Fehler und Queue-Drops explizit als Capture-Invalidierungsgrund markieren.
8. EventBus-/TCP-/Overflow-Health in den Sessionabschluss aufnehmen.

## Phase 2: Messmethodik stabilisieren

9. Control-Callback-, Enqueue- und Drop-Counter trennen.
10. USB-Epoch und Session-Sequenzen ergänzen.
11. `hypothesis_handled` in `device_response_queued` semantisch ergänzen.
12. Observe- und Responder-Modus technisch und im Status trennen.
13. Re-Enumeration als nicht implementiert und nicht beobachtet explizit ausweisen.
14. Deskriptor-Snapshot pro Boot und USB-Epoch exportieren.

## Phase 3: Instrumentierung vertiefen

15. EP0-SETUP-Hook im TinyUSB-Core implementieren.
16. Standard/Class/Vendor-Dispatch protokollieren.
17. Data-/Status-/Completion-Phasen ergänzen.
18. binären Rohtrace unabhängig von EventBus/TCP einführen.
19. Ringpuffer concurrency-sicher machen.
20. Host-Trace-Korrelation mit Zeitstempeln und USB-Epoch vorbereiten.

## Phase 4: Externe Validierung

21. Host-Trace für jeden Negativbefund verpflichtend machen.
22. Known-good- und Problem-Lauf vergleichen.
23. Bei verbleibenden Widersprüchen Hardware-Analyzer einsetzen.
24. Erst danach historische Aussagen zu `51/52/53`, Bulk und Re-Enumeration neu bewerten.

---

# Klare Regeln für zukünftige Reports

## Zulässige Aussage mit aktuellem Instrument

> „Request 51 wurde im `USBVendor.onRequest()`-Callback nicht beobachtet.“

## Nicht zulässige Aussage mit aktuellem Instrument

> „Die HU hat Request 51 nicht gesendet.“

## Zulässige Aussage

> „Im lokalen Device-Vendor-RX-Pfad wurden im Zeitraum T keine Bytes empfangen.“

## Nicht zulässige Aussage

> „Auf dem USB-Bus gab es keinen Bulk-Verkehr.“

## Zulässige Aussage

> „Die USBVendor-Funktion war auf dem Device gemountet.“

## Nicht zulässige Aussage

> „Die BMW-Connected-App war verbunden.“

---

# Schlussbewertung

Der aktuelle C1b-P0/P0.5-Code ist als kontrolliertes **AOA-/Vendor-Observe-Gadget** sinnvoll aufgebaut. Besonders gut sind:

- MSC-Isolation im AOA-Observe-Gadget [12][15],
- Trennung von Raw-Bulk und BCL-Framer [1][3],
- gated TX über `enableWireTx()` [9][10],
- explizite Kennzeichnung des BCL-Framers als synthetisch beziehungsweise hypothesenbasiert [4][6][7],
- vorhandene Drop- und Capture-Health-Zähler [14][18][19].

Die wichtigsten Korrekturen sind jedoch:

1. **P0 muss tatsächlich passiv sein.**
2. **`connected_` darf nicht als Peer-Verbindung gelten.**
3. **`aoa_control_count` muss als Vendor-Callback-Zähler benannt werden.**
4. **P0-B muss Callback-Sichtbarkeit statt AOA-Erfolg ausdrücken.**
5. **Queue- und EventBus-Verluste müssen einen Lauf als negativbefunduntauglich markieren.**
6. **Für vollständige EP0-Aussagen ist ein TinyUSB-Core-Hook erforderlich.**
7. **Für belastbare Negativbefunde zu Hostrequests und Bulk ist ein Host-Trace Pflicht.**

Die wichtigste Zieldefinition für das Instrument lautet daher:

> **Das Gerät soll nicht Connected-App-Funktionalität simulieren, sondern eindeutig dokumentieren, welche Ereignisse auf welcher Beobachtungsebene sichtbar waren — und welche Aussagen daraus ausdrücklich nicht abgeleitet werden dürfen.**

---

# Repo-Abgleich (nicht Teil des gelieferten Plans)

Geprüft gegen `main` @ `849f30e`. Keine der empfohlenen Firmware-Änderungen ist in diesem Commit umgesetzt. Die drei Begrenzungen im Gesamturteil stehen so im Code.

| Stelle im Plan | Stand `849f30e` |
|---|---|
| `connected_` aus `vendorMounted()` | `AoaTransport::arm()` und `poll()` setzen das, wenn `observe_bound_` und `vendorMounted()` und im Poll zusätzlich `!lab_link_`. `vendorMounted()` ist `ready_ && g_vendor.mounted()`. Zusätzlich setzen `injectRx()` und `setConnectedForLab()` `connected_` mit `lab_link_=true`. Das ist ein Laborpfad, kein HU-Peer. |
| `hypothesis_respond_` immer an | `AoaControlTap::hypothesis_respond_` default `true`. `AoaObserveGadget::begin()` ruft für Accessory und Phone-like `setHypothesisRespond(true)` auf. Es gibt nur `AoaIdentityProfile::{Accessory,PhoneLike}`, keinen dritten Responder-Modus. Bulk-TX bleibt davon getrennt: Default `p0_tx_disabled`, `sendKnock()` schaltet `enableWireTx` nur um den einen `write()`. |
| „P0 erzeugt keinen aktiven Protokollverkehr“ [1][3] | `ConnectedAppProbe.h` sagt, BCL-on-wire werde in P0 nicht automatisch eingespeist. Der Live-Pfad ruft `drainTransportRaw()` auf. Das gilt für BCL und Default-TX. Es gilt nicht für die Vendor-Antworten 51/52/53/58. Der Multi-Review §2.2 („kein Protokollspielen“) ist an dieser Stelle zu weit. |
| `aoa_control_count` | Status schreibt bereits `setup_count`, `vendor_setup_count` und `aoa_control_count = vendorSetupCount()`, mit Kommentar „Legacy“. Die Sichtbarkeitsfelder `visibility_scope` / `is_full_ep0_trace` fehlen. Request 58 hat keinen eigenen Zähler. |
| `P0_B` | Seit `4c1d6d2` nur bei 51/52/53/58. `docs/GATES-Connected.md` sagt dasselbe und: Absent bei Direct-`2D00` ist kein AOA-FAIL. Die Aufteilung Entry gegen Audio und das Scope-Feld fehlen. |
| Zähler vor `push()` | So im Tap. Ein verworfener 51 erhöht `aoa51_` und `drops_`, erscheint aber nicht als JSON-Sample. `kQueueDepth` ist 32. Es gibt kein `capture_incomplete_`. |
| `capture_health` | `App::emitCaptureHealth()` publiziert bereits periodisch (`ev=capture_health`, Intervall 5 s, bei USB-Host verdoppelt) `dropped`, `published`, `stream_lost`, `tcp_tx_fail`, `queue_highwater`, `queue_pending`, Sequenzfenster. Nicht darin: Control-Tap-Drops, `rx_overflow` des AOA-Transports, `capture_valid`, ein Pflicht-Event beim Session-Ende. `transport_epoch` in diesem Event kommt von `MscGadget`, nicht von `AoaObserveGadget::transportEpoch()`. |
| USB-Epoch | `attach_count_` steigt bei `ARDUINO_USB_STARTED_EVENT`. `transport_epoch_` steigt dort und zusätzlich in `softAttach()`. `notifyUsbStopped()` erhöht die Epoch nicht. |
| Stage-Enum, im Plan nicht genannt | Lab gegen `aoa_phone_like`: Host-51 → STALL, `setup_count=0`. Hochwahrscheinlich TinyUSB `CONTROL_STAGE_SETUP=1` gegen Arduino `REQUEST_STAGE_SETUP=0`. Der MSC-Tap vergleicht bereits die TinyUSB-Konstante. Umbenennen der Zähler und `hypothesis_respond_=false` beheben diese Blindstelle nicht. Sie gehört vor die Auswertung von Callback-Zählern, zusammen mit dem EP0-Hook in Phase 3. [`2026-10-30-lab-aoa-aaidrive-phone-host.md`](2026-10-30-lab-aoa-aaidrive-phone-host.md) |
| Post-53, VID/PID, MSC, BCL, SPP | Kommentar `post-53 VID re-enum NOT IMPLEMENTED`. Accessory-Boot `0x18D1:0x2D00`, Phone-like `0x04E8:0x6860`. `mscInterfacePresent()` ist `false`. P0-C publiziert die selbst gesetzten IDs mit `direct_observation=false`. `SppTransport::implLevel()` ist `"stub"`. |
