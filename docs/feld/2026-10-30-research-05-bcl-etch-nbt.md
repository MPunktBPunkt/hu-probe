# Technischer Research-Report

## BMW Connected / BCL / Etch / NBT Evo Reverse Engineering

### Öffentliche Evidenzlage zu Protokollen, Transportpfaden und BMW-App-/HU-Kommunikation

**Projekt:** hu-probe  
**Datum:** 2026-10-30  
**Reihe:** Research-Report 5 von 5  
**Problemfelder:** [`2026-10-30-aoa-problemfelder.md`](2026-10-30-aoa-problemfelder.md) — Block 4, Thema 8, Priorität 3  
**Projekt-Epistemik:** [`../CONNECTED-APP.md`](../CONNECTED-APP.md)  
**Charakter:** gelieferte Vorbewertung ohne Live-Webcrawl. Der Abgleich mit dem Baum steht in Abschnitt 8.

> **Transparenzhinweis zur Recherchebasis:** In der Umgebung dieser Notiz stand kein allgemeiner Web-/GitHub-Browser zur Verfügung. Die interne Wissensbasis enthielt keine einschlägigen BMW-Quellen jenseits des bekannten Stands. Der Report ist eine **Vorbewertung auf Basis des bekannten öffentlichen Reverse-Engineering-Stands**, kein Nachweis, dass jede Repository-Version, jeder Issue-Thread oder jedes Suchergebnis live geprüft wurde. Konkrete Repositories sollten vor einer formalen Projektentscheidung direkt geprüft werden.

---

## 1. Ziel der Untersuchung

Untersucht wurde die Frage, ob BMW Connected, BMW Apps, AAIdrive und NBT-Evo-Hauptgeräte einen BMW-spezifischen Nutztransport über USB-Bulk mit einem Framing wie dem projektspezifischen `BclFramer` verwenden.

Im Mittelpunkt stehen:

- die öffentliche Evidenz für BCL, Etch, RHMI und CDS,
- ein mögliches 8-Byte-Header-Framing,
- Destination-/Routing-Werte wie `0x0FA4`,
- ein mögliches Watchdog-Ziel `0x1389`,
- die Abgrenzung zwischen USB, Bluetooth, iAP, SPP, WLAN und BMW-internen App-Pfaden,
- der Diagnosewert dieser Evidenz für das bestehende Projekt.

Die zentrale Frage lautet:

> Wie stark ist die öffentliche Evidenz dafür, dass BMW Connected beziehungsweise NBT Evo einen BCL-/Etch-artigen Nutztransport über USB-Bulk verwendet?

---

## 2. Quellenbasis

### [Q1] AAIdrive beziehungsweise verwandte Open-Source-BMW-App-Projekte

**Typ:** Reverse-Engineering-Projekt / Android-Anwendung / Community-Code  
**Typische Fundstelle:** GitHub-Projekte unter dem Namen `AAIdrive` sowie Forks und Issues  
**Sicherheitsniveau:** wahrscheinlich korrekt  
**Belegwert:** hoch für die Existenz eines BMW-App-/HU-Kommunikationspfads, mittel für konkrete Transportdetails

**Erwarteter oder belegter Inhalt:**

- AAIdrive nutzt beziehungsweise nutzte BMWs eigene App-/Connected-/BMW-Apps-Infrastruktur, um Drittanwendungen in Richtung iDrive/HU zu integrieren.
- Die Projekte zeigen, dass BMW-App-Funktionalität nicht ausschließlich als Cloud-Funktion verstanden werden darf.
- In derartigen Projekten finden sich typischerweise Android-Service-Komponenten, Intent- und Broadcast-Kommunikation, App- beziehungsweise Accessory-Initialisierung, BMW-spezifische UI- oder App-Objekte, Zustands- und Session-Handling.
- AAIdrive ist ein wichtiger Hinweis auf einen realen BMW-App-/Headunit-Integrationspfad, beweist aber allein kein BCL-Framing und kein USB-Bulk-Protokoll.

**Einordnung:**  
AAIdrive ist wahrscheinlich die wichtigste öffentliche Quelle, um den BMW-App-/HU-Anwendungsfall zu verstehen. Die Existenz von AAIdrive beweist nicht automatisch, dass die darunterliegende Übertragung ein frei zugänglicher USB-Bulk-Kanal ist. Entscheidend sind die konkreten Transportadapter im jeweiligen Commit beziehungsweise in der verwendeten BMW-App-Version. Im Projekt gepinnt: AAIdrive@3e93419, siehe [`../CONNECTED-APP.md`](../CONNECTED-APP.md).

---

### [Q2] BMW Connected / BMW Apps Reverse Engineering in AAIdrive-nahen Projekten

**Typ:** APK-/App-Analyse und Reverse Engineering  
**Typische Fundstelle:** Android-Quellcode, dekompilierte BMW-App-Klassen, Issues, Commit-Historie  
**Sicherheitsniveau:** mittel bis wahrscheinlich korrekt  
**Belegwert:** mittel bis hoch für API-/App-Artefakte, niedrig bis mittel für Wire-Level-Aussagen

