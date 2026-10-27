#!/usr/bin/env bash
# Feld-Prep 2026-10-27: Collector + OTA 0.3.21 auf .89 + seq_size_ladder starten.
# Nutzung: ./tools/feld_prep_groessenleiter_027.sh
#   --status   nur Status
#   --flash    OTA jetzt (ESP muss online sein)
#   --wait     warten bis .89 da, dann flash + start (Default)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IP="${IP:-192.168.178.89}"
FW_NAME="${FW_NAME:-hu-probe.0.3.21.esp32s3.bin}"
FW_SRC="$ROOT/firmware/dist/$FW_NAME"
COLLECTOR_PORT="${COLLECTOR_PORT:-9100}"
WEB_PORT="${WEB_PORT:-8088}"
WANT_FW="0.3.21"

mode="${1:---wait}"
case "$mode" in
  --status|--flash|--wait|-h|--help) ;;
  *) echo "usage: $0 [--wait|--flash|--status]"; exit 1;;
esac

step() { printf '\n\033[1m==> %s\033[0m\n' "$1"; }
die() { printf '\033[31mFehler:\033[0m %s\n' "$1" >&2; exit 1; }
[ -f "$FW_SRC" ] || die "Bin fehlt: $FW_SRC"

http_code() {
  curl -s -m 2 -o /dev/null -w '%{http_code}' "http://$IP/api/status" 2>/dev/null || echo 000
}

probe_fw() {
  curl -s -m 3 "http://$IP/api/status" 2>/dev/null | python3 -c '
import sys,json
try:
  d=json.load(sys.stdin)
  print(d.get("fw") or d.get("fwVersion") or "")
except Exception:
  print("")
' 2>/dev/null || true
}

ensure_collector() {
  step "Collector :$COLLECTOR_PORT"
  if curl -s -m 2 -o /dev/null -w '%{http_code}' "http://127.0.0.1:$COLLECTOR_PORT/" 2>/dev/null | grep -q 200; then
    echo "    already up"
    return
  fi
  mkdir -p "$ROOT/sessions"
  if [ -f "$ROOT/collector/.venv/bin/activate" ]; then
    # shellcheck disable=SC1091
    source "$ROOT/collector/.venv/bin/activate"
  fi
  nohup hu-probe-collector --host 0.0.0.0 --port "$COLLECTOR_PORT" --web-port "$WEB_PORT" \
    --sessions "$ROOT/sessions" >"$ROOT/sessions/collector.log" 2>&1 &
  echo $! >"$ROOT/sessions/collector.pid"
  sleep 1
  echo "    pid=$(cat "$ROOT/sessions/collector.pid")"
}

print_status() {
  step "Status"
  echo "    HTTP .89 → $(http_code)"
  fw="$(probe_fw)"; [ -n "$fw" ] && echo "    fw=$fw" || echo "    fw=(unreachable)"
  echo "    bin=$FW_SRC ($(stat -c%s "$FW_SRC") bytes)"
  code_c="$(curl -s -m 2 -o /dev/null -w '%{http_code}' "http://127.0.0.1:$COLLECTOR_PORT/" 2>/dev/null || echo 000)"
  echo "    collector local → $code_c"
}

flash_ota() {
  step "OTA $FW_NAME → $IP"
  curl -sS -m 180 -w "\nHTTP %{http_code}\n" \
    -F "firmware=@${FW_SRC};filename=${FW_NAME}" \
    "http://$IP/ota-upload" || die "OTA upload failed"
  echo "    waiting reboot…"
  for i in $(seq 1 50); do
    sleep 3
    fw="$(probe_fw)"
    echo "    [$i] fw=$fw"
    case "$fw" in
      *"$WANT_FW"*) echo "    OK $WANT_FW"; return 0;;
    esac
  done
  die "timeout waiting for $WANT_FW"
}

verify_presets() {
  step "Presets size_*"
  curl -s -m 5 "http://$IP/api/media/presets" | python3 -c '
import sys,json
d=json.load(sys.stdin)
ids=[p.get("id") for p in d.get("presets") or []]
need=["size_158","size_1m","size_4m","size_16m"]
miss=[x for x in need if x not in ids]
print("    size presets:", [x for x in ids if str(x).startswith("size_")])
if miss:
  raise SystemExit("missing: "+", ".join(miss))
print("    OK")
'
}

start_seq() {
  step "Start seq_size_ladder"
  # Abort stale runner if any
  curl -sS -m 5 -X POST "http://$IP/api/sequence/abort" >/dev/null 2>&1 || true
  sleep 1
  curl -sS -m 10 -X POST -H 'Content-Type: application/json' \
    -d '{"id":"seq_size_ladder"}' \
    "http://$IP/api/sequence/start" | python3 -m json.tool 2>/dev/null || true
  sleep 2
  curl -s -m 3 "http://$IP/api/status" | python3 -c '
import sys,json
d=json.load(sys.stdin)
print("    fw", d.get("fw"))
s=d.get("session") or {}
print("    session", s.get("session_id"), "phase", s.get("phase"), "scenario", s.get("scenario") or d.get("scenario"))
' 2>/dev/null || true
  curl -s -m 3 "http://$IP/api/sequences" | python3 -c '
import sys,json,pprint
d=json.load(sys.stdin)
r=d.get("runner") or d.get("status") or d
print("    runner:")
pprint.pp(r if isinstance(r,dict) else {"raw":r})
' 2>/dev/null | head -40
  echo
  echo "    OPERATOR: Soft-Cycle → TRACK1; LED weiß blink = BOOT kurz zum Armen"
  echo "    GO: docs/feld/2026-10-27-GO-groessenleiter.md"
}

wait_esp() {
  step "Warte auf $IP (alle 3s)…"
  while true; do
    code="$(http_code)"
    if [ "$code" = "200" ]; then
      echo "    online HTTP $code fw=$(probe_fw)"
      return 0
    fi
    printf "    %s still %s\r" "$(date +%H:%M:%S)" "$code"
    sleep 3
  done
}

case "$mode" in
  -h|--help) sed -n '2,6p' "$0"; exit 0;;
  --status) ensure_collector; print_status; exit 0;;
  --flash)
    ensure_collector
    print_status
    fw="$(probe_fw)"
    case "$fw" in
      *"$WANT_FW"*) echo "    already $WANT_FW — skip OTA";;
      *) flash_ota;;
    esac
    verify_presets
    start_seq
    ;;
  --wait)
    ensure_collector
    print_status
    wait_esp
    fw="$(probe_fw)"
    case "$fw" in
      *"$WANT_FW"*) echo "    already $WANT_FW — skip OTA";;
      *) flash_ota;;
    esac
    verify_presets
    start_seq
    print_status
    ;;
esac
