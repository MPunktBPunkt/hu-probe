# Feld C1b-P0 FW 0.3.34 — NBT (erster Lauf + Abend)

**Konsolidiert:** 2026-11-07 aus `feld-c1b-p0-fw034-erster-lauf` + `…-abend`.  
**Status:** siehe [GATES-Connected C1b-P0](../GATES-Connected.md) · Epistemik: [messluecke-ep0-bulk](AOA-EPISTEMIK-EP0.md)

---

## Teil A — Erster Lauf

# Feld: Erster C1b-P0-Lauf mit FW 0.3.34

**Datum:** 2026-10-30 ~16:08–16:12  
**Probe:** Car `192.168.178.89` · MAC `68B6B32BC568`  
**FW:** **0.3.34** (OTA von 0.3.32)  
**Gadget:** `aoa_observe` · `hypothesis_respond=false` · `aoa_mode=observe`  
**HU:** NBT Evo — mehrfach gesteckt, Zubehör in der HU gewählt  
**Instrument-Basis:** Lab Stage-Fix + 51-Respond ([lab-c1b-p05-stage-fix](2026-10-30-lab-c1b-p05-stage-fix.md))  
**Artefakte:** [`artifacts/20261030-feld-c1b-p0-0334/`](artifacts/20261030-feld-c1b-p0-0334/)

---

## Setup

1. OTA-Waiter flashte `.89` → 0.3.34 sobald WLAN da war.  
2. Link armed (`POST /api/connected/link/start`) + soft-attach.  
3. ~90 s Live-Monitor + manuelle Steckzyklen / Gerätewahl an der HU.  
4. Kurz WiFi-Einbruch (`HTTP_000`) → Session-Reset; danach Re-Arm.

Sessions: `20261030-161013_lab_ca_c1_link`, `20261030-161212_lab_ca_c1_link`.

---

## Messwerte (Instrument belastbar)

| Ebene | Befund | Bewertung |
|---|---|---|
| P0-0 Isolation | `aoa_observe`, kein MSC | **PASS** |
| P0-A Attach | mehrfach Mount/Unmount; `attach_count` stieg (Monitor 3→4, nach Reset 1→3) | **PASS** |
| P0-B 51/52/53/58 | Callback-Zähler durchgehend **0**; kein `ca_usb` aoa_control | **ABSENT** (Direct-`2D00`: kein Klassik-Handshake erwartet) |
| P0-C VID/PID | `0x18D1:0x2D00` presented_at_boot | **PASS** (Hypothese-Identität) |
| P0-D Vendor EP | `vendor_mounted` / P0-D Event | **PASS** |
| P0-E Bulk RX | `rx_bytes=0`, `bulk_rx_events=0` | **ABSENT** |
| Capture | `drops=0`, `capture_valid=true`, `hyp=false` | Lauf **passiv** und gültig |
| Application peer | `application_peer_proven=false` | unbewiesen |

Zulässige Aussage: *„Im USBVendor.onRequest-Callback wurden keine 51/52/53 beobachtet.“*  
Nicht zulässig: *„Die HU hat kein 51 gesendet“* — bei Already-Accessory ohnehin schwach; Instrument sieht den Callback jetzt aber (Lab bewiesen).

---

## Events (Kern)

`20261030-161013`: P0-0 → P0-A → P0-C → P0-D → mehrfach `ca_transport` CONNECTED/DISCONNECTED (Steckzyklen). Kein Control-Sample.

`20261030-161212` (Re-Arm): erneut P0-0/A/C/D + CONNECTED.

---

## Deutung

1. **Instrument:** Erster Feldlauf, in dem „51/52/53=0“ im **Vendor-Callback** nicht mehr Stage-Blindheit sein kann (Lab A: 51→`0100`).  
2. **HU:** Zubehör erkannt/wählbar; Direct-Accessory ohne Callback-51/52/53; Device-RX Bulk **absent** trotz UI-Wahl.  
3. **Caveat:** Callback/RX-Absent ≠ Bus-Negativbeweis — [epistemik-messluecke-ep0-bulk](AOA-EPISTEMIK-EP0.md).  
4. **Kein BCL** auf der beobachteten Schicht.

---

## Follow-on

- Android/AAIdrive Accessory-Filter vs. Probe-Strings (M0).  
- Optional kurzer `aoa_phone_like`-Vergleich an der HU (Fingerprint früher negativ).  
- Kein weiteres Blind-Knock ohne Consumer-Nachweis.


---

## Teil B — Abend

**Datum:** 2026-10-30 ~18:07–18:16  
**Probe:** Car `192.168.178.89` · FW **0.3.34**  
**Bezug:** Erster Lauf nachmittag [feld-c1b-p0-fw034-erster-lauf](2026-10-30-feld-c1b-p0-fw034-erster-lauf.md)  
**Artefakte:** [`artifacts/20261030-feld-c1b-p0-0334-abend/`](artifacts/20261030-feld-c1b-p0-0334-abend/)

---

## Ablauf

| Zeit | Aktion | Ergebnis |
|---|---|---|
| 18:07 | Arm `aoa_observe` (passiv), soft-attach | Session `20261030-180741_…` |
| 18:08–18:11 | Mehrfach HU stecken; „USB-Modus aktivieren“ → „Gerät entfernen und erneut verbinden“ | Loop; attach **2→8**; **51/52/53=0**; **Bulk=0** |
| 18:12 | NVS → `aoa_phone_like` + Reboot | HU: **„USB-Gerät wird nicht unterstützt“**, LED gelb |
| 18:14 | Reset → `aoa_observe`, erneut arm | Session `20261030-181448_…` |
| 18:15–18:16 | Wieder stecken | **Gleiches** Menü/Reconnect-Loop; attach↑; Control/Bulk weiter 0 |

---

## Messwerte (passiv, Instrument belastbar)

### `aoa_observe` (beide Abend-Sessions)

| Ebene | Befund |
|---|---|
| P0-A/C/D | **PASS** (Attach/Mount, `18D1:2D00`, Vendor) |
| P0-B 51/52/53 | **ABSENT** (Callback 0; Direct-Accessory) |
| P0-E Bulk | **ABSENT** (`rx=0`) |
| Capture | `drops=0`, `hypothesis_respond=false` |
| HU-UI | Zubehör wählbar → USB-Modus → Reconnect-Aufforderung → **kein Fortschritt** |

### `aoa_phone_like` (kurz)

| Ebene | Befund |
|---|---|
| Identity | `04E8:6860` Phone-like |
| HU | Fingerprint **REJECTED** („nicht unterstützt“) — reproduziert B1 |
| Nutzen | Negativkontrolle Identity-Filter; kein AOA-Pfad |

---

## Deutung

1. Nachmittags- und Abendbefund **identisch** auf der **Messschicht**: Accessory-Erkennung; Callback ohne 51/52/53; Device-RX ohne Bulk; HU-UI-Loop.  
2. Weiteres Replug ändert diese Messung nicht.  
3. `phone_like` an dieser HU: Fingerprint **REJECTED**.  
4. **Nicht bewiesen:** „HU sendet keinen USB-Verkehr.“ Tap/RX ≠ Bus — siehe [epistemik-messluecke-ep0-bulk](AOA-EPISTEMIK-EP0.md).  
5. Nächste Messpriorität: **read-only EP0-Core-Hook** (+ Endpoint-Sicht); parallel Consumer-Filter. Kein Blind-Knock.

---

## Sessions

- `20261030-180741_lab_ca_c1_link` — Observe-Loop (attach bis 8)  
- `20261030-181448_lab_ca_c1_link` — Observe nach Reset (Reproduktion)
