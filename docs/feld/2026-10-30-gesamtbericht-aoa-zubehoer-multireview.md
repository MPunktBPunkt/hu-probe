# Gesamtbericht: hu-probe / USB-Zubehör-Weg (AOA)

## Stand nach Lab-Referenz, HU-Fotos und Multi-KI-Auswertung

**Projekt:** hu-probe  
**Datum:** 2026-10-30  
**Repo-Basis:** `main` @ `849f30e`  
**Kontext:** neuer Weg über **USB-Zubehör / Android Open Accessory (AOA)** statt MSC  
**Zweck:** Konsolidierter Gesamtbericht aus den Auswertungen von Mistral, Grok, GPT und Claude, ergänzt um die repo-nahen Codebefunde aus dem bereitgestellten `hu-probe`-Stand.  
**Lab-Bericht (Phone als Accessory):** [`2026-10-30-lab-aoa-aaidrive-phone-host.md`](2026-10-30-lab-aoa-aaidrive-phone-host.md)  
**Artefakte:** [`artifacts/20261030-lab-aoa-aaidrive/`](artifacts/20261030-lab-aoa-aaidrive/)  
**Verwandt:** [`2026-10-29-c1b-p0-aoa-observe-feld.md`](2026-10-29-c1b-p0-aoa-observe-feld.md) · [`2026-10-29-c1b-p05a-instrumentation.md`](2026-10-29-c1b-p05a-instrumentation.md) · [`2026-10-29-c1b-massnahmen-nach-b1-b2b.md`](2026-10-29-c1b-massnahmen-nach-b1-b2b.md) · [`../GATES-Connected.md`](../GATES-Connected.md)

**Quellenbasis für diesen Bericht:**

- die vier KI-Berichte von **Mistral, Grok, GPT, Claude**
- die beigefügten Fotos
- der bereitgestellte Repo-/Codekontext zu `ConnectedAppProbe`, `AoaControlTap`, `AoaObserveGadget`, `AoaTransport`, `BclFramer`

> **Ablagehinweis zum Stand `849f30e`:** Der Text ab Abschnitt 1 ist der gelieferte Gesamtbericht. Eine Code-Stelle darin beschreibt diesen Commit nicht: Abschnitt 2.5 sowie die Verweise in 6.1 und 11.1 sagen, `drainControlTap()` setze `p0_b_` bei jedem gepoppten Sample. Seit `4c1d6d2` (FW 0.3.28, enthalten in `849f30e`) gilt das nicht. `p0_b_` wird nur gesetzt, wenn `bRequest` 51, 52, 53 oder 58 ist. Der Name `control_count_` im Bericht ist der historische Alias `AoaControlTap::controlCount()`, der `vendor_setup_count_` liefert und nur Vendor-SETUPs zählt.

**Repo-Anker (Dateien, die der Bericht nennt):**

| Thema | Pfad |
|-------|------|
| Probe-Stufen, P0-Flags, Raw-Bulk | `firmware/src/connected/ConnectedAppProbe.h`, `ConnectedAppProbe.cpp` |
| Control-Tap | `firmware/src/connected/aoa/AoaControlTap.h`, `AoaControlTap.cpp` |
| Observe-Gadget | `firmware/src/connected/aoa/AoaObserveGadget.h`, `AoaObserveGadget.cpp` |
| TX-Sperre im Observe-Pfad | `firmware/src/connected/transport/AoaTransport.h`, `AoaTransport.cpp` |
| BCL-Framer | `firmware/src/connected/bcl/BclFramer.h`, `BclFramer.cpp`, `BclTypes.h` |
| BT-SPP-Stub | `firmware/src/connected/transport/SppTransport.h` |

---

# 1. Executive Summary

Der neue Weg als **USB-Zubehör (AOA)** ist **nicht gescheitert**, sondern hat die erste technische Zielmarke klar erreicht:

- Die Probe bzw. das Setup erreicht zuverlässig die **AOA-/Accessory-Ebene**
- Die HU erkennt die Probe als **USB-Zubehör**
- Im Lab lässt sich ein echtes Android-Telefon reproduzierbar per **AOA-Handshake** in den Accessory-Modus bringen
- BMW-/NBT-artige Strings beeinflussen sichtbar die **AAIdrive-/MyBMW-UI** bis hin zu **„verbunden“**

Gleichzeitig ist der wichtigste Negativbefund nun sehr klar:

