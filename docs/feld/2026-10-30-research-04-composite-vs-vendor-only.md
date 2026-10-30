# Technischer Research-Report

## Composite vs vendor-only behavior in automotive USB hosts

### Evidenzlage zu MSC-/Composite-Effekten, Hostklassifikation und methodischem Risiko früherer Befunde

**Projekt:** hu-probe  
**Datum:** 2026-10-30  
**Reihe:** Research-Report 4 von 5  
**Problemfelder:** [`2026-10-30-aoa-problemfelder.md`](2026-10-30-aoa-problemfelder.md) — Block 5, Thema 6  
**Verwandt:** [`2026-10-29-c1b-p0-aoa-observe-feld.md`](2026-10-29-c1b-p0-aoa-observe-feld.md) · [`2026-10-29-c1b-p05a-instrumentation.md`](2026-10-29-c1b-p05a-instrumentation.md) · [`2026-10-29-c1b-b2b-knock-feld.md`](2026-10-29-c1b-b2b-knock-feld.md)  
**Charakter:** gelieferte Recherche-Notiz. Der Repo-Abgleich steht in Abschnitt 8.

**Evidenzkennzeichnung**

- **Belegt:** direkt aus USB-Spezifikationen, offiziellen Protokolldokumenten oder allgemein dokumentiertem Host-/Treiberverhalten ableitbar.
- **Wahrscheinlich:** technisch plausibel und mit typischem Hostverhalten vereinbar, aber nicht spezifisch für eine bestimmte Automotive-Head-Unit belegt.
- **Offen:** ohne Trace, Firmwareanalyse oder kontrollierten Versuch für den konkreten Host nicht belastbar entscheidbar.

---

## 1. Ziel der Untersuchung

Untersucht wird, wie sich ein **USB-vendor-only-Gerät** gegenüber einem **Composite Device mit MSC- und Vendor-Interface** auf Automotive-USB-Hosts und Head Units auswirken kann.

Der zentrale methodische Punkt ist, dass ein früheres Gerät offenbar nicht ausschließlich vendor-specific auftrat, sondern mindestens folgende funktionale Struktur besaß:

```text
Configuration 1
 ├── Interface 0: Mass Storage Class / MSC
 └── Interface 1: Vendor Specific
```

Damit konnte der Host das Gerät möglicherweise als:

- Mass-Storage-Gerät,
- Medien-/Dateiquelle,
- generisches USB-Gerät,
- oder Composite Device mit mehreren möglichen Funktionen

behandeln.

Die zentrale Diagnosefrage lautet daher:

> Sind frühere UI-, Aktivierungs- oder „Connected“-Beobachtungen tatsächlich auf das Vendor- oder Accessory-Verhalten zurückzuführen, oder wurden sie durch die zusätzliche MSC-Funktion bzw. das gesamte Composite-Layout ausgelöst?

Die wichtigste Schlussfolgerung vorweg lautet:

> Ein MSC+Vendor-Composite-Setup kann die Hostreaktion erheblich verändern. Es gibt jedoch keine allgemeine USB-Regel, nach der jeder Host bei Composite-Geräten automatisch MSC gegenüber Vendor-Interfaces bevorzugt. Die konkrete Wirkung hängt vom Host-Stack, der unterstützten Klassen, dem Parser, der Konfiguration und der nachgelagerten Head-Unit-Logik ab.

---

## 2. Quellenbasis

### USB-Grundlagen und Klassenspezifikationen

### [Q1] USB 2.0 Specification

**Typ:** Spezifikationsnahe Quelle  
**Typische Fundstelle:** USB-IF, *Universal Serial Bus Specification Revision 2.0*, insbesondere Device-, Configuration-, Interface- und Endpoint-Descriptors  
**Sicherheitsniveau:** sehr wahrscheinlich korrekt  
**Belegwert:** sehr hoch

**Erwarteter oder belegter Inhalt:**

- Ein USB-Gerät kann mehrere Interfaces innerhalb einer Configuration enthalten.
- Die Funktion eines Interfaces wird grundsätzlich über dessen Interface Descriptor beschrieben.
- `bInterfaceClass`, `bInterfaceSubClass` und `bInterfaceProtocol` können sich von Interface zu Interface unterscheiden.
- Device-Level-Class-Felder und Interface-Level-Class-Felder haben unterschiedliche Bedeutung.
- Ein Composite Device kann mehrere unabhängige oder zusammengehörige Funktionen anbieten.
- Der Host liest die Configuration und die darin enthaltenen Interfaces aus, bevor er geeignete Treiber oder Klassenhandler aktiviert.

**Einordnung:**

Diese Quelle belegt die grundlegende technische Grundlage: Ein MSC+Vendor-Gerät ist nicht nur „ein Vendor-Gerät mit Zusatzinformation“, sondern stellt dem Host mindestens zwei potenziell unterschiedliche Funktionen zur Verfügung.

---

### [Q2] USB Device Class Definition for Mass Storage Devices

**Typ:** Klassenspezifikation  
**Typische Fundstelle:** USB-IF, *Mass Storage Class Specification Overview*, einschließlich Bulk-Only Transport und verwandter Klassendokumente  
**Sicherheitsniveau:** sehr wahrscheinlich korrekt  
**Belegwert:** sehr hoch

**Erwarteter oder belegter Inhalt:**

- MSC wird über standardisierte Interface-Class-Werte und Protokollkombinationen identifiziert.
- Typische MSC-Geräte nutzen:
  - `bInterfaceClass = 0x08` — Mass Storage
  - `bInterfaceSubClass = 0x06` — SCSI transparent command set
  - `bInterfaceProtocol = 0x50` — Bulk-Only Transport
- Ein Host kann nach erfolgreicher Enumeration einen Mass-Storage-Treiber oder einen entsprechenden Klassenhandler laden.
- Danach können SCSI-Kommandos, Medienerkennung, Logical Units und Dateisystemzugriffe folgen.

**Einordnung:**

MSC ist für viele Hosts keine passive Descriptor-Eigenschaft. Nach der Enumeration kann daraus ein eigenständiger Gerätepfad mit weiteren Requests, SCSI-Kommandos und Dateisystemverarbeitung entstehen.

---

### [Q3] USB Device Class Definition for Communication Devices / Vendor-Specific Interfaces

**Typ:** Klassenspezifikation / USB-Klassensystem  
**Typische Fundstelle:** USB-IF-Klassendokumente; USB 2.0 Interface Descriptor Definition  
**Sicherheitsniveau:** hoch  
**Belegwert:** hoch

**Erwarteter oder belegter Inhalt:**

- Ein vendor-specific Interface wird typischerweise mit `bInterfaceClass = 0xFF` gekennzeichnet.
- USB definiert für ein solches Interface nicht automatisch dessen Semantik.
- Ein Host benötigt daher:
  - einen bekannten VID/PID,
  - einen herstellerspezifischen Treiber,
  - ein bekanntes Protokoll,
  - eine bekannte Accessory-/Phone-Identität,
  - oder eine spezielle Anwendungserkennung.
- Ohne passenden Handler kann ein Vendor-Interface zwar korrekt enumerieren, aber funktional ungenutzt bleiben.

**Einordnung:**

Das ist der entscheidende Unterschied zu MSC: Eine vendor-specific Klassifizierung führt nicht automatisch zu einer sichtbaren Hostfunktion. Sie ist zunächst nur ein Hinweis, dass keine standardisierte USB-Klasse angewendet wird.

---

### [Q4] USB Interface Association Descriptor, IAD und Composite Device Handling

**Typ:** Spezifikationsnahe Quelle  
**Typische Fundstelle:** USB 2.0 Specification, USB Device Class Definition for IAD; Microsoft USB Composite Device documentation als Host-Implementierungsbeispiel  
**Sicherheitsniveau:** hoch  
**Belegwert:** hoch

**Erwarteter oder belegter Inhalt:**

- Ein IAD kann mehrere Interfaces zu einer logischen Funktion gruppieren.
- IADs sind insbesondere bei Funktionen mit mehreren zusammengehörigen Interfaces relevant.
- Nicht jeder Host behandelt IADs identisch gut.
- Ein Gerät kann auch ohne IAD mehrere Interfaces anbieten; dadurch kann die logische Gruppierung für einfache oder ältere Parser weniger eindeutig sein.
- Die Device-Class-Werte `0xEF/0x02/0x01` werden häufig für Miscellaneous/Common Class/IAD-bezogene Composite-Strukturen verwendet, sind aber kein universelles „Composite funktioniert überall“-Signal.

**Einordnung:**

