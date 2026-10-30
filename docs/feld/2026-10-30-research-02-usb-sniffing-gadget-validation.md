# Technischer Research-Report

## USB-Sniffing / Host-vs-Device Control Trace / Gadget Validation

### Methoden, Evidenzqualität und beste Validierungsstrategie für vollständige USB-Beobachtung

**Projekt:** hu-probe  
**Datum:** 2026-10-30  
**Reihe:** Research-Report 2 von 5  
**Problemfelder:** [`2026-10-30-aoa-problemfelder.md`](2026-10-30-aoa-problemfelder.md) — Block 3, Themen 1 und 5  
**Research 1:** [`2026-10-30-research-01-aoa-direct-accessory.md`](2026-10-30-research-01-aoa-direct-accessory.md)  
**Charakter:** gelieferte Recherche-Notiz. Exakte Tool-URLs und die Grenzen im konkreten Host-Setup sind im Text selbst als noch live zu prüfen markiert.

**Repo-Anker zur empfohlenen Host-Sicht:**

| Befund | Wo |
|--------|----|
| Lab-Host-Capture `usbmon2` + tshark, Handshake und Bulk | [`2026-10-30-lab-aoa-aaidrive-phone-host.md`](2026-10-30-lab-aoa-aaidrive-phone-host.md), Rohdaten [`artifacts/20261030-lab-aoa-aaidrive/`](artifacts/20261030-lab-aoa-aaidrive/) |
| Capture-Skript | `tools/lab_usbmon_capture.sh` |
| Device-Tap sieht SETUPs, die `USBVendor.onRequest` erreichen, auch Nicht-Vendor. Kein EP0-Sniffer vor dem Stack-Dispatch. Probe-seitig bleibt der Stage-Enum-Bug offen (51 → STALL). | [`2026-10-29-c1b-p05a-instrumentation.md`](2026-10-29-c1b-p05a-instrumentation.md), Lab-Bericht |

Der Lab-Lauf ist die Host-Sicht für **Debian als AOA-Host gegen das Phone**. Ein Host-Trace mit der **echten HU als Host gegen die Probe** fehlt weiter. Die methodische Kernaussage des Reports gilt für diese Lücke: ein Negativbefund nur aus dem Device-Tap trägt nicht.

---

## 1. Ziel der Untersuchung

Untersucht wurde, **mit welchen Methoden sich USB-Control- und Bulk-Verkehr so erfassen lässt, dass belastbare Aussagen über das tatsächliche Verhalten des Hosts gegenüber einem Gadget-/Accessory-/Embedded-Device möglich werden**.

Für das konkrete Problem ist das zentral, weil die bisherige Device-seitige Beobachtung offenbar **nicht als vollständiger EP0-/USB-Trace gelten kann**. Wenn ein vorhandener Tap nur vendor-fokussiert oder stacknah ist, dann ist aus einem Negativbefund wie:

- „Request 51/52/53 wurde nicht gesehen“

**nicht automatisch ableitbar**, dass der Host diese Requests wirklich nicht gesendet hat. Es gibt mindestens vier unterschiedliche Erklärungen:

1. Der Host hat den Request tatsächlich nicht gesendet.
2. Der Host hat ihn gesendet, aber die Device-Instrumentierung erfasst ihn nicht.
3. Der Request wurde gesendet, aber in einem Stack-Layer abgefangen oder intern verarbeitet.
4. Die Device-Sicht ist methodisch zu lückenhaft, um Negativbefunde zu tragen.

Die Diagnosefrage im Zentrum lautet daher:

> **Welche Sniffing- oder Validierungsmethode ist am besten geeignet, um in einem AOA-/Embedded-/USB-Gadget-Setup belastbar zu zeigen, ob bestimmte Control-Requests wirklich nicht gesendet wurden oder nur von der aktuellen Device-Instrumentierung nicht gesehen werden?**

---

## 2. Quellenbasis

Hinweis zur Quellenlage: Dies ist ein **quellenstrukturierter Research-Report mit konservativer Einordnung**. Wo etwas auf offizieller Doku beruht, ist das explizit markiert. Wo etwas typische Praxis oder Herstellerwissen ist, wird das getrennt gekennzeichnet.

### Offizielle Tool- und Systemdokumentation

### [Q1] Linux Kernel USB Monitoring / `usbmon`

**Typ:** Offizielle Primärquelle / Tool-Dokumentation  
**Typische Fundstelle:** Linux Kernel Documentation / `docs.kernel.org` / ältere `Documentation/usb/usbmon.rst`  
**Sicherheitsniveau:** sehr wahrscheinlich korrekt  
**Belegwert:** sehr hoch  
**Erwarteter oder belegter Inhalt:**

- `usbmon` ist das Linux-Subsystem zur Beobachtung von USB-I/O
- kann als Text-API und via mmap/Binary-Interface genutzt werden
- ist Grundlage für Host-seitige USB-Captures unter Linux, u. a. mit Wireshark

**Einordnung:**  
Zentrale Primärquelle für die Frage, **was Linux hostseitig überhaupt erfassen kann** und wie vollständig diese Sicht ist.

### [Q2] Wireshark USB Capture Documentation

**Typ:** Offizielle Primärquelle / Tool-Dokumentation  
**Typische Fundstelle:** `wireshark.org/docs/`, Wireshark Wiki / USB capture pages  
**Sicherheitsniveau:** sehr wahrscheinlich korrekt  
**Belegwert:** sehr hoch  
**Erwarteter oder belegter Inhalt:**

- Wireshark kann USB-Verkehr dekodieren
- unter Linux typischerweise über `usbmon`
- unter Windows typischerweise über USBPcap

**Einordnung:**  
Wichtig, weil Wireshark zwar sehr sichtbar ist, aber **nur so gut wie die zugrunde liegende Capture-Quelle**.

### [Q3] USBPcap Dokumentation

**Typ:** Offizielle Primärquelle / Tool-Dokumentation  
**Typische Fundstelle:** `desowin.org/usbpcap/` und USBPcap-Projektseiten  
**Sicherheitsniveau:** sehr wahrscheinlich korrekt  
**Belegwert:** hoch  
**Erwarteter oder belegter Inhalt:**

