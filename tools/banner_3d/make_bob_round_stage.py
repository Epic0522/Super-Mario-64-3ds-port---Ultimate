#!/usr/bin/env python3
"""Build a simple sealed circular BOB-textured banner stage.

This is the clean first model for the final banner direction:
no props, no characters, no castle, just an Animal Crossing-like round stage
with a slight mound using original BOB-area textures.
"""

from __future__ import annotations

import json
import math
import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance, ImageFilter


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import minimap_extract_textured as mm  # noqa: E402

OUT_DIR = ROOT / "3ds/banner_3d/bob_round_stage"
OUT_BASE = OUT_DIR / "bob_round_stage"

GRASS_SRC = ROOT / "textures/generic/bob_textures.05800.rgba16.png"
ROAD_SRC = ROOT / "textures/generic/bob_textures.09000.rgba16.png"
STONE_SRC = ROOT / "textures/generic/bob_textures.03800.rgba16.png"
CANNON_BASE_SRC = ROOT / "actors/cannon_base/cannon_base.rgba16.png"
CANNON_BARREL_SRC = ROOT / "actors/cannon_barrel/cannon_barrel.rgba16.png"


def norm(v):
    l = math.sqrt(sum(x * x for x in v)) or 1.0
    return tuple(x / l for x in v)


def cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def top_height(r: float) -> float:
    # Gentle rounded mound: high enough to catch naked-eye 3D depth, low enough
    # to read as a small display base rather than a mountain.
    return 0.13 + 0.28 * (1.0 - r * r) + 0.025 * math.cos(r * math.pi * 2)


def top_normal(x: float, z: float, radius: float) -> tuple[float, float, float]:
    r_world = math.sqrt(x * x + z * z)
    if r_world < 1e-5:
        return (0.0, 1.0, 0.0)
    r = min(1.0, r_world / radius)
    dh_dr = -0.56 * r - 0.05 * math.pi * math.sin(r * math.pi * 2)
    dh_dx = dh_dr * x / r_world / radius
    dh_dz = dh_dr * z / r_world / radius
    return norm((-dh_dx, 1.0, -dh_dz))


def add_tri(mesh, a, b, c, uva, uvb, uvc):
    n = norm(cross(sub(b, a), sub(c, a)))
    base = len(mesh["positions"])
    mesh["positions"].extend([a, b, c])
    mesh["normals"].extend([n, n, n])
    mesh["uvs"].extend([uva, uvb, uvc])
    mesh["indices"].extend([base, base + 1, base + 2])


def add_tri_with_normals(mesh, a, b, c, na, nb, nc, uva, uvb, uvc):
    base = len(mesh["positions"])
    mesh["positions"].extend([a, b, c])
    mesh["normals"].extend([na, nb, nc])
    mesh["uvs"].extend([uva, uvb, uvc])
    mesh["indices"].extend([base, base + 1, base + 2])


def add_quad_with_normals(mesh, a, b, c, d, na, nb, nc, nd, uva, uvb, uvc, uvd):
    add_tri_with_normals(mesh, a, b, c, na, nb, nc, uva, uvb, uvc)
    add_tri_with_normals(mesh, a, c, d, na, nc, nd, uva, uvc, uvd)


def add_quad(mesh, a, b, c, d, uva, uvb, uvc, uvd):
    add_tri(mesh, a, b, c, uva, uvb, uvc)
    add_tri(mesh, a, c, d, uva, uvc, uvd)


def transform_point(p, translation=(0, 0, 0), scale=1.0, yaw=0.0, pitch=0.0):
    x, y, z = p[0] * scale, p[1] * scale, p[2] * scale
    cp, sp = math.cos(pitch), math.sin(pitch)
    y, z = y * cp - z * sp, y * sp + z * cp
    cy, sy = math.cos(yaw), math.sin(yaw)
    x, z = x * cy + z * sy, -x * sy + z * cy
    return (x + translation[0], y + translation[1], z + translation[2])


