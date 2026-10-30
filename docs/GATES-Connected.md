# Gates — Connected App (Control Plane)

**Scope:** Instrumentierungs-Pfad BCL/Etch/SAS/RHMI/CDS/AV.  
**Nicht:** MSC Measurement Integrity → [GATES.md](GATES.md) (HU-PROBE-GATE-01).  
**Konzept:** [CONNECTED-APP.md](CONNECTED-APP.md).

Evidence-Baseline: technology-audit `bimmergestalt-idrive-2026-foreign` (alles **R0** bis eigene Captures).  
Reihenfolge ist strikt: C(n+1) erst nach messbarem C(n) oder dokumentiertem Hard-Stop.

---

## HU-PROBE-GATE-C1a — Host BCL Parser (Lab)

**Status:** PASS (2026-10-28) — host-only, synthetic TEST evidence

**Messziel:** 8-Byte-BCL-Framing deterministisch aus synthetischen Streams parsen (Fragmentierung, Fehlerzustände). Kein Fahrzeug, kein Transport.

### Kriterien

- [x] `BclFramer` Streaming: chunked == full buffer
- [x] Unit-Tests T1–T10 + Determinismus + Fragmentierungsinvarianz (`tools/bcl_c1a`)
- [x] `kMaxPayload = 4000` aus Desk (`protocol-bcl.md` / TcpMuxer) — oversized → Error, kein Phantom-Frame
- [x] Kein DEADBEEF-/Magic-Resync (unsupported → sticky error bis `reset()`)
- [x] Szenario `ca_c1_lab_bcl` → JSONL `session` / `ca_bcl` (Host-Adapter)
- [x] Testdaten: `source=synthetic`, `evidence_level=TEST`, `direct_observation=false`
- [x] Device EventBus / WebUI — siehe **C1a.5** (nicht Teil von C1a selbst)

### Epistemik nach C1a

| Klasse | Aussage |
|--------|---------|
| **FACT** | Parser dekodiert die definierte 8-Byte-BCL-Struktur aus synthetischen Streams |
| **TEST EVIDENCE** | `make -C tools/bcl_c1a test` / `scenario` reproduzierbar PASS |
| **UNKNOWN** | realer NBT-Evo Control-Plane-Capture; Transportweg AOA/BT; SAS-Erfolg |
| **NOT IMPLEMENTED** | C1b-P0 Wire, C1b-P1+, C1c, C2–C5 |

**Erlaubte Claim-Form:** *„Der BCL-Parser wurde anhand synthetischer, auditkonformer Testvektoren deterministisch validiert.“*

---

## HU-PROBE-GATE-C1a.5 — Device Integration (EventBus / WebUI)

**Status:** PASS (2026-10-28) — on-device smoke `192.168.178.88`, FW 0.3.21

**Messziel:** Denselben `BclFramer` auf dem ESP32 nutzen; synthetische `ca_c1_lab_bcl`-Bytes → `ca_bcl` über EventBus; Status in WebUI. Kein Transport.

### Kriterien

- [x] `ConnectedAppProbe` + shared `CaC1LabBclStream.h`
- [x] `session.start(..., transport=lab_synthetic)` → chunked feed → `ca_bcl` → `session.stop`
- [x] `GET /api/status` → `connected_app{...}`
- [x] `POST /api/connected/lab_bcl` startet Lab-Lauf (409 wenn Session busy)
- [x] Host-Tests unverändert grün (`make -C tools/bcl_c1a test`)
- [x] Keine Änderungen unter `firmware/src/msc/` / `media/`
- [x] On-device Flash-Smoke: OTA `.88` (überschrieb esp-hub/ergo) → API `ok:true` →
      `state=done` frames=3 malformed=1 unknown=1 → Collector
      `sessions/20261028-235431_lab_ca_c1_lab_bcl` mit 4× `ca_bcl`
      (`source=synthetic`, `evidence_level=TEST`, `direct_observation=false`);
      Session start/stop; MSC `ready`; Heap stabil; WebUI/status responsive

**Ops-Hinweis:** Live-Auto-Session muss vorher gestoppt werden (`POST /api/session/stop`), sonst `409 session_busy`.

**Nicht Teil von C1a.5:** AOA, SPP, SAS, Etch, RHMI, Wire-Claims.

---

## HU-PROBE-GATE-C1 — Connected-App-Verbindung

**Status:** open (C1a + C1a.5 PASS; C1b-Stufen ausstehend)

**Messziel:** Transport steht → BCL erkennbar → Etch-Bytes (falls vorhanden) → SAS-Challenge **oder** klarer Auth-Block.

C1b wird **gestuft** gewertet — ein erfolgreicher USB-AOA-Link ist *nicht* „BCL funktioniert“:

| Stufe | Name | Nachweis |
|-------|------|----------|
| **C1b-P0** | Transport erreichbar | AOA enumeration/control/bulk observed (siehe P0-0…E) |
| **C1b-P1** | Raw → BCL | Bulk bytes through `BclFramer` |
| **C1b-P2** | BCL parse quality | Stable frames from transport bytes |
| **C1b-P3** | Protocol semantics | Desk-Annotationen erlaubt (z. B. dest `0x0FA4` → Etch label) — erst hier |