- USBPcap ist ein Windows-USB-Capture-Treiber
- stellt Capture-Daten für Wireshark bereit
- ist hostseitige Beobachtung im Windows-Stack

**Einordnung:**  
Relevant für Host-Sicht unter Windows. Wichtig für die Einordnung: **kein physikalischer In-Line-Sniffer**, sondern softwareseitiger Capture-Treiber.

### [Q4] Linux USB Gadget / ConfigFS / Gadget Debugging Doku

**Typ:** Offizielle Primärquelle / Tool-Dokumentation  
**Typische Fundstelle:** `docs.kernel.org/usb/`, `gadget_configfs`, USB gadget framework docs  
**Sicherheitsniveau:** sehr wahrscheinlich korrekt  
**Belegwert:** hoch  
**Erwarteter oder belegter Inhalt:**

- Architektur des Linux-Gadget-Frameworks
- Funktions- und Control-Handling im Gadget-Stack
- Debugging-/Tracing-Hinweise im Gadget-Kontext

**Einordnung:**  
Wichtig, um zu erklären, **warum Device-seitige Sicht oft stackabhängig und unvollständig** ist.

### [Q5] Microsoft / Windows USB ETW / USB Diagnostics

**Typ:** Offizielle Primärquelle / Systemdokumentation  
**Typische Fundstelle:** Microsoft Learn / Windows USB tracing / ETW-Dokumentation  
**Sicherheitsniveau:** wahrscheinlich korrekt  
**Belegwert:** mittel bis hoch  
**Erwarteter oder belegter Inhalt:**

- Windows bietet hostseitige Diagnostikpfade für USB-Stack-Ereignisse
- ETW-/Event-basierte Sicht unterscheidet sich von vollständigem Bus-Sniffing
- Fokus oft auf Stack-/Treiberdiagnose, nicht auf vollständige physikalische Signalerfassung

**Einordnung:**  
Hilft bei der Abgrenzung: **Host-Stack-Events sind nicht dasselbe wie Bus-Level-Evidenz**.

### [Q6] Android Open Accessory / AOA Protocol Documentation

**Typ:** Offizielle Primärquelle / Protokoll-Dokumentation  
**Typische Fundstelle:** Android Developers / Open Accessory / AOA docs  
**Sicherheitsniveau:** sehr wahrscheinlich korrekt  
**Belegwert:** hoch  
**Erwarteter oder belegter Inhalt:**

- beschreibt AOA-Handshake via Vendor Control Requests
- listet typische Requests und Re-Enumeration-Ablauf
- gibt Referenz für erwartetes Host-Verhalten im Accessory-Mode

**Einordnung:**  
Zentrale Referenz für die Frage, **welche Control-Requests in einem AOA-Szenario überhaupt erwartet werden**. Bedeutung von 51/52/53/58: Research 1.

### [Q7] TinyUSB Dokumentation / Beispiele / Debug-Hinweise

**Typ:** Tool-Dokumentation / Repo  
**Typische Fundstelle:** TinyUSB GitHub Repo, README, docs, examples  
**Sicherheitsniveau:** wahrscheinlich korrekt  
**Belegwert:** hoch  
**Erwarteter oder belegter Inhalt:**

- Callback-Modell für Control Requests und Klassen
- Unterschiede zwischen Setup-Callback, vendor/class handling und Stack-internem Handling
- Beispiele für Logging und Device-Implementierung

**Einordnung:**  
Wichtig für ESP32-S3/TinyUSB-Fragen: erklärt, **welche Requests Applikationscode überhaupt zu Gesicht bekommt** und welche nicht.

### [Q8] ESP-IDF / ESP32-S3 USB Device / TinyUSB Integration Doku

**Typ:** Offizielle Primärquelle / Herstellerdokumentation  
**Typische Fundstelle:** Espressif Docs / ESP-IDF USB Device Stack / TinyUSB integration  
**Sicherheitsniveau:** wahrscheinlich korrekt  
**Belegwert:** hoch  
**Erwarteter oder belegter Inhalt:**

- USB OTG/Device-Fähigkeiten des ESP32-S3
- TinyUSB-Integration und Debugmöglichkeiten
- Grenzen der Plattform als Gadget/Device, nicht als universeller Analyzer

**Einordnung:**  
Wichtig für die konkrete Embedded-Perspektive.

---

### Analyzer-Herstellerinfos

### [Q9] Total Phase Beagle USB Protocol Analyzer

**Typ:** Herstellerdokumentation / Analyzer-Produktinfo  
**Typische Fundstelle:** `totalphase.com`  
**Sicherheitsniveau:** wahrscheinlich korrekt  
**Belegwert:** hoch  
**Erwarteter oder belegter Inhalt:**

- dedizierter Hardware-Analyzer für USB
- physikalische Beobachtung des Busverkehrs
- Dekodierung von Control/Bulk u. a. je nach Geschwindigkeitsklasse/Modell

**Einordnung:**  
Relevante Referenzklasse für **In-Line-Sniffing mit hoher Beweiskraft**.

### [Q10] Teledyne LeCroy / Voyager / Mercury USB Analyzer

**Typ:** Herstellerdokumentation / Analyzer-Produktinfo  
**Typische Fundstelle:** Herstellerseiten  
**Sicherheitsniveau:** wahrscheinlich korrekt  
**Belegwert:** hoch  
**Erwarteter oder belegter Inhalt:**

- professionelle USB-Protokollanalyse mit Low-/Full-/High-/SuperSpeed-Varianten
- tiefe Dekodierung von Enumeration, Control, Bulk, Fehlerbildern
- Trigger-, Timing- und Re-Enumeration-Analyse

**Einordnung:**  
Goldstandard-Klasse im professionellen Umfeld.

---

### Repositories, Tools, Beispielcode

### [Q11] Wireshark USB Dissector / Capture Support

**Typ:** Repo / Tool-Codebasis  
**Typische Fundstelle:** Wireshark source tree / docs  
**Sicherheitsniveau:** wahrscheinlich korrekt  
**Belegwert:** mittel bis hoch  
**Erwarteter oder belegter Inhalt:**

- USB-Dissectors für Setup-Pakete und Transfers
- Anzeige von Control-Transfer-Feldern
- Abhängigkeit von Capture-Backend

**Einordnung:**  
Wichtig, um zwischen **Decode-Fähigkeit** und **Capture-Vollständigkeit** zu unterscheiden.

