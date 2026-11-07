# Lab-Checkliste: NVS sticky `usb_gadget` + Soft-Detach (FW ≥ 0.3.36)

| | |
|--|--|
| **Bug** | B1 — `usb_gadget` klebte auf `aoa_observe` (Lab 2026-11-02, FW 0.3.34) |
| **Fix** | Namespace `hpgad`/`mode` + Verify (0.3.35); Host-Regression `tools/gadget_nvs` (0.3.36) |
| **Recovery** | `POST /api/factory-reset` löscht `esphub`+`huprobe`+`hpgad` und rebootet |

## Host-Tests (ohne Board)

Lokal und in CI (`host-tests` Workflow auf `main` / PR):

```bash
make -C tools/gadget_nvs test
make -C tools/session_metrics test
make -C tools/bcl_c1a test scenario
```

Test-Binaries sind gitignored — „grün“ = CI-Lauf oder lokaler `make`, nicht committed Artifacts.

## Board-Regression (vor BMW)

Für jeden Mode: `msc` → `aoa_observe` → `aoa_phone_like` → `aoa_id_303a` → zurück `msc`.

1. `POST /api/config` mit `{"usbGadget":"<mode>"}` → Readback = Mode
2. `POST /api/connected/gadget/reboot` → nach Boot `GET /api/status` → `usb_gadget` = Mode
3. Soft-Detach / Soft-Attach → `usb_gadget` unverändert (kein Mode-Switch)
4. `session_metrics.visibility_scope` gesetzt; AOA: `is_full_ep0_trace=false`
5. Optional: `POST /api/factory-reset` → Boot `usb_gadget=msc`

## Erwartung

| Schritt | FACT wenn |
|---------|-----------|
| Save+Reboot hält Mode | sticky `hpgad` MEASURED |
| Soft-Detach ändert Mode nicht | Design-Invariante |
| `aoa_*` → `msc_interface_present=false` | P0-0 Isolation |
| `aoa_id_303a` → VID/PID `303A:1001` | Identity-Experiment armed |