def build_path_mesh() -> dict:
    mesh = {"positions": [], "normals": [], "uvs": [], "indices": []}
    steps = 18
    length = 4.95
    width = 0.78
    thickness = 0.035
    left_top = []
    right_top = []
    left_bottom = []
    right_bottom = []
    for i in range(steps):
        t = i / (steps - 1)
        def edge(t, side):
            z = -2.42 + t * length
            x = -0.24 + math.sin((t - 0.40) * math.pi) * 0.36 + side * width * (0.44 + 0.06 * math.sin(t * math.pi))
            r = min(1.0, math.sqrt(x * x + z * z) / 3.65)
            y = top_height(r) + 0.095
            return (x, y, z)
        l = edge(t, -1)
        rr = edge(t, 1)
        left_top.append(l)
        right_top.append(rr)
        left_bottom.append((l[0], l[1] - thickness, l[2]))
        right_bottom.append((rr[0], rr[1] - thickness, rr[2]))

    for i in range(steps - 1):
        t0 = i / (steps - 1)
        t1 = (i + 1) / (steps - 1)
        a, b, c, d = left_top[i], right_top[i], right_top[i + 1], left_top[i + 1]
        add_quad_with_normals(
            mesh, a, b, c, d,
            top_normal(a[0], a[2], 3.65), top_normal(b[0], b[2], 3.65),
            top_normal(c[0], c[2], 3.65), top_normal(d[0], d[2], 3.65),
            (0, t0 * 4.0), (1, t0 * 4.0), (1, t1 * 4.0), (0, t1 * 4.0),
        )
        # simple side thickness so the road is not coplanar with the grass in
        # Xcode/SceneKit or the 3DS renderer.
        add_quad(mesh, left_top[i], left_top[i + 1], left_bottom[i + 1], left_bottom[i], (0, t0), (0, t1), (0, t1), (0, t0))
        add_quad(mesh, right_top[i + 1], right_top[i], right_bottom[i], right_bottom[i + 1], (1, t1), (1, t0), (1, t0), (1, t1))
    return mesh


def build_stone_pad_mesh() -> dict:
    # Clean custom overlay plinth. It is intentionally banner-specific:
    # BOB original terrain chunks are too broken when flattened onto the round
    # display base, so the plinth is modeled cleanly and textured with BOB stone.
    mesh = {"positions": [], "normals": [], "uvs": [], "indices": []}
    cx, cz = 1.75, -0.25
    rx, rz = 0.48, 0.40
    h = 0.07
    seg = 24
    y = top_height(math.sqrt(cx * cx + cz * cz) / 3.65) + 0.065
    top_ids = []
    bot_ids = []
    for i in range(seg):
        a = 2 * math.pi * i / seg
        x = cx + math.cos(a) * rx
        z = cz + math.sin(a) * rz
        u = 0.5 + math.cos(a) * 0.5
        v = 0.5 + math.sin(a) * 0.5
        top_ids.append(len(mesh["positions"]))
        mesh["positions"].append((x, y, z))
        mesh["normals"].append((0, 1, 0))
        mesh["uvs"].append((u, v))
        bot_ids.append(len(mesh["positions"]))
        mesh["positions"].append((x, y - h, z))
        mesh["normals"].append(norm((math.cos(a), 0.15, math.sin(a))))
        mesh["uvs"].append((i / seg * 2.0, 1.0))
    center = len(mesh["positions"])
    mesh["positions"].append((cx, y, cz))
    mesh["normals"].append((0, 1, 0))
    mesh["uvs"].append((0.5, 0.5))
    for i in range(seg):
        j = (i + 1) % seg
        mesh["indices"].extend([center, top_ids[i], top_ids[j]])
        mesh["indices"].extend([top_ids[i], bot_ids[i], bot_ids[j], top_ids[i], bot_ids[j], top_ids[j]])
    return mesh


