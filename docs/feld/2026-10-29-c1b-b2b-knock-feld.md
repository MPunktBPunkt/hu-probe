# C1b-B2b Knock-TX — Feld 2026-10-29 (Abend, F20 / NBT Evo)

**Auswertung** · Gate-Nähe C1b-P0 / B2b · Desk-Basis BCL Session-Init.

| | |
|--|--|
| **Gerät Auto** | `192.168.178.89` |
| **Gerät Lab** | `192.168.178.88` |
| **FW Feld** | **0.3.32** (Knock 0.3.29 · Hub-RSSI 0.3.30 · OTA-abort 0.3.31 · **hex4** 0.3.32) |
| **Gadget Auto** | `aoa_observe` (`18D1:2D00`, vendor-only) |
| **Bin** | `firmware/dist/hu-probe.0.3.32.esp32s3.bin` |
| **Vorläufer** | [P0 observe](2026-10-29-c1b-p0-aoa-observe-feld.md) · [P0.5-A](2026-10-29-c1b-p05a-instrumentation.md) |

---

## Executive Summary

Vendor-only Accessory-Rohr steht: HU erkennt Zubehör, Spinner „wartet auf App“.  
**Knock-TX (hex4 `12 34 56 78`) geht raus** (`tx≥20`, `vendor_mounted`), **HU antwortet nicht** (`rx=0`, `setup=0`, `P0_E` false).  
ASCII-8-Lookalike war Desk-**REJECTED**; hex4 ist der korrekte BclCarLink-Init — am NBT Evo über USB weiterhin **kein Wire-FACT**.

---

## Was gebaut wurde (0.3.29–0.3.32)

| Ver | Inhalt |
|-----|--------|
| **0.3.29** | B2b: `writeVendor` · `AoaTransport.enableWireTx` · `sendKnock` · `POST /api/connected/link/knock` · WebUI-Button |
| **0.3.30** | Hub-Heartbeat: `rssi` / `version` / `uptime` / Heap; `/api/status` → `wifi.rssi` (Hub klebte sonst RF-Monitor-Werte) |
| **0.3.31** | Web-OTA: `Update.abort()` vor Retry + bei Fail (sticky `OTA fail` nach erstem Abbruch) |
| **0.3.32** | Default-Knock **`hex4`** = raw `12 34 56 78`; `ascii8` nur A/B (REJECTED) |

Patterns: `hex4` (default) · `ascii8` · `bcl_knock` · `bcl_hs`.

---

## Tages-Epistemik (Feld)

### Nachmittag — Identity / Observe (0.3.28, vor Knock)

| Modus | HU-UI | Probe |
|-------|-------|-------|
| `aoa_observe` vendor-only | Zubehör erkannt, Spinner ewig | Attach/Mount ja; `setup/51–53/rx=0` |
| `aoa_phone_like` `04E8:6860` | **„USB nicht unterstützt“**, kein Name | B1 negativ für diesen Fingerprint |

**Deutung:** OS-/Zubehör-Klassifikation bei Already-Accessory möglich; klassischer Host-Handshake 51→52→53 startet bei phone_like **nicht**. Spinner = App-Ebene wartet.

### Abend — B2b Knock (0.3.32)

Operator: OTG an, Verbindung aufbauen, kurz RST, Spinner bleibt „wartet auf App“.

| Messung | Wert | Klasse |
|---------|------|--------|
| FW / Gadget | 0.3.32 / `aoa_observe` | MEASURED |
| Link armed + `vendor_mounted` | true | MEASURED |
| P0_A / P0_C / P0_D | true | MEASURED |
| P0_B (51/52/53) | false, `setup_count=0` | MEASURED absent |
| Knock hex4 TX | ok, `tx` 4→20 (mehrere Sends) | MEASURED |
| Knock `bcl_knock` TX | ok (nachgelegt) | MEASURED |
| Bulk RX / P0_E | **0** / false | MEASURED absent |
| Spinner nach Knock | unverändert (Operator) | MEASURED (UI) |

WLAN im Auto ~−77…−79 dBm (echt); Hub-Anzeige −85 war zuvor **stale** (kein `rssi` im Heartbeat).

---

## Auswertung

### AOA-Reklassifizierung (wichtig)

`aoa_observe` bootet schon als **`18D1:2D00`** (Already-Accessory). Laut AOSP gehören **51→52→53** zum Übergang *in* den Accessory Mode — nicht zur normalen Kommunikation mit einem bereits so enumerierten Gerät.  
→ **`setup/51–53 = 0` bei `aoa_observe` ist kein AOA-Negativbefund.** Belastbar ist: Enum/UI PASS, **kein beobachteter Application-Bulk-RX**.

1. **USB-Rohr ≠ Connected.** Enum + Vendor-Mount reichen für Zubehör-UI; Connected-Transport unbewiesen.
2. **Auf dem getesteten Bulk-Pfad keine HU-Antwort** (`rx=0`) — nicht „HU lauscht nie“, sondern: unser Knock erreichte keinen beobachtbaren Response-Pfad.
3. **Spinner = weiterer Interaktionsschritt** nur **INFERENCE** (nicht bewiesen „warte auf BCL“).
4. **hex4** Desk-korrekt getestet, Wire negativ. Blind-Knock-Kette **beendet**.
5. **B1 phone_like** = Fingerprint-REJECT („USB nicht unterstützt“), kein AOA-Kill.
6. **OTA:** sticky `Update` + stale Hub-RSSI; Bank (−24) stabil.

### Claims

| Claim | Klasse |
|-------|--------|
| HU erkennt `aoa_observe` als Zubehör / Spinner | MEASURED |
| Fehlen 51–53 bei `18D1:2D00` widerlegt AOA | **REJECTED** |
| Klassischer AOA 51–53 bei phone_like | NOT OBSERVED (HU rejected vorher) |
| BCL Session-Init hex4 Bulk OUT | MEASURED (TX) |
| HU-Antwort auf getestetem Bulk-Pfad | **ABSENT** |
| USB trägt BCL am NBT Evo | **UNKNOWN** |
| BCL primär BT RFCOMM | HYPOTHESIS (code-supported, gestärkt) |

---

## Ops / Artefakte

- Beide ESPs geflasht: `.89` + `.88` auf **0.3.32** (Lab `.88` zuletzt `aoa_phone_like`).
- WebUI: Connected → Link → Knock TX (hex4 selected).
- Desk-Handshake-Doc (extern): `bimmergestalt-idrive-refs/applications/bmw-bcl-handshake.md` (Init hex4, Rollen MyBMW vs AAIdrive).

---

## Nächste Schritte

Siehe Maßnahmenkatalog: [2026-10-29-c1b-massnahmen-nach-b1-b2b.md](2026-10-29-c1b-massnahmen-nach-b1-b2b.md).  
**Einzig priorisiert:** FOLLOW-ON **#9** Phone+Connected Referenz-Capture. Kein Blind-Knock.

---

## Kurzfazit

Rohr **PASS** · Knock-TX **PASS** · HU-Antwort auf getestetem Bulk **ABSENT**.  
AOA nicht widerlegt; USB-BCL nicht bewiesen. **Referenz-Capture**, nicht Blind-Tuning.