### [Q12] TinyUSB Examples / Control Transfer Handling

**Typ:** Repo / Beispielcode  
**Typische Fundstelle:** GitHub TinyUSB examples  
**Sicherheitsniveau:** wahrscheinlich korrekt  
**Belegwert:** mittel bis hoch  
**Erwarteter oder belegter Inhalt:**

- Beispiele für vendor/class control handling
- Callback-Pfade und Antworten
- Grenzen des Applikations-Callbacks

**Einordnung:**  
Hilft praktisch bei der Frage, **welche Instrumentierung man auf Device-Seite ergänzen muss**.

### [Q13] Linux usbmon tooling / text capture / tshark workflows

**Typ:** Praxisnahe Toolnutzung / Guides / Repo-Skripte  
**Typische Fundstelle:** Kernel/Wireshark/wiki/blogs  
**Sicherheitsniveau:** wahrscheinlich korrekt  
**Belegwert:** mittel  
**Erwarteter oder belegter Inhalt:**

- Nutzung von `/sys/kernel/debug/usb/usbmon/*`
- Live-Capture mit Wireshark oder `tshark`
- Filterung nach Bus/Device/Endpoint

**Einordnung:**  
Praktisch relevant für reproduzierbare Host-Captures unter Linux. Im Repo: `tools/lab_usbmon_capture.sh`.

---

### Issues, Maintainer-Kommentare, Praxisbelege

### [Q14] Praxisberichte zu unvollständiger Device-Sicht bei USB-Gadgets

**Typ:** Praxisquelle / Community / Issue / Blog  
**Typische Fundstelle:** GitHub issues, Mailinglisten, technische Blogs  
**Sicherheitsniveau:** mittel  
**Belegwert:** mittel  
**Erwarteter oder belegter Inhalt:**

- nicht alle Control Requests erreichen Applikationscallbacks
- Standard-/Stack-intern verarbeitete Requests werden nicht immer appseitig sichtbar
- Negativbefunde aus Device-Logs sind daher gefährlich

**Einordnung:**  
Nicht normativ, aber in Embedded-/Gadget-Praxis sehr wichtig.

### [Q15] Praxisberichte zu USBPcap-/usbmon-Limitierungen

**Typ:** Praxisquelle / Community / Issue / Blog  
**Typische Fundstelle:** Wireshark Q&A, USBPcap issues, mailing lists  
**Sicherheitsniveau:** mittel  
**Belegwert:** mittel  
**Erwarteter oder belegter Inhalt:**

- Capture kann je nach OS/Stack/Timing nicht physikalisch identisch mit Hardware-Sniffing sein
- Enumeration/Re-Enumeration und frühe Phasen sind hostseitig teils empfindlich
- Timing/Fehlerursachen sind hardwareseitig evidenzstärker

**Einordnung:**  
Wichtig für die korrekte Einordnung der Beweiskraft.

### [Q16] AOA-/Android-Accessory-Praxisbeispiele

**Typ:** Praxisquelle / Guide / Blog / Issue  
**Typische Fundstelle:** Android accessory tutorials, embedded blogs, GitHub issues  
**Sicherheitsniveau:** mittel  
**Belegwert:** mittel  
**Erwarteter oder belegter Inhalt:**

- typische Sequenzen: GET_PROTOCOL, Identifikationsstrings, START_ACCESSORY
- Re-Enumeration als kritischer Diagnoseschritt
- Unterschiede zwischen „Handshake gesehen“ und „Accessory mode wirklich erreicht“

**Einordnung:**  
Für die konkrete AOA-Frage sehr relevant, aber häufig nicht normativ dokumentiert. Repo-interner Praxisbeleg: Lab-Bericht 2026-10-30.

---

## 3. Technische Bewertung mit eingetragenen Quellenankern

### 3.1 Host-seitige USB-Traces

**Welche Methoden gibt es?**

Host-seitig gibt es im Wesentlichen drei Klassen:

1. **Linux `usbmon`** als Kernel-USB-Monitoring [Q1]
2. **Windows USBPcap** als Capture-Treiber für Wireshark [Q3]
3. **Windows ETW / OS-spezifische Diagnostik** als ergänzende Stack-Sicht [Q5]

Unter Linux ist `usbmon` die wichtigste Standardmethode. Wireshark ist dort primär **Frontend/Decoder**, nicht die eigentliche Capture-Quelle [Q1][Q2]. Unter Windows übernimmt diese Rolle typischerweise USBPcap [Q3].

**Was sieht man?**

Hostseitige Traces sehen typischerweise:

- Enumeration
- Control Transfers
- Bulk Transfers
- Endpoint-/Transfer-Metadaten
- bei guter Dekodierung auch Setup-Felder eines Control Transfers

Für die Diagnosefrage ist besonders wichtig: Ein hostseitiger Trace kann meist **sehr gut belegen, was der Host-Stack tatsächlich zur USB-Seite hin initiiert hat** [Q1][Q2][Q3].

**Wie vollständig ist die Sicht?**

Hostseitige Sicht ist in der Regel **deutlich belastbarer als reine Device-Logs**, wenn es um die Frage geht:

- Hat der Host den Request überhaupt abgesetzt?
- Welche Setup-Pakete und welche Datenphase wurden initiiert?
- Gab es Bulk-I/O?

Aber: Hostseitige Software-Captures sind **nicht identisch mit physikalischem Bus-Sniffing**. Sie beobachten den Datenfluss an oder nahe dem Host-Stack, nicht zwingend jedes physikalische Signalereignis auf D+/D−. Deshalb ist ihre Beweiskraft hoch, aber unterhalb dedizierter Hardware-Analyzer [Q1][Q3][Q15].

**Welche Betriebssysteme und Tools sind relevant?**

- **Linux:** `usbmon`, Wireshark, `tshark` [Q1][Q2][Q13]
- **Windows:** USBPcap + Wireshark [Q3]
- **macOS:** deutlich weniger standardisiert; ohne zusätzliche Kernel-/Treibermechanismen oder externe Analyzer ist die USB-Capture-Situation typischerweise schwächer als unter Linux/Windows. Für belastbare USB-Protokollanalyse wird macOS oft nicht als Primärplattform gewählt. Das ist eher Praxiswissen als saubere Primärdoku; daher nur vorsichtig belastbar [Q15].

