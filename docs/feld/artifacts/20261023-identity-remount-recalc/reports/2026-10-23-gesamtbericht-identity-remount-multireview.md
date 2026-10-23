# Hu-Probe-Gesamtbericht_2026-10-23

**Projekt:** hu-probe  
**HU:** BMW NBT Evo  
**Datum des Feldtests:** 2026-10-23  
**Firmware:** 0.3.19 (`fw_dirty:false`, clean build)  
**Repo-Stand:** `62cb4f8`  
**Zweck:** Konsolidierter Gesamtbericht für die nächste KI-/Cursor-Instanz ohne GitHub-Zugriff.  
**Quellenbasis:** Gesamtbericht, Mistral-Wire-Nachrechnung, GPT-Review, Grok-Zusammenfassung, Claude-Meta-Review und spätere Korrekturen/Konsolidierungen  
**Operator-Bericht (Roh):** [`2026-10-23-gesamtbericht-identity-remount.md`](2026-10-23-gesamtbericht-identity-remount.md)  
**Self-recalc-Paket:** `docs/feld/artifacts/20261023-identity-remount-recalc/` · Builder `tools/build_identity_remount_recalc_2026_10_23.py`

---

## 1. Executive Summary

Der Feldtest vom 2026-10-23 hat die **Identity-/Remount-Schiene** deutlich geklärt:

- **Soft-Remount** ist für die HU praktisch unsichtbar und kein brauchbarer HU-Reset.
- **Kurze USB-Cycles** führten in den überprüften Sessions zu **FS-Re-Enumeration**, aber **nicht** zu einem erneuten Datei-Head-Scan.
- **Resume überlebt Änderungen an Serial, Label und Volume-ID**, einzeln und kombiniert. Diese drei Merkmale sind damit **nicht ausreichend** als Resume-Key.
- Der eigentliche **Resume-/Cache-Key** ist **weiter unbekannt**. Aus den vorliegenden Daten folgt **nicht**, dass er sicher „der Inhalt“ ist; nur ein Teil des Schlüsselraums wurde ausgeschlossen.
- Nach Re-Mount/Resume wurde bei kleinen Testdateien meist ein **Vollatom** gelesen, häufig als `tail_then_head`, **nicht** als klarer Offset-Read ab Resume-Position.

**Wichtige methodische Einschränkung:**  
Fast alle belastbaren Read-Aussagen beruhen auf Dateien von **ca. 158 KiB**. Diese Befunde dürfen **nicht ohne Größenleiter** auf längere MP3-Dateien extrapoliert werden. Für PiDrive ist das eine der wichtigsten offenen Grenzen.

---

## 2. Ziel dieses Berichts

Dieser Bericht soll der nächsten GPT-/Cursor-Instanz einen **eigenständig verwertbaren Stand** liefern, auch ohne Zugriff auf GitHub oder Rohartefakte. Deshalb enthält er:

1. den **Konsens** aller Auswertungen,  
2. die **abweichenden Positionen** der beteiligten KIs,  
3. eine **Session-by-Session-Evidence-Tabelle**,  
4. die Trennung zwischen **FACT / STRONG INFERENCE / HYPOTHESIS / UNKNOWN**,  
5. ein **konsolidiertes Arbeitsmodell**,  
6. die **wichtigsten Korrekturen am bisherigen Bericht**,  
7. eine **konkrete Testpriorisierung**.

---

## 3. Quellenlage und Beweisgewicht

### 3.1 Mistral
Mistral ist die wichtigste technische Primärquelle, weil dort **vier von sieben Live-Sessions am Wire nachgerechnet** wurden sowie die Offline-I0-Session analysiert wurde. Die meisten belastbaren Aussagen zu READ10, Extents, Head-/Tail-Mustern und Scan-/Resume-Verhalten stammen von dort.

### 3.2 GPT
GPT lieferte eine starke konzeptionelle Einordnung, gute Trennung zwischen Resume-State und Audio-Cache sowie sinnvolle Testmatrizen. Mindestens ein zentraler Punkt musste aber von Mistral korrigiert werden: GPT sprach teilweise von **„erneutem Scan“ nach Cycle**, obwohl die Wire-Befunde der verifizierten kurzen Cycle-Sessions nur **FS-Enum ohne Datei-Head-Scan** zeigen.