- **Der eigentliche USB-Bulk-/Datenkanal ist nicht nachgewiesen**
- Bulk bleibt in den beschriebenen Läufen **leer / absent / timeout**
- Deshalb sind **BCL über USB**, **Etch**, **CDS**, **RHMI**, **AV** über diesen Weg derzeit **nicht belegt**

Die stärkste gemeinsame Aussage aller vier KIs lautet daher:

> **AOA-/Accessory-Erkennung und „verbunden“-UI funktionieren, aber das ist noch kein Beweis für einen offenen Connected-/BCL-Datenkanal.**

---

# 2. Projektkontext: Was der aktuelle Code tatsächlich tut

Der bereitgestellte Repo-Code stützt die Grundarchitektur sehr klar.

## 2.1 Connected-App-Probe ist bewusst in Stufen getrennt

`ConnectedAppProbe` beschreibt sich explizit als:

- **C1a.5 lab BCL**
- **C1b-0 skeleton**
- **C1b-P0 AOA observe**

Das heißt:

- Parser-/Lab-Themen
- AOA-/USB-Observe-Themen
- späteres BCL-on-wire

sind absichtlich **nicht vermischt**.

## 2.2 P0 ist ausdrücklich observe-only

Der Code zeigt:

- in P0 wird nur **Raw Bulk** gelesen, nicht live durch `BclFramer` geschickt
- Kommentar im Code: **„P0: raw bulk only — no BclFramer (C1b-P1).“**
- `AoaTransport::write()` blockiert TX im Observe-Pfad explizit mit `p0_tx_disabled`

Das bedeutet:

- kein aktiver BCL-Verkehr
- kein Knock-TX
- kein versehentliches „Protokollspielen“
- keine unkontrollierte aktive Teilnahme

## 2.3 aoa_observe ist als USBVendor-only Gadget gedacht

`AoaObserveGadget` ist explizit:

- **USBVendor-only gadget for C1b-P0 aoa_observe boot mode**
- Header sagt: **no MSC interface**

Außerdem setzt `begin()` direkt:

- VID `0x18D1`
- PID `0x2D00`
- Manufacturer `"Google"`
- Product `"AOA Observe"`

Das ist wichtig für die Interpretation:

- Direct-Accessory-Boot
- kein klassischer Phone→53→Accessory-Übergang im Probe-Boot
- 51/52/53-Absenz auf diesem Pfad ist deshalb **nicht automatisch AOA-negativ**

## 2.4 AOA-Control-Tap ist Vendor-Control-fokussiert

`AoaControlTap` ist laut Header:

- **Queue USB vendor control SETUPs for C1b-P0 AOA observe**

Im Code:

- `control_count_` wird nur für `vendor && REQUEST_STAGE_SETUP` erhöht
- `aoa51_`, `aoa52_`, `aoa53_` werden dort explizit gezählt
- req52-DATA kann String-Inhalt erfassen und publizieren

Das bestätigt die frühere Analyse:

- der Tap ist **nicht generischer EP0-Sniffer**
- aber **brauchbar für Vendor-Control**
- `control_count_` ist semantisch eher **Vendor-SETUP-Count**

## 2.5 P0-B-Logik im Status ist im aktuellen Code zu breit

In `ConnectedAppProbe::drainControlTap()` wird aktuell:

```cpp
if (!p0_b_) p0_b_ = true;
```

bei **jedem** gepoppten Sample gesetzt.

Das heißt:

- wenn künftig breiter instrumentiert wird,
- dann kann `P0_B` zu leicht auf „true“ springen

Das war bereits in den früheren Diskussionen ein korrekt identifiziertes Problem.

> Siehe Ablagehinweis: auf `849f30e` ist diese Stelle bereits auf 51/52/53/58 eingeschränkt (`4c1d6d2`, FW 0.3.28).

---

# 3. Zusammenfassung der vier KI-Berichte

## 3.1 Mistral

### Kernaussage

Mistral bewertet den Stand als:

- **AOA-Handshake funktioniert**
- **AAIdrive „verbunden“ ist erreichbar**
- **USB-Bulk bleibt tot**
- damit ist **„Connected-Optik“ nicht gleich BCL**

### Besondere Stärken des Berichts

Mistral arbeitet sehr stark mit:

- **MEASURED**
- **ABSENT**
- **NOT OBSERVED**
- **INFERENCE**

und bleibt epistemisch diszipliniert.