**Erwarteter oder belegter Inhalt:**

- BMW Connected beziehungsweise BMW Apps wurden als Vermittler zwischen Smartphone und Fahrzeug-HU untersucht.
- Relevante Artefakte können Android-Broadcasts, Binder-/Service-Schnittstellen, App-Registrierung, Session- und Capability-Objekte sowie UI- beziehungsweise Remote-HMI-Objekte sein.
- Ein App-Artefakt kann zeigen, dass ein BMW-spezifischer Kommunikationsmechanismus existiert, ohne die darunterliegende physikalische Transportschicht zu beweisen.

**Einordnung:**  
APK- und App-Code zeigt, **was** kommuniziert wird und welche Komponenten beteiligt sind. Er zeigt nicht notwendigerweise, ob die Bytes über USB Bulk, Bluetooth RFCOMM/SPP, iAP, WLAN oder einen internen BMW-App-Service laufen.

---

### [Q3] BMW Apps / ConnectedDrive / BMW Apps Interface

**Typ:** Herstellerdokumentation und öffentliches Produkt-/Entwicklerumfeld  
**Typische Fundstelle:** BMW ConnectedDrive- und BMW-Apps-Dokumentation, historische BMW-Apps-SDK- oder Entwicklerunterlagen  
**Sicherheitsniveau:** sehr wahrscheinlich korrekt auf konzeptioneller Ebene  
**Belegwert:** hoch für die Existenz eines BMW-App-Ökosystems, niedrig bis mittel für internes Framing

**Erwarteter oder belegter Inhalt:**

- BMW unterstützte beziehungsweise unterstützt App-Integration in die Fahrzeugbedienung.
- Das BMW-App-Modell umfasst eine Smartphone-Anwendung, die Fahrzeugfunktionen oder HMI-Inhalte in Richtung iDrive bereitstellt.
- Der Begriff „BMW Apps“ bezeichnet primär ein Integrations- und Funktionsmodell, nicht zwingend ein einzelnes öffentlich dokumentiertes Byteprotokoll.
- ConnectedDrive umfasst zusätzlich Cloud- und Telematikfunktionen, die von einer lokalen HU-App-Kommunikation getrennt werden müssen.

**Einordnung:**  
Die Herstellerterminologie reicht nicht aus, um BCL, Etch oder einen 8-Byte-Header zu belegen. „BMW Apps“ und „ConnectedDrive“ dürfen nicht als Synonyme für ein bestimmtes USB-Protokoll verwendet werden.

---

### [Q4] RHMI / Remote-HMI-Artefakte

**Typ:** Community-Reverse-Engineering / App-/UI-Protokollwissen  
**Typische Fundstelle:** Open-Source-Projekte, dekompilierte BMW-App-Klassen, Community-Dokumentation  
**Sicherheitsniveau:** mittel  
**Belegwert:** mittel für eine Anwendungs-/UI-Schicht, niedrig für den Transport

**Erwarteter oder belegter Inhalt:**

- RHMI wird im BMW-Kontext typischerweise als Remote-HMI beziehungsweise Remote-Headunit-Interface interpretiert.
- Der Begriff passt zu einer Schicht, in der UI-Strukturen, Menüs, Texte, Icons, Events und Zustandsänderungen zwischen Smartphone-App und HU ausgetauscht werden.
- Eine Bezeichnung als RHMI beweist nicht, dass die darunterliegende Verbindung USB Bulk verwendet.

**Einordnung:**  
RHMI ist für die Suche nach UI- und Session-Artefakten relevant. Es sollte nicht mit einer konkreten USB-Transportdefinition gleichgesetzt werden. Projekt: RHMI create → resources → initialize → events ist R0-FACT in [`../CONNECTED-APP.md`](../CONNECTED-APP.md), nicht NBT-Wire.

---

### [Q5] CDS / BMW Apps Interface / Capability- und Session-Begriffe

**Typ:** Community- und Code-Artefakte  
**Typische Fundstelle:** APK-Strings, Klassen- und Methodennamen, AAIdrive-nahe Projekte  
**Sicherheitsniveau:** mittel  
**Belegwert:** mittel für logische Schnittstellen, niedrig bis mittel für Wire-Level

**Erwarteter oder belegter Inhalt:**

- CDS kann im Umfeld von BMW-App-/HU-Kommunikation als logische Schnittstelle, Dienst- oder Capability-Schicht auftreten.
- Solche Bezeichnungen können Session-Aufbau, App-Registrierung, Capability-Austausch oder HMI-Funktionen betreffen.
- Aus einem Klassen- oder String-Namen lässt sich nicht ableiten, dass CDS ein eigenständiges physikalisches Protokoll ist.
- Eine klare, öffentlich konsolidierte Spezifikation von CDS als allgemeinem BMW-Bulk-Protokoll ist nicht etabliert.

**Einordnung:**  
CDS ist als Suchbegriff nützlich, aber semantisch unscharf. Für eine belastbare Aussage müssen konkrete Klassen, Call-Sites, Payloads oder Mitschnitte vorliegen.