IADs sind für das konkrete Problem relevant, weil nicht nur die Existenz von MSC und Vendor zählt, sondern auch, ob der Host sie als getrennte Funktionen oder als zusammengehörige Funktion interpretiert.

---

### [Q5] USB Mass Storage Bulk-Only Transport und SCSI-Kommandos

**Typ:** Protokollspezifikation  
**Typische Fundstelle:** USB-IF, *Mass Storage Class Bulk-Only Transport*; SCSI Primary Commands  
**Sicherheitsniveau:** sehr wahrscheinlich korrekt  
**Belegwert:** sehr hoch

**Erwarteter oder belegter Inhalt:**

- Nach der Klassenidentifikation kann der Host MSC-spezifische Requests und Bulk-Transfers ausführen.
- Typische Vorgänge umfassen:
  - `GET_MAX_LUN`,
  - Mass-Storage Reset,
  - Inquiry,
  - Test Unit Ready,
  - Read Capacity,
  - Read/Write,
  - Dateisystemzugriffe nach erfolgreicher Medienerkennung.
- Ein Host kann bereits bei Medien- oder Geräteabfragen sichtbare Zustandsänderungen erzeugen, ohne dass eine vollständige Dateinutzung stattfindet.

**Einordnung:**

Damit ist technisch plausibel, dass ein MSC-Interface UI- oder Zustandsänderungen auslöst, die fälschlich als Vendor-, Accessory- oder Connected-Reaktion interpretiert werden.

---

### Android-/AOA-nahe Quellen

### [Q6] Android Open Accessory Protocol Documentation

**Typ:** Offizielle technische Dokumentation  
**Typische Fundstelle:** Android Developers / Android Open Accessory Protocol Documentation  
**Sicherheitsniveau:** hoch  
**Belegwert:** hoch für AOA, niedrig bis mittel für Automotive allgemein

**Erwarteter oder belegter Inhalt:**

- Android Open Accessory verwendet einen definierten Kontroll-Handshake.
- Der Host beziehungsweise das Accessory kann über Control Requests:
  - Protokollversion abfragen,
  - Hersteller-/Modell-/Beschreibungstexte übertragen,
  - den Wechsel in den Accessory Mode auslösen.
- Im Accessory Mode wird eine andere USB-Identität verwendet, typischerweise mit Google-VID und AOA-PID-Kombinationen.
- AOA ist nicht gleichbedeutend mit jedem beliebigen Vendor-Interface.
- Ein beliebiges `0xFF`-Interface löst nicht automatisch Android Accessory-Verhalten aus.

**Einordnung:**

Für das konkrete Problem besonders wichtig: „Vendor-only“ und „AOA-like“ sind nicht synonym. Ein vendor-only Gerät kann AOA-ähnlich erscheinen, ohne den AOA-Handshake oder die bekannte AOA-Identität zu erfüllen. Bedeutung von 51/52/53 und `18D1:2D00`: Research 1.

---

### [Q7] Android Open Accessory VID/PID- und Accessory-Mode-Verhalten

**Typ:** Offizielle Plattformdokumentation / Android-Implementierungswissen  
**Typische Fundstelle:** Android Open Accessory Documentation, AOSP USB Accessory Framework  
**Sicherheitsniveau:** hoch  
**Belegwert:** hoch für Android-Hosts, nicht automatisch für BMW/NBT

**Erwarteter oder belegter Inhalt:**

- Android Accessory Mode verwendet bekannte VID/PID-Kombinationen, unter anderem aus dem Google-USB-ID-Bereich.
- AOA-Accessory-Mode kann je nach Version zusätzliche Funktionen wie ADB oder Audio beinhalten.
- Die Identität im Accessory Mode unterscheidet sich von der ursprünglichen Geräteidentität.
- Der Host benötigt in der Regel mehr als nur eine generische Vendor-Class, nämlich ein protokollspezifisches Verhalten.

**Einordnung:**

„Phone-like identity“ kann deshalb stärker wirken als die bloße Existenz eines Vendor-Interfaces. Eine Head Unit oder ein Android-basierter Host kann VID/PID, Strings und bekannte Protokollsequenzen kombinieren.

---

### Linux-/Gadget- und Host-Implementierungen

### [Q8] Linux USB Composite Gadget / ConfigFS Documentation

**Typ:** Technische Dokumentation / Referenzimplementierung  
**Typische Fundstelle:** Linux kernel documentation, `Documentation/usb/gadget_configfs.rst`, USB gadget functions  
**Sicherheitsniveau:** sehr wahrscheinlich korrekt  
**Belegwert:** hoch für Gadget-Aufbau, mittel für Automotive

**Erwarteter oder belegter Inhalt:**

- Linux ConfigFS kann mehrere Gadget Functions in einer USB Configuration kombinieren.
- Typische Funktionen umfassen unter anderem:
  - Mass Storage,
  - ACM,
  - Ethernet,
  - HID,
  - FunctionFS/vendor-specific Funktionen.
- Die resultierenden Interfaces, Interface-Nummern, Endpoints und Strings hängen von der Zusammensetzung und Registrierung der Functions ab.
- Das Hinzufügen einer Function kann deshalb die gesamte Descriptor-Struktur verändern.

**Einordnung:**

Für das konkrete Projekt wichtig: Das Hinzufügen von MSC verändert möglicherweise nicht nur „ein zusätzliches Interface“. Es kann auch Interface-Nummern, Endpoint-Zuordnung, Configuration-Struktur und die Reihenfolge der Funktionen beeinflussen.

---

### [Q9] Linux USB Gadget Mass Storage Function

**Typ:** Technische Dokumentation / Referenzimplementierung  
**Typische Fundstelle:** Linux Kernel USB Gadget Documentation, Mass Storage Function  
**Sicherheitsniveau:** hoch  
**Belegwert:** hoch für das Verhalten des Gadgets

**Erwarteter oder belegter Inhalt:**

- Die Mass-Storage-Gadget-Funktion stellt ein standardisiertes MSC-Interface bereit.
- Je nach Konfiguration können Backing File, LUN-Anzahl, Removable-Status und weitere Eigenschaften sichtbar werden.
- Das Interface wird vom Host als Mass Storage erkannt, unabhängig davon, ob daneben weitere Vendor-Functions existieren.

**Einordnung:**

Ein früheres Setup mit MSC kann somit ein echtes und eigenständiges Host-Feature bereitgestellt haben. Die Vendor-Funktion war nicht notwendigerweise die einzige funktionale Erklärung für die Hostreaktion.

---

### [Q10] Linux USB Core und Interface-Treiber-Matching

**Typ:** Referenzimplementierung / allgemeines Hostverhalten  
**Typische Fundstelle:** Linux USB Core, USB driver binding, Interface matching  
**Sicherheitsniveau:** hoch  
**Belegwert:** hoch für Linux-artige Hosts, nicht automatisch für Automotive

**Erwarteter oder belegter Inhalt:**

- Bei Composite-Geräten können Treiber in der Regel auf Interface-Ebene gebunden werden.
- Ein MSC-Treiber kann an das MSC-Interface gebunden werden, während ein anderer Treiber oder eine Anwendung ein Vendor-Interface behandelt.
- Es gibt keine allgemeine USB-Spezifikationsregel, dass ein Standardklassen-Interface alle anderen Interfaces verdrängt.
- Praktisch kann ein Host einzelne Interfaces unterstützen und andere ignorieren.

**Einordnung:**

Das ist eine wichtige Korrektur gegenüber einer zu starken Vereinfachung: „Composite bedeutet automatisch MSC gewinnt“ ist nicht spezifikationskonform. Richtig ist: MSC eröffnet einen bekannten Standardpfad, der von vielen Hosts unabhängig vom Vendor-Interface genutzt werden kann.

---

### Microsoft- und generisches Hostverhalten

### [Q11] Microsoft USB Composite Device / USB Generic Parent Documentation

**Typ:** Betriebssystemdokumentation / Implementierungsbeispiel  
**Typische Fundstelle:** Microsoft Learn, USB composite devices, USB generic parent driver, interface collections  
**Sicherheitsniveau:** hoch  
**Belegwert:** mittel bis hoch für Windows; niedrig für Automotive

**Erwarteter oder belegter Inhalt:**

- Ein Composite Device kann durch einen generischen Parent-Treiber verwaltet werden.
- Einzelne Interfaces oder Interface Collections können an unterschiedliche Funktionstreiber weitergereicht werden.
- Die genaue Behandlung hängt von Descriptoren, IADs, kompatiblen IDs und verfügbaren Treibern ab.

**Einordnung:**

