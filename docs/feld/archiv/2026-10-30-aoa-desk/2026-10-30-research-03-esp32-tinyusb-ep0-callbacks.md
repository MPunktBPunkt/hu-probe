# Technischer Research-Report

## ESP32-S3 / TinyUSB / Arduino USBVendor / EP0-Control-Callbacks

### Callback-Abdeckung, Request-Routing und Diagnosewert von „nicht gesehen“

**Projekt:** hu-probe  
**Datum:** 2026-10-30  
**Reihe:** Research-Report 3 von 5  
**Problemfelder:** [`2026-10-30-aoa-problemfelder.md`](2026-10-30-aoa-problemfelder.md) — Block 2, Themen 1 und 5, Priorität 1  
**Research 1:** [`2026-10-30-research-01-aoa-direct-accessory.md`](2026-10-30-research-01-aoa-direct-accessory.md)  
**Research 2:** [`2026-10-30-research-02-usb-sniffing-gadget-validation.md`](2026-10-30-research-02-usb-sniffing-gadget-validation.md)  
**Framework im Repo:** `espressif32@6.4.0`, Arduino (`firmware/platformio.ini`). `USBVendor` liegt nicht im Baum, nur als `#include "USBVendor.h"`.  
**Charakter:** gelieferte Recherche-Notiz ohne Live-Webcrawl. Der Repo-Abgleich steht in Abschnitt 8.

---

## 1. Ziel der Untersuchung

Untersucht wurde, **welche USB-Control-Requests in einem ESP32-S3-Setup mit TinyUSB und Arduino-ESP32 tatsächlich bis zur Arduino-Ebene, insbesondere `USBVendor.onRequest()`, gelangen** und welche Requests vorher schon von tieferen Schichten verarbeitet oder absorbiert werden.

Für das konkrete Problem ist das wichtig, weil der aktuelle Beobachtungspunkt (`AoaControlTap`) offenbar **vendor-fokussiert** ist und daher möglicherweise **kein vollständiger EP0-/Control-Trace** liefert. Damit ist die zentrale Diagnosefrage:

- Bedeutet **„Request nicht gesehen“** tatsächlich **„Request wurde nicht gesendet“**?
- Oder bedeutet es nur **„dieser Request ist am aktuellen Instrumentierungspunkt nicht sichtbar“**?

Der praktische Kern ist damit die **Belastbarkeit von Negativbefunden** wie etwa `51/52/53 = 0` in einem AOA-/Vendor-Control-Kontext.

---

## 2. Quellenbasis

Hinweis zur Quellenlage:  
Die Notiz trennt zwischen:

- **sehr belastbaren Architektur- und Codeaussagen** aus bekannten Stack-Strukturen von TinyUSB / ESP-IDF / Arduino-ESP32,
- **wahrscheinlichen, aber live zu verifizierenden** Aussagen zur genauen Arduino-ESP32-Implementierung von `USBVendor.onRequest()`,
- **offenen Punkten**, die in der realen Codebasis geprüft werden müssen.

Ein Live-Webcrawl war nicht Teil dieser Notiz.

### Offizielle Doku / Architekturquellen

### [Q1] TinyUSB Device-Stack-Dokumentation und bekannte Callback-Architektur

**Typ:** Offizielle Primärquelle  
**Typische Fundstelle:** TinyUSB-Dokumentation / Beispiele / Device API  
**Sicherheitsniveau:** sehr wahrscheinlich korrekt  
**Belegwert:** hoch  
**Erwarteter oder belegter Inhalt:**

- TinyUSB verarbeitet Control-Transfers über EP0 im Device-Stack
- Standard-Requests werden zu erheblichen Teilen im Core behandelt
- Klassen- und vendor-spezifische Requests werden über Klassen-/Treiberpfade weitergereicht

**Einordnung:**  
Grundlage für die Frage, welche Requests überhaupt bis in anwendungsnahe Callbacks kommen.

### [Q2] ESP-IDF USB Device / TinyUSB Integration für ESP32-S3

**Typ:** Offizielle Primärquelle  
**Typische Fundstelle:** Espressif-Dokumentation zu TinyUSB Device Stack / USB Device Support für ESP32-S3  
**Sicherheitsniveau:** wahrscheinlich korrekt  
**Belegwert:** hoch  
**Erwarteter oder belegter Inhalt:**

- ESP32-S3 nutzt USB OTG/Device-Hardware mit TinyUSB als Device-Stack
- ESP-IDF integriert TinyUSB als Komponente/Wrapper
- Die USB-Protokollbehandlung oberhalb des Controllers liegt nicht direkt in Arduino, sondern in TinyUSB/ESP-IDF-Schichten

**Einordnung:**  
Wichtig für die Trennung zwischen Hardware-Controller, IDF-Integration und Arduino-Wrapper.

### [Q3] USB 2.0 Control Transfer Model

**Typ:** Normative Referenz / Architekturgrundlage  
**Typische Fundstelle:** USB 2.0 Spezifikation, Kapitel zu Control Transfers  
**Sicherheitsniveau:** sehr wahrscheinlich korrekt  
**Belegwert:** sehr hoch  
**Erwarteter oder belegter Inhalt:**

