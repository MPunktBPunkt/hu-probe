# C1a — Host BCL Framer (Lab)

Gate **HU-PROBE-GATE-C1a** (subset of [GATES-Connected.md](../../docs/GATES-Connected.md)):  
deterministischer Parser für die 8-Byte-BCL-Struktur aus **synthetischen** Streams.

```bash
make -C tools/bcl_c1a test
make -C tools/bcl_c1a scenario
```

## Evidence

| Marker | Wert |
|--------|------|
| `source` | `synthetic` |
| `evidence_level` | `TEST` |
| `direct_observation` | `false` |

**FACT (nach PASS der Tests):** Der Parser dekodiert die definierte 8-Byte-BCL-Struktur aus synthetischen Streams deterministisch (inkl. Fragmentierung).

**Nicht bewiesen:** NBT-Evo-Wire, USB-AOA, BT-SPP, Etch-Session, SAS, RHMI, CDS, AV.

## Layout

| Pfad | Rolle |
|------|-------|
| `firmware/src/connected/bcl/BclTypes.h` | Frame-Typen, `kMaxPayload=4000` (Desk), Synthetic-Builder |
| `firmware/src/connected/bcl/BclFramer.{h,cpp}` | Streaming-Framer |
| `test_bcl_framer.cpp` | T1–T10 + Determinismus + Fragmentierungsinvarianz |
| `scenario_ca_c1_lab_bcl.cpp` | Szenario → JSONL auf stdout, Status auf stderr |

## Resync

**UNKNOWN / unsupported.** Kein Magic-Byte-Sync (insbesondere kein `DEADBEEF`). Nach `PayloadTooLarge` bleibt der Framer im Fehlerzustand bis `reset()`.