### Wichtige Punkte von Mistral

1. **AOA-Handshakes sind reproduzierbar**
   - `51 → 02 00`
   - `52 BMW/NBT-Strings`
   - `53 START`
   - Re-Enum auf `18d1:2d00`
2. **AAIdrive zeigt „verbunden“**
   - aber ohne Fahrzeugdaten
   - also UI-/Identity-Reaktion, nicht automatisch Datenkanal
3. **Bulk ist vollständig absent**
   - keine Antworten auf diverse Knock-/Handshake-/Open-Muster
4. **Wichtige Interpretation des Handy-Dialogs**
   - „Keine installierten Apps für dieses USB-Zubehör“
   - deutet stark darauf hin, dass **kein Accessory-Consumer** aktiv ist
5. **Instrument-Nebenbefund**
   - möglicher Stage-Enum-Mismatch
   - alte `51/52/53=0`-Befunde teilweise instrument-blind

### Mistrals Schlussfolgerung

- USB-Zubehör-/AOA-Ebene funktioniert
- aber **BCL über USB ist nicht gezeigt**
- Follow-on #9 am echten NBT bleibt wichtig
- außerdem App-/Accessory-Filter und Logcat am Phone untersuchen

---

## 3.2 Grok

### Kernaussage

Grok formuliert den Stand pragmatisch:

- **AOA PASS**
- **UI „verbunden“ PASS**
- **Bulk FAIL / ABSENT**
- **BCL über USB weiterhin unbekannt**

### Besondere Stärken

Grok eignet sich besonders als **Programmstatusbericht**:

- klare Tabellen
- operative Einordnung
- gute Handlungsempfehlungen

### Wichtige Punkte

1. **AOA + BMW-Strings triggern UI**
2. **Bulk-IN 0 / Bulk-OUT Timeout**
3. **Die Fotos passen exakt zum Lab-Befund**
4. **Instrument-Fix + Logcat + BT-SPP parallel priorisieren**
5. **HU-Hälfte bleibt offen**

### Groks Schlussfolgerung

- Der neue Weg bringt euch **zuverlässig bis zur AOA-/UI-Ebene**
- Der **kritische Datenkanal fehlt**
- Nächste Schritte:
  - Instrument-Fix
  - Accessory-FD/Logcat
  - HU-Strings / echter NBT-Lauf
  - BT-SPP höher priorisieren

---

## 3.3 GPT

### Kernaussage

GPT ordnet die Ergebnisse architektonisch sehr sauber:

- **Accessory-Erkennung / AOA-Control / Connected-UI-Trigger** sind getrennte Ebenen
- **USB-Bulk / BCL** ist weiterhin **nicht nachgewiesen**

### Besondere Stärken

- klare Struktur
- gute Trennung der Ebenen
- gute Ableitung, dass der bisherige Blind-Knock-Ansatz als Primärstrategie ausläuft

### Wichtige Punkte

1. **NBT erkennt Accessory**
2. **Lab-Host kann echtes Android in Accessory-Mode bringen**
3. **BMW-Strings beeinflussen die App-/UI-Seite**
4. **Bulk bleibt leer**
5. **BCL ist nicht belegt**
6. **Die offene Frage ist nicht mehr „geht AOA?“, sondern „was ist der tatsächliche Nutztransport?“**

### Wo GPT etwas theoretischer wird

GPT neigt etwas stärker dazu, die Frage in Richtung „welcher Transport folgt nach erfolgreicher Accessory-Erkennung?“ zu formulieren.

Das ist sinnvoll, aber minimal weitergehend als das rein Belegte. Denn die Bilder und der Android-Dialog lassen auch die simplere Möglichkeit offen:

- **es gibt aktuell gar keinen Accessory-Consumer**

---

## 3.4 Claude

### Kernaussage

Claude liefert einen sehr gut lesbaren technischen Standortbericht.

### Besondere Stärken

- gute Chronologie
- gute Unterscheidung von Auto- und Lab-Perspektive
- sehr gute Auswertung des Handy-Dialogs

### Wichtige Punkte

1. **„Das USB-Rohr steht, die Anwendungsschicht nicht.“**
2. Der Handy-Dialog
   - „Keine installierten Apps für dieses USB-Zubehör“
   - wird als starkes Indiz für fehlenden App-Filter / Consumer gelesen