**Diagnosewert:**  
Wenn die Device-Instrumentierung verdächtig unvollständig ist, ist **ein Host-Trace der erste starke Gegencheck**. Für Aussagen wie „Host hat Request 51/52/53 nicht gesendet“ ist ein sauberer Host-Trace wesentlich belastbarer als Device-App-Logs [Q1][Q2][Q3].

---

### 3.2 Device-seitige Instrumentierung

**Was kann Device-seitiges Logging leisten?**

Device-seitige Instrumentierung kann sehr wertvoll sein für:

- Callback-Einstiegspunkte
- empfangene Vendor-/Class-Requests
- EP0-Handler-Auswahl
- Antwortpfade
- Bulk IN/OUT auf Applikationsebene
- Stackinterne Fehler, Buffer-Zustände, Re-Enumeration-Übergänge

Gerade bei TinyUSB/ESP32-S3 ist das nützlich, um zu sehen:

- welcher Callback feuert
- welche Deskriptoren geliefert wurden
- ob die App bestimmte Vendor Requests behandelt [Q7][Q8][Q12]

**Wo liegen die Grenzen?**

Die zentrale Grenze lautet:

> **Device-Logs zeigen oft nur das, was der jeweilige Stack-Layer oder Callback überhaupt exponiert.**

Das heißt:

- Standard Requests können vollständig oder teilweise stackintern verarbeitet werden
- bestimmte Class/Vendor Requests können vor der App-Schicht entschieden werden
- die Data-/Status-Phase eines Control Transfers ist nicht immer vollständig sichtbar
- ein vendor-fokussierter Tap ist **kein vollständiger EP0-Sniffer**

Gerade deshalb sind Device-Logs **für Negativbefunde schwach**.  
„Nicht gesehen“ bedeutet auf Device-Seite oft nur:

- nicht bis zu diesem Hook gelangt
- nicht mitgeloggt
- in anderem Stackpfad verarbeitet
- zu früh/zu tief abgefangen
- bei Re-Enumeration schon verloren

**Warum ist Device-Sicht oft nicht ausreichend für Negativbefunde?**

Weil Device-Logs strukturell keine neutrale Bus-Sicht haben. Sie sind:

- stackabhängig
- callbackabhängig
- implementierungsabhängig
- häufig nur funktionsspezifisch

Damit sind sie gut für:

- **positive Befunde**: „diesen Request hat der Callback erhalten“
- aber schwach für:
  - „dieser Request wurde sicher nie gesendet“

Für das Probe-Problem ist das besonders wichtig:  
Wenn der bisherige Tap nur das sieht, was `USBVendor.onRequest` erreicht, dann ist ein Befund wie „51/52/53 nicht gesehen“ **methodisch nicht tragfähig**, solange nicht gezeigt ist, dass der Tap wirklich alle SETUP-Pakete auf EP0 vor jeder Stackentscheidung erfasst [Q4][Q7][Q14]. Auf `849f30e` publiziert der Tap jedes SETUP, das diesen Callback erreicht, auch Nicht-Vendor. Das schließt die Lücke vor dem Dispatch nicht.

---

### 3.3 In-Line- / Hardware-Sniffing

**Welche Methoden und Toolklassen gibt es?**

1. **Dedizierte USB-Hardware-Analyzer**  
   z. B. Beagle, LeCroy/Voyager/Mercury [Q9][Q10]

2. **Einfachere passive/halbpassive Methoden**  
   z. B. Logic Analyzer auf D+/D− oder Bastellösungen  
   Diese sind stark geschwindigkeits- und Signalqualitätsabhängig und oft nur eingeschränkt praxistauglich.

**Wie stark ist der Evidenzwert?**

Ein dedizierter Hardware-Analyzer hat die höchste Beweiskraft, weil er den Verkehr **in-line am Bus** beobachtet, nicht über Host- oder Device-Softwarepfade. Für die Diagnosefrage:

- „Host hat Request nicht gesendet“
- „Request ging über den Bus, Device sieht ihn aber nicht“
- „frühe Re-Enumeration / Reset / Disconnect / Retry“

ist ein Hardware-Analyzer der **Goldstandard** [Q9][Q10].

**Vor- und Nachteile praktisch**

Vorteile:

- höchste Evidenz
- gute Sicht auf Enumeration und Re-Enumeration
- sehr gut für Timing, Wiederholungen, Fehler, Control/Bulk-Korrelation
- unabhängig von Device-Instrumentierung

Nachteile:

- Kosten
- je nach Speed/Kabel/Topologie mehr Aufwand
- nicht immer sofort verfügbar
- bei HS/SS muss das Tool passend gewählt werden

**Logic Analyzer / passive D+ / D−-Methoden**

Hier ist Vorsicht nötig. Reine Logic-Analyzer-Methoden sind für USB **nur begrenzt geeignet**, insbesondere bei höheren Geschwindigkeiten. Für Low-/Full-Speed können manche Setups noch praktikabel sein; für High-Speed wird es schnell unzuverlässig oder unpraktisch. Als robustes Mittel für belastbare Negativbeweise sind sie typischerweise **deutlich unter dedizierten USB-Analyzern** einzuordnen. Das ist eher technisches Praxiswissen als normativ aus einer einzelnen Quelle ableitbar [Q15].

---

### 3.4 Sichtbarkeit von Control-Transfers

**SETUP / DATA / STATUS**

Ein vollständiger Control-Transfer besteht logisch aus:

- **SETUP**
- optionale **DATA**
- **STATUS**

Für das konkrete Problem ist entscheidend, ob ein Tool nur abstrakte Requests zeigt oder wirklich die Phasen ausreichend dekodiert.

**Host-seitig**

- `usbmon`/Wireshark und USBPcap/Wireshark können Control Transfers typischerweise gut darstellen, sofern die Capture-Schicht die Informationen liefert [Q1][Q2][Q3].
- Für viele Diagnosefälle reicht das aus, um Setup-Felder und Datenphasen sichtbar zu machen.

**Device-seitig**

- oft unvollständig, insbesondere wenn nur Callbacks oder vendor/class paths geloggt werden [Q7][Q14]

**Hardware-seitig**

- beste Sicht auf komplette Transferabfolge [Q9][Q10]

