# Technischer Research-Report

## AOA / Direct-Accessory / Requests 51, 52, 53, 58

### Diagnosewert von `18D1:2D00` und Bedeutung fehlender `51/52/53` bei Direct-Accessory-Boot

**Projekt:** hu-probe  
**Datum:** 2026-10-30  
**Reihe:** Research-Report 1 von 5  
**Problemfelder:** [`2026-10-30-aoa-problemfelder.md`](2026-10-30-aoa-problemfelder.md) — Block 1, Thema 4  
**Gesamtbericht:** [`2026-10-30-gesamtbericht-aoa-zubehoer-multireview.md`](2026-10-30-gesamtbericht-aoa-zubehoer-multireview.md)  
**Charakter:** gelieferte Recherche-Notiz. Exakte URLs, AOSP-Pfade und die offizielle Formulierung zu Request 58 sind im Text selbst als noch live zu prüfen markiert. [Q8] (externe Praxistraces) ist offen.

**Repo-Anker, die [Q8] repo-intern schon teilweise füllen:**

| Befund | Wo |
|--------|----|
| Lab: `51→52→53`, Re-Enum `18d1:2d00` in ~0,5–0,6 s | [`2026-10-30-lab-aoa-aaidrive-phone-host.md`](2026-10-30-lab-aoa-aaidrive-phone-host.md) |
| Feld: fehlende 51–53 bei Already-Accessory `18D1:2D00` widerlegen AOA nicht | [`2026-10-29-c1b-b2b-knock-feld.md`](2026-10-29-c1b-b2b-knock-feld.md) |
| Gate-Formulierung: `51/52/53=0` bei `18D1:2D00` ≠ AOA-FAIL | [`../GATES-Connected.md`](../GATES-Connected.md) |
| Code: Request 58 als AOAv2 `SET_AUDIO_MODE` kommentiert, Desk-Evidenz `EVID-AOA-2026-10-28-002..004` | `firmware/src/connected/aoa/AoaControlTap.cpp` |

---

## 1. Ziel der Untersuchung

Ziel dieser Untersuchung ist die technische Einordnung eines USB-Verhaltens im Kontext von **Android Open Accessory (AOA)**, insbesondere in Fällen, in denen ein Gerät bzw. Kommunikationspartner **direkt als `VID 0x18D1 / PID 0x2D00`** enumeriert.

Im Zentrum stehen die Fragen,

- was die AOA-Requests **51 / 52 / 53 / 58** bedeuten,
- wie der klassische AOA-Handshake abläuft,
- wie `18D1:2D00` technisch zu interpretieren ist,
- und ob das **Fehlen von 51/52/53** bei bereits sichtbarem `18D1:2D00` als Gegenbeweis gegen AOA taugt.

---

## 2. Quellenbasis

### [Q1] Android Developers – Android Open Accessory

**Typ:** Offizielle Primärquelle  
**Typische Fundstelle:** Android Developers, Abschnitt zu Android Open Accessory / Accessory Development  
**Sicherheitsniveau:** sehr wahrscheinlich korrekt  
**Belegwert:** hoch

**Erwarteter Inhalt:**

- Grundprinzip von Android Open Accessory
- Rollenmodell Accessory ↔ Android-Gerät
- Accessory-Mode
- Verweis auf USB-Handshake und Identifikationsstrings

> Anmerkung: Die offizielle Android-Developer-Dokumentation zu AOA ist seit Jahren bekannt, auch wenn sich URL-Strukturen über Android-Versionen geändert haben.

---

### [Q2] AOSP / `adb/adb.h` bzw. Android accessory constants

**Typ:** Offizielle bzw. quasi-offizielle Codequelle  
**Typische Fundstelle:** AOSP-Header mit Accessory-Request-Konstanten  
**Sicherheitsniveau:** sehr wahrscheinlich korrekt, aber Dateipfad bitte live prüfen  
**Belegwert:** sehr hoch

**Typisch bekannte Konstanten:**

- `ACCESSORY_GET_PROTOCOL` = `51`
- `ACCESSORY_SEND_STRING` = `52`
- `ACCESSORY_START` = `53`

In vielen öffentlichen AOSP-/Android-bezogenen Codefragmenten tauchen genau diese Konstanten auf, häufig in dieser Form:

```c
#define ACCESSORY_GET_PROTOCOL 51
#define ACCESSORY_SEND_STRING  52
#define ACCESSORY_START        53
```

**Einordnung:**  
Diese Quelle ist zentral für die technische Definition von 51/52/53.

---

### [Q3] AOSP / USB accessory documentation oder accessory sample code