Diese Quelle zeigt ein allgemeines Muster, das auch für andere Hosts plausibel ist: Composite-Geräte müssen nicht als unteilbare Einheit behandelt werden. Automotive-Systeme können aber deutlich vereinfachtere Implementierungen haben.

---

### Automotive-/Headunit-Praxis

### [Q12] Öffentliche Automotive-Herstellerdokumentation zu USB-Medien- und Telefonfunktionen

**Typ:** Herstellerdokumentation / Bedienungsanleitungen / Kompatibilitätslisten  
**Typische Fundstelle:** Fahrzeughandbücher, USB-Kompatibilitätslisten, Headunit-Bedienungsanleitungen  
**Sicherheitsniveau:** mittel bis hoch für die jeweils dokumentierte Funktion  
**Belegwert:** mittel für konkrete Klassenunterstützung, niedrig für Parserdetails

**Erwarteter oder belegter Inhalt:**

- Automotive-Headunits unterscheiden häufig zwischen:
  - USB-Mass-Storage,
  - iPod-/Apple-Protokollen,
  - MTP-/Media-Geräten,
  - Smartphones,
  - Android Auto,
  - USB-Audio,
  - nicht unterstützten USB-Geräten.
- Die Nutzeroberfläche kann je nach erkannter Gerätefunktion unterschiedliche Menüs oder Meldungen anzeigen.
- Eine unterstützte Medienfunktion setzt häufig voraus, dass ein standardisiertes oder herstellerspezifisch bekanntes Protokoll vorliegt.

**Einordnung:**

Diese Quellen belegen die funktionale Differenzierung von Head Units, aber normalerweise nicht die genaue interne Priorisierung zwischen MSC und Vendor-Interface.

---

### [Q13] BMW-/NBT-/NBT-Evo-spezifische öffentliche Informationen

**Typ:** Automotive-/Reverse-Engineering-Quelle  
**Typische Fundstelle:** öffentliche BMW-Dokumentation, Entwicklerforen, NBT-/iDrive-Reverse-Engineering-Projekte, Community-Analysen  
**Sicherheitsniveau:** niedrig bis mittel, abhängig vom konkreten Dokument  
**Belegwert:** niedrig bis mittel

**Erwarteter oder belegter Inhalt:**

- Es gibt öffentliche Hinweise auf unterschiedliche BMW-USB-Funktionen, insbesondere Medienwiedergabe, iPod-/iPhone-Integration, Smartphone-Integration und spätere Android-Auto-/CarPlay-Unterstützung.
- Öffentliche Informationen beschreiben typischerweise beobachtete Funktionspfade, aber selten die vollständige USB-Parserlogik.
- Für NBT/NBT Evo ist öffentlich nicht verlässlich belegt, dass ein bestimmtes Interface gegenüber einem anderen generell priorisiert wird.
- Insbesondere ist keine allgemeingültige, öffentlich abgesicherte Regel bekannt, wonach BMW NBT bei jedem MSC+Vendor-Composite zwingend MSC oder zwingend Vendor bevorzugt.

**Einordnung:**

BMW-/NBT-spezifische Aussagen müssen deutlich vorsichtiger formuliert werden als allgemeine USB-Aussagen. Beobachtungen aus einzelnen Fahrzeugen oder Firmwareständen dürfen nicht ohne Trace auf alle NBT-/NBT-Evo-Versionen übertragen werden.

---

### [Q14] Android-Auto-/Accessory-Interoperabilität und USB-Transport

**Typ:** Plattform-/Praxisquelle  
**Typische Fundstelle:** Android-Auto-Dokumentation, AOA/A2DP-/USB-Accessory-Implementierungen, Accessory- und Headunit-Projekte  
**Sicherheitsniveau:** mittel bis hoch für dokumentierte Android-/Accessory-Protokolle  
**Belegwert:** mittel für Automotive allgemein

**Erwarteter oder belegter Inhalt:**

- Android Auto beziehungsweise Accessory-ähnliche Systeme verwenden definierte Handshakes und bekannte Geräteidentitäten.
- Eine reine Enumeration als Vendor-Interface ist nicht gleichbedeutend mit einer erfolgreichen Android-Auto- oder Accessory-Erkennung.
- Zusätzliche Funktionen wie MSC können die Geräteklassifikation verändern oder einen alternativen Hostpfad aktivieren.
- Die konkrete Reaktion hängt vom Hostbetriebssystem und der Integration der Head Unit ab.

**Einordnung:**

AOA-/Android-Auto-Nähe muss deshalb als eigener Testfaktor behandelt werden und darf nicht aus der bloßen Anwesenheit eines Vendor-Interfaces geschlossen werden.

---

### Öffentliche Repositories und Praxisquellen

### [Q15] Linux Kernel USB Gadget Examples und FunctionFS

**Typ:** Repository / Referenzcode  
**Typische Fundstelle:** Linux kernel tree, `drivers/usb/gadget`, `Documentation/usb/gadget`, FunctionFS-Beispiele  
**Sicherheitsniveau:** sehr hoch für die Implementierungsebene  
**Belegwert:** hoch für Descriptor- und Composite-Aufbau

**Erwarteter oder belegter Inhalt:**

- Beispiele für Mehrfach-Functions und Composite-Gadgets.
- Konkrete Descriptor- und Endpoint-Strukturen.
- Unterschiede zwischen standardisierten Klassen und vendor-specific FunctionFS-Interfaces.
- Hinweise darauf, wie Interface-Reihenfolge und Function-Registrierung entstehen.

**Einordnung:**

Geeignet, um das tatsächlich präsentierte USB-Layout zu reproduzieren und Unterschiede zwischen alten und neuen Gadget-Versionen zu ermitteln.

---

### [Q16] USBView, `lsusb -v`, Wireshark USB Capture und USBPcap

**Typ:** Diagnosewerkzeuge / technische Fundstelle  
**Typische Fundstelle:** Linux `usbutils`, Windows USBView/USBPcap, Wireshark USB capture  
**Sicherheitsniveau:** hoch  
**Belegwert:** hoch für die konkrete Geräteevidenz

**Erwarteter oder belegter Inhalt:**

- Vollständige Device-, Configuration-, Interface- und Endpoint-Descriptoren.
- VID/PID, Strings, Interface-Klassen, IADs und Interface-Reihenfolge.
- Host-Requests nach Enumeration, soweit ein Capture möglich ist.
- MSC-spezifische SCSI-Kommandos und Control Requests.

**Einordnung:**

Für die konkrete Untersuchung sind solche Traces wesentlich stärker als allgemeine Aussagen über Automotive-Verhalten. Sie können direkt zeigen, ob die Head Unit MSC initialisiert oder nur das Vendor-Interface anfasst. Host-Capture-Methode: Research 2. Im Repo: `tools/lab_usbmon_capture.sh`.

---

### [Q17] Community-Berichte zu Automotive-USB-Kompatibilität

**Typ:** Praxisquelle / Forenwissen  
**Typische Fundstelle:** BMW-, Android-Auto-, CarPlay-, XDA-, Mikrocontroller- und USB-Gadget-Foren  
**Sicherheitsniveau:** niedrig bis mittel  
**Belegwert:** niedrig bis mittel

**Erwarteter oder belegter Inhalt:**

- Erfahrungsberichte zu USB-Sticks, Smartphones, MTP, Android Auto, CarPlay und Sondergeräten.
- Berichte über unterschiedliche Reaktionen bei verschiedenen Fahrzeugen oder Firmwareständen.
- Hinweise, dass identische Geräte an verschiedenen Head Units unterschiedlich behandelt werden.
- Teilweise Beobachtungen zu Kabeln, VID/PID, Dateisystemen oder Geräteidentitäten.

**Einordnung:**

Solche Berichte können Hypothesen liefern, sind aber ohne Descriptor-Capture und Firmwarekontext nicht ausreichend, um eine allgemeine Regel für Composite-vs-vendor-only abzuleiten.

---

## 3. Technische Bewertung mit eingetragenen Quellenankern

### 3.1 Composite vs vendor-only aus USB-Sicht

#### Composite Device

Ein Composite Device stellt mehrere Interfaces in einer Configuration bereit. Beispiel:

```text
Device Descriptor
 ├── bDeviceClass = 0xEF oder 0x00
 ├── bDeviceSubClass = abhängig von Struktur
 ├── bDeviceProtocol = abhängig von Struktur
 └── Configuration 1
      ├── Interface 0: MSC
      └── Interface 1: Vendor Specific
```