### 3.3 Grok
Grok liegt in vielen Punkten nahe an Mistral und Claude, liefert aber keine klar erkennbare eigene Wire-Verifikation. Deshalb ist Grok für Konsensbildung nützlich, aber nicht führend.

### 3.4 Claude
Claude lieferte den methodisch strengsten Meta-Review: klare Trennung zwischen Beobachtung und Interpretation, Korrekturen bei Überverallgemeinerungen, Ergänzung neuer Kandidaten C6–C8 sowie neue Testachsen wie **Detach-Dauer-Leiter**, **Skip-Test** und **Größenleiter**.

### 3.5 Konsolidierte Gewichtung
Für diesen Bericht gilt:

- **Wire-FACTs:** primär Mistral  
- **Methodische Korrekturen:** primär Claude  
- **Modellbildung/Testplanung:** GPT + Claude + Mistral-Korrekturen  
- **Konsensdarstellung:** Grok unterstützend

---

## 4. Testkontext des Feldtags

Der Feldtag sollte insbesondere klären:

- Wirkung von `/api/usb/remount`
- Wirkung von `/api/usb/cycle`
- Veränderungen von:
  - USB Serial
  - Volume Label
  - Volume-ID
  - Kombination aller drei
- Beobachtung von:
  - Resume-Verhalten
  - Scan-/Re-Scan-Verhalten
  - Bibliothek vs. Dateireads
  - möglichem Resume-/Cache-Key

Untersuchte Schlüsselsessions:
- `074319` ident_serial
- `074455` ident_label
- `074749` ident_b
- `074908` ident_vol
- `072929` offline seq_ident I0

---

## 5. Session-by-Session Evidence Table

Diese Tabelle ist absichtlich konservativ formuliert. Sie trennt zwischen **dokumentiertem Wire-/Operator-Befund** und **zulässiger Schlussfolgerung**.

| Session | Test | Mutation / Zustand | Stimulus | Wire-Befund | Resume / Operator | Zulässige Aussage |
|---|---|---|---|---|---|---|
| **074319** | `ident_serial` | Serial-Mutation; später `HP0S01` dokumentiert | **RST** vor bzw. im Versuch | **Head-Scan sichtbar:** pro Datei `32 + 128 = 160` Blocks; danach bekannte Scan-/Prefetch-Grammatik; frischer Start head-first | Start bei Track 1 | **FACT:** Nach RST wurde der bekannte Head-Scan beobachtet. **Nicht zulässig:** „Serial allein löst Scan aus“, da der Versuch durch RST konfundiert ist |
| **074455** | `ident_label` | Label geändert auf `HUPROBE_L` | kurzer **USB-Cycle** | FS/FAT/Root/Probe-Zugriffe; **kein** 160-Block-Datei-Head-Scan; direkter Wiedereinstieg / Vollatom | Resume blieb erhalten; Operator: Position 1 → 2 → 3 | **FACT:** Label allein verwirft Resume nicht. **FACT:** kurzer Cycle ≠ Datei-Head-Rescan |
| **074749** | `ident_b` | **Serial + Label + Volume-ID neu:** `HP0B01`, `HUPROBE_B`, `0x48500B01` | kurzer **USB-Cycle** | FS/FAT/Root/Probe-Enum; **kein** Datei-Head-Scan; danach Resume-Atom | Resume bei Position 2 | **FACT:** Selbst die kombinierte Mutation aller drei getesteten Identity-Felder löscht Resume nicht |
| **074908** | `ident_vol` | Volume-ID geändert auf `0x48500701` | kurzer **USB-Cycle** | FS/FAT/Root/Probe-Enum; **kein** Datei-Head-Scan; direkter Resume-Einstieg | Resume bei Position 3 | **FACT:** Volume-ID allein ist kein ausreichender Resume-Key |
| **072929** | Offline `seq_ident` / I0 | `cache_a_pin`; Sequenz-/Replay-Kontext | **Offline-Capture / Replay-Kontext** | In den Reviews als Offline-Quelle ausgewertet; zugleich bestehen Replay-/Upload-Probleme und Artefakte (B56/B59) | Operator: Autoplay T1→T2→T3, später Replay/`media_hide` im Verlauf | **Mit Vorsicht verwenden:** nützlich für Muster-/Konsistenzhinweise, aber durch Infrastrukturprobleme konfundiert; nicht gleich stark wie die Live-Wire-Sessions |