3. **UI „verbunden“** kann aus USB-State/Accessory-State kommen, nicht aus Bulk/BCL
4. **Phone-like Fingerprint und Descriptoren** bleiben relevant
5. **Accessory-Filter der APKs** sollte untersucht werden

### Claudes Schlussfolgerung

- Prüfe App-Filter / XML / Manifest / Logcat
- Fixe Instrumentierung
- Echten NBT-Follow-on klein und gezielt
- BT-SPP parallel in Betracht ziehen

---

# 4. Gemeinsamkeiten zwischen Mistral, Grok, GPT und Claude

Hier besteht praktisch Konsens.

## 4.1 Gemeinsame Hauptaussage

Alle vier stimmen überein:

> **AOA-/Accessory-Schicht ist gezeigt, Bulk-/BCL-Schicht nicht.**

## 4.2 Gemeinsame Interpretation der Bilder

Alle vier lesen die Fotos ähnlich:

### HU-Foto

- HU erkennt die Probe als **Zubehörmodus**
- Hinweis „USB-Modus aktivieren“ zielt eher auf Datei-/MTP-/MSC-Logik als auf Connected-App

### Android-Popup

- Android erkennt das Zubehör
- die URI/Description greifen
- aber offenbar kein passender App-Handler / Consumer

### AAIdrive-Screenshot

- UI „verbunden“ bzw. Fahrzeuginfos sichtbar
- aber alle Werte `null`
- also kein echter Datenkanal

## 4.3 Gemeinsamer Negativbefund

Alle vier sind sich einig:

- Bulk-IN = leer
- Bulk-OUT = Timeout / kein Erfolg
- kein Beweis für BCL-on-wire

## 4.4 Gemeinsame methodische Konsequenz

Alle vier lehnen ab:

- weiteres blindes Knock-/Handshake-Raten als Primärstrategie

## 4.5 Gemeinsame nächsten Schritte

Alle vier sehen als sinnvoll:

1. Instrument-/Tap-Fix
2. Android-Seite genauer untersuchen
3. Echten NBT-Lauf gezielt und klein
4. BT-SPP-Hypothese höher priorisieren

---

# 5. Unterschiede in den Meinungen / Gewichtungen

Es gibt keine harten Widersprüche, eher unterschiedliche Schwerpunktsetzungen.

## 5.1 Mistral vs. GPT

### Mistral

- stärker evidenzminimal
- betont stärker:
  - **kein Consumer / kein Accessory-FD**
  - UI-Connected ohne Daten
  - Instrument-Blindheit

### GPT

- stärker architektur- und transportorientiert
- formuliert eher:
  - „Welcher tatsächliche Connected-Transport folgt nach AOA?“

### Bewertung

Beides ist sinnvoll. Strenger evidenzbasiert ist im Moment Mistrals Lesart:

> erst prüfen, ob überhaupt ein Consumer existiert, bevor man zu stark einen „folgenden Transport“ voraussetzt.

## 5.2 Claude vs. Grok

### Claude

- etwas tiefer in Richtung APK-/Accessory-Filter und Android-Systemverhalten

### Grok

- operativer, status-/projektmanagement-näher

### Bewertung

Claude ist stärker für Ursachenanalyse auf Android-Seite, Grok stärker für Status-/Entscheidungsbericht.

---

# 6. Technische Detailauswertung aus Repo + Berichten

## 6.1 Was ist im Code heute über P0 / AOA sicher?

### P0 beobachtet Control und Raw Bulk

- `drainControlTap()` publiziert `ca_usb`-Events mit:
  - `bm_request_type`
  - `b_request`
  - `w_value`
  - `w_index`
  - `w_length`
  - `hypothesis_handled`
  - optional `aoa_desk_name`
  - optional req52-String

### P0 markiert Bulk-RX explizit

- bei erstem Bulk-RX wird `p0_e_ = true` gesetzt

### P0 markiert Attach / VIDPID / EP

- `P0-A`, `P0-C`, `P0-D` sind als Events und Status abgebildet

### P0-B ist aktuell semantisch zu breit

- weil jedes ControlTap-Sample `p0_b_` setzt

> Siehe Ablagehinweis: auf `849f30e` setzt nur 51/52/53/58 `p0_b_`.

---

## 6.2 Was ist über BCL im Code sicher?

### BCL ist vorhanden, aber bewusst getrennt