def build_original_cannon_ground_patch() -> dict:
    vertices, display_lists, geo_dls, _ = mm.collect_level_geometry(ROOT, "bob", "1", include_level_models=False)
    triangles: list[mm.Triangle] = []
    for name in geo_dls:
        mm.run_display_list(name, display_lists, vertices, mm.RenderState(), triangles)

    # Original BOB cannon/start-area patch around script object
    # OBJECT_WITH_ACTS MODEL_CANNON_BASE at (-5694, 128, 5600).
    src_cx, src_cz = -5694, 5600
    dst_cx, dst_cz = 1.75, -0.25
    scale = 0.78 / 1300.0
    mesh = {"positions": [], "normals": [], "uvs": [], "indices": []}

    for tri in triangles:
        mx = sum(v.x for v in tri.vertices) / 3
        mz = sum(v.z for v in tri.vertices) / 3
        if math.hypot(mx - src_cx, mz - src_cz) > 780:
            continue
        # Keep the mostly horizontal original stone/ground bits and drop tall
        # boundary walls that would look like broken level geometry on the tiny
        # banner stage.
        if abs(tri.normal_y) < 0.35:
            continue
        pts = []
        uvs = []
        for v in tri.vertices:
            x = dst_cx + (v.x - src_cx) * scale
            z = dst_cz + (v.z - src_cz) * scale
            r = min(1.0, math.sqrt(x * x + z * z) / 3.65)
            y = top_height(r) + 0.045 + (v.y - 128) * scale * 0.35
            pts.append((x, y, z))
            uvs.append((v.s / 1024.0, v.t / 1024.0))
        add_tri(mesh, pts[0], pts[1], pts[2], uvs[0], uvs[1], uvs[2])

    return mesh


def actor_display_list_mesh(model_paths: list[Path], display_list: str, transform, textured_mat: int, shaded_mat: int) -> list[tuple[str, dict, int]]:
    text = "\n".join(mm.strip_comments(path.read_text()) for path in model_paths)
    vertices = mm.parse_vertices(text)
    display_lists = mm.parse_display_lists(text)
    triangles: list[mm.Triangle] = []
    mm.run_display_list(display_list, display_lists, vertices, mm.RenderState(), triangles)
    groups: dict[int, dict] = {}
    for tri in triangles:
        mat = textured_mat if tri.texture else shaded_mat
        mesh = groups.setdefault(mat, {"positions": [], "normals": [], "uvs": [], "indices": []})
        pts = []
        uvs = []
        for v in tri.vertices:
            # N64 model coords -> banner coords. Flip Z here so the cannon faces
            # towards the front-left of the stage after yaw/pitch placement.
            pts.append(transform_point((v.x, v.y, -v.z), **transform))
            uvs.append((v.s / 1024.0, v.t / 1024.0))
        add_tri(mesh, pts[0], pts[1], pts[2], uvs[0], uvs[1], uvs[2])
    return [(f"cannon_part_{mat}", mesh, mat) for mat, mesh in groups.items()]