---

### [Q6] BimmerConnected beziehungsweise `bimmer_connected`

**Typ:** Open-Source-Projekt / Python-API für BMW ConnectedDrive  
**Typische Fundstelle:** GitHub-Projekt `bimmer_connected` beziehungsweise BimmerGestalt-nahe Projekte  
**Sicherheitsniveau:** wahrscheinlich korrekt  
**Belegwert:** hoch für Cloud-/ConnectedDrive-API, sehr niedrig für lokalen HU-Transport

**Erwarteter oder belegter Inhalt:**

- Zugriff auf BMW-ConnectedDrive-/MyBMW-Dienste kann über eine öffentliche Python-Bibliothek beziehungsweise Community-API erfolgen.
- Typische Funktionen betreffen Fahrzeugstatus, Position, Remote-Dienste, Lade- und Klimainformationen, Authentifizierung und Cloud-Endpunkte.
- Das ist primär ein Internet-/Cloud-Pfad zwischen Client und BMW-Backend.
- Daraus folgt kein Nachweis für einen lokalen USB-, Bluetooth- oder NBT-Evo-Bulk-Kanal.

**Einordnung:**  
Dieses Projekt trennt Cloud-Kommunikation vom lokalen App-/HU-Kanal. Der Name „Connected“ darf nicht automatisch als Beleg für lokale Connected-App-Nutzdaten interpretiert werden.

---

### [Q7] Öffentliche Android- und iOS-Transportmechanismen

**Typ:** Plattformwissen / technische Referenz  
**Typische Fundstelle:** Android USB Host/Accessory, Bluetooth RFCOMM/SPP, Apple iAP beziehungsweise External Accessory  
**Sicherheitsniveau:** sehr hoch für die Plattformmechanismen  
**Belegwert:** hoch für mögliche Transportklassen, aber nicht für BMW-spezifische Nutzung

**Erwarteter oder belegter Inhalt:**

- Android unterstützt USB Host und USB Accessory sowie Bluetooth-Verbindungen.
- iOS kann über MFi-/External-Accessory-/iAP-artige Mechanismen mit zertifizierten Zubehörgeräten kommunizieren.
- Bluetooth SPP/RFCOMM ist technisch ein plausibler serieller Transport, sofern beide Seiten entsprechende Profile unterstützen.
- Die bloße Existenz solcher APIs in einer APK beweist nicht, dass sie für BMW-HU-Kommunikation verwendet werden.
- Ebenso beweist das Vorhandensein einer USB-Device-ID oder eines USB-Descriptors nicht, dass darauf BCL-Nutzdaten laufen.

**Einordnung:**  
Diese Mechanismen liefern die möglichen Kandidaten. Der Nachweis für BMW muss aus konkreten App-Codepfaden, Device-Handles, Endpoints, Bluetooth-UUIDs oder Captures kommen.

---

### [Q8] BCL / Etch / `0x0FA4` / `0x1389` / 8-Byte-Framing

**Typ:** Projektspezifisches Desk-/Audit-Wissen  
**Typische Fundstelle:** vorhandener `BclFramer`, interne Referenzen, Audit-Notizen  
**Sicherheitsniveau:** unbekannt bis niedrig für öffentliche externe Bestätigung  
**Belegwert:** hoch als Projekt-Hypothese, niedrig als unabhängiger öffentlicher Wire-Nachweis

**Bekannter Inhalt aus dem Untersuchungsstand:**

- `kHeaderSize = 8`
- `kDestEtchDesk = 0x0FA4`
- `kDestWatchdogDesk = 0x1389`
- Vorhandener `BclFramer`
- Keine belastbare Bestätigung, dass der reale NBT-Evo-USB-Bulk-Verkehr dieses Framing verwendet
- P0-/Observe-Läufe zeigen keinen belastbaren Bulk-Datenkanal

**Einordnung:**  
Diese Daten sind als interne Arbeitshypothese relevant, aber nicht als unabhängige öffentliche Evidenz zu behandeln. Im Baum sind sie bereits als Desk-R0 gekennzeichnet, nicht als NBT-Wire-FACT. Siehe Abschnitt 8.

---

## 3. Technische Bewertung mit eingetragenen Quellenankern

### 3.1 Öffentliche Gesamt-Evidenzlage zu BMW-App-/HU-Kommunikation

Öffentlich gut gestützt ist folgende Aussage:

> BMW hatte beziehungsweise hat App-Integrationsmechanismen, über die ein Smartphone mit BMW-Fahrzeugfunktionen und teilweise mit der iDrive-/Headunit-Oberfläche interagiert.

Das wird durch BMW-Apps- und ConnectedDrive-Dokumentation [Q3], AAIdrive- und ähnliche Community-Projekte [Q1], APK- und App-Analysen [Q2], RHMI-/UI-bezogene Artefakte [Q4] und Cloud-API-Projekte wie `bimmer_connected` für den separaten ConnectedDrive-Backend-Pfad [Q6] gestützt.

Nicht öffentlich stabil dokumentiert ist dagegen:

