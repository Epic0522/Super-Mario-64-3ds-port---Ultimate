#!/usr/bin/env python3
"""Patch small CGFX material flags after pycgfx conversion.

pycgfx emits correct blend commands for glTF alphaMode=BLEND, but for 3DS HOME
banner samples such as Universal-Updater, transparent materials also have
MTOB.translucency_kind = 1.  HOME appears sensitive to that flag, so patch it
for selected materials without touching the Blender-authored model.
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


def i32(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<i", data, offset)[0]


def rel(data: bytes | bytearray, offset: int) -> int | None:
    value = i32(data, offset)
    return None if value == 0 else offset + value


def cstr(data: bytes | bytearray, offset: int | None) -> str | None:
    if offset is None or offset < 0 or offset >= len(data):
        return None
    end = data.find(b"\0", offset)
    if end < 0:
        return None
    return data[offset:end].decode("latin1", "replace")


def patch_material_translucency(path: Path, material_name: str, translucency_kind: int) -> bool:
    data = bytearray(path.read_bytes())
    changed = False
    start = 0
    while True:
        signature = data.find(b"MTOB", start)
        if signature < 0:
            break
        mtob = signature - 4
        start = signature + 1
        name = cstr(data, rel(data, mtob + 12))
        if name != material_name:
            continue
        old = i32(data, mtob + 32)
        struct.pack_into("<i", data, mtob + 32, translucency_kind)
        path.write_bytes(data)
        print(f"patched {material_name}: translucency_kind {old} -> {translucency_kind}")
        changed = True
        break
    return changed


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("cgfx", type=Path)
    parser.add_argument("material_name")
    parser.add_argument("--translucency-kind", type=int, default=1)
    args = parser.parse_args()

    if not patch_material_translucency(args.cgfx, args.material_name, args.translucency_kind):
        raise SystemExit(f"material not found: {args.material_name}")


if __name__ == "__main__":
    main()