**Typ:** Offizielle/quasi-offizielle Code- oder Beispieldoku  
**Typische Fundstelle:** Android accessory sample / USB accessory demo code  
**Sicherheitsniveau:** sehr wahrscheinlich korrekt  
**Belegwert:** hoch

**Erwarteter Inhalt:**

- Ablauf `GET_PROTOCOL` → `SEND_STRING` → `START`
- Accessory Strings:
  - manufacturer
  - model
  - description
  - version
  - uri
  - serial

Typischerweise sind auch die String-Indizes dokumentiert, etwa:

- 0 = manufacturer
- 1 = model
- 2 = description
- 3 = version
- 4 = URI
- 5 = serial

---

### [Q4] Linux USB ID definitions / AOSP USB PID mapping

**Typ:** Codequelle / systemnahe Referenz  
**Typische Fundstelle:** Linux-Header, Android-Header oder USB-ID-Tabelle  
**Sicherheitsniveau:** sehr wahrscheinlich korrekt  
**Belegwert:** hoch

**Typisch bekannte PID-Zuordnung:**

- `18D1:2D00` = accessory
- `18D1:2D01` = accessory + adb
- `18D1:2D02` = audio
- `18D1:2D03` = audio + adb
- `18D1:2D04` = accessory + audio
- `18D1:2D05` = accessory + audio + adb

**Status:**  
Diese Tabelle ist in der Android-/Linux-USB-Welt sehr bekannt und taucht in mehreren Codebasen auf.  
**Vor Zitierung bitte live gegenprüfen.**

---

### [Q5] AOSP / zusätzliche AOA-Requests inkl. HID und Audio

**Typ:** Offizielle/quasi-offizielle Codequelle  
**Typische Fundstelle:** Header mit erweiterten Accessory Requests  
**Sicherheitsniveau:** wahrscheinlich korrekt, aber unbedingt live prüfen  
**Belegwert:** sehr hoch, falls bestätigt

**Typisch bekannte erweiterte Requests:**

- `54` = register HID
- `55` = unregister HID
- `56` = set HID report descriptor
- `57` = send HID event
- `58` = set audio mode

**Wichtige Einordnung:**  
Wenn diese Zuordnung bestätigt wird, dann ist **Request 58 sehr wahrscheinlich `ACCESSORY_SET_AUDIO_MODE`**.

Das wäre für die Fragestellung zentral, denn dann gilt:

- `51/52/53` = klassischer Accessory-Eintrittspfad
- `58` = Audio-bezogene Erweiterungsfunktion in AOA 2.0-Kontext

Das passt exakt zur fachlichen Voranalyse, dass `58` **nicht** dieselbe Rolle wie `51/52/53` hat.

---

### [Q6] Android Open Accessory Protocol 2.0 Documentation

**Typ:** Offizielle Primär- oder Sekundärquelle  
**Typische Fundstelle:** Android Developers / AOA 2.0  
**Sicherheitsniveau:** sehr wahrscheinlich existent, bitte live prüfen  
**Belegwert:** sehr hoch

**Erwarteter Inhalt:**

- AOA 2.0 Erweiterungen
- Audio support
- HID support
- zusätzliche Requests
- zusätzliche PIDs

---

### [Q7] Open-Source-Implementierungen: Microbridge / Arduino ADK / USB Host Shield

**Typ:** Etablierte Codequellen  
**Sicherheitsniveau:** hoch als Implementierungsbeleg, aber nicht Primärnorm  
**Belegwert:** mittel bis hoch

**Typisch relevante Projekte/Fundstellen:**

- Microbridge / Arduino accessory libraries
- USB Host Shield 2.0 Android accessory support
- Embedded-Libraries mit `ACCESSORY_GET_PROTOCOL`, `SEND_STRING`, `START`

**Belegwert:**

- sehr gut für reale Implementierung
- gut für Reihenfolge und Control-Transfer-Aufbau
- nicht allein ausreichend für normative Aussagen

---

### [Q8] Praxisquellen: Wireshark/usbmon/Foren/GitHub Issues

**Typ:** Praxisbeleg  
**Sicherheitsniveau:** variabel  
**Belegwert:** mittel, aber sehr wertvoll für Diagnose

**Gesuchter Nachweis:**

- Capture mit `51 → 52 → 53 → re-enum → 18d1:2d00`
- Capture/Fall, in dem ein Gerät direkt als `18d1:2d00` erscheint
- Aussage, dass bei bereits aktivem Accessory-Mode die Umschaltrequests nicht erneut sichtbar sein müssen

---

# 3. Technische Bewertung mit eingetragenen Quellenankern

## 3.1 Bedeutung der Requests 51, 52, 53

