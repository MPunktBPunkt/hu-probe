# WireGuard optional transport (hu-probe)

WireGuard is an **optional overlay** for reaching home/lab services over the public internet. It is **never required** for USB-MSC capture, trip storage, drive mode, or the main application path. Failure to bring up WireGuard must not block measurement or local WiFi operation.

Proven stack: [esp32.wireguard](https://github.com/MPunktBPunkt/esp32.wireguard) release **v0.4.9** (`libraries/esp_wireguard`, vendored under `firmware/lib/esp_wireguard`).

## Compile-time gate

| Build | Meaning |
|-------|---------|
| `huprobe-s3` (default) | `HUPROBE_ENABLE_WIREGUARD=0` — stub transport; `lib_ignore = esp_wireguard` |
| `huprobe-s3-wg` | `HUPROBE_ENABLE_WIREGUARD=1` — links vendored `esp_wireguard` + trombik CONFIG\_\* flags |

Normal field binaries stay on `huprobe-s3` until you intentionally ship a WG-enabled build.

**Firmware version:** keep `FW_VERSION` at **0.3.41** for stub/default builds. The **first field binary** built with `huprobe-s3-wg` should bump `FW_VERSION` in `platformio.ini` (document the change in the release notes).

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
3. If `wgEnable` and both private + peer public keys are present, `WireGuardTransport::enable()` runs:
   - `esp_wireguard_init` + `esp_wireguard_connect` on the **tcpip thread** (`tcpip_callback_wait`).
   - Endpoint hostname resolved to IPv4 on the Arduino task when possible (avoids lwIP DNS deadlocks).
4. Main `loop()` calls `WireGuardTransport::loop()` for periodic `peerIsUp` polling only — no blocking waits.

MSC, capture, and TCP streaming continue regardless of WG state. Connect errors are logged and dropped; the device does not restart or detach USB because of WG.

## Selective routing (AllowedIPs)

Default policy matches esp32.wireguard **v0.4.9**:

- No full-tunnel default (`0.0.0.0/0` refused at parse time).
- After connect, a **/24-style** netmask may be applied on the WG netif when local address and AllowedIPs share that LAN semantics.
- If the STA address is already inside that routed network, route install is **refused** (same-LAN blackhole risk). The tunnel may still handshake; traffic to AllowedIPs on-LAN may not use WG — by design.

Use AllowedIPs that name only the remote subnet you need (e.g. home `192.168.178.0/24` via VPN), not the Fritz/LAN the phone hotspot already provides.

## Failover vs WireGuard

**WiFi2 / hotspot failover** (`DriveMode`, stored credentials, reconnect) operates entirely on **WLAN association**. WireGuard does not replace or implement failover; it assumes STA is already connected. If WG is enabled on a hotspot, ensure AllowedIPs do not overlap the hotspot's LAN.

## Operations checklist

1. Generate keys and peer config on the VPN server (outside this doc).
2. Flash `pio run -e huprobe-s3-wg -t upload` only when you need WG in the image.
3. Set `wgEnable` + keys + selective AllowedIPs via Web UI or API; reboot or let the next boot enable the tunnel.
4. Confirm serial `[WG]` logs: endpoint resolved, connect result, route installed or refused.
5. Bump `FW_VERSION` before shipping the first WG field build.

## References

- Vendored library: `firmware/lib/esp_wireguard`
- Adapter: `firmware/src/net/WireGuardTransport.{h,cpp}`
- Upstream patterns: `esphub/esp32.wireguard` → `WireGuardStack.cpp`