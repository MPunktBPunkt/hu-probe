#!/usr/bin/env python3
"""Validate field/plan.json against the rules in firmware/src/core/FieldPlanCore.h.

Experiment and media ids are read from the firmware tables, so a plan that names a profile
the firmware does not know fails here instead of being ignored on the probe.

    python tools/validate_field_plan.py [field/plan.json]
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EXPERIMENTS_H = ROOT / "firmware/src/capture/ExperimentProfiles.h"
MEDIA_H = ROOT / "firmware/src/media/MediaPresets.h"

MAX_BODY = 2048
MAX_JOBS = 8
MAX_REPEAT = 50
MAX_MEASURE_S = 1800
TOKEN = re.compile(r"^[A-Za-z0-9_-]+$")
SCENARIO_MAX = 31
STALL_MAX_JSON = 127
STALL_REGIONS = {"meta", "head", "body", "all"}
PARKED_ONLY_DELAY_MS = 20000


def check_stall(stall, where: str, errors: list[str], warnings: list[str]) -> None:
    """Same limits as ReadStall::configure (except the file name, resolved on the probe)."""
    if not isinstance(stall, dict):
        errors.append(f"{where}.stall: object required")
        return
    if len(json.dumps(stall, separators=(",", ":"))) > STALL_MAX_JSON:
        errors.append(f"{where}.stall: longer than {STALL_MAX_JSON} characters as compact JSON")
    regions = stall.get("regions", "body")
    names = regions if isinstance(regions, list) else [regions]
    if not names or not all(isinstance(r, str) and r in STALL_REGIONS for r in names):
        errors.append(f"{where}.stall.regions: meta/head/body/all")
    elif ("file" in stall or "idx" in stall) and ({"meta", "all"} & set(names)):
        errors.append(f"{where}.stall: file/idx cannot be combined with meta")
    pace = stall.get("pace", "fixed")
    if pace not in ("fixed", "realtime"):
        errors.append(f"{where}.stall.pace: fixed or realtime")
    delay = stall.get("delay_ms", 1000)
    if not isinstance(delay, int) or isinstance(delay, bool) or not 1 <= delay <= 120000:
        errors.append(f"{where}.stall.delay_ms: integer 1-120000")
    elif pace == "fixed" and delay > PARKED_ONLY_DELAY_MS:
        warnings.append(f"{where}.stall.delay_ms {delay} exceeds ~20 s: parked only (SEQ-STALL)")
    factor = stall.get("pace_factor", 1.0)
    if pace == "realtime" and (not isinstance(factor, (int, float)) or isinstance(factor, bool)
                               or not 0 < factor <= 10):
        errors.append(f"{where}.stall.pace_factor: number in (0, 10]")
    count = stall.get("count", 1)
    if not isinstance(count, int) or isinstance(count, bool) or not 0 <= count <= 65535:
        errors.append(f"{where}.stall.count: integer 0-65535 (0 = unlimited)")
    for key, hi in (("arm_delay_s", 600), ("window_s", 3600)):
        v = stall.get(key, 0)
        if not isinstance(v, (int, float)) or isinstance(v, bool) or not 0 <= v <= hi:
            errors.append(f"{where}.stall.{key}: 0-{hi}")


def table_ids(path: Path, table_start: str) -> set[str]:
    """First string of each row in the C table that starts at `table_start`."""
    text = path.read_text(encoding="utf-8")
    start = text.index(table_start)
    end = text.index("};", start)
    return set(re.findall(r'^\s{4}\{"([A-Za-z0-9_-]+)",', text[start:end], re.M))


def token(value, max_len: int) -> bool:
    return isinstance(value, str) and 0 < len(value) <= max_len and bool(TOKEN.match(value))


def validate(raw: bytes, experiments: set[str], media: set[str],
             warnings: list[str] | None = None) -> list[str]:
    errors: list[str] = []
    warnings = warnings if warnings is not None else []
    if len(raw) > MAX_BODY:
        errors.append(f"file is {len(raw)} bytes, probe accepts at most {MAX_BODY}")
    try:
        plan = json.loads(raw)
    except json.JSONDecodeError as e:
        return errors + [f"JSON: {e}"]
    if plan.get("schema") != 1:
        errors.append("schema must be 1")
    if not token(plan.get("plan"), 12):
        errors.append("plan: 1-12 characters [A-Za-z0-9_-]")
    if not token(plan.get("vehicle"), 23):
        errors.append("vehicle: 1-23 characters [A-Za-z0-9_-], must equal the probe's vehicleId")
    for key in ("enabled", "arm_at_home", "online_only"):
        if key in plan and not isinstance(plan[key], bool):
            errors.append(f"{key} must be true/false")
    jobs = plan.get("jobs")
    if not isinstance(jobs, list) or not jobs:
        return errors + ["jobs: non-empty list required"]
    if len(jobs) > MAX_JOBS:
        errors.append(f"jobs: at most {MAX_JOBS}")
    seen: set[str] = set()
    for i, job in enumerate(jobs):
        where = f"jobs[{i}]"
        if not isinstance(job, dict):
            errors.append(f"{where}: object required")
            continue
        jid = job.get("id")
        if not token(jid, 8):
            errors.append(f"{where}.id: 1-8 characters [A-Za-z0-9_-]")
        elif jid in seen:
            errors.append(f"{where}.id: duplicate '{jid}'")
        else:
            seen.add(jid)
        exp = job.get("experiment")
        if exp not in experiments:
            errors.append(f"{where}.experiment: '{exp}' not in firmware ({', '.join(sorted(experiments))})")
        med = job.get("media", "")
        if med and med not in media:
            errors.append(f"{where}.media: '{med}' not in firmware presets")
        if "fine" in job and not isinstance(job["fine"], bool):
            errors.append(f"{where}.fine must be true/false")
        rep = job.get("repeat", 1)
        if not isinstance(rep, int) or isinstance(rep, bool) or not 1 <= rep <= MAX_REPEAT:
            errors.append(f"{where}.repeat: integer 1-{MAX_REPEAT}")
        ms = job.get("measure_s", 0)
        if not isinstance(ms, int) or isinstance(ms, bool) or not 0 <= ms <= MAX_MEASURE_S:
            errors.append(f"{where}.measure_s: integer 0-{MAX_MEASURE_S} (0 = experiment default)")
        if "stall" in job:
            check_stall(job["stall"], where, errors, warnings)
        if token(plan.get("plan"), 12) and token(jid, 8) and isinstance(rep, int):
            scenario = f"fp_{plan['plan']}_{jid}_r{rep}"
            if len(scenario) > SCENARIO_MAX:
                errors.append(f"{where}: scenario '{scenario}' longer than {SCENARIO_MAX}")
    return errors


def main(argv: list[str]) -> int:
    path = Path(argv[1]) if len(argv) > 1 else ROOT / "field/plan.json"
    experiments = table_ids(EXPERIMENTS_H, "static const ExperimentProfile kTable[]")
    media = table_ids(MEDIA_H, "static const MediaPreset kMediaPresets[]")
    warnings: list[str] = []
    errors = validate(path.read_bytes(), experiments, media, warnings)
    for w in warnings:
        print(f"{path}: warning: {w}", file=sys.stderr)
    for e in errors:
        print(f"{path}: {e}", file=sys.stderr)
    if errors:
        return 1
    plan = json.loads(path.read_bytes())
    runs = sum(j.get("repeat", 1) for j in plan["jobs"])
    state = "enabled" if plan.get("enabled") else "disabled"
    print(f"{path}: ok — plan {plan['plan']} ({state}), {len(plan['jobs'])} job(s), {runs} trip(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