Alternativ kann das Device-Level-Class-Feld `0x00` sein, wobei die Klassifizierung auf Interface-Ebene erfolgt. Das ist ein übliches Muster und kein Fehler.

**Belegt [Q1][Q4]:**

- Die relevanten Klassen werden bei Composite-Geräten typischerweise auf Interface-Ebene angegeben.
- Ein Host kann die einzelnen Interfaces getrennt klassifizieren.
- Ein Composite Device kann mehrere unabhängige Funktionstreiber aktivieren.

#### Vendor-only Device

Ein vendor-only-Gerät kann beispielsweise so aussehen:

```text
Device Descriptor
 └── Configuration 1
      └── Interface 0
           ├── bInterfaceClass    = 0xFF
           ├── bInterfaceSubClass = vendor-defined
           ├── bInterfaceProtocol = vendor-defined
           └── Bulk IN / Bulk OUT
```

**Belegt [Q3]:**

- Der Host erkennt ein Interface als vendor-specific.
- Daraus folgt aber noch keine bestimmte Funktion.
- Ohne bekannten Treiber, VID/PID, Protokoll oder Handshake kann das Interface ungenutzt bleiben.

#### Wesentlicher Unterschied

Der Unterschied ist nicht nur:

```text
MSC vorhanden: ja/nein
```

Sondern möglicherweise gleichzeitig:

- andere Interface-Anzahl,
- andere Interface-Reihenfolge,
- andere Endpoint-Adressen,
- andere Configuration-Größe,
- andere Device-Class-Werte,
- andere IAD-Struktur,
- andere Strings,
- andere VID/PID,
- andere Host-Treiberpfade.

**Belegt [Q1][Q4][Q8][Q15].**

Das bedeutet für den Vergleich alter und neuer Setups:

> Wenn beim Wechsel von MSC+Vendor zu vendor-only mehrere Descriptoren geändert wurden, wurde nicht nur MSC entfernt. Es wurde möglicherweise die komplette Geräteidentität verändert.

---

### 3.2 Rolle von MSC und Standardklassen

#### Warum MSC das Hostverhalten stark beeinflussen kann

MSC ist eine bekannte Standardklasse. Viele Hoststacks besitzen dafür einen etablierten Verarbeitungspfad:

1. Device wird enumeriert.
2. Configuration wird gelesen.
3. MSC-Interface wird erkannt.
4. Bulk-Only-Transport wird initialisiert.
5. LUNs und Medium werden abgefragt.
6. SCSI-Inquiry und Kapazitätsabfragen werden ausgeführt.
7. Dateisystem- oder Medienlogik wird aktiviert.
8. Die Head-Unit-Anwendung erhält einen Status wie:
   - Medium vorhanden,
   - USB Storage verfügbar,
   - Quelle erkannt,
   - unterstütztes oder nicht unterstütztes Medium.

**Belegt [Q2][Q5][Q9].**

Dieser Prozess kann bereits vor jeder Vendor-Protokollaktivität stattfinden.

#### Gibt es eine allgemeine MSC-Priorisierung?

**Nicht als allgemeine USB-Regel.**

Ein korrekter Composite-fähiger Host kann:

- MSC an einen Mass-Storage-Treiber binden,
- das Vendor-Interface separat behandeln,
- beide parallel oder teilweise nutzen,
- ein Interface ignorieren,
- oder bei unvollständiger Unterstützung nur eines der Interfaces aktivieren.

**Belegt [Q10][Q11].**

Eine Automotive-Head Unit kann jedoch einen vereinfachten oder selektiven Parser besitzen. Dann sind folgende Verhaltensweisen plausibel:

- Nur die erste Configuration wird berücksichtigt.
- Nur bestimmte Interface-Klassen werden verarbeitet.
- Das erste bekannte Standardklassen-Interface bestimmt die grobe Gerätezuordnung.
- Vendor-Interfaces werden ignoriert, wenn kein bekannter VID/PID- oder String-Match vorliegt.
- Ein Composite Device wird zwar enumeriert, aber nur als Storage verwendet.
- Das Gerät wird wegen einer nicht erwarteten Kombination als „unsupported“ eingestuft.

**Wahrscheinlich, aber nicht BMW-spezifisch belegt [Q12][Q13][Q17].**

#### Medien-/Dateireaktionen versus Accessory-/Vendor-Reaktionen

Reaktionen wie:

- USB-Medium erkannt,
- Ordner-/Dateiansicht erscheint,
- Musikquelle wird aktiviert,
- „No playable files“,
- „USB device not supported“ im Medienkontext

sind eher mit dem MSC-/Media-Pfad vereinbar.

Reaktionen wie:

- Accessory connected,
- Android-Auto-Start,
- Smartphone-Verbindung,
- proprietäre Aktivierung,
- ein bestimmter Vendor-Handschlag

benötigen dagegen zusätzliche Evidenz:

- Control Requests,
- bekannte VID/PID,
- passende Strings,
- spezifische Bulk-Kommunikation,
- AOA-/Android-Auto-Handshake.

**Belegt für die Protokolltrennung [Q2][Q5][Q6][Q7].**

---

### 3.3 Automotive Hosts / Head Units als Sonderfall

Automotive-USB-Hosts unterscheiden sich von generischen Desktop-Hosts typischerweise durch:

- begrenzte unterstützte Protokolle,
- fest definierte Medien- und Telefonpfade,
- proprietäre Anwendungsschichten,
- konservative oder vereinfachte USB-Parser,
- Abhängigkeit von Firmwarestand und Fahrzeugkonfiguration,
- UI-Zustände, die aus mehreren Erkennungsebenen zusammengesetzt werden.

**Wahrscheinlich [Q12][Q13][Q17].**

Ein Desktop-Linux- oder Windows-Host ist oft in der Lage, ein Composite Device sauber aufzuteilen. Eine Head Unit muss dagegen nicht alle Interfaces sichtbar machen. Sie kann beispielsweise:

```text
USB Enumeration
       ↓
bekannte Klasse gefunden?
       ↓
MSC-Pfad aktiviert
       ↓
Medienquelle angelegt
```

oder:

```text
USB Enumeration
       ↓
VID/PID/String-Match?
       ↓
proprietärer Gerätepfad
```

verwenden.

Es ist auch möglich, dass die USB-Enumeration selbst erfolgreich ist, aber die darüberliegende Head-Unit-Anwendung das Gerät nicht akzeptiert.

Daher müssen drei Ebenen getrennt werden:

1. **USB-Enumeration erfolgreich?**
2. **USB-Klasse oder Interface-Treiber aktiviert?**
3. **Head-Unit-Anwendung erkennt eine nutzbare Funktion?**

Ein „Connected“-UI-Status kann auf jeder dieser Ebenen oder auf einer darüberliegenden Zustandsmaschine beruhen.

**Belegt als methodische Trennung [Q1][Q10][Q16]; konkrete NBT-Zuordnung offen [Q13].**

---

### 3.4 Descriptor- und Identity-Faktoren

#### VID/PID

**Belegt:**

- VID/PID können zur Identifikation eines bekannten Geräts oder Protokolls verwendet werden.
- Ein Host kann für einen bekannten VID/PID-Satz einen spezifischen Treiber oder Anwendungspfad auswählen.
- AOA- und Android-nahe Modi verwenden definierte, bekannte VID/PID-Kombinationen.

**Wahrscheinlich für Automotive:**

- Head Units können bekannte Smartphone-, iPod-, Accessory- oder Hersteller-IDs bevorzugen.
- Ein unbekannter VID/PID kann trotz korrekter Enumeration ignoriert oder als nicht unterstützt dargestellt werden.

**Offen:**

- Welche konkreten VID/PID-Werte eine bestimmte BMW-/NBT-Version akzeptiert.
- Ob VID/PID allein genügt oder mit Strings und Protokollverhalten kombiniert wird.

#### Device Class / Subclass / Protocol

**Belegt:**

- Device-Level- und Interface-Level-Klassen sind unterschiedliche Klassifikationsstellen.
- MSC wird üblicherweise über die Interface-Klasse identifiziert.
- Vendor-specific Interfaces haben keine standardisierte Funktion.

**Wahrscheinlich:**

- Vereinfachte Hosts können stärker auf Device-Level-Werte reagieren.
- Ein ungewöhnliches Device-Level-Layout kann einen Parser beeinflussen, selbst wenn die Interfaces formal korrekt sind.

**Offen:**

- Ob der konkrete Host Device-Level- oder Interface-Level-Class-Werte priorisiert.

#### Interface-Reihenfolge

**Belegt:**