- EP0-Control-Transfer besteht aus SETUP, optional DATA, STATUS
- Request-Typen werden über `bmRequestType` klassifiziert: Standard / Class / Vendor
- Routing und Beantwortung hängen von Recipient und Request-Type ab

**Einordnung:**  
Notwendig, um sauber zwischen „SETUP gesehen“, „Data-Phase gesehen“ und „kompletter Transfer sichtbar“ zu unterscheiden.

---

### Codequellen / Repositories

### [Q4] TinyUSB Device Core – Control Dispatch

**Typ:** Codequelle / Repo  
**Typische Fundstelle:** TinyUSB-Repository, Device-Core-Dateien mit EP0- und Control-Request-Dispatch  
**Sicherheitsniveau:** sehr wahrscheinlich korrekt  
**Belegwert:** sehr hoch  
**Erwarteter oder belegter Inhalt:**

- Es gibt zentrale Dispatch-Logik für Control-Requests
- Standard-Requests werden im Core bzw. in generischen Device-Pfaden behandelt
- Klassen-/vendor-bezogene Requests werden über spezifische Handler oder Driver-Callbacks verteilt

**Einordnung:**  
Schlüsselquelle, um zu prüfen, ob `USBVendor.onRequest()` strukturell überhaupt alle Requests sehen könnte.

### [Q5] TinyUSB Vendor Class / Vendor Control Callback

**Typ:** Codequelle / Repo  
**Typische Fundstelle:** TinyUSB-Repository, Vendor-Class-Implementierung und zugehörige Callback-Definitionen  
**Sicherheitsniveau:** wahrscheinlich korrekt  
**Belegwert:** hoch  
**Erwarteter oder belegter Inhalt:**

- Vendor-Class-Handler exponieren typischerweise vendor-spezifische Requests
- Nicht-Vendor-Requests werden nicht automatisch in Vendor-Class-Callbacks gespiegelt
- Es existieren APIs/Helfer für Control-Daten-Transfers auf EP0

**Einordnung:**  
Sehr wichtig für die Frage, ob `USBVendor.onRequest()` semantisch bereits per Design eingeschränkt ist.

### [Q6] Arduino-ESP32 USBVendor Wrapper

**Typ:** Codequelle / Repo  
**Typische Fundstelle:** `arduino-esp32`-Repository, USB-Device-/TinyUSB-Wrapper-Dateien, `USBVendor`-Klasse  
**Sicherheitsniveau:** mittel bis wahrscheinlich korrekt  
**Belegwert:** hoch  
**Erwarteter oder belegter Inhalt:**

- `USBVendor` ist ein Arduino-Wrapper um TinyUSB-Funktionalität
- `onRequest()` wird vermutlich als Hook für vendor-spezifische Control-Requests bereitgestellt
- Es ist zu prüfen, ob Standard/Class-Requests dort überhaupt eintreffen können

**Einordnung:**  
Zentral für die konkrete Diagnosefrage, aber die exakte Implementierung muss live im Repo geprüft werden. In hu-probe ist die Klasse nicht eingecheckt; siehe Abschnitt 8.

### [Q7] ESP-IDF TinyUSB Glue / Device Layer

**Typ:** Codequelle / Repo  
**Typische Fundstelle:** ESP-IDF-Komponenten für TinyUSB-Integration  
**Sicherheitsniveau:** wahrscheinlich korrekt  
**Belegwert:** mittel bis hoch  
**Erwarteter oder belegter Inhalt:**

- IDF übernimmt Stack-Initialisierung, Descriptor-Registrierung, Konfigurationsglue
- Die eigentliche Request-Klassifikation findet typischerweise weiterhin in TinyUSB statt
- Tiefergehende Instrumentierung ist eher in TinyUSB-Core oder DCD/Driver-Nähe möglich als in Arduino

**Einordnung:**  
Relevant für die Frage nach dem besten Instrumentierungspunkt.

---

### Maintainer-Aussagen / Issues / Diskussionen

### [Q8] Wiederkehrendes Community-Muster: „Warum kommt mein Control Request nicht im Callback an?“

**Typ:** Issue / Praxisquelle / Community-Wissen  
**Typische Fundstelle:** GitHub Issues in TinyUSB / arduino-esp32 / Foren  
**Sicherheitsniveau:** mittel  
**Belegwert:** mittel  
**Erwarteter oder belegter Inhalt:**

- Nutzer erwarten oft, dass alle EP0-Requests im App-Callback sichtbar sind
- Tatsächlich filtern Stacks Standard-Requests häufig vorher heraus
- Vendor-/Class-spezifische Hooks sind meist nicht als generische EP0-Sniffer gedacht

**Einordnung:**  
Kein Einzelbeleg, aber ein sehr typisches Muster bei USB-Stacks.

### [Q9] Diskussionen zu vollständiger EP0-Instrumentierung

**Typ:** Maintainer-Kommentar / Praxisquelle  
**Typische Fundstelle:** Issues/PRs/Discussions zu TinyUSB Debugging, EP0 tracing, control transfer logging  
**Sicherheitsniveau:** mittel  
**Belegwert:** mittel  
**Erwarteter oder belegter Inhalt:**

