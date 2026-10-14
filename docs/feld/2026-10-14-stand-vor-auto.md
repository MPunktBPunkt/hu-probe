# Stand vor Auto-Rückkehr — 2026-10-14 ~13:55

## Instrument

| | |
|--|--|
| FW Head | **0.2.7** (`ddcf81ecc645`) · Bin `firmware/dist/hu-probe.0.2.7.esp32s3.bin` |
| GATE-01 | Stufe A (BOT-Synth) erfüllt · Stufe B (Bus-CBW) **offen** |
| Collector | `.187:9100` / Web `:8088` |
| Lab-ESP `.88` | **WiFi offline** (MSC am Proxmox weiter sichtbar) — Neuaufnahme Lab blockiert |
| Auto-ESP `.89` | erwartet mit Fahrzeug · noch **0.2.2**-Feldstand |

## Datenlage

| Artefakt | Status |
|----------|--------|
| Lab-Rohsessions | **gelöscht** (Junk/vorläufig) — nicht mit reifem Stand mischen |
| Lab-Parameter | vorläufig in `profiles/lab_debian.json` + [data-quality-and-params](2026-10-14-data-quality-and-params.md) |
| BMW S0 | **behalten** `sessions/20261014-081637_bmw_s0_plug_enum/` (FW 0.2.2) |
| Kurzer Online-Spike ~13:20 (`2092_…`) | verworfen (kein Stop, Seq-Lücken) |

## Ermittelte Parameter (gültig getrennt Lab ≠ Auto)

### Proxmox / Lab USB (`lab_debian`) — Instrument-Referenz

| Parameter | Wert | Epistemik |
|-----------|------|-----------|
| Topology | Proxmox → Genesys Hub → ESP MSC → LXC `/dev/sda` `HU_PROBE` | FACT |
| Read-Mode | **4096** (512 minor) | FACT |
| Gap-p50 Pace | **~4,0–4,3 ms** | FACT (vorläufig bis Triple-Repeat) |
| Soft detach→attach | **~0,30 s** | FACT |
| BOT-Order ≥0.2.7 | CBW→DATA→read→CSW, `synthesized` | FACT |
| Eager / Prefetch / TUR | — | UNKNOWN |

### BMW NBT Feld (`bmw_nbt_evo`) — Session 081637, FW 0.2.2

| Parameter | Wert | Epistemik |
|-----------|------|-----------|
| Read-Mode | **4096** dominant | FACT |
| Gap-p50 | **3724 µs** | FACT |
| 22-Read-Muster | 6/8 Fenster | FACT |
| Seq 249–250 fehlend; Drain-t_us Fenster 7 | Limitation Instrument 0.2.2 | FACT |
| Eager Body-Stream | Medium zu klein | UNKNOWN |

**Kein Übertrag** Lab→BMW oder BMW→Mazda.

## Wenn Auto da ist

1. Collector prüfen (`:8088`)  
2. `./tools/ota_car_esp.sh` → Auto auf **0.2.7**  
3. BMW S0 neu: physische Replugs + Marker, Medium 512 KiB Silence  
4. Optional parallel: Lab `.88` Power-Cycle → Triple Soft+Remount+`dd` → Lab-Params finalisieren  

Auswertungen: [BMW S0](2026-10-14-bmw-s0-auswertung.md) · [Review-Triage](2026-10-14-review-triage.md) · [Qualität/Params](2026-10-14-data-quality-and-params.md)
