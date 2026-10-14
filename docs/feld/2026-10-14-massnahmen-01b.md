# Maßnahmen aus Mistral/GPT/Grok — GATE-01B (FW 0.2.8)

**Basis:** konsolidierter Gesamtbericht + Grok-Abgleich · HEAD vor Implementierung `a3ff34e`  
**Rahmen:** kein Bus-Analyzer — nur ESP32-S3 / TinyUSB

## Abgeleitete Maßnahmen

| Prio | Maßnahme | Status |
|------|----------|--------|
| P0 | Auto OTA 0.2.8 + BMW S0 neu (512 KiB) | **Feld** — wenn Auto da |
| P0 | Lab Power-Cycle + Triple Soft+Remount+`dd` | **Feld** — Lab `.88` WiFi |
| P1 | **01B.4–.7** echte CBW/CDB/CSW aus TinyUSB `_mscd_itf` | **done in 0.2.8** |
| P1 | Tag-Korrelation Host-`bot_tag` ↔ `read` | **done in 0.2.8** |
| P2 | MSC-Class-Control (GET_MAX_LUN / BOT_RESET) | **done in 0.2.8** (wrap `mscd_control_xfer_cb`) |
| P2 | Standard Control SET_ADDRESS / GET_DESCRIPTOR / SET_CONFIGURATION | **teilweise offen** — nicht über MSC-Class-Hook; braucht usbd-Hook |
| P3 | STALL / Clear-Halt / Fehler-Injektion | später (nach Baseline) |
| P3 | Flash/8‑MiB Architektur | später |
| P3 | VBUS-Tail-Ring | später |
| P4 | Mazda | zuletzt |

## Was 0.2.8 liefert (`bot_source=tinyusb`, `synthesized=false`)

- Host-**CBW** Rohbytes + Decode (Tag, LUN, xfer_len, CDB)
- **DATA**-Chunks mit Host-Tag
- Host-**CSW** Rohbytes (residue/status)
- Komfort-`read` mit demselben Host-`bot_tag`
- SCSI-Nebenpfade: TUR / INQUIRY / CAPACITY / START_STOP / generic `scsi_cb` → CBW(+CSW via poll/complete)
- USB `control`-Events für **MSC-Class** Requests

## Epistemische Grenze (unverändert)

- TinyUSB Device-Stack ≠ physische USB-Paketebene  
- **kein** DATA0/1, ACK/NAK, elektrisches Timing  
- Synth-Pfad (`msc_synth`) bleibt als Fallback im Code, Drain nutzt TinyUSB-Tap

## Auto-Feld mit 0.2.8

```bash
# Collector prüfen, dann:
./tools/ota_car_esp.sh          # default Bin jetzt 0.2.8
# BMW S0: physische Replugs + Marker, Medium 512 KiB Silence
```

Lab und BMW-Sessions **nicht vermischen**; 0.2.2-BMW historisch behalten bis 0.2.8-Baseline steht.