- Vollständige EP0-Sicht erfordert meist Hooks unterhalb einfacher Klassen-Callbacks
- Für vollständige Sicht sind Core-Dispatch- oder DCD-nahe Loggingpunkte besser geeignet
- Arduino-Wrapper abstrahieren eher und sind selten für forensische Vollsicht ausgelegt

**Einordnung:**  
Wichtig für die konkrete Handlungsempfehlung.

---

### Praxisbelege / Implementierungswissen

### [Q10] Typisches TinyUSB-Verhalten bei Standard Requests

**Typ:** Praxisquelle / Architekturwissen  
**Typische Fundstelle:** Device-Core-Implementierungen, Enumeration-Verhalten, Descriptor-Handling  
**Sicherheitsniveau:** sehr wahrscheinlich korrekt  
**Belegwert:** hoch  
**Erwarteter oder belegter Inhalt:**

- Requests wie `GET_DESCRIPTOR`, `SET_ADDRESS`, `SET_CONFIGURATION` werden typischerweise intern bedient
- Die Anwendung bekommt diese nicht zwingend über Klassen-/Vendor-Callbacks
- Ein Callback auf Klassenebene ist daher kein vollständiger Bus- oder EP0-Monitor

**Einordnung:**  
Direkt relevant für die Bedeutung von „nicht gesehen“.

### [Q11] Stage-Sichtbarkeit in TinyUSB

**Typ:** Architektur- / Implementierungsquelle  
**Typische Fundstelle:** Control-xfer APIs, Callback-Signaturen, interne EP0-Zustandsverwaltung  
**Sicherheitsniveau:** wahrscheinlich korrekt  
**Belegwert:** hoch  
**Erwarteter oder belegter Inhalt:**

- TinyUSB kennt intern Transfer- und Stage-Kontext
- Nicht jede Arduino-API exponiert SETUP/DATA/STATUS separat
- App-seitige Callbacks sehen oft primär das SETUP und Datenpuffer, aber nicht zwingend eine voll explizite Stage-Maschine

**Einordnung:**  
Kritisch für die Frage nach vollständiger EP0-Instrumentierung. Repo-Befund zum Stage-Enum: Abschnitt 8.

---

## 3. Technische Bewertung mit eingetragenen Quellenankern

### 3.1 Architektur des Control-Request-Routings

Ein USB-Control-Transfer auf EP0 läuft logisch durch diese Schichten:

1. **USB-Device-Controller des ESP32-S3**
2. **DCD / Low-Level Device Driver**
3. **TinyUSB Device Core**
4. **Klassen-/Funktionsspezifische Handler**
5. **ESP-IDF-Integration / Arduino-ESP32 Wrapper**
6. **Anwendungsnahe Callback-Ebene wie `USBVendor.onRequest()`**

Die wesentliche Klassifikation erfolgt nach USB-Modell schon im SETUP-Paket über `bmRequestType` und `bRequest` [Q3].  
Dabei ist architektonisch sehr wahrscheinlich:

- **Standard Requests** werden in erheblichem Umfang direkt im TinyUSB-Device-Core bzw. generischen Gerätemanagement behandelt [Q1][Q4][Q10].
- **Class Requests** werden an den jeweils zuständigen Class Driver weitergereicht, sofern eine passende Funktion/Klasse registriert ist [Q1][Q4].
- **Vendor Requests** können an vendor-spezifische Klassen-/Funktionspfade gelangen, aus denen dann Arduino-Wrapper wie `USBVendor` einen Callback anbieten [Q5][Q6].

Das bedeutet bereits strukturell:  
Ein Arduino-Callback auf einer Vendor-Funktionsklasse sitzt **nicht am Eingang von EP0**, sondern **hinter einem Routing- und Filterprozess** [Q4][Q6].

**Diagnosefolgerung:**  
Wenn ein Request nicht bis `USBVendor.onRequest()` kommt, heißt das zunächst nur, dass er **nicht diesen Pfad** durchlaufen hat. Es heißt nicht automatisch, dass er **nicht auf dem Bus** war.

---

### 3.2 Bedeutung und Rolle von `USBVendor.onRequest()`

`USBVendor.onRequest()` ist nach der Architektur und Namenssemantik **mit hoher Wahrscheinlichkeit ein anwendungsnaher Callback für vendor-spezifische Control-Requests an die Vendor-Funktion** [Q5][Q6].

Das ist ein entscheidender Punkt:  
Ein solcher Callback ist in typischen USB-Stacks **nicht als generischer EP0-Sniffer** konzipiert, sondern als **Funktions- bzw. Interface-spezifischer Request-Hook**.

Daraus folgt technisch sehr plausibel:

- **Sicher sichtbar** sind dort vor allem Requests, die
  - als **Vendor-Type** klassifiziert sind,
  - an die passende Funktion / das passende Interface / Recipient geroutet werden,
  - nicht schon vorher vom Core absorbiert werden [Q4][Q5][Q6].
- **Nicht sicher sichtbar** sind:
  - Standard Requests
  - viele Class Requests
  - Vendor Requests an andere Recipients/Pfade
  - Requests, die intern beantwortet werden
  - evtl. Stage-spezifische Ereignisse jenseits des initialen SETUP-Hooks [Q4][Q5][Q11]