- `BclFramer` ist ein reiner 8-Byte-Header-Framer
- basiert auf Desk-Evidence, **nicht** auf eigenem NBT-Wire-Fact
- `BclTypes` definieren:
  - `kHeaderSize = 8`
  - `kDestEtchDesk = 0x0FA4`
  - `kDestWatchdogDesk = 0x1389`

### Live-P0 füttert den Framer nicht

- `drainTransportRaw()` statt `drainTransportToFramer()`

Das bestätigt die Berichte:

> Jede Aussage zu BCL-on-wire wäre aktuell zu weit.

---

## 6.3 Was ist über TX sicher?

- `AoaTransport::write()` liefert im Observe-Pfad `p0_tx_disabled`
- also:
  - keine aktive Wire-Teilnahme
  - keine versehentlichen BCL-Claims
  - gute Beobachtungsdisziplin

---

## 6.4 Was ist über BT-SPP im Repo sichtbar?

`SppTransport` ist nur Stub:

- `implLevel() = "stub"`
- `arm() -> not_implemented`

Das passt zu den Berichten:

- BT-SPP ist **Hypothese / nächster Pfad**
- aber im aktuellen Repo noch nicht implementiert

---

# 7. Was man aus den Fotos technisch ableiten darf

## 7.1 HU-Foto

Anzeige:

- „USB: AOA Observe“
- „USB-Gerät: Zubehörmodus“
- „USB-Modus aktivieren“

Belastbare Aussage:

- Die HU klassifiziert die Probe als USB-Zubehör

Starke, aber noch nicht finale Inferenz:

- Die UI spricht eher einen Datei-/USB-Modus als einen BMW-Connected-Datenkanal an

## 7.2 Android-Popup

Anzeige:

- „Keine installierten Apps für dieses USB-Zubehör“
- URI unter `bmw.com`

Belastbare Aussage:

- Android erkennt das Accessory
- die gesendeten Strings/Description/URI greifen bis ins System

Starke Inferenz:

- kein passender App-Handler / Accessory-Filter / kein Consumer

## 7.3 AAIdrive-Fahrzeuginformationen

Anzeige:

- viele BMW-Parameter
- alle `null`

Belastbare Aussage:

- UI/Screen ist aktiv
- Datenkanal liefert nichts

Starke Inferenz:

- „verbunden“ ist hier eher **State/UI**, nicht Nutzdatenfluss

---

# 8. Konsolidierte Wissenslage: FACT / INFERENCE / OPEN

## 8.1 FACT / belastbar

1. `hu-probe` trennt P0 sauber von BCL/P1
2. `aoa_observe` ist als USBVendor-only-Gadget gedacht
3. AOA-Control-Tap ist Vendor-Control-fokussiert
4. TX ist in P0 blockiert
5. BCL-Framing ist separat und nicht live aktiv
6. HU erkennt die Probe als Zubehör
7. Lab-Host kann echtes Android per 51/52/53 in Accessory schalten
8. BMW/NBT-Strings beeinflussen die UI-/Connected-Wahrnehmung
9. Bulk ist in den beschriebenen Läufen nicht gezeigt
10. BCL über USB ist nicht gezeigt

## 8.2 Starke Inference

1. Es gibt wahrscheinlich **keinen aktiven Accessory-Consumer** auf der Phone-Seite
2. Bulk-Timeout passt dazu
3. „verbunden“ ist eher Identity-/USB-State als realer Datenkanal
4. USB könnte für BMW-Connected nicht der eigentliche Datenträger sein oder nicht unter diesen Bedingungen

## 8.3 Weiter offen

1. Welche echten 52-Strings / Descriptor-Kombinationen verwendet die echte BMW HU?
2. Öffnet eine passende Android-App bei exaktem Filtermatch den Accessory-FD?
3. Nutzt der echte NBT für Connected überhaupt USB-Bulk?
4. Oder liegt der eigentliche BCL-Pfad anders, z. B. auf BT/SPP?

---

# 9. Konsolidierte Auswertung der „Berechnungen / Auswertungen“

Die KI-Berichte nennen mehrere konkrete Mess-/Auswertelemente, die sich konsistent ergänzen:

## 9.1 AOA-Re-Enum-Zeit

Mehrfach genannt:

- Re-Enumeration auf `18d1:2d00`
- ungefähr **0,5–0,6 s**

Diese Zahl ist wichtig, weil sie zeigt:

- der 51→52→53-Übergang ist kein theoretischer Pfad
- sondern praktisch sauber messbar

