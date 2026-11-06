# Feld-Runsheet — BMW I-303A (C1 Identity Gate)

| | |
|--|--|
| **FW** | **0.3.36** (clean dist) |
| **Mode** | `aoa_id_303a` — VID/PID `303A:1001`, Vendor-only `FF/FF/00`, **kein MSC** |
| **respond** | `hypothesis_respond=false` (kein AOA-51-Auto-ACK) |
| **Nicht** | BCL · Etch · EP0-Hook · Blind-Knock · weitere MSC-Novel-Muster |

Lab-Voraussetzungen geschlossen: sticky A/B/C/D PASS · AOA Lab A/B PASS · Host-CI  
([Kampagne](2026-11-06-c1-identity-gate-kampagne.md) · [Lab sticky/AOA](2026-11-06-lab-c1-036-sticky-aoa.md)).

---

## Vor dem Stecken

1. Mode speichern → Reboot → `/api/status`: `usb_gadget=aoa_id_303a`.
2. Soft-cycle einmal: Mode bleibt `aoa_id_303a`.
3. Heartbeat starten (kein `.88`-Fallback im Auto):

```bash
python3 tools/feld_status_heartbeat.py --dut <PROBE_IP> \
  --interval 5 --out docs/feld/artifacts/i303a-$(date -u +%Y%m%dT%H%M%SZ)-hb.jsonl
```

4. Session starten (`ca_c1_link` / Connected-App-Gate wie Kampagne).
5. Wandnotiz bereit: Uhrzeit, HU-UI-Text, Reconnect ja/nein — **ohne Interpretation**.

---

## Während der Messung (eine Variable)

Beobachten / notieren — nur FACT:

| # | Frage | Eintrag |
|---|--------|---------|
| 1 | Erkennt BMW das Device? | ja/nein + UI-Text wörtlich |
| 2 | Reconnect-Loop? | ja/nein · Intervall grob |
| 3 | `aoa_51` / `52` / `53` | Zahlen oder unverändert |
| 4 | `aoa51_respond_ok` | (soll bei respond=false unverändert bleiben) |
| 5 | `vendor_setup_count` | Zahl / null |
| 6 | `bulk_rx` / timeline | Zahlen |
| 7 | vs. MSC `303A:1001` | anders / gleich / UNKNOWN |

`0` = gemessen absent · `null` = auf Messschicht nicht beobachtbar ·  
`visibility_scope` muss `usbvendor_onrequest_only` (o.ä.) bleiben.

---

## Ops-Fallen

- **Web-UI tot ≠ Messung tot.** Heartbeat-JSONL + Collector/Events sind die Quelle.
- Unter Lab-OTG hing LocalWeb; im Fahrzeug nur WiFi der Probe — Heartbeat Pflicht.
- Nach `media_hide`/Offline-Lab: DUT oft soft-detached. I-303A braucht **AOA-Gadget attached** (Mode wechseln + soft-attach/cycle laut UI).
- Lab-Offline `Integrity: FAIL` mit `replay_records=0` ist bekannt (OTG-Coex) und **blockiert I-303A nicht**.

---

## Danach

1. Heartbeat stoppen; Status-Endsnap + Events JSONL sichern.
2. Kurzbericht: FACT / UNKNOWN / INFERENCE — keine BCL-Claims.
3. Nur wenn Callback weiter 0 **und** Identity-Gate die Messlücke als Blocker setzt → EP0-Hook priorisieren.
4. Kein weiterer Code vor diesem Feldlauf.

Vergleichspole (nicht erneut als Hauptlauf):  
`18D1:2D00` Accessory · `04E8:6860` Reject · `303A:1001`+MSC P-MOUNT.