- Interfaces besitzen Interface-Nummern.
- Die Reihenfolge ist Teil der präsentierten Descriptorstruktur.
- Composite-Implementierungen können durch Function-Reihenfolge unterschiedliche Interface-Nummern erzeugen.

**Wahrscheinlich:**

- Robuste Hosts sollten nicht allein anhand der Reihenfolge entscheiden.
- Ältere, vereinfachte oder proprietäre Parser können dennoch eine feste Reihenfolge erwarten.
- Wird MSC als Interface 0 statt als Interface 1 präsentiert, kann das bei fehlerhaften Implementierungen einen Unterschied machen.

**Offen:**

- Ob eine konkrete Head Unit Interface 0 bevorzugt oder nur nach Klassen sucht.

Eine Behauptung wie „BMW behandelt immer das erste Interface als Hauptfunktion“ ist ohne spezifischen Trace nicht belastbar.

#### Configuration Structure

**Belegt:**

- Viele Geräte bieten nur eine Configuration an.
- Ein Host kann nur eine Configuration aktivieren.
- Mehrere Configurations werden nicht von allen Hosts gleich behandelt.

**Wahrscheinlich:**

- Für Automotive-Geräte ist eine einzige, einfache Configuration die sicherere Annahme.
- Mehrere Configurations oder ungewöhnliche Alternative Settings können Kompatibilitätsprobleme verursachen.

**Offen:**

- Ob die konkrete Head Unit mehrere Configurations oder nur Configuration 1 unterstützt.
- Ob sie alternative Settings korrekt behandelt.

#### IADs

**Belegt:**

- IADs können mehrere Interfaces zu einer Funktion gruppieren.
- Sie sind besonders für Multi-Interface-Funktionen relevant.

**Wahrscheinlich:**

- Ein IAD kann für einen Host entscheidend sein, wenn dieser die logische Funktion nur über IADs erkennt.
- Ein MSC-Interface sollte nicht künstlich mit einem Vendor-Interface gruppiert werden, wenn beide für den Host unabhängige Funktionen darstellen sollen.

**Offen:**

- Ob und wie die konkrete Head Unit IADs auswertet.

#### String Descriptors

Relevante Strings sind insbesondere:

- Manufacturer,
- Product,
- Serial Number,
- Interface String.

**Belegt:**

- Strings sind Teil der USB-Descriptorstruktur.
- Sie können von Anwendungen zur Anzeige oder Geräteidentifikation verwendet werden.

**Wahrscheinlich:**

- Head Units können Strings für UI-Anzeigen, Gerätekategorien oder heuristische Erkennung nutzen.
- „Android“, „Phone“, „Accessory“, „iPhone“ oder herstellerspezifische Begriffe können die Diagnose erschweren, wenn sie nicht mit dem tatsächlichen Protokoll übereinstimmen.

**Offen:**

- Ob die konkrete Head Unit Strings tatsächlich klassifikatorisch nutzt oder nur anzeigt.

#### Phone-like Identity

Eine phone-like identity kann eine Kombination sein aus:

- bekanntem VID/PID,
- Hersteller-/Produktstrings,
- bekannten Device-Class-Werten,
- typischem Interface-Layout,
- AOA-/MTP-/proprietärem Protokoll,
- bekannten Control Requests,
- spezifischen Endpoint- und Interface-Mustern.

**Wichtig:**

Ein Vendor-Interface allein erzeugt keine phone-like identity.

Ebenso erzeugt ein MSC-Interface allein keine Smartphone-Identität.

**Belegt [Q3][Q6][Q7].**

**Wahrscheinlich:**

Eine Head Unit kann bei einem bekannten Telefonprofil anders reagieren als bei einem generischen Vendor-Gerät mit identischem Bulk-Transfer-Layout.

---

### 3.5 Methodisches Risiko von Composite-verunreinigten Befunden

#### Wie stark kann ein MSC+Vendor-Composite Beobachtungen verfälschen?

Die Antwort hängt von der beobachteten Reaktion ab.

| Beobachtung | Risiko einer MSC-Verfälschung |
|---|---:|
| USB-Gerät erscheint grundsätzlich als verbunden | mittel bis hoch |
| Medien-/Dateimenü erscheint | hoch |
| „Keine Dateien“ oder „nicht unterstütztes Medium“ | sehr hoch |
| USB-Quelle wird aktiviert | hoch |
| Head Unit sendet SCSI-Kommandos | sehr hoch |
| AOA-Handshake nachweisbar | niedrig für MSC als alleinige Erklärung |
| Vendor-Bulk-Transfer nachweisbar | niedrig bis mittel |
| Android-Auto-/Accessory-Start ohne Protokolltrace | mittel bis hoch |
| UI ändert sich, Ursache unbekannt | hoch |
| Gerät wird nur bei MSC+Vendor, nicht vendor-only erkannt | hoch für MSC-/Composite-Effekt |

#### Besonders angreifbare Beobachtungen

Folgende frühere Claims wären methodisch stark angreifbar:

- „Die Vendor-Funktion verursacht die sichtbare USB-Verbindung.“
- „Die Head Unit akzeptiert das Vendor-Interface.“
- „Die Head Unit erkennt das Gerät als Accessory.“
- „Das Gerät wird als Telefon erkannt.“
- „Die Aktivierung zeigt AOA-Kompatibilität.“
- „Die Head Unit reagiert auf das Vendor-Interface, weil sie ein beliebiges `0xFF`-Interface unterstützt.“

Diese Schlussfolgerungen sind nicht haltbar, wenn gleichzeitig ein MSC-Interface vorhanden war und kein Trace den Vendor-Handshake belegt.

#### Weniger angreifbare Beobachtungen

Weniger stark betroffen wären:

- ein eindeutig aufgezeichneter AOA-Control-Handshake,
- ein spezifischer Vendor-Bulk-Transfer,
- ein exakt dokumentierter Wechsel der Host-Requests nach Entfernung von MSC,
- eine Reaktion, die ausschließlich bei unverändertem VID/PID, unveränderten Strings und unveränderter Vendor-Funktion auftritt und nach Entfernung von MSC reproduzierbar bleibt.

Auch hier gilt: Ohne Capture bleibt die Kausalität eingeschränkt.

#### Bewertung des Risikos

Für das konkrete Projekt ist eine **mittlere bis hohe methodische Verunreinigung** wahrscheinlich, wenn:

1. das alte Setup tatsächlich MSC bereitstellte,
2. die beobachtete Reaktion im Medien-/USB-UI auftrat,
3. kein Host-Trace vorliegt,
4. Vendor- und MSC-Aktivität nicht getrennt wurden,
5. sich beim späteren vendor-only-Setup zusätzlich VID/PID, Strings oder Interface-Reihenfolge änderten.

---

### 3.6 Öffentliche Praxisbelege und Vergleichsfälle

#### Gibt es belastbare Automotive-Fälle?

Öffentlich gut belegt sind allgemeine Automotive-Funktionsunterschiede zwischen:

- USB Storage,
- Smartphones,
- MTP,
- iPod-/Apple-Protokollen,
- Android Auto,
- CarPlay,
- nicht unterstützten Geräten.

Weniger gut belegt sind konkrete kontrollierte Experimente mit:

```text
identische VID/PID
identische Strings
identische Vendor-Funktion
nur Unterschied: MSC vorhanden vs entfernt
```

für dieselbe Head Unit.

**Bewertung:**

- Allgemeine Automotive-Kompatibilitätsunterschiede: **belegt bis wahrscheinlich**.
- Konkrete BMW-/NBT-Regel „MSC wird immer gegenüber Vendor bevorzugt“: **nicht belegt**.
- Konkrete Fälle, in denen genau das Hinzufügen von MSC eine NBT-UI-Reaktion verursacht: **offen**, sofern kein reproduzierbarer Capture vorliegt.
- Allgemeine Möglichkeit, dass MSC einen alternativen Medienpfad auslöst: **belegt und technisch hoch plausibel**.

#### Composite versus vendor-only in allgemeinen Hosts

Hier ist die Evidenz deutlich stärker:

- Linux und Windows können Composite-Interfaces separat behandeln.
- MSC kann unabhängig von einem Vendor-Interface aktiviert werden.
- Vendor-only kann korrekt enumerieren, aber funktional ungenutzt bleiben.
- Unterschiedliche Host-Implementierungen können nur einen Teil der Interfaces unterstützen.

**Belegt [Q8][Q10][Q11][Q15].**

#### „Composite funktioniert, vendor-only nicht“

Das ist technisch plausibel, wenn der Host nur das MSC-Interface kennt.

Das bedeutet aber nicht, dass der Host das Vendor-Interface erkannt hat. Es kann schlicht bedeuten:

