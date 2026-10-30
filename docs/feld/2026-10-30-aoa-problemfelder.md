# Problemfelder: hu-probe / USB-Zubehör-Weg (AOA)

**Projekt:** hu-probe  
**Datum:** 2026-10-30  
**Repo-Basis:** `main` @ `849f30e`  
**Methode:** nur die Problemfelder aus dem Gesamtbericht, ohne externe Recherche  
**Quelle:** [`2026-10-30-gesamtbericht-aoa-zubehoer-multireview.md`](2026-10-30-gesamtbericht-aoa-zubehoer-multireview.md)  
**Lab-Bericht:** [`2026-10-30-lab-aoa-aaidrive-phone-host.md`](2026-10-30-lab-aoa-aaidrive-phone-host.md)  
**Instrumentierung:** [`2026-10-29-c1b-p05a-instrumentation.md`](2026-10-29-c1b-p05a-instrumentation.md)  
**Gesamtbewertung der fünf Reports:** [`2026-10-30-gesamtbewertung-aoa-fuenf-reports.md`](2026-10-30-gesamtbewertung-aoa-fuenf-reports.md)  
**Research-Reports:** [1](2026-10-30-research-01-aoa-direct-accessory.md), [2](2026-10-30-research-02-usb-sniffing-gadget-validation.md), [3](2026-10-30-research-03-esp32-tinyusb-ep0-callbacks.md), [4](2026-10-30-research-04-composite-vs-vendor-only.md) und [5](2026-10-30-research-05-bcl-etch-nbt.md) abgelegt. Die fünf Blöcke stehen unten.

> **Ablagehinweis zum Stand `849f30e`:** Der Text ab Abschnitt 1 ist die gelieferte Filterung. Drei Formulierungen beschreiben diesen Commit nicht mehr als offenen Codefehler.
>
> 1. **P0-B (Thema 2, Matrix „konkret fehlerhaft“).** Seit `4c1d6d2` (FW 0.3.28) setzt `ConnectedAppProbe::drainControlTap()` `p0_b_` nur bei `bRequest` 51, 52, 53 oder 58. Der Pre-Field-Fix ist im Repo.
> 2. **`stage` fehlt (Thema 1).** `drainControlTap()` schreibt `stage` und `request_type` in jedes `ca_usb`-Event. Jedes `REQUEST_STAGE_SETUP`, das `USBVendor.onRequest` erreicht, wird gepusht, auch Nicht-Vendor. Offen bleibt, dass `AoaControlTap` kein vollständiger EP0-Sniffer ist und dass nicht belegt ist, welche Requests der Stack bis zum Callback durchreicht (Themen 1 und 5).
> 3. **MSC-Leak (Thema 6).** Der Composite-Leak ist in FW 0.3.27 behoben (`105c791`: `USBMSC` erst in `MscGadget::begin()`). Die methodische Einschränkung gilt für Sessions vor diesem Fix.

---

## 1. Herausgefilterte Themenbereiche mit Problemen / offenen Punkten

### 1. Unvollständige USB-Control-Sicht / Observability-Lücke

**Problem:**  
Der aktuelle `AoaControlTap` ist **kein vollständiger USB-Control-/EP0-Sniffer**. Er ist primär auf **Vendor-Control** ausgelegt und verwirft bzw. publiziert anderes nur eingeschränkt.

**Konkrete Symptome aus dem Bericht:**

- Vendor-Control ist beobachtbar
- vollständiger USB-Control-Trace fehlt
- `stage` fehlt in der Publikation
- Nicht-Vendor-SETUPs sind bisher beobachtungsseitig unzureichend sichtbar
- Toolchain-/Core-Routing bis `USBVendor.onRequest()` ist nicht vollständig repo-seitig bewiesen

**Wahrscheinliche Suchfelder später:**

- TinyUSB / Arduino USBVendor request callback behavior
- vollständiges EP0 tracing
- ESP32-S3 USB gadget control request instrumentation

---

### 2. P0-B-Semantikfehler

**Problem:**  
`P0_B` wird aktuell offenbar **zu breit** gesetzt, nämlich durch jedes gepoppte Control-Sample, statt nur durch wirklich AOA-relevante Requests.

**Konkrete Symptome:**

- bei breiterer Instrumentierung würde `P0_B` semantisch kippen
- AOA-spezifische Aussagekraft würde verwässert
- kleiner Pre-Field-Fix nötig