> BMW Connected verwendet auf NBT Evo immer BCL über USB Bulk.

Dafür fehlen mindestens drei unabhängige Belegklassen:

1. ein klarer Codepfad vom BMW-App-Transportadapter zu USB-Bulk-Endpunkten,
2. ein echter USB-Capture mit wiedererkennbarer BCL-/Etch-Struktur,
3. ein Parser oder eine Testimplementierung, die denselben Header und dieselben Destination-Werte an realem NBT-Verkehr reproduzierbar dekodiert.

Die Begriffe sind nicht automatisch gleichbedeutend:

- **ConnectedDrive:** häufig Cloud-/Backend-/Fahrzeugdienstebene,
- **BMW Apps:** App-Integrationsmodell zwischen Smartphone und Fahrzeug,
- **RHMI:** wahrscheinlich Remote-HMI- beziehungsweise UI-nahe Schicht,
- **CDS:** vermutlich Dienst-/Capability-/Session-Kontext, aber öffentlich nicht als einheitlich spezifiziertes physikalisches Protokoll etabliert,
- **BCL/Etch:** im vorliegenden Kontext Desk-/Community-nahe Protokollbegriffe ohne NBT-Evo-Wire-Nachweis.

---

### 3.2 BCL: Belegt, plausibel oder nur tradiert?

#### Was projektintern plausibel ist

BCL kann als Name für einen BMW-spezifischen Kommunikationsmechanismus plausibel sein, insbesondere wenn ein Framer existiert, ein 8-Byte-Header angenommen wird, Destination-/Routing-Werte verwendet werden, ein Watchdog-Ziel vorgesehen ist und Etch als Ziel oder Dienstkomponente bezeichnet wird.

Diese Punkte sind zunächst eine konsistente Modellbildung. Im Projekt sind sie **R0-FACT aus Desk-Quellen**, nicht Wire-FACT. [`../CONNECTED-APP.md`](../CONNECTED-APP.md) sagt ausdrücklich: Framing auf der eigenen F20 NBT Evo bleibt **UNKNOWN**.

#### Was derzeit nicht ausreichend belegt ist

Nicht ausreichend als NBT-Wire belegt ist:

- eine offizielle BMW-Spezifikation von BCL,
- ein echter NBT-Evo-USB-Capture mit dem behaupteten 8-Byte-Header,
- die Bedeutung der einzelnen Headerbytes auf dem eigenen Fahrzeug,
- ein reproduzierbarer Zusammenhang zwischen `0x0FA4` und realen HU-Paketen,
- ein reproduzierbarer Zusammenhang zwischen `0x1389` und realen HU-Paketen.

Die Desk-Quelle im Code nennt Endianness bereits: vier `u16` big-endian (`command | src | dest | dataLen`). Das ist R0, kein eigener Capture.

#### Bewertung

> **BCL als Desk-/Community-Konzept: im Projekt R0-FACT.**  
> **BCL als nachgewiesenes NBT-Evo-USB-Bulk-Wire-Protokoll: offen.**

Der `BclFramer` ist eine sinnvolle **Arbeitshypothese und Test-Parser**, keine durch NBT-Evo-Verkehr abgesicherte Protokollimplementierung.

---

### 3.3 Etch, Destinations, Watchdog, Routingfelder

Die Werte `0x0FA4`, `0x1389`, `Etch`, `Watchdog` und Destination-/Routingfelder deuten auf eine interne Nachrichtenadressierung hin.

Drei mögliche Interpretationen:

1. **Echte Wire-Destinationen** in real übertragenen Frames.
2. **Interne Software-IDs**, die nicht im Transportframe stehen.
3. **Desk-/Audit-Konstanten** aus Notizen oder einer Softwareversion, die nicht die eigene HU ist.

Für `0x0FA4`:

> Im Code steht `kDestEtchDesk` mit Verweis auf EVID-BG-2026-10-28-002. Das ist Desk-R0. Es gibt keinen eigenen Nachweis, dass `0x0FA4` am NBT-Evo-USB-Wire als Destination übertragen wird.

Für `0x1389`:

> `kDestWatchdogDesk`, EVID-BG-2026-10-28-007, ebenfalls Desk-R0. Die Wire-Bedeutung auf der eigenen HU ist offen.

Prüfen wäre, ob diese Werte als 16-Bit big-endian in festen Headerpositionen, nur bei Session-Setup, periodisch, oder nur in App-Logs auftreten.

---

### 3.4 RHMI / CDS / BMW Apps / ConnectedDrive

#### ConnectedDrive

ConnectedDrive bezeichnet überwiegend Fahrzeugdienste und Backend-/Cloud-Funktionen. Ein ConnectedDrive-API-Request an BMW-Server ist kein Beleg für USB zur HU, Bluetooth zur HU, RHMI, BCL oder Etch.

#### BMW Apps

BMW Apps beschreibt die Integration von Smartphone-Anwendungen mit dem Fahrzeug-/iDrive-Umfeld. Der Begriff sagt allein nicht, welches physikalische Medium verwendet wird, ob die App direkt mit der HU spricht, ob sie als Proxy fungiert, oder ob eine zertifizierte Zubehörschnittstelle beteiligt ist.

