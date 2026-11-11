# Aktueller Stand — hu-probe

**FW im Feld:** **0.3.41** auf Auto-ESP `.89` (Web-OTA 11.11. abends) · Lab `.88` ebenfalls 0.3.41 (Kabel; war Analyzer)  
**Clean-Build:** `c318b1e3c1c6` · `FW_GIT_DIRTY=0` · Dist `firmware/dist/hu-probe.0.3.41.esp32s3.bin`  
**Smoke HU:** Session `20261111-174501_bmw_free` — `replay_records=227` (= offline+post_finalize), `replay_complete`, Upload `delivered`, Integrität PASS; `usb_core.hook_live=true`.  
**Noch offen:** Feldplan-Abnahme (`field/plan.json` bleibt `enabled:false`), Größenleiter neu, Tag/`ota/manifest` nur auf Zuruf. Auftrag: [ANWEISUNG-0.3.41](ANWEISUNG-0.3.41.md).  
Dies ist der **einzige Einstieg**, wenn du wissen willst, wo wir stehen. Lange Alt-Reviews liegen unter `feld/archiv/` und sind kein Einstieg.

## Connected (Control Plane)

| | |
|--|--|
| **Dashboard** | [feld/C1-STAND.md](feld/C1-STAND.md) — FACT / UNKNOWN / NEXT |
| **Gates** | [GATES-Connected.md](GATES-Connected.md) — G1–G4 |
| **Architektur** | [CONNECTED-APP.md](CONNECTED-APP.md) |
| **P0 I-303A** | **Feld erledigt** — [I303A-FELD-2026-11-07.md](feld/I303A-FELD-2026-11-07.md) · Reval [FELD-2026-11-10](feld/FELD-2026-11-10-I303A-REVAL-MSC.md) |
| **P1 jetzt** | [feld/FIELD-CONNECTED-REAL-01.md](feld/FIELD-CONNECTED-REAL-01.md) |

Kurzstand: Vendor-only `303A:1001` → HU ohne Medium (auch mit beiden USBs, Reval 10.11.). MSC-Kontrolle ok (`cache_a`). AOA ≠ BCL ≠ SAS ≠ RHMI. **Nächster Engpass = echter Phone×HU-Transport (P1).**

Kurze Wire-Belege: [AOA-C1B-P0-NBT](feld/AOA-C1B-P0-NBT.md) · [AOA-EPISTEMIK-EP0](feld/AOA-EPISTEMIK-EP0.md).

## Storage (MSC)

| | |
|--|--|
| **Integrität** | [GATES.md](GATES.md) |
| **Konzept/Schema** | [KONZEPT.md](KONZEPT.md) · [SCHEMA.md](SCHEMA.md) |
| **Feld-Survivors** | [MSC-HEIMFAHRT](feld/MSC-HEIMFAHRT.md) · [MSC-GROESSENLEITER-GO](feld/MSC-GROESSENLEITER-GO.md) · [FELD-2026-11-11 Größenleiter](feld/FELD-2026-11-11-MSC-GROESSENLEITER.md) · [MSC-TUR-MOUNT-FACTS](feld/MSC-TUR-MOUNT-FACTS.md) |

MSC-Kurz: Soft-Detach ops **PASS**; RAM-Replay-Deadlock (0.3.39) im HU-Smoke **behoben**. Alte Größenleiter-Sessions 27.10./11.11. mit `replay_records=0` bleiben ungültig — **NEXT MSC = Leiter mit 0.3.41 neu**, danach Feldplan scharf.
| **Produkt-Ampel** | PiDrive [MSC-AKTUELL](../../pidrive/docs/betrieb/MSC-AKTUELL.md) · [HU-Technical-Facts](../../pidrive/docs/fahrzeug/HU-Technical-Facts.md) |

## Nicht lesen als Einstieg

`feld/archiv/` — inkl. `_obsolete-reviews/` und der langen AOA-/Identity-Multi-KI-Berichte.