---

## 6. Die vier entscheidenden Live-Sessions im Detail

### 6.1 074455 — `ident_label`
- Firmware: 0.3.19  
- Mutation: Label `HUPROBE_L`  
- Stimulus: kurzer USB-Cycle  
- Wire: FS/FAT/Root/Probe, **kein Datei-Head-Scan**, danach direkter Wiedereinstieg  
- Operator: Resume Position 1 → 2 → 3

**Bedeutung:**  
Labelwechsel allein löscht den Resume-Zustand nicht. Gleichzeitig zeigt die Session: kurzer Cycle bedeutet hier **FS-Re-Enum**, aber **kein erneutes Einlesen der Datei-Heads**.

---

### 6.2 074749 — `ident_b`
- Mutation:
  - Serial `HP0B01`
  - Label `HUPROBE_B`
  - Volume-ID `0x48500B01`
- Stimulus: kurzer USB-Cycle  
- Wire: FS/FAT/Root/Probe, **kein Datei-Head-Scan**, anschließend Resume-Read  
- Operator: Resume bei Position 2

**Bedeutung:**  
Das ist der stärkste Einzelbeleg gegen die These, dass **Serial / Label / Volume-ID** den Resume-Zustand primär bestimmen. Selbst die gleichzeitige Änderung aller drei reicht nicht aus, um Resume zu verlieren.

---

### 6.3 074908 — `ident_vol`
- Ausgangspunkt: `cache_a_pin`
- Mutation: nur Volume-ID geändert, `0x48500701`
- Stimulus: kurzer USB-Cycle  
- Wire: FS/FAT/Root/Probe, **kein Datei-Head-Scan**  
- Operator: Resume bei Position 3

**Bedeutung:**  
Sauberer Retest für die Volume-ID. Diese Session stützt stark, dass Volume-ID allein **kein ausreichender Resume-Key** ist.

---

### 6.4 074319 — `ident_serial`
- Sessionname: `ident_serial`
- Wichtig: **RST-Konfundierung**
- Nach RST wurde der bekannte Scan wieder sichtbar:
  - `32 + 128` Head-Read je Datei
  - danach Scan-/Prefetch-Grammatik

**Bedeutung:**  
Diese Session zeigt, dass nach **RST** der bekannte Head-Scan wieder auftritt.  
**Nicht zulässig** ist jedoch die Aussage:

> „Serial wurde geändert und dadurch wurde neu gescannt.“

Korrekt ist nur:

> Nach RST trat der bekannte Head-Scan auf; deshalb ist 074319 **kein sauberer Serial-only-Test**.

---

## 7. Was die Sessions gemeinsam gesichert zeigen

| Frage | Ergebnis |
|---|---|
| Soft-Remount für die HU sichtbar? | praktisch nein / Operator-seitig wirkungslos |
| Echter USB-Cycle erzeugt Re-Mount? | ja |
| FS/FAT/Root wird nach kurzem Cycle erneut enumeriert? | ja |
| Datei-Head-Scan nach kurzem Cycle in 074455/074749/074908? | nein |
| Resume nach Label-Mutation? | ja |
| Resume nach Volume-ID-Mutation? | ja |
| Resume nach Serial+Label+Vol-Mutation? | ja |
| Serial/Label/Vol als alleiniger Resume-Key? | verworfen |
| RST kann vollständigen Head-Scan auslösen? | ja |
| Beweist 074319, dass Serial den Scan auslöst? | nein |
| Resume-Key = Dateiinhalt? | unbekannt |
| Resume-State = Audio-Body-Cache? | unbekannt |
| Verhalten bei Dateien deutlich >158 KiB? | unbekannt |

---

## 8. Konsens über alle Reviews

Alle Reviews bzw. ihre späteren Korrekturen stimmen in folgenden Punkten überein:

