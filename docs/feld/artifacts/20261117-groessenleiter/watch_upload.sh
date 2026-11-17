#!/usr/bin/env bash
# Watch for new sessions after Heimkehr upload, then analyze.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"
PRE="$ROOT/docs/feld/artifacts/20261117-groessenleiter/pre_sessions.txt"
OUT="$ROOT/docs/feld/artifacts/20261117-groessenleiter"
cd "$ROOT"

echo "[watch] collector sessions=$ROOT/sessions"
echo "[watch] baseline=$(wc -l <"$PRE") entries — Ctrl-C to stop"

seen_file="$OUT/.seen_new"
: >"$seen_file"

while true; do
  mapfile -t new < <(comm -13 <(sort "$PRE") <(ls -1 sessions | sort))
  for s in "${new[@]}"; do
    grep -qxF "$s" "$seen_file" 2>/dev/null && continue
    echo "[watch] NEW $s"
    echo "$s" >>"$seen_file"
    if [[ -f "sessions/$s/session.json" ]]; then
      python3 - <<PY
import json
from pathlib import Path
p = Path("sessions/$s/session.json")
m = json.loads(p.read_text())
cap = m.get("capture") or {}
media = m.get("media") or {}
print(f"  scenario={m.get('scenario')} media={media.get('variant')} "
      f"recv={cap.get('recv_events')} integrity={cap.get('integrity_pass')} "
      f"end={cap.get('end_reason')} upload={cap.get('upload_status')}")
PY
      if [[ -f "sessions/$s/events.jsonl" ]] || [[ -f "sessions/$s/capture.jsonl" ]] || \
         compgen -G "sessions/$s/*.jsonl" >/dev/null; then
        echo "[watch] analyze $s"
        python3 tools/analyze_session.py "sessions/$s" \
          | tee "$OUT/analyze_${s}.log" || true
      fi
    fi
  done
  # Probe status when reachable
  if st=$(curl -sS -m 2 http://192.168.178.89/api/status 2>/dev/null); then
    python3 -c 'import json,sys; d=json.loads(sys.argv[1]); fp=d.get("drive",{}).get("field_plan") or {};
print(f"[probe] plan={fp.get(\"id\")} runs={fp.get(\"runs_done\")}/{fp.get(\"runs_total\")} next={fp.get(\"next_job\")} r{fp.get(\"next_rep\")} state={d.get(\"drive\",{}).get(\"state\")}")' "$st" 2>/dev/null || true
  fi
  sleep 15
done
