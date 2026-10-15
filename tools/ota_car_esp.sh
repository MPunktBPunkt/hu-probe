#!/usr/bin/env bash
# Erste hu-probe-FW auf den Auto-ESP (192.168.178.89 / PiDrive-USB).
#
# Bevorzugt direkten Upload an laufendes pidrive:
#   POST http://192.168.178.89/ota-upload
#
# Fallback Hub-OTA (braucht Firmware in /tmp/iobroker-esphub-fw auf dem Hub-Host):
#   POST http://HUB/api/ota-push
#
#   ./tools/ota_car_esp.sh              # warten → flashen → Collector
#   ./tools/ota_car_esp.sh --status
#   ./tools/ota_car_esp.sh --collector  # nur Collector starten
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HUB="${HUB:-http://192.168.178.113:8093}"
MAC="${MAC:-68B6B32BC568}"
IP="${IP:-192.168.178.89}"
FW_NAME="${FW_NAME:-hu-probe.0.2.22.esp32s3.bin}"
FW_SRC="$ROOT/firmware/dist/$FW_NAME"
COLLECTOR_PORT="${COLLECTOR_PORT:-9100}"
WEB_PORT="${WEB_PORT:-8088}"

mode="wait"
case "${1:-}" in
  --status) mode="status";;
  --collector) mode="collector";;
  --flash-now) mode="flash";;
  -h|--help) sed -n '2,16p' "$0"; exit 0;;
esac

step() { printf '\n\033[1m==> %s\033[0m\n' "$1"; }
die() { printf '\033[31mFehler:\033[0m %s\n' "$1" >&2; exit 1; }
need() { command -v "$1" >/dev/null || die "$1 fehlt"; }
need curl
need python3

[ -f "$FW_SRC" ] || die "Bin fehlt: $FW_SRC"

probe_http() {
  # curl prints 000 on connect fail and may still exit non-zero — never append a second 000.
  local code
  code="$(curl -s -m 2 -o /dev/null -w '%{http_code}' "http://$IP/api/status" 2>/dev/null || true)"
  echo "${code:-000}"
}

probe_fw() {
  curl -s -m 2 "http://$IP/api/status" 2>/dev/null | python3 -c '
import sys, json
try:
    d = json.load(sys.stdin)
    print(d.get("fw") or d.get("fwVersion") or "")
except Exception:
    print("")
' 2>/dev/null || true
}

hub_device() {
  curl -s -m 5 "$HUB/api/devices" | python3 -c "
import sys, json
mac='${MAC}'.replace(':','').upper()
for d in json.load(sys.stdin):
    if d.get('mac','').replace(':','').upper()==mac:
        print(json.dumps(d)); break
" 2>/dev/null || true
}

print_status() {
  code="$(probe_http)"
  echo "    HTTP $IP/api/status → $code"
  fw="$(probe_fw)"
  [ -n "$fw" ] && echo "    probe fw=$fw"
  j="$(hub_device)"
  if [ -n "$j" ]; then
    # Pipe JSON via stdin — hub ios blobs contain quotes that break shell embedding.
    echo "$j" | python3 -c 'import json,sys; d=json.load(sys.stdin); print("    hub: name=%s online=%s ver=%s ip=%s rssi=%s" % (d.get("name"), d.get("online"), d.get("version"), d.get("ip"), d.get("rssi"))); print("    hub otaUrl=%s" % (d.get("otaUrl") or "-"))'
  else
    echo "    hub: Gerät nicht gefunden"
  fi
}

already_target_fw() {
  local want fw
  want="${FW_NAME#hu-probe.}"
  want="${want%.esp32s3.bin}"
  fw="$(probe_fw)"
  [ -n "$fw" ] && [ "$fw" = "$want" ]
}

wait_reachable() {
  step "Warte bis $IP antwortet (du bist im Auto / ESP online) …"
  while true; do
    code="$(probe_http)"
    j="$(hub_device)"
    online="?"
    ver="?"
    if [ -n "$j" ]; then
      online="$(echo "$j" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("online"))')"
      ver="$(echo "$j" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("version",""))')"
    fi
    printf '    [%s] http=%s hub_online=%s ver=%s\n' "$(date +%H:%M:%S)" "$code" "$online" "$ver"
    # Flash needs HTTP; hub-online alone is not enough (/ota-upload).
    if [ "$code" = "200" ]; then
      return 0
    fi
    sleep 3
  done
}

