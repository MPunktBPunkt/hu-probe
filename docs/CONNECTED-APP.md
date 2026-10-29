# Connected-App Probe Path (Control Plane)

**Status:** C1a + C1a.5 + C1b-0 PASS. **C1b-P0** Software-Gate (Boot-Mode `aoa_observe`) implementiert — NBT-Wire noch OPEN.  
**Desk-Basis:** technology-audit `usb-aoa-bmw-connected-2026-desk` (EVID-AOA-…-001…026; B1/B2 Messfragen).  
**Schichtenregel:** Storage Plane (USB-MSC) und Control Plane (Connected App) bleiben **getrennte** Instrumente. BimmerGestalt überschreibt keine MSC-FACTS ([technology-audit CROSSREF](../../technology-audit/audits/bimmergestalt-idrive-2026-foreign/CROSSREF.md)).

Gates: [GATES-Connected.md](GATES-Connected.md). MSC-Integrität bleibt [GATES.md](GATES.md) / HU-PROBE-GATE-01.

---

## Architektur: Storage vs Control

```
                    ┌─────────────────────────────────────────┐
  NBT Evo           │              hu-probe ESP32-S3          │
                    │                                         │
  USB-MSC ─────────►│  Storage Plane (bestehend)              │
  (Host reads)      │    msc/ · media/ · ReadInstrument       │
                    │    bot_* / read / usb events            │
                    │                                         │
  USB-AOA ──?──────►│  Control Plane (neu, optional)          │
  und/oder          │    connected/                           │
  BT-SPP ──?───────►│    BCL → Etch → SAS → BMWRemoting       │
                    │    ca_* events (eigenes Präfix)         │
                    │                                         │
                    │  Gemeinsam: Session · EventBus · TCP    │
                    │  JSONL · WebUI Status · Collector       │
                    └─────────────────────────────────────────┘
```

| Plane | Transport | Misst | Auth |
|-------|-----------|-------|------|
| **Storage** | USB MSC gadget | Host-Read-Timing, Prefetch, Remount, Busy | keine (MSC) |
| **Control** | USB AOA und/oder BT SPP → BCL | Tunnel, SAS-Gate, RHMI/CDS/AV Wire | SAS hart |

Beide Planes können dieselbe Session/`t_us`/`seq`-Infrastruktur nutzen. Sie teilen **keine** Event-Typen (`bot_*` ≠ `ca_*`). Soft-Attach: Control-Plane-Modus darf MSC soft-detach halten (wie `lab_debian`), damit die HU nicht gleichzeitig Storage- und Accessory-Rollen vermischt — bis Wire belegt, was die NBT Evo zulässt (**UNKNOWN**).

---

## Quellen (technology-audit + Pins)

Kampagne: `technology-audit/audits/bimmergestalt-idrive-2026-foreign/`  
Sheets: `technologies/bmw-bcl|bmw-etch-rpc|bmw-rhmi/SHEET.md`  
Pins: `/tmp/bimmergestalt/` (BclCarLink@7cfd959, AAIdrive@3e93419, Kit@95492b0, …)

| Claim | Evidence | Class | Level |
|-------|----------|-------|-------|
| Stack `Transport → BCL → Etch → SAS → RHMI\|AV\|CDS\|AM` | Audit README / PROTOCOL | FACT (Desk-Konvergenz) | **R0** |
| BCL 8-Byte BE Header cmd/src/dest/dataLen | EVID-BG-…-001 | FACT | R0 |
| dest `0x0FA4` = Etch; `0x1389` = Watchdog | EVID-BG-…-002/007 | FACT | R0 |
| Handshake / SelectProto / Knock | EVID-BG-…-003…005 | FACT | R0 |
| BT RFCOMM (SPP) in BclCarLink | EVID-BG-…-020 Code-Seite | FACT (impl) | R0 |
| USB Accessory / ENET als BCL-Transport | Analysis Doc vs Code | **Contradiction** / HYPOTHESIS | R0 |
| Etch Wire = Symbol-Hashes (`.ewh`) | PROTOCOL / protocol-etch.md | FACT (Doc) | R0 |
| **Etch-Magic `DEADBEEF`** | — | **UNKNOWN / not attested** | — |
| SAS `certificate → challenge → login` | EVID-BG-…-010 | FACT (API-Reihenfolge) | R0 |
| Challenge 16 B | EVID-BG-…-021 | INFERENCE | R0 |
| Ohne Security-Modul kein Prod-Login | etch SHEET | INFERENCE | R0 |
| RHMI create → resources → initialize → events | EVID-BG-…-013 | FACT | R0 |
| `av_create` + `av_requestConnection(ENTERTAINMENT)` | EVID-BG-…-015 | FACT | R0 |
| Audio-Payload über Etch/AV | — | **UNKNOWN** | — |
| PiDrive Pfad C: Audio weiter A2DP | PiDrive Strategie | HYPOTHESIS | — |
| `driving.speedActual` (CDS id 42) | Kit `CDSProperty.kt` / Analysis cds.md | FACT (Pin-Code/Doc); **nicht** in EVID-Matrix | R0 pin |
| AM `am_*` Registrierung Detailsequenz | IDL ja; kein EVID | UNKNOWN in KB | R0 |
| Framing/APIs auf **unserer** F20 NBT Evo | — | **UNKNOWN** | braucht eigenes Lab/Field |