**Wahrscheinliche Suchfelder später:**

- AOA request classification
- sinnvolle Semantik für Accessory-handshake-Indikatoren
- Request 51/52/53/58 Interpretation

---

### 3. Accessory erkannt, aber Connected-/Bulk-Kanal unbewiesen

**Problem:**  
Die HU erkennt die Probe als Accessory, aber der **eigentliche Connected-/BCL-Kanal** ist noch nicht gezeigt.

**Konkrete Symptome:**

- Bulk RX = 0
- Accessory-Erkennung ist belegt
- Connected-App Reachability bleibt offen
- kein Nachweis, dass nach Enum wirklich ein Connected/BCL-Pfad entsteht

**Wahrscheinliche Suchfelder später:**

- Android Open Accessory Bulk behavior
- Host/device first traffic patterns
- BMW Connected App USB transport observations

---

### 4. Direct-2D00-Boot erschwert Interpretation von 51/52/53

**Problem:**  
Da direkt als `0x18D1:0x2D00` gebootet wird, ist **Absenz von 51/52/53** nicht mehr stark diagnostisch.

**Konkrete Symptome:**

- `51/52/53 = 0` ist gemessen
- aber bei Direct-Accessory-Boot erwartbar
- daher kein valider Gegenbeweis gegen AOA
- die Aussagekraft von P0-B/P0-Control muss präziser gefasst werden

**Wahrscheinliche Suchfelder später:**

- AOA direct accessory mode behavior
- 18D1:2D00 enumeration without classic handshake
- post-handshake vs pre-handshake AOA traces

---

### 5. Instrument-Validierung / Callback-Abdeckung ungeklärt

**Problem:**  
Es bleibt offen, **welche Requests der reale Stack** in eurer Build-Umgebung tatsächlich bis `USBVendor.onRequest()` durchreicht.

**Konkrete Symptome:**

- repo-seitig plausibel, aber nicht vollständig bewiesen
- Lab-check gegen Host fehlt bzw. war blockiert
- dadurch bleibt Restunsicherheit, ob „nicht gesehen“ = „nicht gesendet“

**Wahrscheinliche Suchfelder später:**

- TinyUSB control callback coverage
- ESP32 Arduino USBVendor internals
- host-side USB monitoring for gadget validation

---

### 6. MSC-Leak / frühere Composite-Verfälschung

**Problem:**  
Frühere Feldbefunde sind **methodisch verunreinigt**, weil das frühere Setup offenbar unbeabsichtigt **MSC+Vendor-Composite** war.

**Konkrete Symptome:**

- alte Session muss als composite-caveated gelesen werden
- Vergleich alt/neu wird wichtig
- UI-/HU-Reaktion könnte auf MSC statt auf AOA/Vendor reagiert haben

**Wahrscheinliche Suchfelder später:**

- BMW HU behavior with MSC vs vendor-only USB devices
- composite gadget side effects
- Android Auto / AOA / MSC coexistence observations

---

### 7. Reachability-/Identity-Problem statt reines AOA-Problem

**Problem:**  
Der eigentliche Engpass könnte **nicht der AOA-Handshake**, sondern **phone-like identity / Connected-App-Reachability** sein.

**Konkrete Symptome:**

- B1 `aoa_phone_like` ist jetzt der sinnvolle nächste Test
- Vendor-only reicht evtl. nicht
- HU könnte nur auf „telefonähnliche“ Fingerprints in den Connected-Pfad wechseln

**Wahrscheinliche Suchfelder später:**

- phone-like USB descriptors for automotive hosts
- BMW Connected app USB identity checks
- smartphone fingerprinting by host HU

---

### 8. BCL-on-Bulk weiterhin unbewiesen

**Problem:**  
Desk-/Audit-Wissen deutet auf BCL mit 8-Byte-Header hin, aber **USB-Bulk am NBT ist damit nicht bewiesen**.

**Konkrete Symptome:**

- `BclFramer` existiert, wird in P0 aber live nicht benutzt
- B2 bleibt Messung, kein Claim
- kein Nachweis, dass NBT-Bulk tatsächlich diese Struktur trägt

**Wahrscheinliche Suchfelder später:**

- BMW BCL framing
- Etch / dest 0x0FA4
- reverse engineering of BMW Connected USB transport

---