flash_direct() {
  step "Direkt-OTA via pidrive /ota-upload → $IP"
  # Arduino WebServer multipart field name typically \"firmware\" or \"update\";
  # pidrive only cares about the upload stream, filename is logged.
  local tmp code
  tmp="$(mktemp)"
  set +e
  code="$(curl -s -m 180 -o "$tmp" -w '%{http_code}' \
    -F "firmware=@${FW_SRC};filename=${FW_NAME}" \
    "http://${IP}/ota-upload")"
  set -e
  echo "    HTTP $code  body=$(head -c 200 "$tmp")"
  if [ "$code" != "200" ]; then
    rm -f "$tmp"
    die "Direkt-OTA fehlgeschlagen (ESP offline oder nicht pidrive?)"
  fi
  rm -f "$tmp"
  echo "    Upload OK — ESP rebootet"
}

flash_hub() {
  step "Hub-OTA planen (Fallback)"
  # Best-effort: Hub speichert unter /tmp/iobroker-esphub-fw — Upload oft kaputt wenn Dir fehlt.
  curl -s -m 5 -X POST "$HUB/api/ota-push" \
    -H 'Content-Type: application/json' \
    -d "{\"mac\":\"$MAC\",\"firmware\":\"$FW_NAME\"}" || true
  echo
  echo "    Hinweis: Hub-Firmware-Dir muss die Bin enthalten. Direkt-OTA ist zuverlässiger."
}

wait_huprobe() {
  step "Warte auf hu-probe nach Reboot …"
  local i=0
  sleep 5
  while [ "$i" -lt 90 ]; do
    i=$((i+1))
    code="$(probe_http)"
    j="$(hub_device)"
    ver=""
    name=""
    if [ -n "$j" ]; then
      ver="$(echo "$j" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("version",""))')"
      name="$(echo "$j" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("name",""))')"
    fi
    # hu-probe WebUI contains fwType in /api/status
    body=""
    if [ "$code" = "200" ]; then
      body="$(curl -s -m 2 "http://$IP/api/status" || true)"
    fi
    printf '    [%s] http=%s ver=%s\n' "$(date +%H:%M:%S)" "$code" "$ver"
    if echo "$body" | grep -q 'hu-probe'; then
      echo "    OK — hu-probe WebUI antwortet"
      echo "$body" | python3 -m json.tool 2>/dev/null | head -40 || echo "$body" | head -c 400
      return 0
    fi
    case "$ver" in
      0.2.0*|hu-probe*) echo "    OK — Hub zeigt Version $ver"; return 0;;
    esac
    sleep 3
  done
  die "Timeout: hu-probe nicht gesehen — UART-Log prüfen"
}

start_collector() {
  step "Collector :$COLLECTOR_PORT (Sessions → $ROOT/sessions)"
  if curl -s -m 1 "http://127.0.0.1:$WEB_PORT/api/status" >/dev/null 2>&1; then
    echo "    läuft bereits → http://$(hostname -I | awk '{print $1}'):$WEB_PORT/"
    return 0
  fi
  if [ -f "$ROOT/collector/.venv/bin/activate" ]; then
    # shellcheck disable=SC1091
    source "$ROOT/collector/.venv/bin/activate"
  fi
  mkdir -p "$ROOT/sessions"
  nohup hu-probe-collector --host 0.0.0.0 --port "$COLLECTOR_PORT" --web-port "$WEB_PORT" \
    --sessions "$ROOT/sessions" >"$ROOT/sessions/collector.log" 2>&1 &
  echo $! >"$ROOT/sessions/collector.pid"
  sleep 1
  local hostip
  hostip="$(hostname -I | awk '{print $1}')"
  echo "    pid=$(cat "$ROOT/sessions/collector.pid")"
  echo "    web=http://${hostip}:$WEB_PORT/  tcp=${hostip}:$COLLECTOR_PORT"
  echo "    Probe-Default collector_host=192.168.178.187 (diese Maschine)"
}

case "$mode" in
  status) print_status ;;
  collector) start_collector ;;
  flash)
    print_status
    if already_target_fw; then
      echo "    schon auf Ziel-FW — Flash übersprungen"
    else
      flash_direct
      wait_huprobe
    fi
    ;;
  wait)
    start_collector
    wait_reachable
    print_status
    if already_target_fw; then
      echo "    schon auf Ziel-FW — Flash übersprungen"
    else
      flash_direct
      wait_huprobe
    fi
    step "Fertig"
    echo "    Probe-UI:  http://$IP/"
    echo "    Soft-Cycle / Remount in der UI testen"
    echo "    Sessions:  $ROOT/sessions/"
    echo "    Falls keine Events: Collector-IP in Probe-Config = diese Maschine"
    ;;
esac