**Standard / Class / Vendor**

Sichtbarkeit hängt stark von der Instrumentierungsstelle ab:

- **Standard Requests**  
  werden oft stackintern verarbeitet; appseitig daher häufig unvollständig sichtbar [Q4][Q7]

- **Class Requests**  
  je nach Stack teils im Klassenmodul abgefangen

- **Vendor Requests**  
  werden in vielen Implementierungen explizit in App-/Vendor-Callbacks geroutet und erscheinen dadurch überproportional sichtbar

Genau daraus entsteht oft die Fehleinschätzung, dass Device-Logging „den USB-Control-Verkehr“ zeigt, obwohl es in Wahrheit nur den **vendor-sichtbaren Teil** zeigt.

**Re-Enumeration**

Re-Enumeration ist einer der wichtigsten Bereiche, in denen unvollständige Device-Sicht besonders problematisch ist. Wenn ein Host nach bestimmten Control Requests einen Wechsel in Accessory-Mode erwartet, dann ist relevant:

- kamen die Requests?
- kam `START_ACCESSORY`?
- erfolgte Disconnect/Reconnect?
- änderten sich VID/PID / Deskriptoren?
- wurde die neue Enumeration wirklich vollzogen?

Diese Übergänge sind hostseitig oder hardwareseitig meist belastbarer sichtbar als im Device-App-Log [Q6][Q9][Q10][Q16].

**Endpoint- und Transfer-Sichtbarkeit**

- **EP0 / Control**: methodisch kritisch, weil viele Stack-Layer involviert sind
- **Bulk**: hostseitig und hardwareseitig typischerweise leichter als Datenverkehr sichtbar
- **Device-App-Sicht**: kann auch bei Bulk lückenhaft sein, wenn Queues, DMA oder Stacklayer nicht vollständig geloggt werden

---

### 3.5 Bulk-Trace und Datenkanalvalidierung

**Wie weist man zuverlässig nach, ob Bulk tatsächlich leer ist?**

Am belastbarsten durch:

1. **Host-Trace**
2. **zusätzlich Device-Trace**
3. idealerweise **Hardware-Analyzer**

Ein einzelner Device-Log reicht nicht, um „Bulk ist leer“ sicher zu behaupten. Er kann nur zeigen:

- App hat keine Bulk-Daten gesehen

Das ist nicht dasselbe wie:

- auf dem Bus lief kein Bulk-Verkehr

**Welche Fallstricke gibt es?**

- Logging nur auf App-Ebene
- Queueing/DMA/Driver-Pfade nicht sichtbar
- NAK/Retry/kurze Transfers werden missverstanden
- nur IN oder nur OUT betrachtet
- falscher Endpoint/Burst/Interface beobachtet
- Re-Enumeration führt zu neuer Device-Instanz, alter Trace wird fehlinterpretiert

**Wie unterscheidet man „kein Traffic“ von unzureichender Beobachtung?**

Methodisch sauber nur durch Abgleich:

- Host-Trace: zeigt, ob Bulk-URBs/Transfers initiiert wurden
- Device-Trace: zeigt, ob Stack/App Daten bekam oder sendete
- Hardware-Analyzer: zeigt, ob wirklich Verkehr auf dem Bus war

Für einen belastbaren Negativbefund „Bulk wirklich leer“ reicht im Regelfall **mindestens ein guter Host-Trace**, besser plus Hardware [Q1][Q2][Q3][Q9][Q10].

Im Lab 2026-10-30 ist dieser Host-Negativbefund für **Debian-Host gegen Phone** bereits da: Bulk-IN 0, Bulk-OUT Timeout. Für **HU gegen Probe** liegt diese Host-Sicht nicht vor.

---

### 3.6 Gadget Validation / Referenzmessung

**Wie validiert man ein USB-Gadget methodisch sauber?**

Nicht durch einen Einzel-Log, sondern durch eine **mehrschichtige Referenzmessung**:

1. **Erwartetes Protokollverhalten definieren**  
   z. B. AOA-Sequenz laut Android-Doku [Q6]
2. **Host-Trace aufzeichnen**  
   komplette Enumeration + AOA-Handshake + Re-Enumeration
3. **Device-Trace aufzeichnen**  
   möglichst tief, idealerweise EP0-Setup vor vendor/class dispatch
4. **falls möglich Hardware-Trace**  
   für Bus-Wahrheit und Timing
5. **Vergleich mit Referenzlauf**  
   funktionierendes Phone/App/HU-Szenario

**Welche Vergleichsmitschnitte braucht man?**

Ideal sind mindestens zwei:

- **Golden reference / Known-good run**
- **Problem run**

Beide sollten enthalten:

- Erst-Enumeration
- alle Control Transfers auf EP0
- eventuelle AOA-Vendor-Requests
- Re-Enumeration
- Bulk-Phase danach
- Zeitbezug und Device-Identität

**Wie sollte ein Referenzlauf aussehen?**

Ein guter Referenzlauf sollte möglichst dieselbe Topologie verwenden:

- echtes Phone
- echte App
- echte HU bzw. echter Host
- gleiche Kabel/Adapter soweit praktikabel
- gleicher Boot-/Attach-Ablauf

Gerade bei AOA ist wichtig, dass nicht nur einzelne Requests gesehen werden, sondern die **gesamte Übergangskette**:

- Initial Enumeration
- AOA query / protocol
- accessory identification strings
- start accessory
- USB reset/disconnect/reconnect oder Re-Enumeration
- neue VID/PID/Interfaces
- Bulk/Accessory Data Path

---

### 3.7 Host-vs-Device-Abgleich

**Wie korreliert man beide Sichtweisen?**

Am besten über:

- Zeitstempel
- Bus-/Device-Adresse soweit verfügbar
- Request-Typ, `bmRequestType`, `bRequest`, `wValue`, `wIndex`, `wLength`
- Richtung IN/OUT
- Re-Enumeration-Grenzen
- neue Deskriptoren / neue Device-Adresse

**Was tun bei Widersprüchen?**

Typische Fälle:

1. **Host-Trace zeigt Request, Device-Trace nicht**  
   → Device-Instrumentierung unvollständig oder zu spätes Hooking