Wenn der `AoaControlTap` an `USBVendor.onRequest()` hängt, dann ist die Grundannahme **sehr stark**, dass er **keine vollständige EP0-Sicht** liefert. Der Header im Repo sagt das ausdrücklich: „Publishes every SETUP that arrives on this callback (not a full EP0 sniffer).“

---

### 3.3 Behandlung von Standard-, Class- und Vendor-Requests

#### Standard Requests

Typische Standard Requests wie:

- `GET_DESCRIPTOR`
- `SET_ADDRESS`
- `SET_CONFIGURATION`
- `GET_CONFIGURATION`
- evtl. `GET_STATUS`, `CLEAR_FEATURE`, `SET_FEATURE`

werden im Device-Stack üblicherweise **intern behandelt** [Q1][Q4][Q10].  
Das ist notwendig für Enumeration und Gerätezustandsverwaltung.

**Folge:** Solche Requests sieht ein Vendor-Callback typischerweise **nicht**.

#### Class Requests

Class Requests werden normalerweise an die passende USB-Funktionsklasse verteilt, z. B. CDC, HID, MSC oder vendor-spezifische Klassenlogik, falls dort klassenähnliche Control-Pfade existieren [Q1][Q4].  
Auch diese Requests gehen **nicht automatisch** in einen generischen Arduino-Vendor-Callback.

**Folge:** Ein `USBVendor.onRequest()` ist sehr wahrscheinlich **kein Sammelpunkt für Class Requests**.

#### Vendor Requests

Vendor Requests sind die Requests, für die `USBVendor.onRequest()` mit hoher Wahrscheinlichkeit gebaut wurde [Q5][Q6].  
Aber selbst hier ist Vorsicht nötig:

Nicht jeder Vendor Request erreicht zwingend denselben App-Callback. Das hängt ab von:

- Recipient: Device / Interface / Endpoint / Other
- gebundener Funktion / Interface-Nummer
- Arduino-Wrapper-Design
- TinyUSB-Dispatch-Regeln
- möglicher Vorverarbeitung im Stack

**Wichtige Diagnosefolge:**  
Auch innerhalb der Kategorie **Vendor** kann es sein, dass der aktuelle Tap **nur eine Teilmenge** sieht. Im hu-probe-Code ist die Teilmenge enger als „alles, was den Callback erreicht“: gezählt und beantwortet wird nur, wenn `stage == REQUEST_STAGE_SETUP`. Siehe Abschnitt 8.

---

### 3.4 SETUP / DATA / STATUS und Stage-Sichtbarkeit

Ein Control-Transfer besteht aus:

- **SETUP**
- **optional DATA**
- **STATUS** [Q3]

Für die Sichtbarkeit gilt sehr wahrscheinlich:

#### SETUP

Das SETUP-Paket ist der primäre Routingpunkt.  
TinyUSB muss dieses Paket intern vollständig kennen, um Request-Typ und Ziel zu bestimmen [Q4][Q11].  
App-Callbacks erhalten häufig zumindest Informationen aus dem SETUP-Header, **wenn** der Request bis zu ihnen weitergereicht wird.

#### DATA

Die DATA-Phase kann über Hilfsfunktionen und Control-Transfer-APIs verarbeitet werden.  
Aber: Ob die Arduino-Ebene diese Phase explizit als getrennte Stage sichtbar macht, ist unklar und wahrscheinlich **nur eingeschränkt** [Q11].

Im Tap gibt es einen eigenen Pfad für req52-DATA: nach `REQUEST_STAGE_DATA` wird der String-Scratch publiziert (`AoaControlTap.cpp`). Das ist eine Stage, kein vollständiger STATUS-Trace.

#### STATUS

Die STATUS-Phase wird oft intern vom Stack abgewickelt.  
Es ist eher untypisch, dass Arduino-High-Level-Callbacks eine vollständige, explizite STATUS-Phasenbeobachtung bieten.

**Bewertung:**  
Auf Arduino-/`USBVendor`-Ebene ist eher mit einer **anwendungsorientierten Request-Sicht** zu rechnen, nicht mit einem **voll expliziten dreiphasigen EP0-Trace**.  
Wenn Stage-Informationen gebraucht werden, insbesondere zur Unterscheidung:

- SETUP kam an
- DATA kam / kam nicht
- STATUS wurde abgeschlossen / gestallt

dann ist die Arduino-Ebene wahrscheinlich **zu hoch**.

---

### 3.5 Diagnosewert von „nicht gesehen“

Das ist der wichtigste Abschnitt für das konkrete Projekt.

Die Aussage **„Request X wurde nicht gesehen“** hat in dieser Stack-Konstellation **nur dann hohen Diagnosewert**, wenn vorher belegt ist, dass der gewählte Hook **alle relevanten Requests dieser Klasse vollständig sieht**.

Für `USBVendor.onRequest()` bzw. einen an diesen Callback gehängten `AoaControlTap` ist das nach der Stack-Architektur **nicht belegt**, sondern eher **unwahrscheinlich** [Q4][Q5][Q6][Q10].

Daraus folgt:

#### Was man aus „nicht gesehen“ eher nicht schließen darf

