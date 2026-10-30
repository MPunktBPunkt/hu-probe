#!/usr/bin/env bash
# Capture USB bus traffic via usbmon (Phase-1 lab). Default: bus 2 (Genesys hub / probe).
# Run as root or with rw on /dev/usbmonN. Prefer PVE host if LXC node missing.
set -euo pipefail
BUS="${BUS:-2}"
OUT="${1:-/tmp/hu-probe-usbmon-bus${BUS}-$(date +%Y%m%d-%H%M%S).pcap}"
DEV="/dev/usbmon${BUS}"
IFACE="usbmon${BUS}"
if [[ ! -e "$DEV" ]]; then
  echo "FAIL: $DEV missing — on PVE: modprobe usbmon; pct set 100 -dev4 path=$DEV,mode=0666" >&2
  exit 3
fi
echo "capturing $IFACE → $OUT (Ctrl-C to stop)"
exec tshark -i "$IFACE" -w "$OUT"