2. **Device-Trace zeigt Request, Host-Trace nicht**  
   → Capture-/Filterproblem, Fehlkorrelation, oder Device-Log ist kein Rohtrace sondern abstrahierte interne Sicht
3. **Host zeigt AOA-Sequenz, Re-Enumeration fehlt**  
   → Device reagiert falsch oder zu spät; Reset/Disconnect/Descriptors prüfen
4. **Bulk im Host sichtbar, im Device nicht**  
   → Endpoint-/Queue-/Class-Handling auf Device-Seite unvollständig beobachtet

**Welche Seite ist für welche Aussage belastbarer?**

- **„Host hat Request initiiert“** → Host-Trace stärker
- **„App/Callback hat Request verarbeitet“** → Device-Trace stärker
- **„Bus-seitig ist das wirklich passiert“** → Hardware-Analyzer am stärksten
- **„Request wurde sicher nicht gesendet“** → Hardware > Host >> Device

Das ist die Kernaussage für das Probe-Problem.

---

### 3.8 Geeignetste Strategie für das konkrete Problem

**Welche Methode wäre im beschriebenen Projekt am besten?**

Wenn die aktuelle Device-Instrumentierung nur stacknah an `USBVendor.onRequest` hängt und potenziell blind für Teile von EP0 ist, dann ist die beste Strategie:

1. **Host-seitiger kompletter USB-Trace**
2. **parallel verbesserte Device-Instrumentierung**
3. **bei weiterem Zweifel Hardware-Analyzer**

**Was ist Minimalaufwand?**

- Linux-Host verwenden, wenn möglich
- `usbmon` + Wireshark oder `tshark` [Q1][Q2][Q13]
- vollständigen Attach-/Enumeration-/AOA-Lauf mitschneiden
- Device-Logs mit Zeitstempeln parallel
- Requests 51/52/53 gezielt im Trace suchen

Wenn der Host Windows ist:

- USBPcap + Wireshark [Q3]

**Was ist Goldstandard?**

- dedizierter Hardware-Analyzer zwischen Host und Device [Q9][Q10]
- gleichzeitig Host-Trace
- gleichzeitig Device-Trace auf tiefer EP0-Ebene
- Known-good Reference Run + Problem Run

**Welche Reihenfolge ist sinnvoll?**

1. Host-Trace beschaffen
2. Device-EP0-Instrumentierung verbessern
3. Referenzlauf aufzeichnen
4. Problemfall dagegen halten
5. bei verbleibender Unsicherheit Hardware-Analyzer einsetzen

Für die Phone-Hälfte ist Schritt 1 im Lab schon gelaufen. Offen ist derselbe Host-Trace mit der HU.

---

## 4. Vorläufiges Fazit mit Quellenankern

1. **Ein vendor-fokussierter oder nur callback-naher Device-Tap ist kein belastbarer Nachweis dafür, dass bestimmte EP0-Control-Requests nicht gesendet wurden.**  
   Das ist für Gadget-/TinyUSB-/Embedded-Stacks methodisch ein schwacher Negativbefund [Q4][Q7][Q14].

2. **Für die Aussage „der Host hat Request 51/52/53 nicht gesendet“ ist ein Host-Trace deutlich evidenzstärker als Device-Logging.**  
   Unter Linux primär via `usbmon`, unter Windows primär via USBPcap [Q1][Q2][Q3].

3. **Der höchste Belegwert entsteht durch In-Line-Hardware-Sniffing.**  
   Wenn wirklich maximal belastbar unterschieden werden soll zwischen „nicht gesendet“ und „nur nicht gesehen“, ist ein dedizierter USB-Analyzer Goldstandard [Q9][Q10].

4. **Device-Logs sind gut für Positivbefunde, aber schwach für Negativbefunde.**  
   Sie zeigen zuverlässig, was ein bestimmter Stackpfad oder Callback gesehen hat; sie belegen aber nicht zuverlässig, was auf dem Bus nicht passiert ist [Q7][Q14].

5. **AOA-/Accessory-Diagnostik muss die gesamte Sequenz betrachten, nicht nur einzelne Vendor Requests.**  
   Entscheidend sind Handshake, `START_ACCESSORY`, Re-Enumeration und anschließender Bulk-Pfad [Q6][Q16].

6. **Bulk-Leere lässt sich nicht sauber allein aus Device-Seite ableiten.**  
   Für „Bulk ist wirklich leer“ braucht man mindestens einen Host-Trace, besser zusätzlich Hardware [Q1][Q2][Q3][Q9].

7. **Wenn das Problem ESP32-S3/TinyUSB betrifft, sollte die Device-Instrumentierung so erweitert werden, dass EP0-Setup-Pakete vor funktionsspezifischer Dispatch-Logik sichtbar werden.**  
   Sonst bleibt die Device-Sicht methodisch lückenhaft [Q7][Q8][Q12].

8. **Die sauberste Referenzmessung ist ein Known-good-Lauf mit echtem Phone/App/HU plus kompletter Host-Aufzeichnung, idealerweise ergänzt durch Device-Logs und Hardware-Trace.**  
   Nur so kann man Problem- und Referenzlauf sauber gegeneinander abgleichen [Q6][Q9][Q10][Q16].

9. **Gesichert ist: Host-Trace schlägt Device-Trace für die Frage „wurde gesendet?“. Wahrscheinlich ist: Hardware-Analyzer ist für dieses Problem die stärkste Evidenzquelle. Offen bleibt ohne Live-Verifikation: genaue Toolgrenzen im konkreten Host-OS-/Controller-/Speed-Setup.**

---

## 5. Tools, Repositories und praktische Fundstellen

### 1) `usbmon`

- **Warum relevant:** Standard-Host-Capture unter Linux
- **Wofür geeignet:** Enumeration, Control, Bulk, Host-seitige Evidenz
- **Einschränkungen:** kein physikalischer In-Line-Sniffer; Linux-Host erforderlich
- **Was konkret geprüft oder eingerichtet werden sollte:**
  - `usbmon` Kernel-Support aktiv
  - Zugriff auf usbmon-Interfaces
  - vollständiger Capture vom Attach bis nach Re-Enumeration
- **Praktische Hinweise:**
  - Wireshark kann `usbmon` live lesen
  - `tshark`/Text-Workflows sind für reproduzierbare Vergleiche oft nützlich
  - im Repo: `tools/lab_usbmon_capture.sh`
