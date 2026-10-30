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
3. **Caveat:** Callback/RX-Absent ≠ Bus-Negativbeweis — [epistemik-messluecke-ep0-bulk](2026-10-30-epistemik-messluecke-ep0-bulk.md).  
4. **Kein BCL** auf der beobachteten Schicht.

---

## Follow-on

- Android/AAIdrive Accessory-Filter vs. Probe-Strings (M0).  
- Optional kurzer `aoa_phone_like`-Vergleich an der HU (Fingerprint früher negativ).  
- Kein weiteres Blind-Knock ohne Consumer-Nachweis.