#### RHMI

RHMI ist am plausibelsten als Remote-HMI- beziehungsweise UI-/Applikationsschicht zu verstehen: UI-Aufbau, Menüs, Text, Icons, Benutzeraktionen, Status, App-Lifecycle. Der Begriff beweist kein USB Bulk.

#### CDS

CDS scheint eher einen Dienst-, Capability- oder Session-Kontext zu bezeichnen. Ohne konkrete Quellstellen ist die Auflösung nicht sicher.

Eine belastbare Architekturhypothese, die das Projekt schon so zeichnet:

```text
Cloud-/ConnectedDrive-Dienste
        |
BMW-App / App-Service
        |
BMW Apps / CDS / Session- und Capability-Schicht
        |
RHMI-/UI-Schicht
        |
plattformabhängiger lokaler Transport
        |
HU / NBT / NBT Evo
```

Im Projekt steht darunter die Kette `Transport → BCL → Etch → SAS → RHMI|AV|CDS|AM` als **Desk-Konvergenz R0**, nicht als gemessener NBT-Stack. [`../CONNECTED-APP.md`](../CONNECTED-APP.md)

---

### 3.5 Transportpfade: USB, Bluetooth, iAP, sonstige Wege

#### USB allgemein

USB ist technisch plausibel. Daraus folgt nicht, dass die App einen frei zugänglichen USB-Bulk-Endpunkt erhält, dass Android USB Host verwendet, dass BMW Connected den Nutzkanal selbst kontrolliert, oder dass ein BCL-Frame direkt auf Bulk-Endpunkten liegt.

Für einen echten USB-Bulk-Nachweis wären notwendig: Enumeration, VID/PID, Interface- und Endpoint-Beschreibung, Richtung, zeitliche Korrelation, dekodierbare Daten, Reproduzierbarkeit.

Der Stand „kein belastbarer Bulk-Datenkanal in P0-/Observe-Läufen“ und der Lab-Host-Befund Bulk absent gegen das Phone schwächen die USB-Hypothese. Das Projekt stuft „USB Accessory / ENET als BCL-Transport“ bereits als **Contradiction / HYPOTHESIS** ein.

#### Bluetooth SPP/RFCOMM

Bluetooth ist technisch und im Desk-Code von BclCarLink belegt: `SppTransport.h` nennt BT RFCOMM als Desk-FACT, der Stub ist nicht armiert. [`../CONNECTED-APP.md`](../CONNECTED-APP.md) führt denselben Claim als FACT (impl) R0.

Das ist ein Code-Befund in BclCarLink, kein Capture auf der eigenen HU. Zu prüfen blieben RFCOMM-Kanäle, UUIDs, SDP, Sockets, Paketgrößen, Session-Init und Keepalive.

#### iAP / MFi / External Accessory

Für iOS-nahe BMW-Apps plausibel, nicht automatisch für Android und nicht automatisch für NBT Evo.

#### WLAN / IP-Sockets

Eine weitere mögliche Klasse. Ein Socket-Artefakt allein beweist kein BCL. Der Desk-`TcpMuxer` mit 4000-Byte-Puffer ist im Kommentar zu `kMaxPayload` genannt, ebenfalls nicht als NBT-USB-FACT.

#### Zwischenbewertung

Am stärksten belegt, im Projekt als R0:

1. ein BMW-App-/HU-Integrationsmodell,
2. App-/Service-/UI-Artefakte,
3. die Trennung zwischen ConnectedDrive-Cloud und lokaler Fahrzeugkommunikation,
4. ein 8-Byte-BE-Header und die beiden Destinationen **in Desk-Quellen**.

Nicht als NBT-Wire belegt:

- BCL über USB Bulk,
- `0x0FA4` und `0x1389` auf dem eigenen Fahrzeug,
- ein universeller Transportweg über alle HU-Generationen.

---

### 3.6 Repositories, Parser und Reverse-Engineering-Artefakte

#### AAIdrive

Wertvoll für BMW-App-Integration, Drittanbieter-Registrierung, Android-Service-Lifecycle, UI-/HMI, Session und Capability, Abhängigkeit von der BMW Connected App. Nicht ausreichend für USB-Bulk-Framing, NBT-Evo-Wireformat oder BCL-/Etch-Destinationen auf dem eigenen Auto.

#### `bimmer_connected`

Wertvoll für ConnectedDrive-Cloud. Für die USB-/HU-Frage nur als Negativabgrenzung: Cloud-API und lokale HU-Kommunikation sind getrennte Objekte.

#### APK-Analysen

Der höchste Erkenntniswert liegt in Call-Chains:

```text
UI/Event
 -> BMW-Service
 -> Session-Aufbau
 -> Transportadapter
 -> Byte-Serialisierung
 -> Gerät/Socket/Accessory
```

#### Vorhandener `BclFramer`