- **Plattform:** Linux
- **Capture-Format:** pcap/pcapng via Wireshark/tshark-Pipeline

### 2) Wireshark

- **Warum relevant:** zentrale Dekodier- und Analyseoberfläche
- **Wofür geeignet:** Sicht auf Setup-Felder, URBs/Transfers, Bulk-Verkehr
- **Einschränkungen:** Capture-Vollständigkeit hängt vom Backend ab
- **Was konkret geprüft werden sollte:**
  - richtiger Capture-Adapter (`usbmon`, USBPcap)
  - Filter nach Device-Adresse / Endpoint / Control/Bulk
  - Attach-/Reset-/Re-Enumeration-Phase nicht verpassen
- **Plattform:** Linux, Windows, eingeschränkt je nach Setup auch anders
- **Capture-Format:** pcapng

### 3) USBPcap

- **Warum relevant:** Hauptweg für USB-Capture auf Windows
- **Wofür geeignet:** Host-seitige USB-Captures auf Windows
- **Einschränkungen:** stacknah, nicht physikalisch in-line
- **Was konkret geprüft werden sollte:**
  - korrekter Host-Controller/Hub-Port gewählt
  - Capture früh genug gestartet
  - Re-Enumeration vollständig im Log
- **Plattform:** Windows
- **Capture-Format:** Wireshark-kompatibel

### 4) TinyUSB Repo / Docs / Examples

- **Warum relevant:** erklärt Control-Handling auf Device-Seite
- **Wofür geeignet:** gezielte Instrumentierung eines TinyUSB-basierten Gadgets
- **Einschränkungen:** kein Sniffer, sondern Stack-/Beispielquelle
- **Was konkret geprüft werden sollte:**
  - wo Standard/Class/Vendor Requests verarbeitet werden
  - welcher Callback vor/nach Stackdispatch liegt
  - ob EP0-Setup zentral geloggt werden kann
- **Plattform:** Embedded / diverse MCUs

### 5) ESP-IDF / ESP32-S3 USB Device Doku

- **Warum relevant:** konkrete Plattformdoku für ESP32-S3
- **Wofür geeignet:** Stackarchitektur, USB-Device-Integration, Debugansatz
- **Einschränkungen:** keine Bus-Wahrheit, nur Plattformsicht
- **Was konkret geprüft werden sollte:**
  - TinyUSB-Integrationspunkte
  - USB OTG/device mode details
  - Möglichkeiten für frühes EP0-Logging

### 6) Android Open Accessory Doku

- **Warum relevant:** Referenz für erwartete AOA-Control-Sequenz
- **Wofür geeignet:** Soll-Sequenz definieren, Referenztrace bewerten
- **Einschränkungen:** beschreibt Protokoll, nicht Capturemethodik
- **Was konkret geprüft werden sollte:**
  - welche Requests genau erwartet werden
  - welche Reihenfolge und Parameter
  - wann Re-Enumeration folgen muss

### 7) Total Phase Beagle Analyzer

- **Warum relevant:** etablierte Hardware-Analyzer-Klasse
- **Wofür geeignet:** starke Evidenz für Bus-Level Control/Bulk
- **Einschränkungen:** Kosten, modellabhängige Speed-Unterstützung
- **Was konkret geprüft werden sollte:**
  - passende Speed-Klasse
  - Inline-Setup zwischen Host und Device
  - Trigger auf AOA-Requests / Re-Enumeration

### 8) Teledyne LeCroy USB Analyzer

- **Warum relevant:** professionelle Goldstandard-Klasse
- **Wofür geeignet:** tiefes Timing-, Error- und Enumeration-Debugging
- **Einschränkungen:** Kosten, Verfügbarkeit
- **Was konkret geprüft werden sollte:**
  - Modell passend zu USB-Speed
  - Trigger auf EP0 / Vendor Requests / Reset / Disconnect
  - Export-/Decode-Möglichkeiten für Vergleichsläufe

---

## 6. Praktische Hinweise für das konkrete Problem

### Welche Methode sollte verwendet werden, wenn die aktuelle Device-Instrumentierung vendor-fokussiert und möglicherweise blind für Teile von EP0 ist?

**Primäre Empfehlung:**  
**Host-seitigen vollständigen USB-Trace erstellen.**

Begründung:

- Ein Tap an `USBVendor.onRequest` ist für EP0-Negativbefunde methodisch nicht ausreichend.
- Gebraucht wird eine Sicht, die **nicht vom Device-Callback-Modell abhängt**.
- Unter Linux ist `usbmon` + Wireshark die pragmatisch beste erste Wahl.
- Unter Windows ist USBPcap + Wireshark die pragmatische erste Wahl.

**Parallel dazu** sollte die Device-Seite verbessert werden:

- Logging direkt am frühestmöglichen EP0-Setup-Eingang
- Log vor Vendor/Class/Standard-Dispatch
- Log aller 8 Setup-Bytes plus Richtung und `wLength`
- Log der Antwortpfade und Result-Codes
- Log von USB Reset / SetConfiguration / SetInterface / Reconnect-Indikatoren

### Wie bekommt man einen belastbaren Nachweis, ob `51/52/53` wirklich nicht gesendet wurden?

Methodisch sauber in dieser Reihenfolge:

1. **Vollständigen Host-Capture vom ersten Attach an starten**
2. **Im Capture nach allen Control Requests mit passenden `bRequest`-Werten suchen**
3. **Prüfen, ob diese Requests vor Re-Enumeration, nach Re-Enumeration oder gar nicht auftreten**
4. **Parallel Device-Log mit Zeitstempeln erfassen**
5. **Wenn Host-Trace die Requests zeigt, Device aber nicht:**  
   → Device-Instrumentierung ist unvollständig
6. **Wenn Host-Trace sie nicht zeigt:**  
   → starke Evidenz, dass der Host sie nicht gesendet hat
7. **Wenn maximale Beweiskraft nötig ist:**  
   → Hardware-Analyzer einsetzen

Kurz:  
**Für „51/52/53 wurden wirklich nicht gesendet“ ist Host-Trace der erste belastbare Nachweis; Hardware-Trace ist der stärkste Nachweis.**  
**Device-Logs allein reichen dafür nicht.**