1. **Soft-Remount ist kein brauchbarer HU-Reset**  
2. **USB-Cycle ist ein echter Re-Mount mit erhaltenem Resume**  
3. **Serial / Label / Volume-ID sind nicht der Resume-Key**, auch nicht kombiniert  
4. **Resume-State und Audio-Cache sind getrennt zu betrachten**  
5. **Infrastruktur zuerst:** B59 Upload-Stall und B56 Replay-Artefakte  
6. **Identity-Spielereien bringen PiDrive nicht weiter**; wichtig sind kontrollierte Medienobjekte und große Dateien

---

## 9. Wichtigste Korrekturen gegenüber früheren Formulierungen

### 9.1 „USB-Cycle → erneuter Scan“
Diese Formulierung ist für die geprüften kurzen Cycle-Sessions **zu stark**. Korrekt ist:

> Bei den untersuchten kurzen USB-Cycles wurde das Dateisystem neu enumeriert, **ohne erneuten Datei-Head-Scan**. Ein Head-Scan wurde in dieser Serie nach dem RST-Fall beobachtet.

---

### 9.2 „Resume keyed am Inhalt“
Diese Formulierung ist **nicht bewiesen**. Bisher ausgeschlossen wurde nur ein Teil des Schlüsselraums:
- Serial
- Label
- Volume-ID

Konstant geblieben und damit **noch ungetestet** sind u. a.:
- VID/PID
- Product-/Manufacturer-String
- SCSI INQUIRY
- Kapazität / `sector_count`
- Port
- weitere USB-/SCSI-Merkmale

Korrekte Formulierung:

> Der getestete Identity-Teil des Resume-Schlüssels ist ausgeschlossen; der verbleibende Schlüsselraum umfasst sowohl USB-/SCSI-Metadaten als auch Medien-/Dateieigenschaften.

---

### 9.3 „Entry-tail-first nach jedem Mount“
Auch das war zu stark formuliert. Claude und Mistrals spätere Korrektur begrenzen die Aussage:

- beobachtetes `tail_then_head` n=7 betrifft **Re-Mount/Restore bekannter Zustände**
- **Erst-Mounts / Frischstart** lesen head-first

Korrekt ist daher:

> Tail-first korreliert mit Resume/Restore eines bekannten Zustands, nicht allgemein mit „Mount“.

---

### 9.4 „Elevator“ als gesichertes Modell
Das ist **nicht belegt**. Die Daten reichen nicht aus, um Elevator sauber von anderen Navigation-/Sprunghypothesen zu trennen. Deshalb:

- **Nicht als FACT führen**
- nur als offene Hypothese / unterbestimmtes Modell behandeln

---

### 9.5 „Re-Play ohne Cache-Hit“ als Gegenbeleg
Der M14-Befund ist wegen `media_hide` / Remount-Kontext **konfundiert**. Er taugt nicht als harter Gegenbeleg gegen bekannte A→B→A-Cache-Hit-Beobachtungen ohne Remount.

---

## 10. Konsolidiertes Arbeitsmodell

```text
Detach / Reconnect
        │
        ▼
Mount / FS-Enum (FAT, Root, Probes)
        │
        ├─ kurzer Cycle, bekannter Zustand
        │      ├─ Bibliothek/Heads bleiben
        │      ├─ Resume-Position bleibt
        │      ├─ kein Datei-Head-Scan
        │      └─ Wiedereinstieg / Vollatom bei kleinem Medium
        │
        └─ RST / längere Abwesenheit / Zustand verloren (?)
               ├─ Head-Scan 32+128 je Datei
               ├─ Start bei Track 1
               └─ frischer Start head-first
```

Ergänzend:
- Resume-Key weiter offen  
- Audio-Body-Cache separat betrachten  
- Navigation-/Skip-Leseordnung offen  
- Verhalten großer Dateien offen

---

## 11. FACT / STRONG INFERENCE / HYPOTHESIS / UNKNOWN

### FACT
- Soft-Remount praktisch unsichtbar / kein brauchbarer Reset
- Kurze USB-Cycles: FS-Enum, aber kein Datei-Head-Scan in den geprüften Sessions
- Resume überlebt Serial/Label/Volume-ID einzeln und kombiniert
- RST-Fall zeigt Head-Scan
- TUR ≈ 2238 ms, Chunk-Pace ≈ 3,94 ms p50, FW 0.3.19 clean