Der `BclFramer` ist die direkteste technische Referenz im Baum und zugleich das größte Risiko einer Selbstbestätigung. Er parst synthetische Ströme. P0 füttert ihn nicht. Ohne unabhängiges Wire-Artefakt ist er kein Beweis für das Framing auf der NBT Evo.

---

### 3.7 Diagnosewert für das konkrete Projekt

Die Hypothese „BCL läuft über USB Bulk am realen NBT Evo“ ist **nicht belastbar nachgewiesen**. Als Desk-Modell ist der Header stark. Als USB-Transport ist er im Projekt schon als Widerspruch/Hypothese geführt, und die Läufe haben keinen Bulk-Kanal gezeigt.

Gestützt wird das Modell durch Desk-Evidence, den 8-Byte-Framer und die Etch-/Watchdog-Konstanten.

Geschwächt als USB-Pfad durch:

- keinen Bulk-Kanal in den P0-/Observe-Läufen,
- Lab 2026-10-30: Bulk-IN 0, Bulk-OUT Timeout gegen das Phone trotz UI „verbunden“,
- Abendfeld 0.3.32: hex4-Knock-TX, `rx=0`, ausdrücklich kein Wire-FACT,
- USB Accessory als BCL-Transport im eigenen Epistemik-Blatt als Contradiction/HYPOTHESIS,
- BclCarLink-BT-RFCOMM als den im Desk-Code vorhandenen Transport, im Firmware-Baum nur als Stub.

Vorsichtige Aussage, die zum Projektblatt passt:

> BMW-App-/HU-Kommunikation existiert als Desk-Modell. BCL-on-USB-Bulk für die eigene NBT Evo ist nicht belegt. Der Header bleibt R0, bis ein eigener Capture ihn hebt.

---

## 4. Vorläufiges Fazit mit Quellenankern

1. **Gut gestützt:** BMW bietet beziehungsweise bot App-Integrationsmechanismen zwischen Smartphone, BMW-App und Fahrzeug-/iDrive-Umgebung [Q1], [Q3].
2. **Cloud getrennt:** ConnectedDrive über Backend-Dienste ist nicht lokale HU-Kommunikation [Q6].
3. **Wahrscheinlich bis R0:** AAIdrive-nahe Projekte und die Desk-Kampagne liefern die Anwendungs- und Session-Schicht [Q1], [Q2].
4. **R0, nicht Wire:** RHMI und CDS sind im Projektblatt logische Schichten [Q4], [Q5].
5. **Nicht als NBT-Wire bestätigt:** BCL auf der eigenen F20.
6. **Nicht als NBT-USB bestätigt:** das 8-Byte-Header-Framing. Im Code ist es Desk-BE.
7. **Nicht als NBT-USB bestätigt:** `0x0FA4` und `0x1389`. Im Code sind es Desk-Konstanten.
8. **USB Bulk:** technisch möglich, im Projekt widersprüchlich/hypothetisch, durch fehlende Bulk-Beobachtungen geschwächt.
9. **Bluetooth SPP:** im BclCarLink-Code als Desk-FACT geführt, in der Firmware nicht implementiert.
10. **Projektentscheidung:** `BclFramer` bleibt **Desk-/Audit-basierter Test-Decoder**, keine bestätigte NBT-Evo-Wire-Implementierung.

---

## 5. Repositories, Parser, Code und praktische Fundstellen

| Fundstelle / Projektklasse | Warum relevant | Betroffene Ebene | Gezielt prüfen |
|---|---|---|---|
| AAIdrive und Forks | Reale BMW-App-/HU-Integration | App, Session, HMI | Services, Broadcasts, Transportadapter. Pin im Projekt: @3e93419 |
| BMW Connected-/MyBMW-APK-Analysen | Hersteller-App | App, Session, Transport | Manifest, native Libraries, USB/Bluetooth/iAP |
| `bimmer_connected` | Cloud vs. lokale HU | Cloud/API | Keine lokalen USB-Annahmen daraus ableiten |
| RHMI-nahe Parser | UI-Strukturen | Anwendung/UI | Eventtypen, Session-State |
| Android USB Host/Accessory | Echter USB-Pfad | Transport | `UsbManager`, Endpoints, Bulk |
| Android Bluetooth | SPP/RFCOMM | Transport | UUIDs, SDP, Sockets |
| `firmware/src/connected/bcl/BclTypes.h` | Header-Hypothese | Framing | 8 Byte BE, `0x0FA4`, `0x1389`, Kommentar „not NBT Evo wire FACT“ |
| `firmware/src/connected/bcl/BclFramer.h` | Test-Parser | Framing | synthetische Ströme, kein DEADBEEF-Resync |
| `firmware/src/connected/transport/SppTransport.h` | BT-Stub | Transport | `implLevel = stub`, nicht armiert |
| [`../CONNECTED-APP.md`](../CONNECTED-APP.md) | Epistemik-Blatt | alle Schichten | R0 vs. UNKNOWN auf der eigenen HU |
| USB-Captures aus realem Fahrzeug | Einziger direkter Wire-Nachweis | Link | Enumeration, Endpoint, Wiederholbarkeit |