- nicht sicher: „der Host hat den Request nicht gesendet“
- nicht sicher: „auf EP0 gab es diesen Request nicht“
- nicht sicher: „TinyUSB hat diesen Request nicht empfangen“
- nicht sicher: „es gab keine Data- oder Status-Phase“

#### Was „nicht gesehen“ eher bedeuten kann

- der Request war ein **Standard Request** und wurde intern behandelt
- der Request war ein **Class Request** und wurde an einen anderen Pfad geroutet
- der Request war zwar **Vendor**, aber nicht auf dem von `USBVendor` exponierten Pfad
- der Request wurde im Stack beantwortet, ohne an Arduino hochgereicht zu werden
- der aktuelle Tap sieht nur **SETUPs einer Teilmenge**
- der Tap sieht zwar SETUP, aber nicht DATA/STATUS
- die Instrumentierung ist **semantisch enger als angenommen**
- der Callback wurde aufgerufen, aber die Stage-Konstante passte nicht (Abschnitt 8)

**Kernaussage:**  
In der vorliegenden Konstellation ist **„nicht gesehen“ nur schwache Evidenz**, solange kein Nachweis für vollständige Abdeckung existiert.

---

### 3.6 Möglichkeiten für vollständigere EP0-Instrumentierung

Wenn das Ziel ein **möglichst vollständiger EP0-/Control-Trace** ist, dann sollte die Instrumentierung **unterhalb von `USBVendor.onRequest()`** ansetzen.

Technisch sinnvolle Optionen:

#### Option A – TinyUSB Device-Core Hooking

Der beste Kandidat ist ein Loggingpunkt dort, wo TinyUSB:

- das SETUP-Paket für EP0 entgegennimmt,
- Request-Typ und Recipient klassifiziert,
- zwischen Standard/Class/Vendor dispatcht,
- Control Data/Status vorbereitet.

**Vorteil:**

- wesentlich vollständiger als Arduino-Callback
- Request-Klassifikation direkt sichtbar
- gute Chance auf Sicht auf alle SETUP-Pakete

**Nachteil:**

- Eingriff in TinyUSB oder dessen Integration nötig
- Upstream-/Patch-Pflege

#### Option B – Instrumentierung an TinyUSB-Klassen-Dispatch-Stellen

Wenn Vollständigkeit nicht absolut, aber deutlich besser als jetzt sein soll:

- Logging vor oder in den Dispatch-Zweigen für Standard / Class / Vendor
- zusätzlich Logging in Vendor-Class-Handlern

**Vorteil:**

- minimaler als DCD-nah
- gute semantische Sicht

**Nachteil:**

- Data/Status evtl. noch nicht vollständig

#### Option C – ESP-IDF-/TinyUSB-Glue erweitern

Wenn Arduino-ESP32 auf IDF/TinyUSB aufsetzt, kann ein eigener Instrumentierungspfad zwischen Wrapper und TinyUSB eingebaut werden.

**Vorteil:**

- weniger invasiv als ganz unten
- näher an bestehendem Projekt

**Nachteil:**

- möglicherweise immer noch zu hoch für Vollsicht

#### Option D – DCD / Controller-nahe EP0-Logging

Der forensisch stärkste Punkt wäre noch tiefer:

- unmittelbar beim Empfang und Abarbeiten von EP0-Events im Device-Controller-Treiber.

**Vorteil:**

- maximale Rohsicht auf EP0-Ereignisse

**Nachteil:**

- am invasivsten
- semantisch schwerer auszuwerten
- wahrscheinlich unnötig, wenn TinyUSB-Core-Hook schon reicht

**Praktische Empfehlung:**  
Für dieses Problem ist **TinyUSB-Core-Dispatch-Logging** wahrscheinlich der beste nächste Instrumentierungspunkt:  
früh genug für hohe Abdeckung, aber semantisch noch gut interpretierbar.

Im MSC-Pfad gibt es dieses Muster schon: `firmware/src/msc/MscBotTap.cpp` wrappt `mscd_control_xfer_cb` und vergleicht `stage == CONTROL_STAGE_SETUP`. Der AOA-Tap vergleicht `REQUEST_STAGE_SETUP`.

---

## 4. Vorläufiges Fazit mit Quellenankern