### STRONG INFERENCE
- tail-first korreliert mit Restore/Resume eines bekannten Zustands
- Zweistufiges Prefetch-Modell:
  - nach Scan ≈ 7,8 s Vorlauf
  - nach Cycle/Resume ≈ 0 Lead

### HYPOTHESIS
- Elevator-/Navigationsmodell
- Zwei-Schichten-Modell Bibliothek/Resume vs. Body-Cache, soweit hu-probe-intern noch nicht reproduziert

### UNKNOWN
- eigentlicher Resume-Key  
- Rolle von C1–C8  
- Verhalten bei großen Dateien  
- exakter Zusammenhang READ → hörbares Audio (GATE-01)

---

## 12. Offene Kandidaten für den Resume-/Cache-Key

### Bereits ausgeschlossen
- Serial
- Label
- Volume-ID
- Kombination aus allen drei

### Noch offen
**Medien-/Dateieigenschaften**
- C1 ID3
- C2 Audioinhalt / Seed
- C3 Dateigröße
- C4 Pfad / Dateiname
- C5 FAT/LBA-Layout

**USB-/SCSI-Metadaten**
- C6 VID/PID/Product-String
- C7 INQUIRY Vendor/Product/Revision
- C8 Kapazität / `sector_count`

**Wichtige methodische Notiz:**  
C6–C8 sind **nicht wahrscheinlicher** als Content-Kandidaten. Sie sind nur **bisher nicht getestet**.

---

## 13. Neue Kritik am bisherigen Bericht

### 13.1 Session-Evidenz war zu stark komprimiert
Die frühere Fassung nannte zwar die Sessions, machte aber nicht hinreichend deutlich, **welche Session welchen Schluss trägt**. Für eine KI ohne GitHub-Zugriff ist das gefährlich, weil sonst leicht falsche Kausalitäten entstehen, z. B.:

```text
ident_serial
   ↓
RST
   ↓
Head-Scan
   ↓
Serial verursacht Scan
```

Diese Schlusskette ist **nicht zulässig**.

---

### 13.2 074319 muss explizit als konfundiert markiert werden
Der Bericht sagte zwar, dass 074319 das bekannte Scan-Muster zeigte und dies nach RST geschah. Das ist richtig, aber nicht scharf genug.  
Die präzisere Aussage lautet:

> 074319 ist **kein valider isolierter Serial-Test**, weil der RST den Versuch konfundiert.

---

### 13.3 074455 / 074749 / 074908 sind die eigentliche Kernevidenz
Diese drei Sessions bilden zusammen das stärkste Mini-Experiment des Tages:

- Label ändern → Resume bleibt
- Volume-ID ändern → Resume bleibt
- Serial+Label+Vol ändern → Resume bleibt
- kurzer Cycle → FS-Enum → kein Datei-Head-Scan → Resume

Diese Evidenzgruppe verdient im Bericht höheres Gewicht als 074319.

---

### 13.4 „Resume keyed am Inhalt“ muss UNKNOWN bleiben
Die Daten schließen nur einen Teil des Schlüssels aus. Sie identifizieren **keinen neuen Key eindeutig**.

---

### 13.5 Die 158-KiB-Einschränkung muss an die Wire-Befunde gekoppelt bleiben
Aussagen wie:
- „kein Offset-Read“
- „vollständiges Atom nach Resume“
- „Lead ≈ 0“
gelten nur für den bisher getesteten Bereich kleiner Dateien und dürfen nicht direkt auf große MP3s übertragen werden.

---

### 13.6 Das Zweistufen-Prefetch-Modell bleibt Modell
Ohne `audible_change` bzw. akustischen Marker bleibt der exakte Vorlauf relativ zum hörbaren Trackbeginn nicht vollständig abgesichert.

---

### 13.7 072929 nur vorsichtig verwenden
Die Offline-I0-Session ist wertvoll für Muster- und Infrastrukturhinweise, aber wegen Upload-/Replay-Problemen nicht gleich belastbar wie die verifizierten Live-Sessions.

---

## 14. Konsequenzen für PiDrive

### Was nicht mehr priorisiert werden sollte
- weitere Identity-Varianten mit Serial/Label/Vol

