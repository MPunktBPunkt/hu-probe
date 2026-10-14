# GATE-01B: expose TinyUSB `_mscd_itf` (host CBW/CSW) + callback wraps.
Import("env")  # type: ignore  # noqa: F821 — PlatformIO SCons

from pathlib import Path
import shutil
import subprocess


def _prepare(env):
    platform = env.PioPlatform()
    framework = Path(platform.get_package_dir("framework-arduinoespressif32"))
    toolchain = Path(platform.get_package_dir("toolchain-xtensa-esp32s3"))
    src_a = framework / "tools" / "sdk" / "esp32s3" / "lib" / "libarduino_tinyusb.a"
    if not src_a.is_file():
        print(f"[gate01b] skip — missing {src_a}")
        return

    build_dir = Path(env.subst("$BUILD_DIR"))
    build_dir.mkdir(parents=True, exist_ok=True)
    dst_a = build_dir / "libarduino_tinyusb.a"
    objcopy = toolchain / "bin" / "xtensa-esp32s3-elf-objcopy"
    shutil.copy2(src_a, dst_a)
    subprocess.check_call([str(objcopy), "--globalize-symbol=_mscd_itf", str(dst_a)])
    print(f"[gate01b] globalized _mscd_itf → {dst_a}")

    # Prefer patched archive over SDK copy when resolving -larduino_tinyusb.
    env.Prepend(LIBPATH=[str(build_dir)])
    env.Append(
        LINKFLAGS=[
            "-Wl,--wrap=mscd_xfer_cb",
            "-Wl,--wrap=tud_msc_test_unit_ready_cb",
            "-Wl,--wrap=tud_msc_inquiry_cb",
            "-Wl,--wrap=tud_msc_capacity_cb",
            "-Wl,--wrap=tud_msc_scsi_cb",
            "-Wl,--wrap=tud_msc_start_stop_cb",
            "-Wl,--wrap=mscd_control_xfer_cb",
        ]
    )


_prepare(env)  # noqa: F821