```text
Composite:
  MSC bekannt → Gerät erscheint nutzbar

Vendor-only:
  kein bekannter Klassenhandler → keine sichtbare Funktion
```

**Wahrscheinlich [Q2][Q3][Q10].**

#### „Vendor-only funktioniert, Composite nicht“

Auch das ist plausibel, wenn:

- der Host einen spezifischen Vendor-/Accessory-Pfad erwartet,
- das Composite-Layout den Parser verwirrt,
- IADs oder Interface-Reihenfolge nicht passen,
- eine bekannte Geräteidentität durch das zusätzliche MSC-Interface nicht mehr wie erwartet aussieht.

**Wahrscheinlich, aber nicht allgemein belegt für Automotive [Q4][Q8][Q13].**

---

### 3.7 Diagnosewert für das konkrete Projekt

Die frühere MSC+Vendor-Session sollte nicht als direkter Nachweis einer Vendor- oder Accessory-Wirkung behandelt werden, solange nicht mindestens einer der folgenden Nachweise vorliegt:

- MSC-Requests und Vendor-Requests sind getrennt aufgezeichnet.
- Die Reaktion bleibt bei identischem Descriptor- und Identity-Setup erhalten, wenn MSC entfernt wird.
- Die Head Unit sendet einen spezifischen Vendor-/AOA-Handshake.
- Die UI-Reaktion ist unabhängig von Medien-, Dateisystem- oder Storage-Zuständen.
- Ein Kontrollgerät mit reinem MSC erzeugt die Reaktion nicht, während das vendor-only-Gerät sie erzeugt.

Die alte Session ist besonders kritisch, wenn die sichtbare Wirkung lediglich war:

- Gerät wird erkannt,
- USB-Symbol erscheint,
- Medienquelle wird angeboten,
- eine „connected“-Meldung wird angezeigt,
- die Head Unit fragt nach Dateien oder Medien.

In diesen Fällen ist MSC eine mindestens ebenso plausible Erklärung wie ein Vendor- oder Accessory-Pfad.

---

## 4. Vorläufiges Fazit mit Quellenankern

1. **Gesichert:** Ein MSC+Vendor-Composite stellt dem Host mindestens zwei unterschiedliche USB-Funktionen bereit. Die Funktionen können auf Interface-Ebene getrennt behandelt werden [Q1][Q2][Q3].
2. **Gesichert:** MSC aktiviert einen standardisierten Hostpfad mit Bulk-Only-Transport und typischerweise weiteren SCSI-/Medienabfragen [Q2][Q5].
3. **Gesichert:** Ein vendor-only Interface besitzt ohne passenden Treiber, VID/PID-Match oder Protokoll nicht automatisch eine sichtbare Funktion [Q3][Q10].
4. **Gesichert:** Es gibt keine allgemeine USB-Regel, nach der MSC bei jedem Composite Device automatisch alle Vendor-Interfaces verdrängt [Q10][Q11].
5. **Wahrscheinlich:** Eine Automotive-Head Unit kann einen vereinfachten oder selektiven Parser besitzen und deshalb bei Composite-Geräten nur eine Funktion nutzen oder eine Standardklasse gegenüber einer unbekannten Vendor-Funktion bevorzugen [Q12][Q13][Q17].
6. **Wahrscheinlich:** MSC kann insbesondere Medien-, Datei-, USB-Quelle- und „unsupported medium“-Reaktionen erzeugen, die fälschlich als Accessory- oder Vendor-Reaktion interpretiert werden [Q2][Q5][Q12].
7. **Wahrscheinlich:** Interface-Reihenfolge, IADs, Device-Class-Werte, Strings und VID/PID können bei proprietären oder vereinfachten Automotive-Parsern relevant sein; eine generelle BMW-/NBT-Regel ist jedoch nicht öffentlich belastbar belegt [Q4][Q8][Q13].
8. **Gesichert:** AOA-/Accessory-Nähe lässt sich nicht aus einem beliebigen Vendor-Interface ableiten. Dafür sind ein passendes Protokoll, ein Handshake und häufig eine bekannte Identität erforderlich [Q6][Q7].
9. **Bewertung:** Frühere Beobachtungen aus einem unbeabsichtigten MSC+Vendor-Setup sollten mit **mittlerer bis hoher methodischer Vorsicht** bewertet werden. Medien- und allgemeine Connected-Reaktionen sind besonders stark gefährdet.
10. **Offen:** Ohne USB-Capture oder kontrollierten A/B-Test lässt sich nicht bestimmen, ob die konkrete BMW-/NBT-Head Unit im Einzelfall MSC priorisierte, das Vendor-Interface ignorierte oder beide Funktionen parallel verarbeitete.

---

## 5. Repositories, Dokus, Praxisbeispiele und technische Fundstellen

| Fundstelle | Warum relevant | Betroffene Ebene | Konkret prüfen |
|---|---|---|---|
| USB 2.0 Specification, USB-IF | Grunddefinition von Device-, Configuration-, Interface- und Endpoint-Descriptors | USB-Spezifikation | Device Class, Interface Class, Configurations, Interface-Nummern |
| USB Mass Storage Class Specification | Definition der MSC-Erkennung und des Klassenverhaltens | USB-Klasse | `0x08/0x06/0x50`, LUNs, BOT |
| USB Mass Storage Bulk-Only Transport | Erklärt Hostverkehr nach Enumeration | Protokoll | Reset, `GET_MAX_LUN`, CBW/CSW, SCSI |
| USB IAD-Dokumentation | Gruppierung mehrerer Interfaces | Descriptorstruktur | IAD vorhanden, korrekt platziert, Interface-Anzahl |
| Linux `drivers/usb/gadget` | Referenz für Composite-Gadget-Aufbau | Device-Seite | Function-Reihenfolge, Endpoint-Layout |
| Linux ConfigFS Gadget Documentation | Aufbau von MSC+Vendor-/FunctionFS-Kombinationen | Gadget-Konfiguration | Erzeugte Descriptoren und Interface-Nummern |
| Linux Mass Storage Gadget Function | Standardisierte MSC-Gadget-Funktion | Device-/Klassenebene | Backing File, Removable, LUN, Inquiry |
| Linux FunctionFS / Vendor Functions | Umsetzung vendor-specific USB-Funktionen | Vendor-Interface | Class/Subclass/Protocol, Bulk-Endpunkte |
| Android Open Accessory Protocol | AOA-Control-Handshake und Accessory-Mode | Accessory-Protokoll | Control Requests, Strings, Mode Switch |
| Android/AOSP USB Accessory Framework | AOA-Identität und Accessory-Erkennung | Plattformidentität | VID/PID, Accessory-PIDs, ADB-/Audio-Varianten |
| Microsoft USB Composite Device Docs | Beispiel eines Composite-Hoststacks | Hosttreiber | Parent-/Function-Treiber, Interface Binding |
| `lsusb -v` / USBView | Descriptorvergleich | Diagnose | Vollständige Descriptoren beider Setups |
| Wireshark USB Capture / USBPcap | Host-Request-Analyse | Laufzeitverhalten | MSC-Requests versus Vendor-/AOA-Requests |
| BMW-/NBT-Foren und Reverse-Engineering-Projekte | Hinweise auf beobachtete Headunit-Funktionen | Automotive-Praxis | Nur mit Fahrzeug-, Firmware- und Trace-Kontext verwenden |
| Android-Auto-/Car-Accessory-Projekte | Praktische Accessory-Interoperabilität | Automotive-/Accessory-Ebene | Protokoll- und Identity-Anforderungen |

### Descriptor-Beispiele

#### Vendor-only

```text
Device Descriptor
  bDeviceClass    = 0x00
  idVendor        = test/vendor VID
  idProduct       = test PID

Configuration 1
  Interface 0
    bInterfaceClass    = 0xFF
    bInterfaceSubClass = 0x00
    bInterfaceProtocol = 0x00
    Endpoint IN        = Bulk
    Endpoint OUT       = Bulk
```

#### MSC+Vendor Composite

```text
Device Descriptor
  bDeviceClass    = 0x00 oder 0xEF
  idVendor        = identisch oder verändert
  idProduct       = identisch oder verändert

Configuration 1
  Interface 0
    bInterfaceClass    = 0x08
    bInterfaceSubClass = 0x06
    bInterfaceProtocol = 0x50
    Endpoint IN        = Bulk
    Endpoint OUT       = Bulk

  Interface 1
    bInterfaceClass    = 0xFF
    bInterfaceSubClass = 0x00
    bInterfaceProtocol = 0x00
    Endpoint IN        = Bulk
    Endpoint OUT       = Bulk
```

