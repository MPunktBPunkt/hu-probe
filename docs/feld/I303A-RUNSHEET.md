# Feld-Runsheet — BMW I-303A (G1-Abschluss)

| | |
|--|--|
| **Priorität** | **P0** — billiger Identity-Abschluss, **nicht** der Connected-Hauptversuch |
| **FW** | **0.3.36** (clean dist) |
| **Mode** | `aoa_id_303a` — VID/PID `303A:1001`, Vendor-only `FF/FF/00`, **kein MSC** |
| **respond** | `hypothesis_respond=false` |
| **Misst** | nur **G1** (Identity/Class); Callback höchstens als Nebenbeobachtung |
| **Beantwortet** | „Wie reagiert die HU auf diese USB-Identity?“ |
| **Beantwortet nicht** | Connected-Session · BCL/Etch/SAS · „wie sieht echter Phone-USB-Pfad aus?“ |

Danach ohne Identity-Matrix-Aufblasen → **[FIELD-CONNECTED-REAL-01](FIELD-CONNECTED-REAL-01.md)** (P1).

Lab-Voraussetzungen: sticky A/B/C/D PASS · AOA Lab A/B PASS · Host-CI  
([Kampagne](C1-IDENTITY-KAMPAGNE.md) · [Lab](lab/C1-036-STICKY-AOA.md)).

---

## Vor dem Stecken

1. Mode speichern → Reboot → `/api/status`: `usb_gadget=aoa_id_303a`.
2. Soft-cycle: Mode bleibt `aoa_id_303a`.
3. Heartbeat:

```bash
python3 tools/feld_status_heartbeat.py --dut <PROBE_IP> \
  --interval 5 --out docs/feld/artifacts/i303a-$(date -u +%Y%m%dT%H%M%SZ)-hb.jsonl
```

4. Session (`ca_c1_link` o.ä.).
5. Wandnotiz: Uhrzeit, HU-UI wörtlich, Reconnect ja/nein — **ohne Interpretation**.

---

## Während der Messung (eine Variable)

| # | Frage | Eintrag |
|---|--------|---------|
| 1 | Erkennt BMW das Device? | ja/nein + UI-Text |
| 2 | Reconnect-Loop? | ja/nein · Intervall grob |
| 3 | `aoa_51` / `52` / `53` | Zahlen oder unverändert |
| 4 | `aoa51_respond_ok` | bei respond=false unverändert erwartet |
| 5 | `vendor_setup_count` | Zahl / null |
| 6 | `bulk_rx` / timeline | Zahlen |
| 7 | vs. MSC `303A:1001` | anders / gleich / UNKNOWN |

`0` = gemessen absent · `null` = nicht beobachtbar · `visibility_scope` bleibt Callback-Schicht.

---

## Ops-Fallen

- Web-UI tot ≠ Messung tot — Heartbeat/Events sind Quelle.
- Soft-detached nach Offline-Lab → Mode + soft-attach/cycle.
- Lab-Offline Integrity FAIL (`replay_records=0`) blockiert I-303A nicht.

---

## Danach (G1 schließen)

1. Heartbeat stoppen; Endsnap + Events sichern.
2. Kurzbericht: FACT Identity+Config→HU-Reaktion — **keine** BCL/SAS-Claims.
3. **Keine** weitere Identity-Serie (I-ACC/I-PHN nur bei klarem Bedarf).
4. EP0-Hook nur wenn I-303A die Messlücke **als Blocker für G1** setzt — nicht „für Connected“.
5. Sofort Priorität **P1:** [FIELD-CONNECTED-REAL-01](FIELD-CONNECTED-REAL-01.md).

Vergleichspole (nicht erneut als Hauptlauf):  
`18D1:2D00` Accessory · `04E8:6860` Reject · `303A:1001`+MSC P-MOUNT.
