# Maßnahmen: C1b-P0.5 Instrumentierungs- und Consumer-Validierung

**Projekt:** hu-probe  
**Datum:** 2026-10-30  
**Repo-Basis:** `main` @ `f878d49`  
**Gegenstand:** Bewertung des Analysestands (PR #10) und abgeleitete Maßnahmen.  
**FW-Umsetzung:** `0.3.33` — Schritte 1+3+teilweise 4 im Code; Lab A/B und Schritt 0 noch offen.  
**Quellen:** [Verbesserungsplan](2026-10-30-verbesserungsplan-c1b-p0.md) · [Codeanalyse](2026-10-30-codeanalyse-connected-aoa.md) · [Problemfelder](2026-10-30-aoa-problemfelder.md) · [Gesamtbewertung](2026-10-30-gesamtbewertung-aoa-fuenf-reports.md) · Lab [`2026-10-30-lab-aoa-aaidrive-phone-host.md`](2026-10-30-lab-aoa-aaidrive-phone-host.md)  
**Abweichung:** Dieses Dokument überschreibt die Phasenreihenfolge des Verbesserungsplans für den nächsten Codekick.

---

## 1. Bewertung des Analysestands

| Kriterium | Note | Kurz |
|---|---:|---|
| Analytische Qualität | 9/10 | Ebenen getrennt: Stack ≠ Callback ≠ Accessory ≠ Peer ≠ BCL |
| Code-Nähe | 9/10 | Zentrale Claims am Code verifiziert (`hypothesis_respond_`, Tap-Limits, P0-B bereits gefixt) |
| Epistemische Sauberkeit | 9.5/10 | Pins auf `849f30e`, Ablagehinweise, zulässige/nicht zulässige Aussagen |
| Priorisierung für Codekick | 7.5/10 | Plan Phase 1 startet mit Umbenennungen; Stage-Fix und Android-Consumer fehlen dort |

**Gesamturteil:** Als Entscheidungsgrundlage reif. Nicht spekulativ. Die substanzielle Schwäche ist die Phasenreihenfolge, nicht die Befundlage.

### Verifizierte Code-Fakten (`f878d49` / gleicher FW-Stand)

- `AoaObserveGadget::begin()` setzt `setHypothesisRespond(true)` bedingungslos; Default in `AoaControlTap.h` ist `true`.
- Respond und Observe teilen denselben Gate: `hypothesis_respond_ && … && stage == REQUEST_STAGE_SETUP`.
- `AoaControlTap` vergleicht `REQUEST_STAGE_SETUP` (Arduino); `MscBotTap` vergleicht `CONTROL_STAGE_SETUP` (TinyUSB). Lab: Host-51 → STALL, `setup_count=0`.
- `connected_` kommt aus `vendorMounted()`, nicht aus Application-Peer.
- P0-B-Gate und MSC-Leak sind bereits behoben (Ablagehinweise korrekt).

### Korrektur zur Hypothesen-Aussage

| Frage | Status |
|---|---|
| Q1: Ist P0 passiv konfiguriert? | **Nein** (`hypothesis_respond_=true`) |
| Q2: Hat die Probe im Feld auf 51/52/53 geantwortet? | **Unbekannt** — bei Stage-Mismatch wahrscheinlich nein |

Zulässige Formulierung für Retro-Caveats:

> Die Firmware war im AOA-Observe-Pfad mit `hypothesis_respond=true` konfiguriert; die Läufe waren daher nicht strikt passiv konfiguriert. Ob hypothetische Antworten den Bus erreichten, ist wegen ungeklärter Stage-Semantik nicht nachgewiesen.

---

## 2. Was der Verbesserungsplan falsch priorisiert

1. **Stage-Enum** steht nur im Repo-Abgleich, nicht in Phase 1 — gehört aber vor jede Callback-Auswertung.
2. **Android Accessory-Consumer** (AAIdrive Manifest/`res/xml`/logcat/`dumpsys usb`) fehlt als konkreter Task, obwohl Multi-Review und Screenshot („Keine installierten Apps…“) ihn als stärksten Hebel markieren.
3. **Umbenennungen** (`connected_`, P0-B-Split, Zähler-Aliase) verbessern Interpretation, machen aber blinde Instrumente nicht sehend.
4. **Queue 32→128** und **EP0-Core-Hook** sind richtig langfristig, aber nachträglich zur Frage „kommt überhaupt etwas im Callback an?“.
5. Stage-Fix und `hypothesis_respond=false` dürfen **nicht gleichzeitig** geändert werden — sonst fehlt der Lab-Vergleich.

Research-Reports 2 und 4 sind Recherche-Hintergrund; für den Codekick sind Report 3 (EP0) und Report 1 (51/52/53) sowie Lab + Codeanalyse ratgebend.

---

## 3. Maßnahmenreihenfolge (verbindlich für den nächsten Kick)

### Schritt 0 — kein Firmware-Build (sofort)

**Ziel:** Existiert ein Accessory-Consumer für unseren AOA-Fingerprint?

| # | Maßnahme | Done-Kriterium |
|---|---|---|
| M0.1 | AAIdrive-APK: Manifest + `USB_ACCESSORY_ATTACHED` + `res/xml/*accessory*` | Erwartete `manufacturer`/`model`/`version`/`uri`/`serial` dokumentiert |
| M0.2 | Abgleich mit Probe-Strings (`AoaObserveGadget` Accessory- vs. PhoneLike-Profil) | Match / Partial-Match / No-Match klar benannt |
| M0.3 | Lab-Phone: `dumpsys usb` + logcat um Attach/Detach/openAccessory | Ob `openAccessory()` / FD je vorkam; Korrelation zum UI-Toast |
| M0.4 | Ergebnis in Lab-Bericht oder kurzem Addendum festhalten | Aussage: Consumer fehlt / erwartet andere IDs / unklar |

Kein FW nötig. Kann Parallel zu Schritt 1 vorbereitet werden.

---

### Schritt 1 — minimaler FW-Fix: nur Stage-Semantik

**Eine** Änderung: Stage-Vergleich in `AoaControlTap` an TinyUSB-Semantik angleichen (wie `MscBotTap`: `CONTROL_STAGE_SETUP`), bzw. beide Konstanten explizit mappen und loggen.

| # | Maßnahme | Done-Kriterium |
|---|---|---|
| M1.1 | Stage-Gate korrigieren / eindeutig machen | Code vergleicht die Konstante, die der Callback tatsächlich liefert |
| M1.2 | Optional: `stage`-Rohwert immer in Sample/Counter sichtbar | Lab kann SETUP vs. DATA vs. STATUS unterscheiden |
| M1.3 | **Nicht** ändern: `hypothesis_respond_`, Renames, Queue, EP0-Hook, P0-B-Split | Diff bleibt minimal und attributierbar |

---

### Schritt 2 — Lab A: Stage-Fix bei noch aktivem Responder

Host = echtes Android (wie Lab 2026-10-30), Probe = `aoa_phone_like` (oder bekannter AOA-Host-Pfad).

| # | Maßnahme | Done-Kriterium |
|---|---|---|
| M2.1 | Host sendet 51, dann 52×6, dann 53 | usbmon/pyusb-Trace vorhanden |
| M2.2 | Device: Callback? stage? Counter? Response? USB-Ergebnis? | Pro Request: gesehen / geantwortet / STALL / drop |
| M2.3 | Entscheid Q2 vorläufig | „Responder funktioniert nach Stage-Fix“ ja/nein |

Wenn nach Stage-Fix weiterhin `setup_count=0` bei nachweislichem Host-51: dann erst EP0-Hook (Schritt 5), nicht Umbenennungen.

---

### Schritt 3 — Lab B: Responder abschalten (echter Observe)

| # | Maßnahme | Done-Kriterium |
|---|---|---|
| M3.1 | `setHypothesisRespond(false)` als Default für Observe; Responder nur in explizitem Modus (`PhoneLikeResponder` o.ä.) | Observe ≠ Responder im Status erkennbar |
| M3.2 | Denselben Host-Lauf wie Lab A wiederholen | Vergleich A vs. B: Counter gleich? Responses weg? Host-Verhalten? |
| M3.3 | Observe und Capture von Response entkoppeln | Request-52-Daten und Counter nicht hinter `hypothesis_respond_` verstecken |

Architekturziel (Plan § Response/Observe trennen), minimal umgesetzt:

```text
USB request ──┬── observe / counter / queue   (immer)
              ├── capture data stage          (wenn sichtbar)
              └── optional respond            (nur Responder-Modus)
```

---

### Schritt 4 — Semantik bereinigen (nach Lab A/B)

Erst wenn die Instrument-Sicht geklärt ist:

| # | Maßnahme | Priorität |
|---|---|---|
| M4.1 | `connected_` → `usb_function_mounted_` / Statusfelder trennen; `application_peer_proven=false` | hoch, geringes Risiko |
| M4.2 | `P0_B` → `aoa_entry_callback_seen` (+ optional Audio); keine große State-Machine | mittel |
| M4.3 | `hypothesis_handled` → `device_response_queued`; Capture-Validität: `drop_count==0` Pflicht für Negativbefunde | hoch |
| M4.4 | Zähler-Aliase / Legacy-Markierung `aoa_control_count` | niedrig |
| M4.5 | Queue 32→128 nur wenn Drops in Lab/Feld real vorkommen | später |
| M4.6 | Retro-Caveat in Feldberichten 0.3.26–0.3.32 (Formulierung aus §1) | Dokumentation |

---

### Schritt 5 — EP0-Core-Hook (bedingt)

Nur wenn nach Schritt 2/3 noch eine relevante Beobachtungslücke bleibt (Host sendet 51, Callback bleibt tot).

| # | Maßnahme |
|---|---|
| M5.1 | TinyUSB EP0-SETUP vor/unter `USBVendor.onRequest` |
| M5.2 | Dispatch + DATA/STATUS/Completion |
| M5.3 | Rohtrace unabhängig von EventBus für Negativbefunde |

Das ist C1b-P1-Instrumentierung, nicht ein weiterer P0-Hack.

---

## 4. Explizit zurückstellen

| Thema | Warum zurück |
|---|---|
| BCL-/Knock-Erweiterung | Bulk-Consumer unbewiesen; weiteres Knocking nicht diskriminierend |
| SPP-Implementierung | Stub belassen; nur Beobachtungsvorbereitung laut Plan |
| Composite MSC+Vendor wieder aktivieren | Methodisch kontaminiert (bereits isoliert) |
| Große P0-B-Zerlegung / LinkState-Enum v1 | Semantik ok, messungsblindheit nicht |
| Hardware-Analyzer | Erst bei Widerspruch Host-Trace ↔ Device nach Stage-Fix |

---

## 5. Wissensgraph (aktuell)

```text
AOA Accessory-Identity     → PASS (HU + Lab UI)
51/52/53 Transition        → zu validieren (Stage + Lab A/B)
Android erkennt Headunit   → PASS
UI „verbunden“             → PASS
Accessory-Consumer / FD    → Schritt 0
Bulk auf Device-RX         → nicht nachgewiesen
BCL                        → UNKNOWN
```

AOA-Weg nicht verwerfen. Nicht als Connected-App-Transport behandeln. Nächster diskriminierender Block ist **C1b-P0.5** (dieses Dokument), nicht Phase 1–3 des Verbesserungsplans in Originalreihenfolge.

---

## 6. Definition of Done für C1b-P0.5

1. Consumer-Frage (M0) schriftlich beantwortet.  
2. Stage-Semantik lab-verifiziert (M1–M2).  
3. Observe-Modus wirklich passiv und mit Lab-B-Vergleich belegt (M3).  
4. Feldberichte mit Retro-Caveat versehen (M4.6).  
5. Entscheidung dokumentiert: EP0-Hook nötig ja/nein (M5 Gate).

Danach erst: umfassende Statussemantik und ggf. EP0-Hook laut Verbesserungsplan Phase 2/3.

---

## 7. Umsetzungsstand FW 0.3.33

| Maßnahme | Status |
|---|---|
| M1.1 Stage-Gate TinyUSB `CONTROL_STAGE_*` in `AoaControlTap` | **Code** |
| M1.2 `stage` / `stage_name` in `ca_usb` | **Code** |
| M3.1 `aoa_observe` passiv; `aoa_phone_like` Responder; API-Toggle | **Code** |
| M3.3 Observe-Zähler unabhängig von Respond | **Code** |
| M4.1 `usb_function_mounted` / `application_peer_proven` | **Code** |
| M4.2 `P0_B_aoa_entry_callback_seen` + scope | **Code** |
| M4.3 `device_response_queued` + `capture_valid` | **Code** |
| MSC: `stage` in usb-control Events; `control_setup_count` / `capture_valid` in health | **Code** |
| MSC: AOA `transport_epoch` / `aoa_capture_valid` in `capture_health` bei AOA-Boot | **Code** |
| M0 Android-Consumer | offen |
| M2/M3 Lab A/B | **PASS** — siehe [`2026-10-30-lab-c1b-p05-stage-fix.md`](2026-10-30-lab-c1b-p05-stage-fix.md); 51-Response in **0.3.34** behoben (`0100`) |
| M4.6 Retro-Caveat Feldberichte | offen |
| M5 EP0-Hook | zurückgestellt (Callback sieht SETUP — Hook nicht nötig für Zählen) |
