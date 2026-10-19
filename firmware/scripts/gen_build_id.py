Import("env")  # type: ignore  # noqa: F821 — PlatformIO
import datetime
import os
import pathlib
import re
import subprocess


def _git(*args: str) -> str:
    return (
        subprocess.check_output(
            ["git", *args],
            cwd=env["PROJECT_DIR"],  # type: ignore
            stderr=subprocess.DEVNULL,
        )
        .decode()
        .strip()
    )


def _git_sha() -> str:
    try:
        return _git("rev-parse", "--short=12", "HEAD")
    except Exception:
        return "unknown"


def _git_dirty() -> int:
    """1 = uncommitted changes, 0 = clean, -1 = git unavailable (B20)."""
    try:
        return 1 if _git("status", "--porcelain", "--untracked-files=no") else 0
    except Exception:
        return -1


def _head_tags() -> list:
    try:
        return [t for t in _git("tag", "--points-at", "HEAD").splitlines() if t]
    except Exception:
        return []


def _fw_version() -> str:
    for flag in env.get("BUILD_FLAGS", []):  # type: ignore
        m = re.search(r'FW_VERSION=\\?"([^"\\]+)', str(flag))
        if m:
            return m.group(1)
    hdr = pathlib.Path(env["PROJECT_DIR"]) / "include" / "BuildFlags.h"  # type: ignore
    m = re.search(r'#define FW_VERSION "([^"]+)"', hdr.read_text(encoding="utf-8"))
    return m.group(1) if m else ""


sha = _git_sha()
dirty = _git_dirty()
ts = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
build_id = f"{sha}{'-dirty' if dirty == 1 else ''}-{ts}"

# B20: field firmware must match a committed, tagged tree (HUPROBE_RELEASE=1).
if os.environ.get("HUPROBE_RELEASE") == "1":
    version = _fw_version()
    want_tag = f"v{version}" if version else ""
    problems = []
    if dirty != 0:
        problems.append("git tree dirty or git unavailable")
    if not want_tag or want_tag not in _head_tags():
        problems.append(f"HEAD not tagged {want_tag or 'v<FW_VERSION>'}")
    if problems:
        print(f"[gen_build_id] RELEASE BLOCKED: {'; '.join(problems)}")
        env.Exit(1)  # type: ignore

out = pathlib.Path(env["PROJECT_DIR"]) / "include" / "BuildId.h"  # type: ignore
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(
    "#pragma once\n"
    f'#define FW_GIT_SHA "{sha}"\n'
    f'#define FW_BUILD_ID "{build_id}"\n'
    f'#define FW_BUILD_UTC "{ts}"\n'
    f"#define FW_GIT_DIRTY {1 if dirty == 1 else 0}\n",
    encoding="utf-8",
)
print(f"[gen_build_id] {out.name}: {build_id} dirty={dirty}")