**Regel:** Alles aus BimmerGestalt bleibt **R0**, bis Wire/Live auf dem eigenen F20/NBT Evo es hebt. Kein stiller Transfer MSC↔Connected-App.

---

## Modulplan (Firmware)

Neuer Pfad parallel zu `msc/` — **kein** Umbau von `MscBotTap` / BOT-Hooks.

```
firmware/src/connected/
  ConnectedAppProbe.h/.cpp     # Orchestrator: Mode, Gate-State, loop tick
  aoa/
    AoaObserveGadget.h/.cpp    # C1b-P0 USBVendor-only boot gadget
    AoaControlTap.h/.cpp       # Vendor control queue + AOA 51/52/53 hypothesis
  transport/
    AoaTransport.h/.cpp        # skeleton (C1b-0) / observe bind (C1b-P0)
    SppTransport.h             # BT SPP stub → später (C1c)
    ITransport.h               # read/write/connected
  bcl/
    BclFramer.h/.cpp           # 8-Byte Header parse/build, cmd enum
    BclSession.h/.cpp          # Handshake/SelectProto/Knock/MUX OPEN-DATA-CLOSE
  etch/
    EtchObserver.h/.cpp        # Byte-stream log; Symbol-Hash-Hinweis; KEIN DEADBEEF-Annahme
  sas/
    SasGate.h/.cpp             # State: idle→cert_sent→challenge→login|blocked
                               # Kein Signing ohne explizites Cert+Security-Backend
  rhmi/ cds/ av/               # Stub bis Gate C2+ (nach SAS)
```

### Integration (minimal)

| Stelle | Änderung |
|--------|----------|
| `App.h/.cpp` | Optional `ConnectedAppProbe` Member; `begin`/`loop` nur wenn `probe_plane=control` oder Scenario `ca_*` |
| `ConfigStore` | `probePlane`: `storage` \| `control` \| `dual` (default `storage`) |
| `ExperimentProfiles.h` + `profiles/experiments.json` | Szenarien `ca_c1_link`, `ca_c2_rhmi`, … |
| `EventBus` / SCHEMA | Neue `ev` mit Präfix `ca_` (siehe unten) |
| `WebUi` `/api/status` | Nest `connected_app: { transport, bcl, etch, sas, rhmi, cds, av }` |
| `profiles/bmw_nbt_evo.json` | Später Abschnitt `control_plane` — erst nach Messdaten |
| Collector | unverändert (JSONL agnostisch) |

MSC-Pfad bleibt Default und unverändert, wenn `probePlane=storage`.

---

## Event-Schema (Vorschlag, Schema v2 erweitern)

Gemeinsame Felder unverändert (`v`, `sid`, `seq`, `t_us`, `ev`).

| `ev` | Gate | Inhalt (kurz) |
|------|------|----------------|
| `ca_transport` | C1 | `transport=aoa\|spp`, `state=up\|down`, `note` |
| `ca_bcl` | C1 | `cmd`, `src`, `dest`, `data_len`, optional `raw_hex` (cap) |
| `ca_etch` | C1 | `kind=frame\|rpc_hint`, `len`, optional hash/hex prefix |
| `ca_sas` | C1 | `phase=cert\|challenge\|login\|blocked`, `challenge_len`, **kein** Key-Material |
| `ca_rhmi` | C2 | `phase=create\|resource\|init\|event`, `action_id` / `hmi_event` |
| `ca_cds` | C3 | `property`, `value` (JSON string, gekürzt) |
| `ca_av` | C4 | `op=create\|requestConnection\|state`, `connection=ENTERTAINMENT` |
| `ca_audio` | C5 | erst wenn Payload-Pfad beobachtbar — sonst weglassen |

`capture_health` bleibt plane-übergreifend. Marker/phase wie bisher.

---

## Erster implementierbarer Schritt (Gate C1)

**Ziel:** Messbarer Link-Versuch + BCL-Framing-Erkennung. Kein vollständiger AAIdrive-Port. Kein SAS-Bypass.

### C1a — Lab ohne Fahrzeug — **PASS**

Umgesetzt (Host-only, kein Device-EventBus / keine WebUI-Device-Anbindung):

| Artefakt | Pfad |
|----------|------|
| Framer | `firmware/src/connected/bcl/BclFramer.{h,cpp}`, `BclTypes.h` |
| Tests | `tools/bcl_c1a/test_bcl_framer.cpp` — `make -C tools/bcl_c1a test` |
| Scenario | `ca_c1_lab_bcl` — `tools/bcl_c1a/scenario_ca_c1_lab_bcl.cpp` |

**FACT:** Parser dekodiert 8-Byte-BCL (cmd/src/dest/len BE + payload ≤ 4000) aus synthetischen Streams, fragmentierungsinvariant.  
**TEST EVIDENCE:** Unit-Tests T1–T10 + Determinismus PASS.  
**UNKNOWN:** NBT-Evo-Wire, Transport AOA/BT, SAS.  
**NOT IMPLEMENTED (nach C1a):** C1b-0/C1b/C1c/C2–C5.