### HU-PROBE-GATE-C1b-0 — AOA Transport Skeleton

**Status:** PASS skeleton (2026-10-29 Smoke @ `.88`) — `usb_stack_bound=false`

| Artefakt | Pfad |
|----------|------|
| `ITransport` | `firmware/src/connected/transport/ITransport.h` |
| `AoaTransport` | `…/AoaTransport.{h,cpp}` — byte ring, lab inject; observe bind ab P0 |
| `SppTransport` | stub only |
| Scenario | `ca_c1_link` (skeleton path deprecated for live; P0 needs `aoa_observe`) |
| Smoke | `sessions/20261029-000050_lab_ca_c1_link` — `ca_transport` + inject→`ca_bcl` |

**Rolle (Desk):** Connected-App-Pfad = Device-in-Accessory-Mode; HU = AOA Host.  
**PASS C1b-0:** Skeleton armierbar, Events `ca_transport`, Inject → `BclFramer` → `ca_bcl` (TEST).  
**Abgelöst für Live:** C1b-P0 Boot-Mode Isolation.

### HU-PROBE-GATE-C1b-P0 — USB/AOA Observability (passive)

**Status:** PARTIAL — P0-0/A/C/D **PASS** am NBT (Feld 2026-10-29); P0-E **FAIL/absent**.  
B1 phone_like **NEGATIVE** (Fingerprint rejected). B2b hex4 **TX ok / keine HU-Antwort**.  
Blind-Knock beendet → FOLLOW-ON #9 Referenz-Capture ([Maßnahmen](feld/2026-10-29-c1b-massnahmen-nach-b1-b2b.md)).  
Note: `51/52/53=0` bei Already-Accessory `18D1:2D00` ≠ AOA-FAIL (AOSP: Transition-Requests).  
**Lab 2026-10-30 (Debian=AOA-Host ↔ OnePlus/AAIdrive):** Handshake+Re-Enum `2D00` PASS; AAIdrive UI „verbunden“ mit BMW-Strings; **USB-Bulk ABSENT** (OUT Timeout). Bericht: [feld/2026-10-30-lab-aoa-aaidrive-phone-host.md](feld/2026-10-30-lab-aoa-aaidrive-phone-host.md). Konsolidierter Multi-Review (Mistral, Grok, GPT, Claude): [feld/2026-10-30-gesamtbericht-aoa-zubehoer-multireview.md](feld/2026-10-30-gesamtbericht-aoa-zubehoer-multireview.md).

**FW 0.3.34 (C1b-P0.5 Instrument):** Stage-Gate TinyUSB `CONTROL_STAGE_*`; DATA/ACK nach Respond → `true` (51-STALL behoben). Lab: 51→`0100`, 52/53 ok; Lab B passiv ([feld/2026-10-30-lab-c1b-p05-stage-fix.md](feld/2026-10-30-lab-c1b-p05-stage-fix.md)).

**Boot-Mode Isolation (P0-0) — Voraussetzung:**

| `usb_gadget` (NVS) | USB-Fingerprint |
|--------------------|-----------------|
| `msc` (default) | Golden Path USBMSC only — **kein** Vendor/AOA |
| `aoa_observe` | USBVendor only Already-Accessory `0x18D1:0x2D00` — **kein** MSC; observe-only |
| `aoa_phone_like` | USBVendor only B1 phone-like `0x04E8:0x6860` — **kein** MSC; 51/52/53 Responder |

Umschalten **nur** NVS + Reboot (`POST /api/config` + `/api/connected/gadget/reboot`). Soft-Detach ≠ Moduswechsel.

| Level | Befund |
|-------|--------|
| **P0-0** | Gadget isolation: `msc_interface_present` / `vendor_interface_present` match boot mode |
| **P0-A** | USB attach/enum observed (`ca_usb` attach) |
| **P0-B** | AOA-relevant control seen (`bRequest` ∈ {51,52,53,58}). Generic callback SETUPs count in `setup_count` only — do **not** set P0_B. Direct-2D00-Boot: Absent ≠ AOA-FAIL |
| **P0-C** | VID/PID path documented (`aoa_observe` → `0x18D1:0x2D00`; `aoa_phone_like` → `0x04E8:0x6860`; post-53 re-enum **NOT IMPLEMENTED**) |
| **P0-D** | Bulk endpoints identified (USBVendor duplex) |
| **P0-E** | First bulk RX capturable as raw `ca_usb` / counters — **not** fed to `BclFramer` |

**PASS C1b-P0:** P0-0 + mindestens P0-A reproduzierbar; Ideal: P0-E am NBT.  
**Nicht PASS / nicht claimed:** BCL frames, SAS, „Connected App works“, AOA connected = BCL connected.

**Artefakte:** `AoaObserveGadget`, `AoaControlTap`, `AoaTransport` (`impl=observe`), Events `ca_usb` / `ca_transport` gate=`C1b-P0`.