Die Requests 51, 52 und 53 sind die Kernbestandteile des klassischen Android Open Accessory Initialisierungs- und Umschaltpfads.

### Request 51

**Definition:** `ACCESSORY_GET_PROTOCOL`  
**Wert:** `51`  
**Quelle:** [Q2], [Q3]  
**Bedeutung:** Abfrage der vom Android-Gerät unterstützten Accessory-Protokollversion.

### Request 52

**Definition:** `ACCESSORY_SEND_STRING`  
**Wert:** `52`  
**Quelle:** [Q2], [Q3]  
**Bedeutung:** Übertragung der Accessory-Identifikationsstrings an das Android-Gerät.

### Request 53

**Definition:** `ACCESSORY_START`  
**Wert:** `53`  
**Quelle:** [Q2], [Q3]  
**Bedeutung:** Veranlasst den Wechsel des Android-Geräts in den Accessory-Mode.

Diese Dreierfolge ist in Android-/AOSP-nahen Implementierungen seit langem etabliert und bildet den klassischen AOA-Einstiegspfad.  
**Quellen:** [Q1], [Q2], [Q3]

---

## 3.2 Bedeutung von Request 58

Nach bekannter AOA-2.0-Systematik ist Request 58 **sehr wahrscheinlich**:

### Request 58

**Definition:** `ACCESSORY_SET_AUDIO_MODE`  
**Wert:** `58`  
**Quelle:** [Q5], [Q6]  
**Sicherheitsvermerk:** live gegenprüfen

Wenn das bestätigt wird, ist die Diagnose eindeutig:

- `58` ist **kein** Basisrequest des klassischen Initialhandshakes
- `58` gehört zu **erweiterten AOA-2.0-Funktionen**, insbesondere Audio
- damit ist `58` funktional anders zu bewerten als `51/52/53`

Das würde direkt die zentrale Diagnosefrage beantworten:  
**Request 58 ist kein Ersatzindikator für einen fehlenden klassischen 51/52/53-Handshake.**

---

## 3.3 Klassischer AOA-Ablauf

Der klassische Ablauf ist nach Android-/AOSP-Logik wie folgt:

1. Android-Gerät enumeriert zunächst in einem normalen USB-Zustand
2. Accessory sendet `ACCESSORY_GET_PROTOCOL` (`51`)
3. Accessory sendet mehrere `ACCESSORY_SEND_STRING` (`52`)
4. Accessory sendet `ACCESSORY_START` (`53`)
5. Android-Gerät re-enumeriert als Google-AOA-Device
6. Danach erscheint typischerweise `VID 18D1 / PID 2D00` oder eine verwandte AOA-PID

**Quellen:** [Q1], [Q2], [Q3], [Q4]

---

## 3.4 Bedeutung von `18D1:2D00`

Nach bekannter Android-/Linux-USB-Zuordnung gilt:

- `18D1` = Google Vendor ID
- `2D00` = AOA accessory

**Quelle:** [Q4]

Verwandte Modi sind typischerweise:

- `2D01` = accessory + adb
- `2D02` = audio
- `2D03` = audio + adb
- `2D04` = accessory + audio
- `2D05` = accessory + audio + adb

**Quelle:** [Q4], [Q6]

Daraus folgt:

> Wenn ein Gerät bereits als `18D1:2D00` enumeriert, ist das ein starker Hinweis darauf, dass der Accessory-Mode bereits aktiv ist.

---

## 3.5 Bewertung von Direct-Accessory-Boot

Wenn ein Gerät direkt als `18D1:2D00` erscheint, gibt es mindestens drei technisch plausible Erklärungen:

1. Das Gerät startet bereits im Accessory-Mode
2. Der Umschaltprozess ist vor Beginn des Mitschnitts erfolgt
3. Der Umschaltprozess wurde in einem anderen Pfad/Bootstage vollzogen

In allen drei Fällen gilt:

- Das Ausbleiben von `51/52/53` im aktuellen Trace ist **nicht überraschend**
- Denn `51/52/53` gehören zum **Eintritt in den Modus**, nicht zwingend zum **Betrieb im Modus**

**Quellenbasis dafür:**

- Protokollmodell: [Q1], [Q2], [Q3], [Q4]
- Praxisbelege: [Q8] noch zu ergänzen

---

## 3.6 Diagnosewert von `51/52/53 = 0`

Die Aussage

> „`51/52/53` wurden nicht beobachtet“

ist **nicht äquivalent** zu

> „AOA liegt nicht vor“.

Stattdessen ist technisch sauberer zu sagen:

> Es wurde kein klassischer AOA-Umschaltschritt beobachtet.  
> Wenn jedoch gleichzeitig `18D1:2D00` sichtbar ist, bleibt AOA als Erklärung ausdrücklich konsistent und wahrscheinlich.

