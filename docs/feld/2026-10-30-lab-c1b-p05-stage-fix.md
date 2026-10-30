# Lab C1b-P0.5: Stage-Fix + Lab A/B (FW 0.3.33)

**Datum:** 2026-10-30  
**Probe:** `192.168.178.88` · FW **0.3.33** · `aoa_phone_like`  
**Host:** Proxmox `192.168.178.108` (pyusb / usbfs; LXC passthrough weiterhin unzuverlässig)  
**Maßnahmen:** [`2026-10-30-massnahmen-c1b-p05.md`](2026-10-30-massnahmen-c1b-p05.md)

## Ergebnis

| Schritt | Ergebnis |
|---|---|
| OTA 0.3.33 | **PASS** |
| Lab A (Responder an) | **PASS** — `aoa51/52/53` ↑; Host: 52×6 + 53 **ok**; 51 weiterhin Pipe/STALL |
| Lab B (Responder aus) | **PASS** — Zähler weiter ↑; Host 51/52/53 alle Pipe/STALL; `aoa_mode=observe` |

### Lab A (Δ)

```
setup_count 0→8 · aoa51 0→1 · aoa52 0→6 · aoa53 0→1 · drops=0
51 err Pipe · 52×6 ok · 53 ok
```

Stage-Fix bestätigt: Callback sieht SETUP (`CONTROL_STAGE_SETUP`). Responder funktioniert für 52/53. **GET_PROTOCOL (51) antwortet noch nicht** (Host STALL trotz Zähler+`hypothesis_respond=true`) — eigener Folgepunkt, nicht Stage-Blindheit.

### Lab B (Δ)

```
setup 8→16 · aoa51 1→2 · aoa52 6→12 · aoa53 1→2
hypothesis_respond=false · alle Host-Transfers Pipe/STALL
```

Observe und Respond sind getrennt: Zählen ohne Antworten.

## Offen

- 51-Response-Pfad debuggen (`sendResponse` / bmRequest-Rekonstruktion)
- LXC usbfs-Passthrough (`pct` Node nach Re-Enum)
- M0 Android-Consumer; Retro-Caveat Feldberichte