1. **Gesichert bzw. sehr stark:** `USBVendor.onRequest()` sitzt architektonisch nicht am Roh-Eingang von EP0, sondern hinter TinyUSB-/Wrapper-Dispatch [Q4][Q5][Q6].
2. **Sehr wahrscheinlich:** `USBVendor.onRequest()` ist primär für **vendor-spezifische Requests** gedacht und **kein generischer Empfänger für Standard- oder Class-Requests** [Q5][Q6].
3. **Sehr wahrscheinlich:** Standard Requests werden im TinyUSB-Device-Core teilweise oder weitgehend intern behandelt und daher nicht an Arduino-Vendor-Callbacks hochgereicht [Q1][Q4][Q10].
4. **Wahrscheinlich:** Auch die Sicht auf **DATA- und STATUS-Phasen** ist auf Arduino-Ebene unvollständig oder zumindest nicht als vollständiger Stage-Trace exponiert [Q11]. Req52-DATA ist im Tap ein Sonderfall.
5. **Belastbar:** Ein an `USBVendor.onRequest()` gehängter `AoaControlTap` ist **nicht ausreichend belegt** als vollständige EP0-Instrumentierung. Der eigene Header sagt das.
6. **Diagnostisch wichtig:** Der Befund **„Request X wurde nicht gesehen“** ist in dieser Konstellation **kein starker Negativbeweis**, solange die Abdeckung des Hooks nicht nachgewiesen ist [Q4][Q5][Q10].
7. **Für das Projekt besonders relevant:** Aussagen wie `51/52/53 = 0` können **durch echte Abwesenheit**, **durch Direct-2D00** (Research 1) oder **durch Instrument-Blindheit** erklärt sein. Die Lab-Messung in Abschnitt 8 spricht für Blindheit auf dem Probe-Tap, nicht für Absenz auf dem Lab-Host.
8. **Technisch bester nächster Schritt:** Instrumentierung **im TinyUSB-Control-Dispatch vor der Klassifikation bzw. direkt an der EP0-SETUP-Verarbeitung**, nicht nur in `USBVendor.onRequest()` [Q4][Q9][Q11]. Zusätzlich den Stage-Vergleich auf die TinyUSB-Konstante ziehen, die der MSC-Tap schon benutzt.
9. **Offen, live zu verifizieren:** Ob Arduino-ESP32 in `espressif32@6.4.0` zusätzliche Filterungen in `USBVendor` einführt, ist im hu-probe-Baum nicht lesbar [Q6].
10. **Gesamtbewertung:** Die aktuelle Observability ist mit hoher Wahrscheinlichkeit **zu lückenhaft**, um ausbleibende AOA-Control-Requests allein über `USBVendor.onRequest()` belastbar negativ zu interpretieren.

---

## 5. Repositories / Code-Fundstellen

### 1) TinyUSB

- **Repo / Projektname:** TinyUSB
- **Relevante Datei oder vermutete Datei:** Device-Core-Dateien für Control-Transfer-Dispatch; Vendor-Class-Implementierung
- **Warum wichtig:** Hier wird entschieden, welche EP0-Requests intern behandelt und welche weitergereicht werden
- **Was darin konkret geprüft werden sollte:**
  - zentrale EP0-/Control-Dispatch-Funktion
  - Unterscheidung Standard / Class / Vendor
  - Übergang in Klassen-/Vendor-Handler
  - APIs für Control DATA/STATUS
- **Mögliche Suchbegriffe/Funktionsnamen:**
  - `control`, `setup packet`, `vendor request`, `class request`, `tud_control_xfer`
  - EP0 state / setup request struct / dispatch function

### 2) arduino-esp32

- **Repo / Projektname:** arduino-esp32, im Build über `espressif32@6.4.0`
- **Relevante Datei oder vermutete Datei:** USB-Device-/TinyUSB-Wrapper; `USBVendor`-Implementierung
- **Warum wichtig:** Hier muss geprüft werden, was `USBVendor.onRequest()` genau exponiert
- **Was darin konkret geprüft werden sollte:**
  - Signatur und Registrierung von `onRequest()`
  - Welche Requests den Callback triggern
  - Ob nur Vendor-Type durchgereicht wird
  - Ob Recipient/Interface gefiltert wird
  - Ob Data-Phase getrennt sichtbar ist
  - numerische Werte von `REQUEST_STAGE_SETUP` gegen TinyUSB `CONTROL_STAGE_SETUP`
- **Mögliche Suchbegriffe/Funktionsnamen:**
  - `USBVendor`
  - `onRequest`
  - `vendor request`
  - `control request`
  - callback registration

### 3) ESP-IDF

- **Repo / Projektname:** esp-idf
- **Relevante Datei oder vermutete Datei:** TinyUSB-Komponente / USB Device Glue / S3 USB Device Integration
- **Warum wichtig:** Zeigt, wie Arduino auf den IDF-Stack aufsetzt und wo zusätzliche Hooks möglich wären
- **Was darin konkret geprüft werden sollte:**
  - TinyUSB stack init
  - Device descriptors / class registration
  - mögliche Logging-/Glue-Stellen
  - event callbacks / wrapper functions
- **Mögliche Suchbegriffe/Funktionsnamen:**
  - `tinyusb`, `usb device`, `tusb_init`, `descriptor`, `driver install`

### 4) Beispielcode / Community-Projekte

- **Repo / Projektname:** diverse TinyUSB-/ESP32-S3-Beispiele
- **Relevante Datei oder vermutete Datei:** Vendor-Class-Beispiele, custom USB device examples
- **Warum wichtig:** Praxisbelege dafür, welche Callback-Schicht Anwender tatsächlich nutzen
- **Was darin konkret geprüft werden sollte:**
  - Gibt es Beispiele für vollständigen EP0-Trace?
  - Oder nur für vendor/class callbacks?
  - Werden Standard Requests jemals auf App-Ebene geloggt?

### 5) hu-probe, bereits im Baum