Diese Schlussfolgerung stützt sich auf die Unterscheidung zwischen:

- **Transition Evidence**: `51/52/53`
- **State Evidence**: `18D1:2D00`

**Quellen:** [Q1]–[Q4], ergänzend [Q8]

---

# 4. Vorläufiges Fazit mit Quellenankern

Auf Basis der bekannten Android-/AOSP-Quellenlage ergibt sich folgende technisch belastbare Bewertung:

1. **Request 51** ist `ACCESSORY_GET_PROTOCOL`.  
   **Quelle:** [Q2], [Q3]

2. **Request 52** ist `ACCESSORY_SEND_STRING`.  
   **Quelle:** [Q2], [Q3]

3. **Request 53** ist `ACCESSORY_START`.  
   **Quelle:** [Q2], [Q3]

4. **Request 58** ist sehr wahrscheinlich `ACCESSORY_SET_AUDIO_MODE` und damit Teil erweiterter AOA-2.0-Funktionalität, nicht des Basis-Handshake-Pfads.  
   **Quelle:** [Q5], [Q6] — live verifizieren

5. `VID 18D1 / PID 2D00` bezeichnet sehr wahrscheinlich den AOA-Accessory-Mode.  
   **Quelle:** [Q4]

6. Ein Gerät, das bereits direkt als `18D1:2D00` enumeriert, ist daher mit hoher Wahrscheinlichkeit bereits im Accessory-Mode.

7. In einem solchen Fall ist das Fehlen von `51/52/53` **kein valider Gegenbeweis gegen AOA**, sondern nur ein Hinweis darauf, dass der **klassische Umschaltpfad im beobachteten Trace nicht sichtbar war**.

---

# 5. Quellenliste in fast zitierfähiger Form

- **[Q1]** Android Developers, *Android Open Accessory*.  
  Offizielle Übersicht zu AOA, Accessory-Mode und Entwicklungsmodell.  
  **Status:** Quelle bekannt, URL live prüfen.

- **[Q2]** Android / AOSP Header mit Accessory-Request-Konstanten (`ACCESSORY_GET_PROTOCOL`, `ACCESSORY_SEND_STRING`, `ACCESSORY_START`).  
  **Status:** Fundstelle sehr wahrscheinlich korrekt, exakten Pfad live prüfen.

- **[Q3]** Android accessory sample code / AOSP accessory implementation examples.  
  Belegt die Reihenfolge 51 → 52 → 53 und die Accessory-Strings.  
  **Status:** Quelle sehr wahrscheinlich, konkrete Datei live prüfen.

- **[Q4]** Android-/Linux-nahe USB PID definitions for Google Accessory Mode (`18D1:2D00` ff.).  
  **Status:** sehr wahrscheinlich korrekt, final gegenprüfen.

- **[Q5]** AOSP / accessory request extensions (HID, audio), inkl. Request `58`.  
  **Status:** inhaltlich wahrscheinlich korrekt, unbedingt live verifizieren.

- **[Q6]** Android Open Accessory 2.0 documentation.  
  Belegt Audio-/HID-Erweiterungen und zusätzliche PIDs/Requests.  
  **Status:** live verifizieren.

- **[Q7]** Open-source implementations such as Arduino ADK / USB Host Shield / Microbridge.  
  Implementierungsbelege für reale Nutzung von 51/52/53.  
  **Status:** als Sekundär- und Praxisbeleg geeignet.

- **[Q8]** USB captures / Wireshark / usbmon / GitHub issues / technical forum traces.  
  Praxisbeleg für klassischen Handshake versus direkte `18D1:2D00`-Enumeration.  
  **Status:** noch konkret zu sammeln. Repo-intern liegen Lab-Handshake und der Feld-Schluss „Absent ≠ AOA-FAIL“ bereits vor (siehe Kopf).

---

# 6. Ehrliche Quellenlage

## Was hier mit hoher Sicherheit gesagt wird

- 51/52/53 = `GET_PROTOCOL` / `SEND_STRING` / `START`
- `18D1:2D00` ist der zentrale AOA-Accessory-Identifier
- `51/52/53` sind Transition-Requests, `18D1:2D00` ist Zustandsindikator
- fehlende `51/52/53` bei direktem `18D1:2D00` widerlegen AOA nicht

## Was ohne Live-Prüfung nicht als final zitierfähig gilt

- exakter aktueller AOSP-Dateipfad
- exakte aktuelle URL der Android-Developer-Seite
- exakte offizielle Textformulierung zu Request 58
- konkrete Praxistrace-Links