### Besonders wertvolle Suchmuster

```text
BCL
Etch
0x0FA4
0x1389
Watchdog
RHMI
CDS
UsbManager
bulkTransfer
BluetoothSocket
RFCOMM
SPP
iAP
MFi
destination
session
keepalive
```

Native Libraries getrennt prüfen. Ein Java-String kann ein Wrapper sein.

---

## 6. Praktische Hinweise für das konkrete Problem

### 6.1 Belastbarkeit der USB-Bulk-Annahme

> **Technisch möglich, im Projekt nicht als Standardpfad geführt, durch fehlende Bulk-Beobachtungen geschwächt.**

Nicht als „wahrscheinlicher Standardpfad“, solange keine dieser Beobachtungen vorliegt:

- USB-Bulk-Transfers während einer reproduzierbaren BMW-App-Aktion,
- ein Interface mit geeigneten Bulk-Endpunkten, das die HU wirklich nutzt,
- zeitliche Korrelation zwischen App-Ereignis und Transfer,
- wiederkehrendes 8-Byte-Framing auf dem Capture,
- die Destination-/Watchdog-Werte byteweise im Capture,
- Dekodierung mindestens eines vollständigen Nutzframes vom eigenen Fahrzeug.

### 6.2 Einordnung des `BclFramer`

> **Arbeitshypothese und host-testbarer Parser auf Desk-R0. Kein beweisener NBT-Evo-Wire-Framer.**

Zwei Regeln, die der Code schon umsetzt:

1. Der Framer darf für synthetische Tests und spätere Instrumentierung verwendet werden.
2. P0 schickt Raw-Bulk nicht durch den Framer (`drainTransportRaw`, Kommentar „no BclFramer (C1b-P1)“).

Passende Statuszeile, konsistent mit `BclTypes.h`:

```text
BCL framing status:
- Source: desk/audit evidence (EVID-BG-2026-10-28-001, BE u16×4)
- Independent NBT Evo wire validation: absent
- USB-bulk validation: absent
- Project use: hypothesis/test decoder
- Live P0: framer not fed
```

### 6.3 Was eher für USB spricht

Nur allgemeine Faktoren: physische USB-Verbindung, AOA-Mechanik, ein projekteigenes Framing-Modell. Die AAIdrive-UI „verbunden“ ohne Bulk (Lab 2026-10-30) spricht dagegen, UI-Connected mit USB-Nutzdaten gleichzusetzen.

### 6.4 Was eher für Bluetooth, SPP, iAP oder andere Pfade spricht

Der stärkste repo-interne Hinweis ist der BclCarLink-Kommentar: BT RFCOMM ist der im Desk-Code vorhandene Transport. Die Firmware hat dafür nur `SppTransport` mit `not_implemented`. USB Accessory als BCL-Transport ist im Epistemik-Blatt Contradiction/HYPOTHESIS.

Damit gilt:

> USB Bulk ist nicht bestätigt. Andere Pfade, vor allem BT RFCOMM aus dem Desk-Code, bleiben offen und sind im Baum noch nicht messbar.

### 6.5 Nächste gezielte Prüfungen

1. **Transportadapter in den gepinnten Quellen**, nicht nur Strings: UI → Service → Session → Socket/Accessory → Write.
2. **USB-Enumeration unabhängig vom Framer**, bevor nach 8-Byte-Frames gesucht wird. Research 2.
3. **Parallel-Capture** von USB, Bluetooth, Logcat und HMI bei derselben Aktion.
4. **Header an echten Bytes falsifizieren:** Länge inklusiv oder exklusiv, BE wie im Desk, Sequenz, CRC, ob `0x0FA4` und `0x1389` byteweise auf dem Medium stehen.
5. **Generationen trennen:** CIC, NBT, NBT Evo, MGU, iOS, Android, BMW Connected, MyBMW, AAIdrive. Ein Artefakt aus einer anderen Generation hebt den F20-Status nicht.

### 6.6 Claims, die aktuell nicht gemacht werden sollten

- „BCL ist das BMW-Standardprotokoll für NBT Evo.“ Der Desk sagt R0, die eigene HU bleibt UNKNOWN.
- „BCL läuft nachweislich über USB Bulk.“
- „`0x0FA4` ist die Etch-Destination im HU-Wire-Protokoll.“ Sie ist die Desk-Konstante.
- „`0x1389` ist der NBT-Evo-Watchdog.“ Ebenso Desk.
- „RHMI und BCL sind dieselbe Protokollschicht.“
- „ConnectedDrive-Kommunikation ist lokale HU-Kommunikation.“
- „AAIdrive beweist einen USB-Bulk-Datenpfad.“ Das Lab zeigt UI „verbunden“ bei totem Bulk.
- „Kein beobachteter USB-Bulk bedeutet, dass es keinen BMW-App-Datenverkehr gibt.“ Der Pfad kann woanders liegen. Der BT-Stub ist genau diese offene Tür.

---

## 7. Ehrliche Quellenlage

### Was mit hoher Sicherheit gesagt werden kann