| Datei | Was sie zeigt |
|-------|----------------|
| `firmware/src/connected/aoa/AoaObserveGadget.cpp` | `g_vendor.onRequest(onVendorControl)` |
| `firmware/src/connected/aoa/AoaControlTap.h` | Callback ist kein vollständiger EP0-Sniffer |
| `firmware/src/connected/aoa/AoaControlTap.cpp` | Push und Zähler nur bei `REQUEST_STAGE_SETUP`; Vendor-Antworten nur für 51/52/53/58 |
| `firmware/src/connected/ConnectedAppProbe.cpp` | `p0_b_` nur bei 51/52/53/58; `stage` wird publiziert |
| `firmware/src/msc/MscBotTap.cpp` | Tieferer Hook: `__wrap_mscd_control_xfer_cb`, `CONTROL_STAGE_SETUP` |

---

## 6. Praktische Hinweise für das konkrete Problem

### Was bedeutet die Recherche für einen `AoaControlTap`, der an `USBVendor.onRequest()` hängt?

Sehr wahrscheinlich Folgendes:

- Der `AoaControlTap` beobachtet **nicht „EP0 allgemein“**, sondern **nur einen Teilraum von Control-Transfers**.
- Dieser Teilraum ist:
  - das, was der Arduino-Vendor-Callback überhaupt aufruft,
  - danach nochmals gefiltert auf `stage == REQUEST_STAGE_SETUP`,
  - für die AOA-Antwort zusätzlich auf Vendor und 51/52/53/58.

Der Tap ist nützlich für **AOA-/Vendor-Pfad-Diagnose**, aber **nicht belastbar als vollständiger EP0-Sensor**.

Innerhalb des Callbacks publiziert er seit P0.5-A **jedes** SETUP, das ankommt, auch Nicht-Vendor. Die Enge sitzt vor allem **vor** dem Callback und im Stage-Vergleich, nicht mehr in einem „nur Vendor pushen“.

### Wie wahrscheinlich ist es, dass Nicht-Vendor-Requests unsichtbar bleiben?

**Hoch wahrscheinlich**, bevor sie den Callback erreichen.

Insbesondere:

- Standard Requests: sehr wahrscheinlich unsichtbar am Vendor-Callback
- Class Requests: wahrscheinlich unsichtbar, sofern sie nicht explizit auf denselben Wrapperpfad gelegt sind
- DATA-/STATUS-Phasen: wahrscheinlich unvollständig sichtbar; req52-DATA ist die Ausnahme im Tap
- selbst manche Vendor Requests: möglicherweise unsichtbar, wenn sie nicht in genau den vom Wrapper exponierten Pfad fallen, oder sichtbar im Callback, aber vom Stage-Check verworfen

### Was bedeutet das für Aussagen wie `51/52/53 = 0`?

Wenn `51/52/53` als „bestimmte AOA-Control-Requests wurden nicht beobachtet“ stehen, dann ist die Aussage **nur dann stark**, wenn klar ist:

1. diese Requests wären überhaupt **vendor-spezifisch**,
2. sie würden an **genau diese Funktion / diesen Recipient** gehen,
3. `USBVendor.onRequest()` sieht **alle** solchen Requests vollständig,
4. und die Instrumentierung verpasst weder Dispatch-Varianten noch Stages.

Solange das nicht belegt ist, darf `51/52/53 = 0` **nicht automatisch** als „Host hat nicht gesendet“ interpretiert werden.  
Es kann genauso gut heißen:  
**„Der aktuelle Tap ist für diese Requests blind.“**

Zusätzliche Lesarten, die Research 1 und der Lab-Bericht schon tragen:

- bei Direct-`18D1:2D00` ist Absenz von 51/52/53 erwartbar
- im Lab hat der Host 51/52/53 gegen das Phone gesendet; gegen die Probe blieb `setup_count=0` bei STALL

### Welche Instrumentierungsänderung wäre der beste nächste Schritt?

**Beste nächste Maßnahme:**  
Einen zusätzlichen Loggingpunkt **vor `USBVendor.onRequest()`**, idealerweise **im TinyUSB-Control-Dispatch auf EP0-SETUP-Ebene** einbauen. Kurz davor, im bestehenden Tap, den Stage-Vergleich auf `CONTROL_STAGE_SETUP` prüfen, analog zu `MscBotTap`.

Minimal sinnvoll zu loggen:

- vollständiges SETUP-Paket:
  - `bmRequestType`
  - `bRequest`
  - `wValue`
  - `wIndex`
  - `wLength`
- Klassifikation:
  - Standard / Class / Vendor
  - Recipient
  - Richtung
- Dispatch-Entscheidung:
  - intern beantwortet
  - an class handler
  - an vendor handler
  - stalled / unsupported
- optional:
  - Data-Phase expected / actual length
  - Completion / Status result
- den rohen `stage`-Wert, nicht nur die Arduino-Konstante

Damit entsteht die Trennung zwischen:

- **nicht gesendet**
- **gesendet, aber intern verarbeitet**
- **gesendet, aber nicht an Arduino weitergereicht**
- **gesendet, Callback aufgerufen, Stage-Filter hat die Probe verworfen**
- **gesendet und im Vendor-Callback angekommen**

### Wie verhindert man, dass `P0_B` semantisch kippt, wenn die Instrumentierung breiter wird?

Wenn `P0_B` bisher „AOA-relevanter Control“ meint, darf es nach einer breiteren Instrumentierung **nicht** zu „alle EP0-Requests“ werden.