Resync: **unsupported** (kein Magic/`DEADBEEF`); nach `PayloadTooLarge` sticky error bis `reset()`.

### C1a.5 — Device EventBus / WebUI — **PASS** (Smoke 2026-10-28 @ `.88`)

| Artefakt | Pfad |
|----------|------|
| Probe | `firmware/src/connected/ConnectedAppProbe.{h,cpp}` |
| Shared stream | `firmware/src/connected/bcl/CaC1LabBclStream.h` |
| API | `POST /api/connected/lab_bcl` |
| Status | `GET /api/status` → `connected_app` |
| Smoke session | `sessions/20261028-235431_lab_ca_c1_lab_bcl` |

Gleicher synthetischer Stream wie Host. Kein AOA/SPP/SAS. MSC unangetastet.  
**Claim:** Device-Integration des Lab-Parsers belegt — **kein** NBT-/AOA-Wire-Claim.

### C1b-0 — AOA Transport Skeleton — PASS (lab)

```
ITransport ← AoaTransport (skeleton)
                │  bytes only — no BCL inside AOA
                ▼
            BclFramer → ca_bcl / ca_transport   (TEST inject only)
```

`usb_stack_bound=false` im MSC-Boot. Live-Link braucht C1b-P0.

### C1b-P0 — Boot-Mode AOA Observe — software implemented / Wire OPEN

**Isolation (P0-0):** `ConfigStore.usbGadget` = `msc` | `aoa_observe` — Umschalten nur NVS + Reboot.

| Boot | USB |
|------|-----|
| `msc` | USBMSC Golden Path, kein Vendor |
| `aoa_observe` | USBVendor only (`AoaObserveGadget`), kein MSC |

```
NBT Evo (Host) ──USB──► AoaObserveGadget
                           │
                           ├── ca_usb P0-A…E (attach / control / bulk raw)
                           └── AoaTransport (impl=observe)
                                 └── kein Auto-BclFramer (P1)
```

| API | Wirkung |
|-----|---------|
| Config `usbGadget` + `POST /api/connected/gadget/reboot` | Moduswechsel |
| `POST /api/connected/link/start` | nur bei `aoa_observe`; sonst 409 `wrong_usb_gadget` |
| `POST /api/connected/link/inject` `{hex}` | Lab/TEST → Framer |
| `POST /api/connected/link/stop` | disarm + session stop |

Desk-Hypothesen (bis NBT belegt): Control 51/52/53; präsentierte VID/PID `0x18D1:0x2D00`.  
Post-53 Re-Enum mit VID-Wechsel: **NOT IMPLEMENTED** (Arduino TinyUSB lockt Descriptor nach `USB.begin`).  
**Claim-Grenze:** AOA connected ≠ BCL connected. WebUI: Tab **Connected**.

### C1c — BT-SPP — stub only (`SppTransport.h`)

### SAS-Hard-Stop (dokumentiert)

Weiter als Challenge beobachten nur mit:

- gültigem App-Zertifikat (P7B / APP_AUTH+APP laut Analysis — **Existenz** FACT, Details Security-sensitiv),
- Signing-Backend (Classic Connected Security-Service / äquivalent),
- bewusster Lab-Freigabe.

**Probe-Verhalten ohne Cert:** `ca_sas.phase=blocked`, Gate C1 = „Auth-Challenge sichtbar oder klar blockiert“ — **PASS möglich ohne Login**. C2–C5 bleiben geschlossen.

---

## Bewusst UNKNOWN (nicht glätten)

- Identisches BCL über USB-AOA vs BT auf NBT Evo
- Etch-Magic `DEADBEEF` (nicht in Pins/Audit; Wire = Symbol-Hashes)
- Exponierte BMWRemoting-Methoden genau dieser HU
- RHMI-Capabilities / `hmi.type` unserer NBT Evo
- Ob AV nur Fokus/Control liefert oder Audio-Bytes trägt (C5)
- Dual-Role USB (MSC + AOA gleichzeitig)

---

## Docs / Profile Updates

| Datei | Aktion |
|-------|--------|
| [GATES-Connected.md](GATES-Connected.md) | neu — C1–C5 |
| [KONZEPT.md](KONZEPT.md) | Abschnitt Control Plane + Link hierher |
| [SCHEMA.md](SCHEMA.md) | `ca_bcl` Host-Adapter-Events (C1a); Device-Schema bei EventBus-Anbindung |
| `profiles/bmw_nbt_evo.json` | `control_plane` erst nach Lab-Traces |
| technology-audit FOLLOW-ON #1/#5 | eigene Capture-Audits, nicht dieses Doc |

---

## Nicht-Ziele

- Kein produktiver PiDrive-RHMI-Client in diesem Schritt
- Kein SAS-Bypass / Cert-Fälschung
- Kein Merge von Connected-App-Claims in MSC-Golden-FACTS
- Kein Ersatz der MSC-Media-Leiter durch AV ohne C4/C5-Evidence
