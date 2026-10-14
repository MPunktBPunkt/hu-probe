#!/usr/bin/env python3
"""Build a FAT12 demo image with Silence-MP3 stubs for hu-probe MSC.

Default: 512 KiB volume, three ~120 KiB tracks (enough for S1/S2 pacing,
still fits ESP32-S3 flash alongside firmware).
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

# Minimal MPEG-1 L3 48 kbit/s stereo frame (same family as Mp3Silence.h).
FRAME = bytes(
    [
        0xFF,
        0xFB,
        0x30,
        0x64,
        0x00,
        0x0F,
        0xF0,
        0x00,
        0x00,
        0x69,
        0x00,
        0x00,
        0x00,
        0x08,
        0x00,
        0x00,
        0x0D,
        0x20,
        0x00,
        0x00,
        0x01,
        0x00,
        0x00,
        0x01,
        0xA4,
        0x00,
        0x00,
        0x00,
        0x20,
        0x00,
        0x00,
        0x34,
        0x80,
        0x00,
        0x00,
        0x04,
    ]
) + (b"LAME3.100" + b"U" * (156 - 36 - 9))


def silence_mp3(size: int) -> bytes:
    head = bytearray(44)
    head[0:3] = b"ID3"
    head[3] = 4
    # synchsafe size 34 → total ID3 44
    head[6:10] = bytes([0x00, 0x00, 0x00, 0x22])
    body_need = max(0, size - 44)
    frames = (body_need + len(FRAME) - 1) // len(FRAME)
    body = (FRAME * frames)[:body_need]
    out = bytes(head) + body
    return out[:size].ljust(size, b"\x00")


def _fat12_encode(entries: list[int]) -> bytes:
    """Pack FAT12 table from 12-bit cluster entries."""
    raw = bytearray()
    i = 0
    while i < len(entries):
        a = entries[i] & 0xFFF
        b = entries[i + 1] & 0xFFF if i + 1 < len(entries) else 0
        raw.append(a & 0xFF)
        raw.append(((a >> 8) & 0x0F) | ((b & 0x0F) << 4))
        raw.append((b >> 4) & 0xFF)
        i += 2
    return bytes(raw)


def build_fat12(
    volume_bytes: int,
    files: list[tuple[str, bytes]],
    label: str = "HUPROBE",
) -> bytes:
    if volume_bytes % 512:
        raise SystemExit("volume size must be multiple of 512")
    sector = 512
    sector_count = volume_bytes // sector
    reserved = 1
    fats = 2
    root_entries = 32  # 1 sector of root (32*32=1024 → use 16 entries / half? )
    # Use 16 root entries = 1 sector for simplicity on small volumes? FAT12 often 224.
    # Keep 32 entries = 1 KiB = 2 sectors for a few files + volume label.
    root_entries = 32
    root_sectors = (root_entries * 32 + sector - 1) // sector

    # Estimate FAT size: need enough clusters for file data.
    # Iterate to converge.
    fat_sectors = 1
    for _ in range(8):
        data_start = reserved + fats * fat_sectors + root_sectors
        data_sectors = sector_count - data_start
        if data_sectors <= 0:
            raise SystemExit("volume too small")
        # cluster size 1 sector
        max_clusters = data_sectors + 2  # include 0,1
        fat_bytes_needed = (max_clusters * 3 + 1) // 2
        fat_sectors_needed = (fat_bytes_needed + sector - 1) // sector
        if fat_sectors_needed == fat_sectors:
            break
        fat_sectors = fat_sectors_needed

    data_start = reserved + fats * fat_sectors + root_sectors
    data_sectors = sector_count - data_start

    img = bytearray(volume_bytes)

    # BPB / boot sector
    bpb = bytearray(sector)
    bpb[0:3] = bytes([0xEB, 0x3C, 0x90])
    bpb[3:11] = b"mkfs.fat"
    struct.pack_into("<H", bpb, 11, sector)
    bpb[13] = 1  # sectors per cluster
    struct.pack_into("<H", bpb, 14, reserved)
    bpb[16] = fats
    struct.pack_into("<H", bpb, 17, root_entries)
    struct.pack_into("<H", bpb, 19, sector_count if sector_count < 65536 else 0)
    bpb[21] = 0xF8
    struct.pack_into("<H", bpb, 22, fat_sectors)
    struct.pack_into("<H", bpb, 24, 32)  # track
    struct.pack_into("<H", bpb, 26, 2)  # heads
    bpb[54:62] = b"FAT12   "
    bpb[510] = 0x55
    bpb[511] = 0xAA
    img[0:sector] = bpb

    # Cluster allocation starting at 2
    fat = [0x000] * (data_sectors + 2)
    fat[0] = 0xFF8
    fat[1] = 0xFFF
    next_cluster = 2

    def alloc_chain(nbytes: int) -> int:
        nonlocal next_cluster
        nclus = (nbytes + sector - 1) // sector
        if nclus == 0:
            nclus = 1
        first = next_cluster
        for i in range(nclus):
            c = next_cluster
            next_cluster += 1
            if next_cluster > data_sectors + 1:
                raise SystemExit("out of clusters — increase volume")
            if i == nclus - 1:
                fat[c] = 0xFFF
            else:
                fat[c] = c + 1
            # data written by caller
        return first

    def write_clusters(first: int, data: bytes) -> None:
        c = first
        off = 0
        while True:
            lba = data_start + (c - 2)
            chunk = data[off : off + sector]
            img[lba * sector : lba * sector + len(chunk)] = chunk
            if fat[c] >= 0xFF8:
                break
            c = fat[c]
            off += sector

    def sfn(name: str) -> bytes:
        stem, _, ext = name.upper().partition(".")
        stem = (stem + "       ")[:8]
        ext = (ext + "   ")[:3]
        return (stem + ext).encode("ascii")

    root = bytearray(root_sectors * sector)
    # volume label entry
    lab = (label.upper() + "           ")[:11].encode("ascii")
    root[0:11] = lab
    root[11] = 0x08

    dir_index = 1
    for name, payload in files:
        first = alloc_chain(len(payload))
        write_clusters(first, payload)
        ent = bytearray(32)
        ent[0:11] = sfn(name)
        ent[11] = 0x20
        struct.pack_into("<H", ent, 26, first)
        struct.pack_into("<I", ent, 28, len(payload))
        root[dir_index * 32 : dir_index * 32 + 32] = ent
        dir_index += 1

    fat_bytes = _fat12_encode(fat)
    fat_bytes = fat_bytes.ljust(fat_sectors * sector, b"\x00")
    for fi in range(fats):
        off = (reserved + fi * fat_sectors) * sector
        img[off : off + len(fat_bytes)] = fat_bytes
    root_off = (reserved + fats * fat_sectors) * sector
    img[root_off : root_off + len(root)] = root
    return bytes(img)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "firmware/src/msc/demo_fat.bin",
    )
    ap.add_argument("--size-kib", type=int, default=512, choices=[256, 512, 768, 1024])
    ap.add_argument("--label", default="HUPROBE")
    args = ap.parse_args()

    vol = args.size_kib * 1024
    # Leave ~48 KiB for FS structures; split remainder across 3 tracks.
    payload_budget = vol - 48 * 1024
    each = max(16 * 1024, (payload_budget // 3) // 512 * 512)
    files = [
        ("TRACK1.MP3", silence_mp3(each)),
        ("TRACK2.MP3", silence_mp3(each)),
        ("TRACK3.MP3", silence_mp3(each)),
    ]
    img = build_fat12(vol, files, label=args.label)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(img)
    print(f"wrote {args.output} ({len(img)} bytes, {args.size_kib} KiB)")
    print(f"  tracks: {[f[0] for f in files]} x {each} bytes silence-mp3")
    hdr = Path(__file__).resolve().parents[1] / "firmware/src/msc/DemoFatImage.h"
    hdr.write_text(
        "#pragma once\n"
        "#include <stdint.h>\n"
        f"static const uint32_t DEMO_FAT_SIZE = {len(img)}u;\n"
        "static const uint16_t DEMO_FAT_SECTOR_SIZE = 512;\n"
        f"static const uint32_t DEMO_FAT_SECTOR_COUNT = {len(img) // 512}u;\n"
        "extern const uint8_t DEMO_FAT_IMAGE[];\n",
        encoding="utf-8",
    )
    print(f"updated {hdr}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
