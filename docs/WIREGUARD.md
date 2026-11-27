# WireGuard optional transport (hu-probe)

WireGuard is an **optional overlay** for reaching home/lab services over the public internet. It is **never required** for USB-MSC capture, trip storage, drive mode, or the main application path. Failure to bring up WireGuard must not block measurement or local WiFi operation.

Proven stack: [esp32.wireguard](https://github.com/MPunktBPunkt/esp32.wireguard) release **v0.4.9** (`libraries/esp_wireguard`, vendored under `firmware/lib/esp_wireguard`).

## Compile-time gate

| Build | Meaning |
|-------|---------|
| `huprobe-s3` (default) | `HUPROBE_ENABLE_WIREGUARD=0` — stub transport; `lib_ignore = esp_wireguard` |
| `huprobe-s3-wg` | `HUPROBE_ENABLE_WIREGUARD=1` — links vendored `esp_wireguard` + trombik CONFIG\_\* flags |

Normal field binaries stay on `huprobe-s3` until you intentionally ship a WG-enabled build.

**Firmware version:** `huprobe-s3` stays on **0.3.41**. `huprobe-s3-wg` overrides to **0.3.42** so lab/field evidence can tell stub vs WG-linked binaries apart without a parallel version scheme. Bump further on the same line when shipping the next WG-capable image.

## Configuration (NVS + Web UI JSON)

All keys live in the `huprobe` NVS namespace via `ConfigStore`.

| Field | Default | Notes |
|-------|---------|--------|
| `wgEnable` | `false` | Master switch; ignored if keys missing |
| `wgPrivateKey` | empty | Never exported in JSON (only `wgPrivateKeySet`) |
| `wgPublicKey` | empty | Never exported (only `wgPublicKeySet`) |
| `wgPresharedKey` | empty | Optional; only `wgPresharedKeySet` in JSON |
| `wgAddress` | empty | Interface address, e.g. `10.8.0.2/32` |
| `wgAllowedIps` | empty | **Selective** routes only — first comma-separated entry is used; `0.0.0.0/0` is rejected |
| `wgEndpointHost` | empty | Hostname or IPv4 (resolved before tcpip thread) |
| `wgEndpointPort` | `51820` | UDP port |
| `wgKeepalive` | `25` | `PersistentKeepalive` seconds |

You can set fields individually via the config API, or paste a minimal **wg-quick** blob into `wgConfigText` on import (parser fills the same fields; secrets are not echoed back).

## Enable path (boot)

1. WiFi STA is up (home WLAN or WiFi2 / drive failover — **failover stays at the WiFi layer**, not inside WireGuard).
2. SNTP / `configTime` runs when `enableNtp` is true (WireGuard needs sane time for handshakes).
3. If `wgEnable` and both private + peer public keys are present, `WireGuardTransport::enable()` **requests** connect (HTTP already up). Connect is deferred until SNTP yields a sane epoch (same gate as esp32.wireguard).
4. `loop()` completes connect under `LOCK_TCPIP_CORE` (Arduino-ESP32 2.x has no `tcpip_callback_wait`), then polls `peer_is_up`. `/api/status` → `wg.{linked,enable,active,peer_up,err}` (no WG Web-UI).
5. Lab USB: `LAB WIFI ssid|password` joins with `WiFi.persistent(false)` for guest/WAN data-plane tests without overwriting Fritz NVS.

MSC, capture, and TCP streaming continue regardless of WG state. Connect errors are logged and dropped; the device does not restart or detach USB because of WG.

## Selective routing (AllowedIPs)

Default policy matches esp32.wireguard **v0.4.9**:

- No full-tunnel default (`0.0.0.0/0` refused at parse time).
- After connect, a **/24-style** netmask may be applied on the WG netif when local address and AllowedIPs share that LAN semantics.
- If the STA address is already inside that routed network, route install is **refused** (same-LAN blackhole risk). The tunnel may still handshake; traffic to AllowedIPs on-LAN may not use WG — by design.

Use AllowedIPs that name only the remote subnet you need (e.g. home `192.168.178.0/24` via VPN), not the Fritz/LAN the phone hotspot already provides.

## Failover vs WireGuard

**WiFi2 / hotspot failover** (`DriveMode`, stored credentials, reconnect) operates entirely on **WLAN association**. WireGuard does not replace or implement failover; it assumes STA is already connected. If WG is enabled on a hotspot, ensure AllowedIPs do not overlap the hotspot's LAN.

## Lab validation (2026-11-27, FW **0.3.42** / `huprobe-s3-wg`)

`esp32.wireguard` **v0.4.9** remains the closed transport milestone. hu-probe validation is a separate track on `feat/wireguard-transport`.

| Step | Result |
|------|--------|
| Version distinct from stub 0.3.41 | `FW_VERSION=0.3.42` only on `huprobe-s3-wg` |
| WG default off | `wgEnable=false`; MSC/session smoke OK |
| Enable → `peer_up` (Heimnetz) | PASS (`/api/status` → `wg.peer_up`) |
| Guest WAN → route + `peer_up` | PASS (`FRITZ!Box Gast` → `192.168.179.3`, netmask `/24` installed) |
| Hub over WG from guest | PASS (`LAB HUB NOW` → `lastOk=1`) |
| WG off mid-session | PASS (session stays active; stop/marker OK) |
| Same-LAN AllowedIPs | route refused (by design); handshake still OK |

USB lab lines (no WG Web-UI): `LAB WIFI ssid\|pass`, `LAB WG ON\|OFF\|STATUS`, `LAB HUB NOW`.

## Operations checklist

1. Generate keys and peer config on the VPN server (outside this doc).
2. Flash `pio run -e huprobe-s3-wg -t upload` only when you need WG in the image.
3. Set `wgEnable` + keys + selective AllowedIPs via config API; reboot or let `loop()` enable after SNTP.
4. Confirm serial `[WG]` / `/api/status.wg`: endpoint resolved, connect result, route installed or refused, `peer_up`.
5. Field decision only after the lab table above stays green on the intended DUT.

## References

- Vendored library: `firmware/lib/esp_wireguard`
- Adapter: `firmware/src/net/WireGuardTransport.{h,cpp}`
- Upstream patterns: `esphub/esp32.wireguard` → `WireGuardStack.cpp`