Das Beispiel ist bewusst schematisch. Ein gültiges konkretes Gadget kann andere Device-Class-Werte, Interface-Nummern, IADs, Strings und Endpoint-Adressen verwenden.

### Bekannte Host-Inkompatibilitäten, die geprüft werden sollten

- Host verarbeitet nur Configuration 1.
- Host ignoriert IADs.
- Host erwartet MSC als erstes Interface.
- Host behandelt nur ein Interface.
- Host akzeptiert nur bekannte VID/PID-Kombinationen.
- Host zeigt Strings an, nutzt sie aber nicht zur Klassifikation.
- Host erkennt MSC, ignoriert Vendor.
- Host erkennt Vendor nur bei bestimmtem PID.
- Host enumeriert, startet aber keinen funktionalen Protokollpfad.
- Host beginnt MSC-Abfragen, obwohl das Medium leer oder nicht mountbar ist.

Keine dieser Möglichkeiten darf ohne Capture als BMW-/NBT-spezifischer Fakt bezeichnet werden.

---

## 6. Praktische Hinweise für das konkrete Problem

### 6.1 Wie stark muss eine alte Session abgewertet werden?

Die Abwertung sollte nach dem beobachteten Ergebnis differenziert werden.

#### Hohe Abwertung erforderlich

Alte Claims sollten stark abgewertet werden, wenn sie lauten:

- „Vendor-only wurde von der Head Unit erkannt“, obwohl das alte Gerät MSC+Vendor war.
- „Accessory-Mode wurde ausgelöst“, ohne AOA-/Accessory-Handshake.
- „Connected zeigt die Akzeptanz des Vendor-Interfaces.“
- „Die Head Unit reagiert auf das Vendor-Protokoll“, obwohl parallel MSC-Transfers stattfanden.
- „Die Head Unit erkennt ein Telefon“, obwohl nur USB-Storage-Funktionen beobachtet wurden.

Empfohlene Einstufung:

```text
Evidenzstatus: hypothesengeleitet / composite-contaminated
Vertrauensniveau: niedrig bis mittel
```

#### Mittlere Abwertung

Mittelmäßig gefährdet sind:

- allgemeine USB-Verbindungsanzeigen,
- Erscheinen einer USB-Quelle,
- Statusänderungen ohne erkennbare Medienansicht,
- zeitliche Korrelation zwischen Vendor-Aktivität und UI-Reaktion.

Empfohlene Einstufung:

```text
Evidenzstatus: plausibel, aber nicht kausal isoliert
Vertrauensniveau: mittel
```

#### Geringere Abwertung

Weniger gefährdet sind:

- eindeutig aufgezeichnete Vendor-Requests,
- AOA-Control-Transfers,
- bekannte Accessory-PID nach einem dokumentierten Mode Switch,
- Vendor-Kommunikation bei einem reinen vendor-only-Kontrollgerät.

Empfohlene Einstufung:

```text
Evidenzstatus: protokollseitig belegt
Vertrauensniveau: mittel bis hoch
```

---

### 6.2 Welche Beobachtungen könnten eher durch MSC ausgelöst worden sein?

Besonders MSC-verdächtig sind:

- Head Unit bietet eine Musik-/Dateiquelle an.
- UI zeigt USB-Medium oder USB-Quelle.
- Meldung „keine abspielbaren Dateien“.
- Meldung „USB-Gerät nicht unterstützt“ im Medienkontext.
- Längere Aktivität unmittelbar nach Enumeration.
- Wiederholte Initialisierung nach Medien- oder LUN-Abfragen.
- Reaktion tritt auch auf, wenn das Vendor-Protokoll nicht angesprochen wird.
- Gerät wird unabhängig vom Vendor-Handshake sichtbar.
- Host sendet `GET_MAX_LUN`, SCSI Inquiry, Test Unit Ready oder Read Capacity.

Wenn solche Transfers beobachtet wurden, ist MSC als Ursache mindestens stark plausibel.

---

### 6.3 Wie sollte ein sauberer Neuvergleich aufgebaut werden?

Es sollte eine kontrollierte Matrix verwendet werden.

#### Testgruppe A: reines vendor-only

```text
Device identity: festgelegt
Configuration count: 1
Interface count: 1
Interface class: 0xFF
Endpoints: festgelegt
Strings: festgelegt
VID/PID: festgelegt
```

#### Testgruppe B: MSC-only

```text
Device identity: möglichst separat dokumentiert
Interface count: 1
Interface class: 0x08/0x06/0x50
LUN: festgelegt
Medium: festgelegt
```

#### Testgruppe C: MSC+Vendor Composite

```text
Alle Parameter dokumentieren:
- Interface-Reihenfolge
- IAD
- Device Class
- Endpoint-Adressen
- Strings
- VID/PID
- MSC-Medium
```

#### Testgruppe D: vendor-only mit phone-like identity

Nur gezielt variieren:

- VID/PID,
- Manufacturer,
- Product,
- Serial,
- Interface Class,
- Device Class,
- Protokollhandshake.

Dabei muss klar festgehalten werden, dass dies kein reiner Klassenvergleich mehr ist, sondern ein Identity-Vergleich.

#### Testgruppe E: AOA-/Accessory-nah

Separat prüfen:

- AOA-Control-Requests,
- Protocol-Version,
- Manufacturer,
- Model,
- Description,
- Version,
- URI,
- Serial,
- Mode Switch,
- resultierender VID/PID.

AOA-nah darf nicht lediglich „Vendor-only mit Accessory-String“ bedeuten.

---

### 6.4 Descriptoren und Interfaces, die strikt kontrolliert werden müssen

Für jedes Testgerät sollte ein vollständiger Descriptor-Dump gespeichert werden.

Mindestens zu dokumentieren:

#### Device Descriptor

- `bcdUSB`
- `bDeviceClass`
- `bDeviceSubClass`
- `bDeviceProtocol`
- `bMaxPacketSize0`
- `idVendor`
- `idProduct`
- `bcdDevice`
- `iManufacturer`
- `iProduct`
- `iSerialNumber`
- `bNumConfigurations`

#### Configuration Descriptor

- `wTotalLength`
- `bNumInterfaces`
- `bConfigurationValue`
- `iConfiguration`
- `bmAttributes`
- `bMaxPower`

#### Interface Descriptor

Für jedes Interface:

- Interface-Nummer
- Alternate Setting
- `bNumEndpoints`
- `bInterfaceClass`
- `bInterfaceSubClass`
- `bInterfaceProtocol`
- Interface String
- Zugehöriger IAD
- erwarteter Treiber oder Protokollhandler

#### Endpoint Descriptor

- Endpoint-Adresse
- Direction
- Transfer Type
- Maximum Packet Size
- Interval
- Bulk-/Interrupt-/Isochronous-Eigenschaften

#### Laufzeitverkehr

- Enumeration Requests
- `GET_DESCRIPTOR`
- `SET_CONFIGURATION`
- `GET_MAX_LUN`
- MSC Reset
- SCSI Inquiry
- Test Unit Ready
- Read Capacity
- Vendor Control Requests
- Vendor Bulk Transfers
- AOA-Control-Requests
- USB-Reset-Ereignisse
- Suspend/Resume
- Disconnect/Reconnect

---

### 6.5 Welche Unterschiede gezielt getestet werden sollten

Die höchste Priorität haben folgende A/B-Vergleiche:

1. **Identisches Vendor-only-Gerät mit und ohne MSC**
   - gleiche VID/PID,
   - gleiche Strings,
   - gleiche Vendor-Endpunkte,
   - nur MSC hinzufügen oder entfernen.
2. **MSC-only gegen MSC+Vendor**
   - zeigt, ob die sichtbare Reaktion bereits allein durch MSC entsteht.
3. **Vendor-only gegen phone-like vendor-only**
   - verändert nur Identity-Faktoren,
   - nicht gleichzeitig das Protokoll.
4. **Gleiche Interface-Klassen, unterschiedliche Reihenfolge**
   - MSC zuerst,
   - Vendor zuerst.
5. **Gleiche Interfaces mit und ohne IAD**
   - sofern das Gadget dies erlaubt.
6. **Gleiche Identität, unterschiedliche Device-Class-Werte**
   - `0x00`,
   - `0xEF/0x02/0x01`,
   - gegebenenfalls weitere formal gültige Varianten.
7. **Gleiche Descriptoren mit leerem versus gültigem MSC-Medium**
   - zeigt, ob bereits die MSC-Enumeration oder erst die Medienerkennung die UI verändert.
