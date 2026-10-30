# Feld Abend: C1b-P0 FW 0.3.34 — Reconnect-Loop + phone_like Reject

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

1. Nachmittags- und Abendbefund **identisch**: Accessory-Erkennung ohne Consumer/Bulk → UI-Loop.  
2. Weiteres Replug ändert die Messung nicht.  
3. `phone_like` an dieser HU ist tot für Connected-USB.  
4. Nächster Hebel bleibt **Accessory-Consumer / App-Filter** (oder anderer Transport), nicht Control-Knock.

---

## Sessions

- `20261030-180741_lab_ca_c1_link` — Observe-Loop (attach bis 8)  
- `20261030-181448_lab_ca_c1_link` — Observe nach Reset (Reproduktion)