### Kriterien (Gesamt-C1)

- [~] Transport wählbar: **USB-AOA observe** (C1b-P0 Boot-Mode); BT-SPP ausstehend
- [x] BCL 8-Byte-Header geparst: `cmd`, `src`, `dest`, `len` → Event `ca_bcl` (**Lab/Host+Device**; Wire offen)
- [ ] Dest `0x0FA4` (Etch) und/oder Handshake/SelectProto/Knock sichtbar **oder** explizit absent geloggt *(Wire / ab C1b-P3)*
- [x] Kein DEADBEEF als Sync-/Wire-Annahme (C1a)
- [ ] SAS: Challenge empfangen **oder** `ca_sas.phase=blocked` …
- [x] Session + EventBus auf Device für Lab-Pfad (C1a.5); Wire-Session offen
- [x] WebUI: `connected_app` (Lab); Transport-Felder ab C1b-0

### PASS / FAIL

| Ergebnis | Bedeutung |
|----------|-----------|
| PASS-link | BCL-Frames + (Challenge **oder** dokumentierter Block) |
| FAIL | kein Transport / kein Framing / stilles Schweigen ohne Status |
| BLOCKED-auth | erwarteter Hard-Stop ohne Cert — **kein** C2 |

**Nicht Teil von C1:** Login erzwingen, RHMI, Audio.

---

## HU-PROBE-GATE-C2 — RHMI Minimal-App

**Status:** blocked (depends C1 + SAS login)

**Messziel:** „Hello PiDrive“ + Button; Controller-/Action-Event kommt in der Probe an.

### Kriterien

- [ ] Nach erfolgreichem `sas_login` (legitimiert)
- [ ] `rhmi_create` → Ressourcen (DESCRIPTION/TEXTDB/IMAGEDB) → `rhmi_initialize`
- [ ] Action- + HMI-Event-Handler registriert
- [ ] UI zeigt festen Text + einen Button
- [ ] Button/Controller → Event `ca_rhmi` in JSONL
- [ ] Optional: `hmi.type` / `hmi.version` loggen (ID4/5/6 — R0 bis gemessen)

**PASS:** mindestens ein Action-/HMI-Event mit `t_us` in Session.  
**Hinweis:** `rhmi_create_version` in IDL ungenutzt (EVID-025) — nicht verwenden.

---

## HU-PROBE-GATE-C3 — CDS

**Status:** blocked (depends C2 oder C1+login)

**Messziel:** eine harmlose Property abonnieren und loggen.

### Kriterien

- [ ] Property z. B. `driving.speedActual` (Pin: CDS id 42 — R0 bis Wire)
- [ ] Event `ca_cds` mit gekürztem Value
- [ ] Keine Annahme, dass alle IDL-Properties auf NBT Evo existieren

**PASS:** ≥1 Property-Update oder klarer Reject vom HU geloggt.

---

## HU-PROBE-GATE-C4 — AV Connection

**Status:** blocked (depends C1+login)

**Messziel:** `av_create` + `av_requestConnection(ENTERTAINMENT)` — Wire + NBT-Verhalten beobachten.

### Kriterien

- [ ] Events `ca_av` für create / request / granted|denied|playerState
- [ ] Parallel notieren: ändert sich HU-Audioquelle? (Operator-Marker)
- [ ] **Kein** Claim „Audio läuft über Etch“ aus C4 allein

**PASS:** Request + Antwort (oder Timeout) messbar dokumentiert.

---

## HU-PROBE-GATE-C5 — Audio-Datenpfad

**Status:** blocked (depends C4)

**Messziel:** Existiert ein realer Audio-Transport (PCM / compressed / nur Control + A2DP)?

### Kriterien

- [ ] Nach granted Entertainment: Payload auf Etch/BCL? AOA Audio? nur BT A2DP?
- [ ] Jede Hypothese mit Capture oder explizitem `UNKNOWN`
- [ ] PiDrive-Annahme „Audio weiter A2DP“ bleibt HYPOTHESIS bis belegt

**PASS:** belastbare Klassifikation des Datenpfads (auch negativ: „nur Control“).

---

## SAS — Hard Blocker (übergreifend)

| Fehlt | Folge |
|-------|--------|
| App-Zertifikat (P7B / APP_AUTH+APP) | C1 max. bis Challenge/Block; C2–C5 zu |
| Security-Service / Signing | Login unmöglich; `ca_sas=blocked` |
| Legitime Lab-Freigabe | kein Prod-Login-Versuch |

Probe dokumentiert den Blocker — implementiert **keinen** Bypass.

---

## Bezug MSC

| Gate | Beziehung |
|------|-----------|
| HU-PROBE-GATE-01 | weiterhin Voraussetzung für audit-fähige Timestamps/Drops — gilt auch für `ca_*` |
| HU-A* in `bmw_nbt_evo.json` | MSC-Produktgates; Control-Plane später eigener `open_gates`-Block |
| CROSSREF | BG-Claims nicht in MSC-FACTS mergen |