### 9. Referenz-Capture fehlt

**Problem:**  
Es fehlt ein belastbarer **Referenzmitschnitt mit echtem Phone + Connected-App + NBT Evo**.

**Konkrete Symptome:**

- unklar, wer zuerst sendet
- unklar, ob 51/52/53 in der Referenz wirklich auftauchen
- unklar, welche Strings / Bulk-Muster / Re-Enum-Sequenzen normal sind

**Wahrscheinliche Suchfelder später:**

- captured AOA sessions
- phone-to-car USB traces
- accessory mode reverse-engineering repos

---

## 2. Strukturierte Problem-Matrix

| Themenbereich | Problemtyp | Status im gelieferten Filter | Stand `849f30e` |
|---|---|---|---|
| USB-Control-Observability | Instrumentierung unvollständig | offen | offen als EP0-Lücke; `stage` wird publiziert |
| P0-B-Semantik | falscher Trigger / Messsemantik | konkret fehlerhaft | behoben in FW 0.3.28 (`4c1d6d2`) |
| Connected/Bulk-Kanal | Nachweis fehlt | offen | offen |
| 51/52/53-Interpretation | diagnostisch eingeschränkt | offen | offen |
| Callback-Abdeckung | Toolchain-/Stack-Routing unklar | offen | offen |
| MSC-Leak alt | alte Evidenz verfälscht | methodisch kritisch | Leak in FW 0.3.27 behoben; Alt-Sessions bleiben caveated |
| Phone-like Reachability | Identität evtl. entscheidend | offen | Boot `aoa_phone_like` vorhanden; Feld/Lab-Nachweis offen |
| BCL-on-Bulk | Protokoll auf Wire unbewiesen | offen | offen |
| Referenz-Capture | Vergleichsbasis fehlt | offen | offen; Lab-Host ersetzt nur die Phone-Hälfte |

Die Spalte „Status im gelieferten Filter“ ist der Originaltext. Die letzte Spalte ist der Abgleich mit dem Repo.

---

## 3. Geplante Rechercheblöcke

Die Internet-Recherche wäre in diese 5 Blöcke geschnitten:

1. **AOA / Direct-Accessory / Request 51/52/53/58** — [`2026-10-30-research-01-aoa-direct-accessory.md`](2026-10-30-research-01-aoa-direct-accessory.md)
2. **ESP32-S3 / TinyUSB / Arduino USBVendor / EP0-Control-Callbacks** — [`2026-10-30-research-03-esp32-tinyusb-ep0-callbacks.md`](2026-10-30-research-03-esp32-tinyusb-ep0-callbacks.md)
3. **USB-Sniffing / Host-vs-Device Control Trace / Gadget Validation** — [`2026-10-30-research-02-usb-sniffing-gadget-validation.md`](2026-10-30-research-02-usb-sniffing-gadget-validation.md)
4. **BMW Connected / BCL / Etch / NBT Evo Reverse Engineering** — [`2026-10-30-research-05-bcl-etch-nbt.md`](2026-10-30-research-05-bcl-etch-nbt.md)
5. **Composite vs vendor-only behavior in automotive USB hosts** — [`2026-10-30-research-04-composite-vs-vendor-only.md`](2026-10-30-research-04-composite-vs-vendor-only.md)

### Priorisierung für die spätere Recherche

1. **TinyUSB / Arduino / USBVendor callback coverage** — [`2026-10-30-research-03-esp32-tinyusb-ep0-callbacks.md`](2026-10-30-research-03-esp32-tinyusb-ep0-callbacks.md)
2. **AOA direct 2D00 behavior and meaning of absent 51/52/53** — [`2026-10-30-research-01-aoa-direct-accessory.md`](2026-10-30-research-01-aoa-direct-accessory.md)
3. **BMW Connected / BCL / NBT Evo USB reverse engineering** — [`2026-10-30-research-05-bcl-etch-nbt.md`](2026-10-30-research-05-bcl-etch-nbt.md)
4. **Composite MSC side effects on automotive hosts** — [`2026-10-30-research-04-composite-vs-vendor-only.md`](2026-10-30-research-04-composite-vs-vendor-only.md)
5. **Reference traces / AOA sniffing tools** — [`2026-10-30-research-02-usb-sniffing-gadget-validation.md`](2026-10-30-research-02-usb-sniffing-gadget-validation.md)
