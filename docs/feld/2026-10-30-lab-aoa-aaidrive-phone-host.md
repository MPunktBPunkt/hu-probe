# Lab — Debian als AOA-Host gegen Phone/AAIdrive — 2026-10-30

**Auswertung** · Gate-Nähe C1b / FOLLOW-ON #9 (Lab-HU-Emulation, **nicht** NBT-Wire)  
**Artefakte:** [`artifacts/20261030-lab-aoa-aaidrive/`](artifacts/20261030-lab-aoa-aaidrive/)  
**Multi-Review (Mistral, Grok, GPT, Claude):** [`2026-10-30-gesamtbericht-aoa-zubehoer-multireview.md`](2026-10-30-gesamtbericht-aoa-zubehoer-multireview.md)  
**FW Lab-Probe (Kontrast, danach wieder gesteckt):** **0.3.32** `aoa_phone_like` @ `192.168.178.88`  
**Tools:** `tools/lab_aoa_iface.py`, `tools/lab_usbmon_capture.sh`, Host-Skripte unter Artefakte/`tools/`

| | |
|--|--|
| **Lab-Host** | Proxmox `192.168.178.108` (usbfs + usbmon) · Capture/JSONL von PVE; LXC `.187` für Docs/Collector |
| **Phone** | OnePlus 12 · VID/PID **`22d9:2764`** (MTP/PTP class `0x06`) · Serial `67c52c27` |
| **App** | AAIdrive (+ MyBMW Zubehör-UI) |
| **Capture** | `usbmon2` / tshark · Bus 2 (Genesys-Hub) |
| **Vorläufer** | [Maßnahmen nach B1/B2b](2026-10-29-c1b-massnahmen-nach-b1-b2b.md) · Desk [`bmw-bcl-handshake.md`](../../../technology-audit/projects/bimmergestalt-idrive-refs/applications/bmw-bcl-handshake.md) |

---

## Executive Summary

Debian-PC als **AOA-Host (HU-Rolle)** gegen echtes Phone: **51→52→53 mit `manufacturer=BMW` / `model=NBT Evo` reproduzierbar PASS**; Re-Enum in ~0,6 s auf **`18d1:2d00`**. AAIdrive wechselt UI von „warte auf Verbindung mit Fahrzeug“ → **„verbunden“** (Operator MEASURED).  

**USB-Bulk bleibt tot:** Host→Phone Bulk-OUT = **Timeout** (bzw. ENODEV auf stale Link); Phone→Host Bulk-IN = **0 Bytes** über alle Läufe. BCL-Muster (HU-HANDSHAKE, hex4, Knock, OPEN/Etch, …) erzeugen **keine Antwort**.  

**Epistemik:** AOA-Identität + BMW-Substring steuern die AAIdrive-„Connected“-Optik; **das ist nicht BCL**. Fahrzeugdaten/CDS über USB sind damit **noch nicht** möglich. FOLLOW-ON #9 am **echten NBT** bleibt offen; Lab ersetzt die Phone-Hälfte der Frage, nicht die HU-Hälfte.

---

## Setup / Infrastruktur (Lab)

| Thema | Befund |
|-------|--------|
| LXC `/dev/bus/usb` leer | sysfs sichtbar, `ctrl_transfer` → ENODEV — behoben via `pct set` usbfs-Node + Tests **auf PVE** mit `python3-usb` |
| PVE DNS | `resolv.conf` zeigte tot auf `192.168.4.1` — gefixt auf `192.168.178.1` / `8.8.8.8`; danach `python3-usb` installierbar |
| usbmon | `modprobe usbmon`, `/dev/usbmon2` in CT100; tshark Capture PASS |
| Probe-Tap 51 | Gegen `aoa_phone_like` (FW **0.3.32**): Standard-GET_DESCRIPTOR ok, Vendor **51 → Pipe/STALL**, `setup_count=0` — Instrument-Bug (**Stage-Enum**: TinyUSB `CONTROL_STAGE_SETUP=1` vs Arduino `REQUEST_STAGE_SETUP=0`). **FW 0.3.33** korrigiert den Stage-Vergleich; Lab-A-Retest ausstehend |
| TinyUSB SDK | Header **0.15.0**; Upstream `usbd.c` routet Vendor **vor** Recipient-Switch |

