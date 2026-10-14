Import("env")  # type: ignore  # noqa: F821 — PlatformIO
import datetime
import pathlib
import subprocess


def _git_sha() -> str:
    try:
        return (
            subprocess.check_output(
                ["git", "rev-parse", "--short=12", "HEAD"],
                cwd=env["PROJECT_DIR"],  # type: ignore
                stderr=subprocess.DEVNULL,
            )
            .decode()
            .strip()
        )
    except Exception:
        return "unknown"


sha = _git_sha()
ts = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
build_id = f"{sha}-{ts}"

out = pathlib.Path(env["PROJECT_DIR"]) / "include" / "BuildId.h"  # type: ignore
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(
    "#pragma once\n"
    f'#define FW_GIT_SHA "{sha}"\n'
    f'#define FW_BUILD_ID "{build_id}"\n'
    f'#define FW_BUILD_UTC "{ts}"\n',
    encoding="utf-8",
)
print(f"[gen_build_id] {out.name}: {build_id}")
