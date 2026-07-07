#!/usr/bin/env python3
"""Build a tiny glTF test banner model from the in-game Power Star geometry.

This intentionally uses the decomp actor source as input so the first 3D
banner test is sourced from the game assets, not from a separate hand-made
placeholder.
"""

from __future__ import annotations

import json
import re
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MODEL = ROOT / "actors/star/model.inc.c"
OUT_DIR = ROOT / "3ds/banner_3d"
OUT_BASE = OUT_DIR / "star_test"


def pad4(data: bytearray) -> None:
    while len(data) % 4:
        data.append(0)


def parse_star_body() -> tuple[list[tuple[float, float, float]], list[tuple[float, float, float]], list[int]]:
    text = MODEL.read_text()

    vertex_match = re.search(
        r"star_seg3_vertex_0302B6F0\[\]\s*=\s*\{(?P<body>.*?)\};",
        text,
        re.S,
    )
    if not vertex_match:
        raise RuntimeError("Could not find star body vertex array")

    vertices: list[tuple[float, float, float]] = []
    normals: list[tuple[float, float, float]] = []
    vertex_re = re.compile(
        r"\{\{\{\s*(-?\d+),\s*(-?\d+),\s*(-?\d+)\},\s*0,\s*"
        r"\{\s*-?\d+,\s*-?\d+\},\s*"
        r"\{\s*(0x[0-9a-fA-F]+|-?\d+),\s*(0x[0-9a-fA-F]+|-?\d+),\s*(0x[0-9a-fA-F]+|-?\d+),\s*(0x[0-9a-fA-F]+|-?\d+)\}\}\},"
    )

    def s8(value: str) -> int:
        raw = int(value, 0)
        if raw > 127:
            raw -= 256
        return raw

    for match in vertex_re.finditer(vertex_match.group("body")):
        x, y, z = (int(match.group(i)) for i in range(1, 4))
        nx, ny, nz = (s8(match.group(i)) / 127.0 for i in range(4, 7))
        # Scale the N64 model coordinates into banner-camera friendly units and
        # flip Z so the star faces the Home Menu camera.
        vertices.append((x / 60.0, y / 60.0, -z / 60.0))
        normals.append((nx, ny, -nz))

    dl_match = re.search(
        r"star_seg3_dl_0302B7B0\[\]\s*=\s*\{(?P<body>.*?)\};",
        text,
        re.S,
    )
    if not dl_match:
        raise RuntimeError("Could not find star body display list")

    indices: list[int] = []
    tri_re = re.compile(r"gsSP(?:1Triangle|2Triangles)\((.*?)\)", re.S)
    for call in tri_re.finditer(dl_match.group("body")):
        nums = [int(n) for n in re.findall(r"\b\d+\b", call.group(1))]
        if call.group(0).startswith("gsSP1Triangle"):
            indices.extend(nums[:3])
        else:
            indices.extend(nums[:3])
            indices.extend(nums[3:6])

    if not vertices or not indices:
        raise RuntimeError("Parsed empty star geometry")
    return vertices, normals, indices


def write_gltf() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    positions, normals, indices = parse_star_body()

    blob = bytearray()
    pos_offset = len(blob)
    for vertex in positions:
        blob.extend(struct.pack("<3f", *vertex))
    pad4(blob)

    normal_offset = len(blob)
    for normal in normals:
        blob.extend(struct.pack("<3f", *normal))
    pad4(blob)

    index_offset = len(blob)
    for index in indices:
        blob.extend(struct.pack("<H", index))
    pad4(blob)

    bin_name = OUT_BASE.with_suffix(".bin").name
    gltf = {
        "asset": {"version": "2.0", "generator": "make_star_test_gltf.py"},
        "scene": 0,
        "scenes": [{"nodes": [0, 1]}],
        "nodes": [
            {
                "name": "COMMON",
                "mesh": 0,
                "rotation": [0.0, 0.0, 0.0, 1.0],
                "scale": [1.0, 1.0, 1.0],
                "translation": [0.0, 0.0, 0.0],
            },
            {
                "name": "Banner Camera",
                "camera": 0,
                "translation": [0.0, 1.0, 44.786],
            },
        ],
        "cameras": [
            {
                "name": "Banner Camera",
                "type": "perspective",
                "perspective": {
                    "aspectRatio": 1.66666666667,
                    "yfov": 0.523599,
                    "znear": 26.5,
                    "zfar": 1000.0,
                },
            }
        ],
        "meshes": [
            {
                "name": "Power Star",
                "primitives": [
                    {
                        "attributes": {"POSITION": 0, "NORMAL": 1},
                        "indices": 2,
                        "material": 0,
                        "mode": 4,
                    }
                ],
            }
        ],
        "materials": [
            {
                "name": "PowerStarGold",
                "doubleSided": True,
                "pbrMetallicRoughness": {
                    "baseColorFactor": [1.0, 0.78, 0.08, 1.0],
                    "metallicFactor": 0.0,
                    "roughnessFactor": 0.55,
                },
            }
        ],
        "buffers": [{"uri": bin_name, "byteLength": len(blob)}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": pos_offset, "byteLength": len(positions) * 12, "target": 34962},
            {"buffer": 0, "byteOffset": normal_offset, "byteLength": len(normals) * 12, "target": 34962},
            {"buffer": 0, "byteOffset": index_offset, "byteLength": len(indices) * 2, "target": 34963},
        ],
        "accessors": [
            {
                "bufferView": 0,
                "componentType": 5126,
                "count": len(positions),
                "type": "VEC3",
                "min": [min(v[i] for v in positions) for i in range(3)],
                "max": [max(v[i] for v in positions) for i in range(3)],
            },
            {
                "bufferView": 1,
                "componentType": 5126,
                "count": len(normals),
                "type": "VEC3",
            },
            {
                "bufferView": 2,
                "componentType": 5123,
                "count": len(indices),
                "type": "SCALAR",
            },
        ],
    }

    OUT_BASE.with_suffix(".bin").write_bytes(blob)
    OUT_BASE.with_suffix(".gltf").write_text(json.dumps(gltf, indent=2) + "\n")
    print(f"Wrote {OUT_BASE.with_suffix('.gltf')} ({len(positions)} vertices, {len(indices) // 3} triangles)")


if __name__ == "__main__":
    write_gltf()