---

## Chronologie der Läufe

| Run-ID | Ziel | Kernbefund |
|--------|------|------------|
| Positivkontrolle (PVE) | 51 an OnePlus | **PASS** `02 00` = AOA **2.0** — Host-Script entlastet |
| Erster voller Handshake | 51/52(BMW…)/53 + URI `http://www.bmw.com` | Re-Enum `18d1:2d00`; Browser/Landing **bmw.de** (AOA-URI); Bulk 0 |
| `run-080224` | already_accessory + Knock | Bulk OUT Timeout; nach Replug Handshake PASS, OUT weiter Timeout |
| `run-080649` / `080911` | Handshake + AAIdrive-Fenster | AOA PASS; Bulk 0; Timing-Hinweis Operator |
| `run-081139` | Timed: Signal „JETZT AAIdrive“ | AOA PASS; Bulk 0 |
| `run-081427-blink` | usb-state + Blink-Korrelation | MTP = blinkt / „warte…“; nach Handshake UI **verbunden** (Operator) |
| Handshake im Blink-Run | gezieltes Verbinden | UI: warte → verbunden; Bulk weiterhin 0 |
| `run-082047-patterns` | Pattern auf **stale** Accessory | sofort EIO/ENODEV — Link tot |
| `run-082126-hs-patterns` | Frischer Handshake + HU-Muster-Suite | AOA PASS; **13× OUT Timeout, 0 RX**; UI-Meldung flackerte |

Rohdaten: `artifacts/20261030-lab-aoa-aaidrive/runs/`.

---

## MEASURED Ergebnisse

### A — AOA Control / Enum

| Claim | Klasse | Evidence |
|-------|--------|----------|
| Phone antwortet GET_PROTOCOL 51 mit AOA 2 | **MEASURED** | PVE pyusb; JSONL `aoa51` hex `0200` |
| 52 Strings + 53 START → Re-Enum `18d1:2d00` in ~0,5–0,6 s | **MEASURED** | alle erfolgreichen Handshake-JSONL + lsusb |
| URI-String öffnet Landing (bmw.com→bmw.de) | **MEASURED** | Operator; URI später auf `""` gesetzt |
| usbmon sieht Enum/Accessory | **MEASURED** | pcaсs in Artefakten |

### B — AAIdrive / MyBMW UI

| Claim | Klasse | Evidence |
|-------|--------|----------|
| USB ab → Auto grau | **MEASURED** (Operator) | Blink-Run |
| USB an (MTP) → Auto blinkt / „warte auf Fahrzeug“ | **MEASURED** | Operator |
| Nach AOA+BMW-Strings → UI „verbunden“ | **MEASURED** | Operator, mehrfach reproduziert |
| MyBMW Zubehörseite / Registrierung | **MEASURED** | Operator nach erstem BMW-String-Lauf |
| „warte…“-Toast kann erscheinen und verschwinden ohne Bulk | **MEASURED** | während Pattern-Suite |

### C — USB Bulk / BCL

| Claim | Klasse | Evidence |
|-------|--------|----------|
| Bulk-IN vom Phone nach Accessory | **ABSENT** | alle `listen_end` total=0 |
| Bulk-OUT Host→Phone (hex4, HU-HANDSHAKE, Knock, OPEN, …) | **FAIL** (Timeout) bzw. ENODEV wenn Link stale | `bulk_out_err` in JSONL |
| BCL-Session / Etch / CDS über USB | **NOT OBSERVED** | — |

Desk-Rolle (BclCarLink): Phone sendet hex4 und wartet auf **HU-HANDSHAKE**. Lab hat beides versucht — ohne dass der Phone-Bulk-Endpunkt Host-Daten annimmt (OUT Timeout ⇒ Device ACK’t OUT nicht / kein Reader am Accessory-FD).

