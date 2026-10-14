#!/usr/bin/env bash
# Lab WiFi coex under HU-like burst reads (not sustained dd).
#   HOLD_ON_S=3 HOLD_OFF_S=20 CYCLES=8 ./tools/lab_wifi_burst_test.sh
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IP="${IP:-192.168.178.88}"
DEV="${DEV:-/dev/sda}"
HOLD_ON_S="${HOLD_ON_S:-3}"
HOLD_OFF_S="${HOLD_OFF_S:-20}"
CYCLES="${CYCLES:-8}"

probe() {
  curl -s -m 2 -o /tmp/lab_burst_st.json -w '%{http_code}' "http://${IP}/api/status" 2>/dev/null || echo 000
}

echo "==> burst coex test on=$HOLD_ON_S off=$HOLD_OFF_S cycles=$CYCLES dev=$DEV"
code="$(probe)"
echo "    pre http=$code"
python3 -c 'import json;d=json.load(open("/tmp/lab_burst_st.json"));print("   ",d.get("fw"),"plugged",d.get("msc_plugged"),"soft",d.get("msc_soft_attached"),"tcp",d.get("tcp_ok"))' 2>/dev/null || true

down_events=0
for c in $(seq 1 "$CYCLES"); do
  echo "--- cycle $c: READ ${HOLD_ON_S}s ---"
  timeout "$HOLD_ON_S" dd if="$DEV" of=/dev/null bs=4k status=none 2>/dev/null || true
  code="$(probe)"
  reads=$(python3 -c 'import json;print(json.load(open("/tmp/lab_burst_st.json")).get("msc_reads","?"))' 2>/dev/null || echo "?")
  echo "    after_on http=$code reads=$reads"
  [ "$code" = "200" ] || down_events=$((down_events + 1))

  echo "--- cycle $c: IDLE ${HOLD_OFF_S}s ---"
  sleep "$HOLD_OFF_S"
  code="$(probe)"
  echo "    after_off http=$code"
  [ "$code" = "200" ] || down_events=$((down_events + 1))
done

echo "SUMMARY down_samples=$down_events (of $((CYCLES * 2)) checks)"
[ "$down_events" -eq 0 ] && echo "PASS wifi held across burst cycles" && exit 0
echo "FAIL wifi dropped at least once — see LED/HTTP"
exit 1