def build_stage(radius=3.65, rings=18, segments=128):
    top = {"positions": [], "normals": [], "uvs": [], "indices": []}
    side = {"positions": [], "normals": [], "uvs": [], "indices": []}
    bottom = {"positions": [], "normals": [], "uvs": [], "indices": []}

    def p(r_i, s_i):
        r = r_i / rings
        a = 2 * math.pi * s_i / segments
        x = math.cos(a) * radius * r
        z = math.sin(a) * radius * r
        y = top_height(r)
        return (x, y, z)

    def uv_top(r_i, s_i):
        r = r_i / rings
        a = 2 * math.pi * s_i / segments
        # Planar UVs avoid the pinwheel/sliced look that polar UVs create at
        # the centre of a circular banner stage. Keep repetition modest because
        # the original BOB grass texture is intentionally low-resolution.
        tile = 0.72
        return (0.5 + math.cos(a) * r * tile, 0.5 + math.sin(a) * r * tile)

    # Top: shared indexed grid. The first ring is a single centre vertex to
    # avoid many duplicate vertices at the mound peak.
    top["positions"].append(p(0, 0))
    top["normals"].append((0.0, 1.0, 0.0))
    top["uvs"].append((0.5, 0.5))

    def top_index(r_i: int, s_i: int) -> int:
        if r_i == 0:
            return 0
        return 1 + (r_i - 1) * segments + (s_i % segments)

    for r_i in range(1, rings + 1):
        for s_i in range(segments):
            pos = p(r_i, s_i)
            top["positions"].append(pos)
            top["normals"].append(top_normal(pos[0], pos[2], radius))
            top["uvs"].append(uv_top(r_i, s_i))

    for s_i in range(segments):
        top["indices"].extend([top_index(0, 0), top_index(1, s_i + 1), top_index(1, s_i)])
    for r_i in range(1, rings):
        for s_i in range(segments):
            a = top_index(r_i, s_i)
            b = top_index(r_i, s_i + 1)
            c = top_index(r_i + 1, s_i + 1)
            d = top_index(r_i + 1, s_i)
            top["indices"].extend([a, b, c, a, c, d])

    y_bottom = -0.34
    edge_top = top_height(1.0)
    for s_i in range(segments):
        a0 = 2 * math.pi * s_i / segments
        top0 = (math.cos(a0) * radius, edge_top, math.sin(a0) * radius)
        bot0 = (math.cos(a0) * radius, y_bottom, math.sin(a0) * radius)
        u0 = s_i / segments * 6.0
        outward = norm((math.cos(a0), 0.0, math.sin(a0)))
        side["positions"].extend([top0, bot0])
        side["normals"].extend([outward, outward])
        side["uvs"].extend([(u0, 0), (u0, 1.2)])

        bottom["positions"].append(bot0)
        bottom["normals"].append((0.0, -1.0, 0.0))
        bottom["uvs"].append((0.5 + math.cos(a0) * 0.5, 0.5 + math.sin(a0) * 0.5))

    for s_i in range(segments):
        s0 = s_i * 2
        s1 = ((s_i + 1) % segments) * 2
        side["indices"].extend([s0, s1, s1 + 1, s0, s1 + 1, s0 + 1])

    bottom_center = len(bottom["positions"])
    bottom["positions"].append((0.0, y_bottom, 0.0))
    bottom["normals"].append((0.0, -1.0, 0.0))
    bottom["uvs"].append((0.5, 0.5))
    for s_i in range(segments):
        bottom["indices"].extend([bottom_center, (s_i + 1) % segments, s_i])

    # pycgfx / 3DS banner primitives are happier with small mesh chunks. Split
    # the smooth top into angular sectors so every mesh stays well below 255
    # vertices while preserving the indexed optimisation.
    meshes = []
    sector_count = 32
    sector_width = segments // sector_count
    for sector in range(sector_count):
        chunk = {"positions": [], "normals": [], "uvs": [], "indices": []}
        remap = {}
        s_start = sector * sector_width
        s_end = s_start + sector_width

        def add_old(idx):
            if idx not in remap:
                remap[idx] = len(chunk["positions"])
                chunk["positions"].append(top["positions"][idx])
                chunk["normals"].append(top["normals"][idx])
                chunk["uvs"].append(top["uvs"][idx])
            return remap[idx]

        for i in range(0, len(top["indices"]), 3):
            tri = top["indices"][i:i+3]
            # Include triangle if its non-centre vertices fall into this sector.
            slots = []
            for idx in tri:
                if idx == 0:
                    continue
                slot = (idx - 1) % segments
                slots.append(slot)
            if not slots:
                continue
            # handle normal sectors; triangles never span the 159->0 seam except
            # in the final sector because indices were emitted modulo segments.
            belongs = any(s_start <= slot <= s_end for slot in slots)
            if sector == sector_count - 1:
                belongs = belongs or any(slot == 0 for slot in slots)
            if belongs:
                chunk["indices"].extend(add_old(idx) for idx in tri)
        meshes.append((f"round_grass_top_{sector:02d}", chunk, 0))

    side_sector_count = 8
    side_width = segments // side_sector_count
    for sector in range(side_sector_count):
        chunk = {"positions": [], "normals": [], "uvs": [], "indices": []}
        remap = {}
        s_start = sector * side_width
        s_end = s_start + side_width
        def add_side_old(idx):
            if idx not in remap:
                remap[idx] = len(chunk["positions"])
                chunk["positions"].append(side["positions"][idx])
                chunk["normals"].append(side["normals"][idx])
                chunk["uvs"].append(side["uvs"][idx])
            return remap[idx]
        for i in range(0, len(side["indices"]), 3):
            tri = side["indices"][i:i+3]
            slots = [(idx // 2) % segments for idx in tri]
            belongs = any(s_start <= slot <= s_end for slot in slots)
            if sector == side_sector_count - 1:
                belongs = belongs or any(slot == 0 for slot in slots)
            if belongs:
                chunk["indices"].extend(add_side_old(idx) for idx in tri)
        meshes.append((f"round_dirt_side_{sector:02d}", chunk, 1))

    meshes.append(("round_stone_bottom", bottom, 2))
    meshes.append(("bob_start_road_clean_overlay", build_path_mesh(), 3))
    meshes.append(("cannon_stone_pad_clean_overlay", build_stone_pad_mesh(), 4))

    pad_y = top_height(math.sqrt(1.75 * 1.75 + (-0.25) * (-0.25)) / 3.65) + 0.18
    cannon_transform = {
        "translation": (1.75, pad_y, -0.25),
        "scale": 0.0024,
        "yaw": math.radians(-38),
        "pitch": math.radians(-12),
    }
    meshes.extend(actor_display_list_mesh([ROOT / "actors/cannon_base/model.inc.c"], "cannon_base_seg8_dl_080057F8", cannon_transform, 5, 7))
    barrel_transform = {
        "translation": (1.75, pad_y + 0.135, -0.25),
        "scale": 0.0024,
        "yaw": math.radians(-38),
        "pitch": math.radians(-30),
    }
    meshes.extend(actor_display_list_mesh([ROOT / "actors/cannon_barrel/model.inc.c"], "cannon_barrel_seg8_dl_08006660", barrel_transform, 6, 7))
    return meshes


def copy_textures():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    # Use original BOB textures as sources, but prepare banner-friendly copies.
    # The source grass is tiny and noisy; direct magnification reads as broken
    # pixels on the large HOME Menu stage, so upscale/smooth it lightly.
    grass = Image.open(GRASS_SRC).convert("RGBA").resize((256, 256), Image.Resampling.BICUBIC)
    grass = grass.filter(ImageFilter.GaussianBlur(0.45))
    grass = ImageEnhance.Color(grass).enhance(1.18)
    grass = ImageEnhance.Contrast(grass).enhance(0.92)
    grass.save(OUT_DIR / "bob_grass.png")

    stone = Image.open(STONE_SRC).convert("RGBA").resize((128, 128), Image.Resampling.BICUBIC)
    stone.save(OUT_DIR / "bob_stone.png")

    road = Image.open(ROAD_SRC).convert("RGBA").resize((128, 128), Image.Resampling.BICUBIC)
    road = ImageEnhance.Contrast(road).enhance(0.92)
    road.save(OUT_DIR / "bob_road.png")

    Image.open(CANNON_BASE_SRC).convert("RGBA").resize((128, 128), Image.Resampling.NEAREST).save(OUT_DIR / "cannon_base.png")
    Image.open(CANNON_BARREL_SRC).convert("RGBA").resize((128, 128), Image.Resampling.NEAREST).save(OUT_DIR / "cannon_barrel.png")


def pad4(blob):
    while len(blob) % 4:
        blob.append(0)


def write_gltf(meshes):
    blob = bytearray()
    buffer_views = []
    accessors = []
    gltf_meshes = []
    nodes = [{"name": "COMMON", "children": []}]

    def add_view(data, target=None):
        off = len(blob)
        blob.extend(data)
        pad4(blob)
        view = {"buffer": 0, "byteOffset": off, "byteLength": len(data)}
        if target:
            view["target"] = target
        buffer_views.append(view)
        return len(buffer_views) - 1

    def add_accessor(view, component_type, count, typ, minv=None, maxv=None):
        acc = {"bufferView": view, "componentType": component_type, "count": count, "type": typ}
        if minv is not None:
            acc["min"] = list(minv)
        if maxv is not None:
            acc["max"] = list(maxv)
        accessors.append(acc)
        return len(accessors) - 1

    for name, mesh, mat_idx in meshes:
        pos_data = b"".join(struct.pack("<3f", *p) for p in mesh["positions"])
        nrm_data = b"".join(struct.pack("<3f", *n) for n in mesh["normals"])
        uv_data = b"".join(struct.pack("<2f", *uv) for uv in mesh["uvs"])
        idx_data = b"".join(struct.pack("<H", i) for i in mesh["indices"])
        pv = add_view(pos_data, 34962)
        nv = add_view(nrm_data, 34962)
        uvv = add_view(uv_data, 34962)
        iv = add_view(idx_data, 34963)
        pa = add_accessor(
            pv, 5126, len(mesh["positions"]), "VEC3",
            [min(p[i] for p in mesh["positions"]) for i in range(3)],
            [max(p[i] for p in mesh["positions"]) for i in range(3)],
        )
        na = add_accessor(nv, 5126, len(mesh["normals"]), "VEC3")
        uva = add_accessor(uvv, 5126, len(mesh["uvs"]), "VEC2")
        ia = add_accessor(iv, 5123, len(mesh["indices"]), "SCALAR")
        gltf_meshes.append({
            "name": name,
            "primitives": [{
                "attributes": {"POSITION": pa, "NORMAL": na, "TEXCOORD_0": uva},
                "indices": ia,
                "material": mat_idx,
                "mode": 4,
            }],
        })
        nodes.append({"name": name, "mesh": len(gltf_meshes) - 1})
        nodes[0]["children"].append(len(nodes) - 1)

    gltf = {
        "asset": {"version": "2.0", "generator": "make_bob_round_stage.py"},
        "scene": 0,
        "scenes": [{"nodes": [0, len(nodes)]}],
        "nodes": nodes + [{
            "name": "Banner Camera",
            "camera": 0,
            "translation": [0, 1.0, 44.786],
        }],
        "cameras": [{
            "name": "Banner Camera",
            "type": "perspective",
            "perspective": {"aspectRatio": 1.66666666667, "yfov": 0.523599, "znear": 26.5, "zfar": 1000.0},
        }],
        "meshes": gltf_meshes,
        "images": [
            {"uri": "bob_grass.png"},
            {"uri": "bob_stone.png"},
            {"uri": "cannon_base.png"},
            {"uri": "cannon_barrel.png"},
        ],
        "textures": [
            {"source": 0, "sampler": 0},
            {"source": 1, "sampler": 0},
            {"source": 2, "sampler": 0},
            {"source": 3, "sampler": 0},
        ],
        "samplers": [{"magFilter": 9728, "minFilter": 9728, "wrapS": 10497, "wrapT": 10497}],
        "materials": [
            {"name": "BOB Grass Top", "doubleSided": False, "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}, "baseColorFactor": [1.0, 1.0, 1.0, 1], "roughnessFactor": 0.72, "metallicFactor": 0}},
            {"name": "Plain Brown Side", "doubleSided": False, "pbrMetallicRoughness": {"baseColorFactor": [0.42, 0.22, 0.08, 1], "roughnessFactor": 0.82, "metallicFactor": 0}},
            {"name": "BOB Stone Bottom", "doubleSided": False, "pbrMetallicRoughness": {"baseColorTexture": {"index": 1}, "baseColorFactor": [0.80, 0.80, 0.80, 1], "roughnessFactor": 0.85, "metallicFactor": 0}},
            {"name": "BOB Dirt Road", "doubleSided": False, "pbrMetallicRoughness": {"baseColorFactor": [0.45, 0.25, 0.09, 1], "roughnessFactor": 0.84, "metallicFactor": 0}},
            {"name": "BOB Stone Pad", "doubleSided": False, "pbrMetallicRoughness": {"baseColorTexture": {"index": 1}, "baseColorFactor": [0.88, 0.88, 0.88, 1], "roughnessFactor": 0.86, "metallicFactor": 0}},
            {"name": "Cannon Base Texture", "doubleSided": False, "pbrMetallicRoughness": {"baseColorTexture": {"index": 2}, "baseColorFactor": [1.0, 1.0, 1.0, 1], "roughnessFactor": 0.65, "metallicFactor": 0}},
            {"name": "Cannon Barrel Texture", "doubleSided": False, "pbrMetallicRoughness": {"baseColorTexture": {"index": 3}, "baseColorFactor": [1.0, 1.0, 1.0, 1], "roughnessFactor": 0.65, "metallicFactor": 0}},
            {"name": "Cannon Dark Shade", "doubleSided": False, "pbrMetallicRoughness": {"baseColorFactor": [0.04, 0.04, 0.08, 1], "roughnessFactor": 0.7, "metallicFactor": 0}},
        ],
        "buffers": [{"uri": OUT_BASE.with_suffix(".bin").name, "byteLength": len(blob)}],
        "bufferViews": buffer_views,
        "accessors": accessors,
    }
    OUT_BASE.with_suffix(".bin").write_bytes(blob)
    OUT_BASE.with_suffix(".gltf").write_text(json.dumps(gltf, indent=2) + "\n")
    print(f"Wrote {OUT_BASE.with_suffix('.gltf')} ({sum(len(m['positions']) for _, m, _ in meshes)} vertices)")


def sample(img, uv):
    w, h = img.size
    u = uv[0] % 1.0
    v = uv[1] % 1.0
    return img.getpixel((int(u * (w - 1)), int(v * (h - 1))))


def render_preview(meshes):
    W, H = 1200, 720
    img = Image.new("RGB", (W, H), (227, 232, 238))
    draw = ImageDraw.Draw(img, "RGBA")
    textures = {
        0: Image.open(OUT_DIR / "bob_grass.png").convert("RGBA"),
        2: Image.open(OUT_DIR / "bob_stone.png").convert("RGBA"),
        4: Image.open(OUT_DIR / "bob_stone.png").convert("RGBA"),
        5: Image.open(OUT_DIR / "cannon_base.png").convert("RGBA"),
        6: Image.open(OUT_DIR / "cannon_barrel.png").convert("RGBA"),
    }
    flat_colors = {
        1: (107, 56, 20, 255),
        3: (115, 64, 23, 255),
        7: (16, 16, 30, 255),
    }
    cam = (5.8, 3.6, -7.6)
    target = (0.0, 0.1, 0.1)
    fwd = norm((target[0]-cam[0], target[1]-cam[1], target[2]-cam[2]))
    right = norm(cross(fwd, (0, 1, 0)))
    up = cross(right, fwd)
    focal = 720
    light_dir = norm((-0.35, 0.88, -0.28))
    polys = []
    for _, mesh, mat_idx in meshes:
        tex = textures.get(mat_idx)
        for i in range(0, len(mesh["indices"]), 3):
            idx = mesh["indices"][i:i+3]
            pts = [mesh["positions"][j] for j in idx]
            uvs = [mesh["uvs"][j] for j in idx]
            center = tuple(sum(p[k] for p in pts) / 3 for k in range(3))
            depth = sum((center[k] - cam[k]) * fwd[k] for k in range(3))
            if depth <= 0:
                continue
            n = norm(cross(sub(pts[1], pts[0]), sub(pts[2], pts[0])))
            shade = max(0.50, min(1.15, 0.72 + 0.38 * sum(n[k] * light_dir[k] for k in range(3))))
            uv = tuple(sum(u[k] for u in uvs) / 3 for k in range(2))
            col = sample(tex, uv) if tex is not None else flat_colors.get(mat_idx, (180, 180, 180, 255))
            rgba = tuple(max(0, min(255, int(col[k] * shade))) for k in range(3)) + (255,)
            poly = []
            ok = True
            for p in pts:
                rel = (p[0]-cam[0], p[1]-cam[1], p[2]-cam[2])
                z = sum(rel[k] * fwd[k] for k in range(3))
                if z <= 0.1:
                    ok = False
                    break
                x = sum(rel[k] * right[k] for k in range(3))
                y = sum(rel[k] * up[k] for k in range(3))
                poly.append((W/2 + focal*x/z, H/2 - focal*y/z))
            if ok:
                polys.append((depth, poly, rgba))
    for _, poly, rgba in sorted(polys, key=lambda p: p[0], reverse=True):
        draw.polygon(poly, fill=rgba, outline=(0, 0, 0, 18))
    path = OUT_DIR / "bob_round_stage_preview.png"
    img.save(path)
    print(f"Wrote {path}")


OBJ_MATERIALS = [
    ("BOB_Grass_Top", "bob_grass.png", (0.65, 1.0, 0.65)),
    ("Plain_Brown_Side", None, (0.42, 0.22, 0.08)),
    ("BOB_Stone_Bottom", "bob_stone.png", (0.55, 0.55, 0.55)),
    ("BOB_Dirt_Road", None, (0.45, 0.25, 0.09)),
    ("BOB_Stone_Pad", "bob_stone.png", (0.62, 0.62, 0.62)),
    ("Cannon_Base_Texture", "cannon_base.png", (0.75, 0.75, 0.75)),
    ("Cannon_Barrel_Texture", None, (0.34, 0.34, 0.40)),
    ("Cannon_Dark_Shade", None, (0.04, 0.04, 0.08)),
]


def write_obj(meshes):
    obj_lines = ["mtllib bob_round_stage.mtl\n"]
    mtl_lines = []
    for name, tex, kd in OBJ_MATERIALS:
        mtl_lines.append(f"newmtl {name}\nKd {kd[0]} {kd[1]} {kd[2]}\n")
        if tex:
            mtl_lines.append(f"map_Kd {tex}\n")
        mtl_lines.append("\n")

    vbase = 1
    vtbase = 1
    for name, mesh, mat_idx in meshes:
        mat = OBJ_MATERIALS[mat_idx][0]
        obj_lines.append(f"o {name}\nusemtl {mat}\n")
        for p in mesh["positions"]:
            obj_lines.append(f"v {p[0]:.6f} {p[1]:.6f} {p[2]:.6f}\n")
        for uv in mesh["uvs"]:
            obj_lines.append(f"vt {uv[0]:.6f} {1.0 - uv[1]:.6f}\n")
        for i in range(0, len(mesh["indices"]), 3):
            a, b, c = (mesh["indices"][i + j] for j in range(3))
            obj_lines.append(
                f"f {vbase+a}/{vtbase+a} {vbase+b}/{vtbase+b} {vbase+c}/{vtbase+c}\n"
            )
        vbase += len(mesh["positions"])
        vtbase += len(mesh["uvs"])

    (OUT_DIR / "bob_round_stage.obj").write_text("".join(obj_lines))
    (OUT_DIR / "bob_round_stage.mtl").write_text("".join(mtl_lines))
    print(f"Wrote {OUT_DIR / 'bob_round_stage.obj'}")


def main():
    copy_textures()
    meshes = build_stage()
    write_gltf(meshes)
    write_obj(meshes)
    render_preview(meshes)


if __name__ == "__main__":
    main()