---

## Muster-Suite (HU-Rolle, nach frischem Handshake)

Gesendet u. a. (Desk [`bmw-bcl-handshake`](../../../technology-audit/projects/bimmergestalt-idrive-refs/applications/bmw-bcl-handshake.md)):

- HU HANDSHAKE v4/v3 (Unit-Test-Layout `00 04 …`)
- Session-Init hex4 `12 34 56 78` (+ Repeat)
- SELECTPROTO(3), KNOCK 20 B, OPEN Etch `0x0FA4`
- `DEADBEEF`, zeros, ascii

**Ergebnis:** keine einzige erfolgreiche Bulk-Übertragung in diesem Lauf (`any_tx=false`, `any_rx=false`).

---

## Instrument / Probe (Nebenbefund)

Gegen Lab-Probe `aoa_phone_like` (`04e8:6860`): Host-51 → **STALL**, Probe-Zähler `aoa51` bleiben 0.  

Ursache **hochwahrscheinlich:** Stage-Enum-Mismatch in `AoaControlTap` (TinyUSB SETUP=1 vs Arduino REQUEST_STAGE_SETUP=0). Feld-„51/52/53=0“ an Already-Accessory/phone_like ist damit **teilweise Instrument-blind**. Fix = FW; nicht Teil dieses Phone-Labs.

---

## Epistemik / Einordnung zu FOLLOW-ON #9

| Frage aus Maßnahmen-M1 | Lab-Antwort |
|------------------------|-------------|
| Phone Composite / VID-PID | **ja** — `22d9:2764`, 1× Interface Imaging/PTP |
| HU sendet 51/52/53? | Lab-HU **ja**; echte NBT **weiter UNKNOWN** |
| Re-Enum 2Dxx? | **ja** unter Lab-Host |
| Erstes Bulk / BCL-Init? | **ABSENT** auf USB trotz UI-Connected |
| Connected ohne USB-Bulk? | **ja (UI)** — stärkt „AOA≠BCL“ und BT-Hypothese |

**Nicht erledigt:** Capture Phone↔**NBT Evo**; echte HU-52-Strings; BCL-Wire-FACT.

---

## Deliverables im Repo

| Artefakt | Pfad |
|----------|------|
| Dieser Bericht | `docs/feld/2026-10-30-lab-aoa-aaidrive-phone-host.md` |
| Multi-Review | `docs/feld/2026-10-30-gesamtbericht-aoa-zubehoer-multireview.md` |
| Rohdaten | `docs/feld/artifacts/20261030-lab-aoa-aaidrive/` |
| Lab-Host-CLI | `tools/lab_aoa_iface.py` · `tools/lab_usbmon_capture.sh` |
| Tap-Check Wrapper | `tools/lab_aoa_control_tap_check.py` → iface |

---

## Empfohlene nächste Schritte (für Auswertung / Follow-on)

1. **FW:** Stage-Enum-Fix + Lab-Tap-Check gegen Probe (aoa51↑).  
2. **AAIdrive:** klären, wann `UsbAccessory` FD geöffnet wird (Logcat) — OUT-Timeout = kein Consumer.  
3. **Transport:** BT-SPP (#11) parallel priorisieren, wenn USB-Bulk weiter absent.  
4. **#9 NBT:** kurzer Auto-Lauf nur noch für HU-52-Strings / ob NBT überhaupt USB-BCL nutzt — Lab hat Phone-Seite weitgehend ausgeleuchtet.

---

## PASS / FAIL Snapshot (Lab-HU vs Phone)

| Check | Status |
|-------|--------|
| Host-Script 51 Positivkontrolle | **PASS** |
| AOA Handshake + Re-Enum 2D00 | **PASS** |
| AAIdrive UI „verbunden“ nach BMW-Strings | **PASS** (UI) |
| USB Bulk TX/RX / BCL | **FAIL / ABSENT** |
| CDS / Fahrzeugdaten | **NOT POSSIBLE** (kein Tunnel) |