8. **Vendor-only ohne Protokollaktivität**
   - trennt reine Enumeration von funktionaler Vendor-Nutzung.
9. **AOA-Handschlag ohne MSC**
   - zeigt, ob der Host Accessory-Verhalten unabhängig von Storage unterstützt.

---

### 6.6 Welche Claims aus alten Composite-caveated Sessions nicht mehr abgeleitet werden sollten?

Folgende Aussagen sollten ausdrücklich zurückgezogen oder als unbewiesen markiert werden:

- „Die Head Unit unterstützt beliebige Vendor-only-Geräte.“
- „Die Head Unit reagiert auf `bInterfaceClass = 0xFF`.“
- „Das Gerät wurde als Accessory erkannt.“
- „Die UI-Anzeige belegt eine Vendor- oder AOA-Aktivierung.“
- „Die Head Unit hat das Smartphone-/Phone-Profil erkannt.“
- „Die Verbindung wurde durch das Vendor-Interface ausgelöst.“
- „Das Entfernen oder Hinzufügen bestimmter Vendor-Requests war die Ursache der UI-Reaktion.“
- „MSC war für die Beobachtung irrelevant.“

Solche Claims sind erst wieder zulässig, wenn ein kontrollierter vendor-only-Test oder ein Laufzeittrace sie stützt.

Für die hu-probe-Feldnotizen ist diese Liste nicht pauschal anzuwenden. Der P0-Bericht zu FW 0.3.23 ist bereits composite-caveated. Der Abendlauf FW 0.3.32 ist vendor-only und zeigt die Zubehör-UI weiter. Siehe Abschnitt 8.

---

## 7. Ehrliche Quellenlage

### Was mit hoher Sicherheit gesagt werden kann

- Ein MSC+Vendor-Composite-Gerät bietet dem Host mehrere eigenständige USB-Funktionen an.
- MSC wird über standardisierte Interface-Descriptoren erkannt und kann einen eigenen Hostpfad mit SCSI-/Medienabfragen auslösen.
- Ein vendor-only Interface erzeugt ohne passenden Treiber, bekannte Identität oder Protokoll nicht automatisch eine funktionale Hostreaktion.
- Composite-Interfaces können von einem Host getrennt behandelt, teilweise ignoriert oder unterschiedlich gebunden werden.
- AOA-/Accessory-Verhalten ist nicht aus einem beliebigen Vendor-Interface ableitbar.
- Frühe Beobachtungen aus einem MSC+Vendor-Setup sind für Medien- und allgemeine Connected-Reaktionen methodisch kontaminiert.

### Was plausibel, aber noch live zu verifizieren ist

- Eine bestimmte Automotive-Head Unit könnte MSC gegenüber einem unbekannten Vendor-Interface bevorzugen.
- Ein vereinfachter Headunit-Parser könnte Interface-Reihenfolge, IADs oder Device-Level-Class-Werte stärker berücksichtigen als ein Desktop-Host.
- BMW-/NBT-/NBT-Evo-Firmwarestände könnten sich bei Composite-Layouts unterschiedlich verhalten.
- VID/PID, Strings und phone-like descriptors könnten zusammen eine andere Anwendungsklassifikation auslösen als ein neutrales vendor-only-Layout.
- Das Hinzufügen von MSC könnte bei einer bestimmten Head Unit eine andere UI-Reaktion erzeugen als ein sonst identisches vendor-only-Gerät.

### Was offen bleibt

- Ob die konkrete BMW-/NBT-Head Unit MSC tatsächlich priorisiert.
- Ob sie mehrere Interfaces parallel verarbeitet.
- Ob sie Vendor-Interfaces nach VID/PID, Strings, Interface-Klasse oder proprietären Requests erkennt.
- Ob eine frühere „Connected“- oder Aktivierungsreaktion durch MSC, Composite-Erkennung, Vendor-Kommunikation oder eine Kombination ausgelöst wurde.
- Ob die frühere Gerätekonfiguration neben MSC weitere unkontrollierte Unterschiede enthielt.
- Ob phone-like identity allein oder nur zusammen mit einem bestimmten Protokoll die beobachtete Reaktion auslösen kann.

---

## Gesamteinschätzung zur Diagnosefrage

Die Wahrscheinlichkeit, dass frühere Automotive-/Headunit-Beobachtungen durch ein unbeabsichtigtes **MSC+Vendor-Composite-Setup** methodisch verfälscht wurden, ist:

- **hoch** für Medien-, Datei-, USB-Quelle- und allgemeine Storage-Reaktionen,
- **mittel bis hoch** für unspezifische „Connected“- oder UI-Zustände,
- **mittel** für vermutete Accessory-/Phone-Reaktionen ohne Protokollnachweis,
- **niedrig bis mittel** für einen tatsächlich nachgewiesenen Vendor-/AOA-Handshake,
- **niedrig** nur dann, wenn ein vollständiger USB-Trace die relevante Vendor-/AOA-Kommunikation eindeutig belegt.

Die belastbarste nächste Maßnahme ist kein weiterer Vergleich auf UI-Ebene, sondern ein kontrollierter Descriptor- und USB-Trace-Vergleich zwischen:

```text
vendor-only
MSC-only
MSC+vendor composite
vendor-only mit phone-like identity
AOA-/Accessory-Protokoll mit vendor-only
```

Dabei müssen insbesondere **VID/PID, Device Class, Interface-Reihenfolge, IADs, Strings, Endpoints, Configuration-Struktur und tatsächlicher Hostverkehr** getrennt dokumentiert werden.

---

## 8. Repo-Abgleich (nicht Teil der gelieferten Notiz)

| Frage | Befund im Repo |
|-------|----------------|
| Gab es das Composite wirklich? | Ja. Bis FW 0.3.26 hat `static USBMSC` im Konstruktor `tinyusb_enable_interface(MSC)` aufgerufen, auch ohne `MscGadget::begin()`. Unter `aoa_observe` sah der Host **MSC(08)+Vendor(ff)**. |
| Ist der Leak noch im aktuellen Stand? | Nein. Fix `105c791`, FW **0.3.27**: `USBMSC` erst in `MscGadget::begin()`. Lab-Verifikation: Host nur noch **1× Interface class=ff**. [`2026-10-29-c1b-p05a-instrumentation.md`](2026-10-29-c1b-p05a-instrumentation.md) |
| Welche Feld-Session ist composite-caveated? | P0-Feld FW **0.3.23**. Der Bericht sagt das selbst: UI „Zubehör“ gilt für den damaligen Fingerprint, nicht als reiner Vendor-Beweis. [`2026-10-29-c1b-p0-aoa-observe-feld.md`](2026-10-29-c1b-p0-aoa-observe-feld.md) |
| Gibt es eine vendor-only Wiederholung an der HU? | Ja. Abendfeld FW **0.3.32**, `aoa_observe` `18D1:2D00`, vendor-only: HU erkennt Zubehör, Spinner „wartet auf App“, Bulk-RX bleibt 0. [`2026-10-29-c1b-b2b-knock-feld.md`](2026-10-29-c1b-b2b-knock-feld.md) |
| Sind die Boots heute XOR? | Ja. `msc`, `aoa_observe` und `aoa_phone_like` schließen sich aus. Soft-Composite MSC+AOA im selben Boot ist kein aktueller Pfad. [`../GATES-Connected.md`](../GATES-Connected.md) |
| Entspricht `aoa_phone_like` Testgruppe D? | Teilweise. VID/PID `04E8:6860`, Strings `Android` / `AOA Phone-like`, weiter vendor-only. B1 an der HU: Fingerprint abgelehnt („USB nicht unterstützt“), kein AOA-Handshake. Das ist ein Identity-Vergleich, kein reiner Klassenvergleich. |
| Ist die A/B-Matrix A–E gelaufen? | Nein. A (vendor-only nach 0.3.27) und ein Stück D sind da. B, C, Reihenfolge, IAD und „nur MSC hinzu“ an derselben Identität fehlen. E als Lab-Host gegen das Phone ist in [`2026-10-30-lab-aoa-aaidrive-phone-host.md`](2026-10-30-lab-aoa-aaidrive-phone-host.md), nicht als HU-gegen-Probe. |

Die Zubehör-UI vom Abend 0.3.32 steht damit nicht mehr unter dem Composite-Vorbehalt von 0.3.23. Sie bleibt UI-State, kein BCL-Nachweis. Ob die HU bei 0.3.23 zusätzlich den MSC-Pfad benutzt hat, ist ohne Host-Trace dieser Session offen.