Getrennte Zähler, wie die Notiz sie vorschlägt:

- **P0_B_vendor_callback** — nur das, was im bisherigen `USBVendor.onRequest()` sichtbar war
- **P0_B_ep0_setup_all** — alle auf EP0 empfangenen SETUP-Pakete
- **P0_B_dispatched_vendor** — alle als Vendor klassifizierten und zum Vendor-Handler gerouteten Requests
- **P0_B_completed** — erfolgreich abgeschlossene Transfers

Auf `849f30e` ist ein Teil davon schon da, ohne diese Namen:

- `p0_b_` nur bei `bRequest` 51, 52, 53 oder 58 (`4c1d6d2`, FW 0.3.28)
- `setup_count_` jedes `REQUEST_STAGE_SETUP`, das den Callback erreicht
- `vendor_setup_count_` / `controlCount()` nur Vendor-SETUPs
- ein Zähler für **alle** EP0-SETUPs vor dem Vendor-Callback fehlt

---

## 7. Ehrliche Quellenlage

### Was mit hoher Sicherheit gesagt werden kann

- Ein Arduino-Wrapper-Callback wie `USBVendor.onRequest()` ist architektonisch **nicht automatisch ein vollständiger EP0-Sniffer**.
- Standard-Requests werden in USB-Stacks wie TinyUSB typischerweise **zum erheblichen Teil intern behandelt**.
- Eine Instrumentierung nur an diesem Callback ist **als Negativbeweis für allgemeine EP0-Abwesenheit nicht ausreichend**.
- Für vollständigere Observability ist ein Hook **unterhalb der Arduino-Vendor-Ebene** erforderlich.
- Der hu-probe-Header von `AoaControlTap` sagt dieselbe Grenze.

### Was plausibel, aber noch live zu verifizieren ist

- Die exakte Implementierung von `USBVendor.onRequest()` in `espressif32@6.4.0` / dem mitgelieferten arduino-esp32. Die Quellen liegen nicht in diesem Git-Baum.
- Ob der Callback ausschließlich `bmRequestType` Vendor erhält oder zusätzlich Recipient/Interface filtert.
- Welche Stage-Informationen Arduino-ESP32 konkret exponiert, als Quelltext gelesen. Die Lab-Hypothese dazu steht in Abschnitt 8.
- Ob es in aktuellen Issues/PRs bereits bekannte Einschränkungen oder Regressionen dazu gibt.

### Was offen bleibt

- Die genaue Dispatch-Kette in der Framework-Version, inklusive Dateipfaden innerhalb von TinyUSB und `USBVendor.cpp`.
- Ob bestimmte AOA-relevante Requests durch eine Sonderbehandlung in TinyUSB oder Arduino an anderen Stellen sichtbar wären.
- Ob es bereits einen weniger invasiven, offiziellen Hook für generisches EP0-Tracing in ESP-IDF/Arduino-ESP32 gibt.

---

## 8. Repo-Abgleich (nicht Teil der gelieferten Notiz)

Gelesen auf dem Stand dieser Ablage, Framework `espressif32@6.4.0`.

| Frage aus der Notiz | Befund im Baum |
|---------------------|----------------|
| Ist der Tap ein EP0-Sniffer? | Nein. `AoaControlTap.h`: publiziert jedes SETUP, **das diesen Callback erreicht**. |
| Wer registriert den Callback? | `AoaObserveGadget::begin()` → `g_vendor.onRequest(onVendorControl)`. |
| Sieht der Tap nur Vendor, sobald der Callback feuert? | Nein. Jedes `REQUEST_STAGE_SETUP` wird gepusht. `setup_count_` zählt alle, `vendor_setup_count_` nur Vendor. AOA-Antworten nur für Vendor 51/52/53/58. |
| Fehlt `stage` in der Publikation? | Nein. `drainControlTap()` schreibt `stage` und `request_type`. |
| Kippt `P0_B` bei jedem Sample? | Nein, seit FW 0.3.28 nur 51/52/53/58. |
| Gibt es einen tieferen Hook im Projekt? | Ja, nur auf dem MSC-Pfad: `__wrap_mscd_control_xfer_cb` mit `CONTROL_STAGE_SETUP`. |
| Ist `51/52/53 = 0` auf der Probe Instrument-blind? | Lab 2026-10-30, `aoa_phone_like`: Host-51 → STALL, `setup_count=0`. Ursache im Lab-Bericht als **hochwahrscheinlich** der Stage-Enum-Mismatch (TinyUSB `CONTROL_STAGE_SETUP=1`, Arduino `REQUEST_STAGE_SETUP=0`). Fix ist Firmware, nicht Teil des Phone-Labs. [`2026-10-30-lab-aoa-aaidrive-phone-host.md`](2026-10-30-lab-aoa-aaidrive-phone-host.md) |

Damit ist „nicht gesehen“ auf der Probe für Vendor-51 im Lab schon ein konkreter Blindheitsfall: der Host hat 51 gesendet, der Tap hat 0 gezählt. Das ersetzt nicht das Lesen von `USBVendor.cpp` und ersetzt nicht einen Host-Trace HU gegen Probe (Research 2).