### Was stattdessen relevant ist
- stabile Medienobjekte
- Directory / Head / Size / Extents
- Verhalten bei großen Dateien
- Trennung von Resume-State und Audio-Body
- GATE-01 / hörbare Wiedergabe

### Wichtigste offene PiDrive-Fragen
1. Was unterscheidet kurzen Cycle von RST/langer Abwesenheit?  
2. Überlebt der Audio-Body-Cache einen Cycle?  
3. Wie verhält sich die HU bei 1 MiB, 4 MiB, 16 MiB?  
4. Welche Leseordnung entsteht bei nicht-sequenziellem Play-Intent?  
5. Wann entsteht tatsächlich hörbarer Output?

---

## 15. Nächste Tests – priorisiert

### P0 – Infrastruktur
- **B59** Upload-Stall beheben  
- **B56** Replay-Artefakte beheben

### Session A – billig, vorhandenes Medium
- **T1 Detach-Dauer-Leiter**: 0,2 / 1 / 3 / 10 / 30 s
- **T6 A→B→A ohne Remount**
- **T2 Leseordnung bei nicht-sequenziellem Play-Intent**:
  - T1→T3
  - T3→T2
  - T1→T2 in verschiedenen Zeitlagen
- Audible-Tap opportun mitlaufen lassen

### Session B – PiDrive-kritisch
- **T4 Größenleiter**:
  - 158 KiB
  - 1 MiB
  - 4 MiB
  - 16 MiB

Ziel:
- Plateau
- Mitten-Nachladen
- verzögerte Reads
- EOF-Verhalten

### Session C – Schlüsselraum
- **T3a** per Cycle:
  - C6 VID/PID/Product
  - C7 INQUIRY
  - C8 Kapazität
- **T3b** in-session ohne Remount:
  - C1 ID3
  - C2 Audio
  - C3 Size
  - C4 Path
  - C5 LBA

---

## 16. Was die nächste KI nicht tun soll

- „Resume keyed am Inhalt“ als FACT schreiben
- 074319 als Serial-Beweis behandeln
- kurze Cycles als Datei-Re-Scan beschreiben
- Elevator als bewiesen darstellen
- M14 als harten Gegenbeleg gegen A→B→A-Cache-Hit verwenden
- 158-KiB-Befunde auf große Dateien extrapolieren
- weitere Serial/Label/Vol-Cycles priorisieren

---

## 17. Kurzliste für Cursor / GPT ohne GitHub

### Sofort übernehmen
- Soft-Remount kein brauchbarer Reset
- kurze Cycles: FS-Enum, aber kein Datei-Head-Scan in den geprüften Sessions
- Resume überlebt Label / Vol-ID / Serial+Label+Vol
- 074319 ist durch RST konfundiert
- B56/B59 zuerst beheben

### Als Hypothese markieren
- tail-first = Restore bekannter Zustand
- Zweistufen-Prefetch
- Zwei-Schichten-Modell

### Neu in den Plan aufnehmen
- Detach-Dauer-Leiter
- Größenleiter
- USB-Deskriptor-/INQUIRY-/Kapazitätsmutationen
- GATE-01 / Audible-Tap bei Gelegenheit mitlaufen lassen

---

## 18. Endfazit

Der Feldtag 2026-10-23 hat die Identity-Schiene weitgehend abgeschlossen:

- **Serial, Label und Volume-ID sind nicht ausreichend als Resume-Key**
- **Kurze USB-Cycles führen in den geprüften Sessions zu FS-Re-Enum, aber nicht zu Datei-Head-Rescan**
- **Soft-Remount ist als HU-Reset unbrauchbar**

Die nächste sinnvolle Arbeit liegt deshalb **nicht mehr in weiteren Identity-Experimenten**, sondern in drei Richtungen:

1. **Infrastruktur stabilisieren**  
2. **kurzer Cycle vs. RST/lange Abwesenheit sauber trennen**  
3. **große Dateien und echten Audio-Pfad untersuchen**

Damit ist die HU-Seite für **kleine Dateien** bereits recht gut verstanden. Für PiDrive fehlen aber noch die entscheidenden Restbedingungen:
- Größenleiter
- Body-Cache-Reproduktion
- Resume-Key-Zerlegung über den verbleibenden Schlüsselraum
- GATE-01 / hörbarer Audio-Nachweis
