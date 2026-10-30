# Epistemik: Messlücke EP0/Bulk — nach Feld FW 0.3.34

**Datum:** 2026-10-30  
**Anlass:** GPT-Nachbewertung zu PR #14/#15 (Feld Observe)  
**Repo-Basis:** `main` nach Abendbericht  
**Bezug:** [Abend](2026-10-30-feld-c1b-p0-fw034-abend.md) · [Maßnahmen](2026-10-30-massnahmen-c1b-p05.md) · [Lab Stage-Fix](2026-10-30-lab-c1b-p05-stage-fix.md)

---

## Konsens (übernommen)

`AoaControlTap` / Device-RX messen **nicht** den physikalischen USB-Bus, sondern Ereignisse, die bis Arduino/`USBVendor.onRequest` bzw. Vendor-Bulk-RX durchgereicht werden.

| Aussage | Status |
|---|---|
| HU erkennt Probe als AOA-Accessory | **FACT** |
| Enum als `18D1:2D00`, Vendor gemountet | **FACT** |
| Vendor-Callback sieht 51/52/53 nicht (Feld, Direct-2D00) | **FACT** (Messschicht) |
| HU sendet keine 51/52/53 auf dem Bus | **UNKNOWN** |
| HU sendet keinen weiteren USB-Verkehr | **UNKNOWN** |
| Probe sieht sämtlichen USB-Verkehr | **NEIN** / nicht nachgewiesen |
| Kein Bulk-Payload | nur für **unsere** RX-Messschicht (`rx=0`) |
| Application peer | **false** auf beobachteter Schicht |
| AOA funktioniert grundsätzlich (Lab Phone↔Host) | **FACT** |
| BMW-BCL über diesen Pfad | **UNKNOWN** |

Zulässig: *„Im Callback / Device-RX nicht beobachtet.“*  
Nicht zulässig: *„Die HU hat nichts gesendet.“*

Lab A/B (Debian↔Probe) zeigt: bekannte Host-51/52/53 **erreichen** den Tap nach Stage-Fix. Das schließt eine Feld-Messlücke für **andere** Request-Klassen / Bulk / Early-EP0 **nicht** aus.

---

## Drei Ebenen

| Ebene | Frage | Instrument jetzt | Nächstes |
|---|---|---|---|
| **A Bus** | Sendet der Host Transfers? | nein | Host-Trace / Analyzer |
| **B Core** | Sieht TinyUSB/Controller sie? | teilweise (Vendor-CB, Bulk-RX) | **read-only EP0-Core-Hook** + EP-Complete |
| **C App** | Consumer / BCL / Peer? | nein | erst nach A/B |

Drei diskriminierende Fälle nach Core-Hook:

1. Core=0 und Tap=0 → stark: wenig/kein weiterer Host-Verkehr (für sichtbare Pfade).  
2. Core>0, Tap=0 → bisherige Schicht blind.  
3. Core/Tap/Bulk>0, Parser/Peer=0 → USB da, Protokollmodell falsch.

---

## Priorität (abweichend von „nur Consumer“)

1. **EP0-Core-Hook read-only** (SETUP + Stage, nichts beantworten)  
2. Endpoint-/Bulk-Completion-Beobachtung soweit stack-seitig möglich  
3. Parallel: usbmon-Referenz PC↔Phone (URB versucht vs. App öffnet FD nicht)  
4. Consumer/App-Filter (M0) bleibt relevant, ersetzt A/B nicht  
5. Kein weiteres Blind-Knock / Protokollraten

Hook-Anforderung: original TinyUSB-Handling unverändert; Observer → RAM-Ring; Negativbefunde nur mit `capture_valid` + Hook-Health.

---

## Korrektur an Feldberichten #14/#15

Formulierungen „Bulk=0“ / „kein Control“ in den Abend-/Nachmittagsberichten meinen **Messschicht absent**, nicht Bus-Negativbeweis. Dieser Hinweis gilt rückwirkend.