## 9.2 Bulk-Auswertung

Wiederkehrende Auswertung:

- **Bulk-IN total = 0**
- **Bulk-OUT = Timeout / ENODEV / EIO je nach Zustand**

Interpretation:

- kein lebender Datentunnel
- kein überzeugender Hinweis auf BCL-Frames

## 9.3 BCL-Muster-Suite

Die Berichte nennen Muster wie:

- Handshake v3/v4
- `12 34 56 78`
- `SELECTPROTO`
- `KNOCK`
- `OPEN Etch`
- `DEADBEEF`
- `zeros`
- ASCII

Konsens:

- keine der Pattern-Varianten ergab verwertbare Antwort

Wichtige gemeinsame Schlussfolgerung:

> Die Suche nach der „richtigen Blind-Knock-Sequenz“ ist damit nicht mehr die beste Primärstrategie.

---

# 10. Konsolidierte Bewertung des aktuellen Projektpunkts

Ich würde den Gesamtstand so formulieren:

> **Der neue USB-Zubehör-Weg ist bis zur AOA-/Accessory-Ebene und bis zur App-/UI-Wirkung erfolgreich. Er liefert aber bislang keinen nachgewiesenen USB-Bulk-/BCL-Datenkanal. Die Tests zeigen damit, dass AOA-Identität und „verbunden“-UI alleine nicht ausreichen, um einen echten BMW-Connected-Transport zu belegen.**

Oder noch schärfer:

> **Ihr habt die Identitäts- und Umschaltebene erreicht, aber noch nicht das Datenrohr.**

---

# 11. Empfohlene nächsten Schritte aus dem Konsens aller Berichte

## 11.1 Instrument-Fix / Stage-Fix

Sehr wichtig:

- ControlTap-/Stage-Mismatch sauberstellen
- alte `51/52/53=0`-Befunde danach ggf. neu einordnen

Repo-seitig wichtig:

- `P0_B`-Semantik sauber halten

> `P0_B` ist auf `849f30e` bereits auf 51/52/53/58 begrenzt. Offen bleibt der Stage-Enum-Mismatch (TinyUSB `CONTROL_STAGE_SETUP` vs Arduino `REQUEST_STAGE_SETUP`), beschrieben im [Lab-Bericht](2026-10-30-lab-aoa-aaidrive-phone-host.md).

## 11.2 Android-App-/Accessory-Filter prüfen

Hohe Priorität:

- APK-Manifest
- `res/xml` Accessory-Filter
- version/manufacturer/model
- `dumpsys usb`
- `logcat`
- wann wird `UsbAccessory` wirklich geöffnet?

Das ist der direkteste Hebel, weil das Popup sehr stark in diese Richtung zeigt.

## 11.3 Echten NBT-Lauf klein und gezielt

Nicht mehr breit:

- nur echte HU-Strings / echtes Verhalten
- gezielter Wire-/Phone-/HU-Vergleich

## 11.4 BT-SPP parallel höher priorisieren

Nicht als „USB ist tot“-Urteil, sondern weil das Lab die BT-Hypothese relativ stärkt und den Nutzen weiterer USB-Blindversuche senkt.

---

# 12. Klare Schlussbewertung

## Gemeinsamer Nenner aller vier KIs

- **AOA / Accessory: PASS**
- **UI / „verbunden“: PASS**
- **Bulk / BCL über USB: FAIL / ABSENT / UNKNOWN**
- **nächster Fokus: Instrument + Android-Consumer + echter HU-String + BT-Hypothese**

## Mein Gesamtranking der Berichte

1. **Mistral** – beste Balance aus Technik, Epistemik und Konsequenzen
2. **Grok** – beste operative Statuszusammenfassung
3. **Claude** – sehr stark bei Android-/App-Filter-Interpretation
4. **GPT** – sehr gute Architekturtrennung, etwas stärker hypothetisch in Richtung Folgetransport

---

# 13. Schlussformulierung in einem Satz

> **Der neue Weg als USB-Zubehör ist bis zur AOA-/Accessory- und UI-Ebene experimentell bestätigt; der eigentliche Connected-/BCL-Datenkanal über USB ist jedoch weiterhin nicht nachgewiesen, und die stärkste aktuelle Arbeitshypothese ist, dass entweder kein passender Accessory-Consumer aktiv wird oder der BMW-Datenpfad nicht auf diesem USB-Bulk-Weg läuft.**