Im Lab gegen das Phone sind 51/52/53 im Host-Trace gesehen worden. Für die HU gegen die Probe steht dieser Nachweis noch aus. Bei `aoa_observe` ist Absenz von 51/52/53 außerdem erwartbar, siehe Research 1.

### Wie sieht man Direct-Accessory-Re-Enumeration sauber?

**Beste Praxis:**

1. Host-Trace
2. wenn möglich Hardware-Trace
3. Device-Trace nur ergänzend

Was unbedingt sichtbar sein sollte:

- Initial Enumeration
- AOA-relevante Vendor Control Requests
- `START_ACCESSORY` bzw. Startsignal
- darauf folgender Reset/Disconnect/Reconnect/Re-Enumeration
- neue VID/PID / neue Interface-Struktur
- Beginn oder Ausbleiben des Bulk-Kanals

Re-Enumeration ist genau die Art von Vorgang, die auf Device-App-Ebene leicht lückenhaft erscheint. Host oder Hardware sind hier viel aussagekräftiger.

### Wie weist man nach, ob Bulk wirklich leer ist?

Nicht durch „im Device wurde nichts gesehen“.

Sondern durch:

- Host-Trace über den gesamten relevanten Zeitraum
- Filter auf Bulk-Endpunkte
- Prüfen, ob URBs/Transfers überhaupt angesetzt werden
- wenn möglich Hardware-Trace gegenprüfen
- Device-seitig zusätzlich Logs für Queue/Read/Write/Callback

**Belastbare Aussageformen:**

- stark: „Im Host-Trace sind keine Bulk-Transfers an Endpoint X/Y im Zeitraum T zu sehen.“
- stärker: „Im Host- und Hardware-Trace sind keine Bulk-Transfers sichtbar.“
- schwach: „Im Device-Log wurde nichts gesehen.“

### Wie sähe ein guter Referenzmitschnitt mit echtem Phone / echter App / echter HU idealerweise aus?

Ein idealer Referenzmitschnitt enthält:

1. **Vollständigen Start bei physischem Attach**
2. **Initiale Enumeration**
   - Device Descriptor
   - Configuration Descriptor
   - Standard Control Requests
3. **AOA-Handshake**
   - Protokollabfrage
   - Identifikationsstrings
   - Start Accessory
4. **Re-Enumeration**
   - Disconnect/Reset/Neuaufbau
   - neue VID/PID / neues Interface-Layout
5. **Bulk-Phase**
   - erste IN/OUT-Transfers
   - Aktivität oder belastbares Ausbleiben
6. **Zeitkorrelation**
   - Host-Trace, Device-Log, ggf. Analyzer-Trace synchronisiert
7. **Known-good und Problemfall**
   - identischer Versuchsablauf
   - gleiche Kabel/Topologie möglichst beibehalten

### Minimalstrategie und Best-Practice

**Minimalstrategie**

- Host-Trace aufzeichnen:
  - Linux: `usbmon` + Wireshark
  - Windows: USBPcap + Wireshark
- Device-Log verbessern:
  - alle EP0-Setup-Pakete so früh wie möglich loggen
- Known-good und Problemfall vergleichen
- gezielte Suche nach `51/52/53`
- Bulk-Endpunkte separat prüfen

**Best-Practice-Strategie**

- dedizierten USB-Hardware-Analyzer in-line einsetzen
- parallel Host-Trace
- parallel tiefes Device-EP0-Logging
- Referenzlauf mit echtem Phone/App/HU
- Problemfall unter gleichen Bedingungen wiederholen
- Vergleichsmatrix anlegen:
  - Request vorhanden?
  - Reihenfolge korrekt?
  - Antwortphase?
  - Re-Enumeration?
  - neue Deskriptoren?
  - Bulk aktiv/inaktiv?

Klare Empfehlung:

> **Zuerst einen vollständigen Host-Trace erstellen. Wenn damit noch Zweifel bleiben oder die Beweiskraft maximiert werden soll, ist ein dedizierter Hardware-Analyzer die methodisch beste Lösung. Die aktuelle Device-Instrumentierung allein ist für Negativbefunde zu schwach.**

---

## 7. Ehrliche Quellenlage

### Was mit hoher Sicherheit gesagt werden kann

- `usbmon` ist die Standardmethode für Host-seitige USB-Captures unter Linux [Q1].
- Wireshark dekodiert USB-Captures, ist aber von der zugrunde liegenden Capture-Quelle abhängig [Q2].
- USBPcap ist die zentrale Host-Capture-Methode für USB unter Windows [Q3].
- Device-seitige Logs sind stack- und callbackabhängig und daher für Negativbefunde strukturell schwächer als Host- oder Hardware-Traces [Q4][Q7].
- Dedizierte Hardware-Analyzer haben für Bus-Level-Evidenz die höchste Beweiskraft [Q9][Q10].
- AOA muss als komplette Sequenz aus Handshake, Re-Enumeration und Datenkanal betrachtet werden [Q6].

### Was plausibel, aber noch live zu verifizieren ist

- Wie vollständig im konkreten Host-Setup `usbmon` bzw. USBPcap die relevanten frühen Phasen zeigt.
- Welche TinyUSB-/ESP32-S3-Hooks in der konkreten Firmware-Version den frühesten EP0-Zugriff erlauben.
- Wie gut ein bestimmter verfügbarer Hardware-Analyzer die verwendete Speed-/Topologie-Kombination unterstützt.

Die Bedeutung von 51/52/53/58 ist in Research 1 eingeordnet. Der Lab-Host hat 51/52/53 gegen das Phone gemessen.

### Was offen bleibt

- Das exakte Verhalten von HU, Phone und App im echten NBT-Lauf ohne Mitschnitt Phone ↔ NBT.
- Ob die HU 51/52/53 gegen die Probe sendet, oder ob der Device-Tap sie nur nicht sieht. Bei Direct-`2D00` ist Absenz zusätzlich erwartbar (Research 1).
- Ob Bulk auf dem HU↔Probe-Pfad wirklich leer ist oder nur device-seitig nicht sichtbar war. Auf dem Lab-Pfad Debian↔Phone ist Bulk im Host-Trace absent.
- Ob eine Re-Enumeration im HU-Lauf vollständig stattfindet oder in einer frühen Phase scheitert.
