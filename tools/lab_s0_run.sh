#!/usr/bin/env bash
# Lab S0 dry-run on Debian USB-Host (.88). Tags session as lab_debian — never bmw.
#
#   ./tools/lab_s0_run.sh              # wait → flash 0.2.5 if needed → measure
#   ./tools/lab_s0_run.sh --measure    # only measure (ESP already hu-probe)
#   ./tools/lab_s0_run.sh --flash-now
#
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IP="${IP:-192.168.178.88}"
FW_NAME="${FW_NAME:-hu-probe.0.2.6.esp32s3.bin}"
FW_SRC="$ROOT/firmware/dist/$FW_NAME"
COLLECTOR_PORT="${COLLECTOR_PORT:-9100}"
WEB_PORT="${WEB_PORT:-8088}"
HOLD_S="${HOLD_S:-25}"

mode="all"
case "${1:-}" in
  --measure) mode="measure";;
  --flash-now) mode="flash";;
  --status) mode="status";;
  -h|--help) sed -n '2,10p' "$0"; exit 0;;
esac

step() { printf '\n\033[1m==> %s\033[0m\n' "$1"; }
die() { printf '\033[31mFehler:\033[0m %s\n' "$1" >&2; exit 1; }

probe_body() { curl -s -m 3 "http://$IP/api/status" || true; }
probe_ok() { curl -s -m 2 -o /dev/null -w '%{http_code}' "http://$IP/api/status" 2>/dev/null || echo 000; }

ensure_collector() {
  if curl -s -m 1 "http://127.0.0.1:$WEB_PORT/api/status" >/dev/null 2>&1; then
    echo "    collector already up"
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
  echo "    collector pid=$(cat "$ROOT/sessions/collector.pid")"
}

wait_esp() {
  step "Warte auf Lab-ESP $IP …"
  while true; do
    code="$(probe_ok)"
    printf '    [%s] http=%s\n' "$(date +%H:%M:%S)" "$code"
    [ "$code" = "200" ] && return 0
    sleep 3
  done
}

flash_direct() {
  [ -f "$FW_SRC" ] || die "Bin fehlt: $FW_SRC"
  step "OTA $FW_NAME → $IP"
  local tmp code
  tmp="$(mktemp)"
  code="$(curl -s -m 180 -o "$tmp" -w '%{http_code}' \
    -F "firmware=@${FW_SRC};filename=${FW_NAME}" \
    "http://${IP}/ota-upload" || true)"
  echo "    HTTP $code body=$(head -c 120 "$tmp")"
  rm -f "$tmp"
  [ "$code" = "200" ] || die "OTA fehlgeschlagen (noch PiDrive? Serial/esptool nötig)"
  sleep 8
}

wait_huprobe() {
  step "Warte auf hu-probe WebUI …"
  for _ in $(seq 1 60); do
    body="$(probe_body)"
    if echo "$body" | grep -q 'hu-probe'; then
      echo "$body" | python3 -m json.tool 2>/dev/null | head -30
      return 0
    fi
    sleep 2
  done
  die "hu-probe nicht gesehen"
}

measure() {
  step "Lab-Session starten (vehicle=lab_debian, profile=lab_debian)"
  curl -s -m 5 -X POST "http://$IP/api/config" -H 'Content-Type: application/json' \
    -d '{"vehicleId":"lab_debian","profileId":"lab_debian","scenarioId":"s0_plug_enum"}' || true
  echo
  curl -s -m 5 -X POST "http://$IP/api/session/stop" || true
  echo
  curl -s -m 5 -X POST "http://$IP/api/session/start" -H 'Content-Type: application/json' \
    -d '{"vehicle":"lab_debian","scenario":"s0_plug_enum","profile_id":"lab_debian"}'
  echo
  curl -s -m 5 -X POST "http://$IP/api/marker" -H 'Content-Type: application/json' \
    -d '{"name":"lab_s0_start","note":"debian usb-host dry-run; not a vehicle HU"}'
  echo
  step "Soft-Cycle + ${HOLD_S}s beobachten (USB-Stick am Debian-Host stecken)"
  curl -s -m 5 -X POST "http://$IP/api/usb/cycle" -H 'Content-Type: application/json' \
    -d '{"holdMs":600}'
  echo
  sleep "$HOLD_S"
  curl -s -m 5 -X POST "http://$IP/api/marker" -H 'Content-Type: application/json' \
    -d '{"name":"lab_s0_end","note":"end lab dry-run"}'
  echo
  curl -s -m 5 -X POST "http://$IP/api/session/stop"
  echo
  step "Letzte lab_debian Session"
  latest="$(ls -1d "$ROOT"/sessions/*_lab_debian_* 2>/dev/null | tail -1 || true)"
  if [ -z "$latest" ]; then
    echo "    noch keine lab_debian Session — Collector-IP prüfen"
    return 0
  fi
  echo "    $latest"
  python3 -m json.tool "$latest/session.json" | head -40
  python3 "$ROOT/tools/validate_session.py" "$latest" || true
  echo
  echo "    Unterscheidung Auto:"
  echo "      vehicle/profile = lab_debian   (Auto: bmw / bmw_nbt_evo)"
  echo "      usb_host_kind   = linux_debian (Auto: bmw_nbt_evo)"
  echo "      mac             = Lab .88      (Auto .89 = 68:b6:b3:2b:c5:68)"
}

case "$mode" in
  status) probe_body | python3 -m json.tool 2>/dev/null || echo offline ;;
  flash) ensure_collector; wait_esp; flash_direct; wait_huprobe ;;
  measure) ensure_collector; wait_esp; wait_huprobe; measure ;;
  all)
    ensure_collector
    wait_esp
    body="$(probe_body)"
    if echo "$body" | grep -q 'hu-probe'; then
      ver="$(echo "$body" | python3 -c 'import sys,json; print(json.load(sys.stdin).get("fw",""))' 2>/dev/null || true)"
      echo "    already hu-probe fw=$ver"
      if [ "$ver" != "0.2.6" ] && [ -f "$FW_SRC" ]; then
        flash_direct
        wait_huprobe
      fi
    else
      flash_direct
      wait_huprobe
    fi
    measure
    ;;
esac