- BMW verfügt beziehungsweise verfügte über App-/HU-Integrationsmechanismen.
- AAIdrive-nahe Projekte sind relevante öffentliche Quellen. Das Projekt pinnt einen Commit.
- ConnectedDrive-Cloud ist von lokaler HU-Kommunikation zu trennen.
- RHMI, CDS und BMW Apps gehören zu unterschiedlichen logischen Ebenen.
- Der 8-Byte-Header und die beiden Destinationen sind im Baum als Desk-R0 dokumentiert, big-endian, und ausdrücklich nicht als eigener NBT-Capture.
- P0 füttert `BclFramer` nicht. `SppTransport` ist ein Stub.

### Was plausibel, aber noch live zu verifizieren ist

- Ob die gepinnten AAIdrive-/BclCarLink-Commits denselben Transport zeigen, den die Desk-Kampagne als R0 geführt hat. Die Pins liegen außerhalb dieses Git-Baums (`/tmp/bimmergestalt/`, technology-audit).
- Ob RHMI-Payloads und CDS-Capabilities auf der eigenen HU in dieser Form ankommen.
- Ob BCL und Etch interne BMW-Bezeichnungen sind, deren öffentliche Evidenz fragmentarisch bleibt.

### Was offen bleibt

- Ob das 8-Byte-Framing auf realem NBT-Evo-Verkehr existiert.
- Ob `0x0FA4` und `0x1389` auf USB, Bluetooth, iAP, IP oder nur in Software vorkommen.
- Ob die untersuchte App-Generation auf dieser HU überhaupt USB Bulk für Connected nutzt.
- Ob der BT-RFCOMM-Pfad aus BclCarLink auf diesem Fahrzeug aktiv ist.

---

## Gesamtbewertung

Die Evidenz stützt **einen realen BMW-App-/HU-Integrationskontext als Desk-Modell**. Sie stützt nicht die Behauptung eines **BCL-/Etch-Nutztransports über USB Bulk an der eigenen NBT Evo**.

> **BCL-on-USB-Bulk ist eine unbestätigte Arbeitshypothese. Der 8-Byte-`BclFramer` ist Desk-R0 und wird in P0 nicht live gefüttert. Der im Desk-Code genannte Transport BT RFCOMM ist in der Firmware noch nicht implementiert. Ein eigener Capture auf der F20 fehlt.**

Der nächste Erkenntnisgewinn ist die Korrelation von App-Version, HU-Generation, USB-Enumeration, Bluetooth-/Socket-Aktivität, Call-Chain und einem Capture, erst danach der Vergleich mit dem 8-Byte-Framing.

---

## 8. Repo-Abgleich (nicht Teil der gelieferten Notiz)

| Punkt der Notiz | Stand im Baum |
|------------------|---------------|
| 8-Byte-Header, BE, `0x0FA4`, `0x1389` | `BclTypes.h`: „Header layout from BimmerGestalt desk evidence (R0), not own wire captures.“ Layout `command u16 BE \| src u16 BE \| dest u16 BE \| dataLen u16 BE`. |
| Epistemik-Klasse | [`../CONNECTED-APP.md`](../CONNECTED-APP.md): Header und Destinationen **FACT R0**. „Framing/APIs auf unserer F20 NBT Evo“ **UNKNOWN**. Regel: R0 bleibt R0, bis eigenes Wire es hebt. |
| USB als BCL-Transport | Dasselbe Blatt: **Contradiction / HYPOTHESIS**. |
| BT RFCOMM | Dasselbe Blatt: FACT (impl) R0, weil BclCarLink es so implementiert. `SppTransport`: `implLevel() = "stub"`, `arm()` → `not_implemented`. |
| Framer live? | Nein. `BclFramer.h`: synthetische Ströme, kein DEADBEEF-Resync. `CONNECTED-APP.md`: Etch-Magic `DEADBEEF` ist **UNKNOWN / not attested**. P0: `drainTransportRaw()`, Kommentar „no BclFramer (C1b-P1)“. |
| Knock auf USB | Abendfeld FW 0.3.32: hex4 `12 34 56 78` ist der Desk-Init, TX ging raus, `rx=0`, **kein Wire-FACT**. [`2026-10-29-c1b-b2b-knock-feld.md`](2026-10-29-c1b-b2b-knock-feld.md) |
| Phone-Bulk | Lab 2026-10-30: AOA und UI „verbunden“, Bulk absent. [`2026-10-30-lab-aoa-aaidrive-phone-host.md`](2026-10-30-lab-aoa-aaidrive-phone-host.md) |
| Pins | `CONNECTED-APP.md`: BclCarLink@7cfd959, AAIdrive@3e93419. Die Bäume liegen nicht in diesem Repo. |

Die Notiz und das Epistemik-Blatt widersprechen sich nicht. Die Notiz sagt „öffentlich nicht als NBT-Wire bestätigt“. Das Blatt sagt „Desk-R0, eigene HU UNKNOWN“. Beides lässt den `BclFramer` als Testwerkzeug stehen und verbietet den Claim, USB-Bulk auf der NBT Evo sei gezeigt.
