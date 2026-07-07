#!/usr/bin/env python3
"""Export a BOB original-geometry stage slice for 3DS banner prototyping.

This is the opposite of the earlier blockout: it consumes the actual BOB
Fast3D display lists through the existing minimap extractor helpers, keeps a
round-ish slice near Mario's start area, and writes a glTF plus a quick preview.
It is intentionally a first pass: terrain first, props/actors next.
"""

from __future__ import annotations

import json
import math
import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import minimap_extract_textured as mm  # noqa: E402


OUT_DIR = ROOT / "3ds/banner_3d/bob_original"
OUT_BASE = OUT_DIR / "bob_original_stage"


def v_sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def v_cross(a, b): return (
    a[1] * b[2] - a[2] * b[1],
    a[2] * b[0] - a[0] * b[2],
    a[0] * b[1] - a[1] * b[0],
)
def v_norm(a):
    l = math.sqrt(sum(x*x for x in a)) or 1.0
    return tuple(x/l for x in a)


def collect_bob_triangles():
    vertices, display_lists, geo_dls, _ = mm.collect_level_geometry(ROOT, "bob", "1", include_level_models=False)
    triangles: list[mm.Triangle] = []
    for name in geo_dls:
        mm.run_display_list(name, display_lists, vertices, mm.RenderState(), triangles)
    return triangles


def texture_average(texture_paths, symbol):
    if not symbol or symbol not in texture_paths:
        return (0.55, 0.70, 0.38, 1.0)
    try:
        img = Image.open(texture_paths[symbol]).convert("RGBA").resize((1, 1), Image.Resampling.BOX)
        r, g, b, a = img.getpixel((0, 0))
        return (r/255, g/255, b/255, max(a, 210)/255)
    except Exception:
        return (0.55, 0.70, 0.38, 1.0)


def choose_slice(triangles):
    # Mario starts at (-6558, 0, 6464), very close to the level edge. Shift the
    # display slice inward so it includes the spawn-side road/boxes/goombas but
    # avoids relying on the one-sided boundary.
    cx, cz = -3900, 6100
    radius = 3050
    kept = []
    for tri in triangles:
        mx = sum(v.x for v in tri.vertices) / 3
        mz = sum(v.z for v in tri.vertices) / 3
        if (mx - cx) ** 2 + (mz - cz) ** 2 <= radius ** 2:
            kept.append(tri)
    return kept, (cx, cz, radius)


def write_gltf(triangles, slice_info):
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    cx, cz, radius = slice_info
    texture_paths = mm.parse_texture_symbols(ROOT)

    # Group by source texture. This keeps original display-list material splits,
    # while using average texture colors for a light first-pass glTF.
    groups: dict[str | None, list[mm.Triangle]] = {}
    for tri in triangles:
        groups.setdefault(tri.texture, []).append(tri)

    blob = bytearray()
    buffer_views = []
    accessors = []
    meshes = []
    nodes = [{"name": "COMMON", "children": []}]
    materials = []
    mat_index = {}

    def pad4():
        while len(blob) % 4:
            blob.append(0)

    def view(data, target=None):
        off = len(blob)
        blob.extend(data)
        pad4()
        obj = {"buffer": 0, "byteOffset": off, "byteLength": len(data)}
        if target:
            obj["target"] = target
        buffer_views.append(obj)
        return len(buffer_views) - 1

    def accessor(v, component_type, count, typ, minv=None, maxv=None):
        obj = {"bufferView": v, "componentType": component_type, "count": count, "type": typ}
        if minv is not None: obj["min"] = list(minv)
        if maxv is not None: obj["max"] = list(maxv)
        accessors.append(obj)
        return len(accessors) - 1

    def material_for(texture):
        key = texture or "vertex_color"
        if key in mat_index:
            return mat_index[key]
        color = texture_average(texture_paths, texture)
        idx = len(materials)
        materials.append({
            "name": key,
            "doubleSided": True,
            "pbrMetallicRoughness": {
                "baseColorFactor": list(color),
                "metallicFactor": 0.0,
                "roughnessFactor": 0.75,
            },
        })
        mat_index[key] = idx
        return idx

    scale = 3.7 / radius
    y_base = min(v.y for tri in triangles for v in tri.vertices)
    y_scale = scale * 0.92

    for texture, tris in sorted(groups.items(), key=lambda item: str(item[0])):
        positions = []
        normals = []
        indices = []
        for tri in tris:
            pts = []
            for v in tri.vertices:
                # Convert SM64 x/y/z to compact banner x/y/z. Keep the original
                # height variation but compress slightly for HOME Menu framing.
                pts.append(((v.x - cx) * scale, (v.y - y_base) * y_scale + 0.05, (v.z - cz) * scale))
            n = v_norm(v_cross(v_sub(pts[1], pts[0]), v_sub(pts[2], pts[0])))
            base = len(positions)
            positions.extend(pts)
            normals.extend([n, n, n])
            indices.extend([base, base + 1, base + 2])
        if not positions:
            continue
        pos_data = b"".join(struct.pack("<3f", *p) for p in positions)
        nrm_data = b"".join(struct.pack("<3f", *n) for n in normals)
        idx_data = b"".join(struct.pack("<H", i) for i in indices)
        pv = view(pos_data, 34962)
        nv = view(nrm_data, 34962)
        iv = view(idx_data, 34963)
        pa = accessor(pv, 5126, len(positions), "VEC3",
                      [min(p[i] for p in positions) for i in range(3)],
                      [max(p[i] for p in positions) for i in range(3)])
        na = accessor(nv, 5126, len(normals), "VEC3")
        ia = accessor(iv, 5123, len(indices), "SCALAR")
        mesh_idx = len(meshes)
        meshes.append({
            "name": f"BOB_{texture or 'color'}",
            "primitives": [{
                "attributes": {"POSITION": pa, "NORMAL": na},
                "indices": ia,
                "material": material_for(texture),
                "mode": 4,
            }],
        })
        nodes.append({"name": f"BOB_{texture or 'color'}", "mesh": mesh_idx})
        nodes[0]["children"].append(len(nodes) - 1)

    gltf = {
        "asset": {"version": "2.0", "generator": "export_bob_original_stage.py"},
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
        "meshes": meshes,
        "materials": materials,
        "buffers": [{"uri": OUT_BASE.with_suffix(".bin").name, "byteLength": len(blob)}],
        "bufferViews": buffer_views,
        "accessors": accessors,
    }

    OUT_BASE.with_suffix(".bin").write_bytes(blob)
    OUT_BASE.with_suffix(".gltf").write_text(json.dumps(gltf, indent=2) + "\n")
    print(f"Wrote {OUT_BASE.with_suffix('.gltf')} with {len(triangles)} original BOB terrain triangles in {len(meshes)} material groups")


def render_preview(triangles, slice_info):
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    cx, cz, radius = slice_info
    W, H = 1200, 720
    img = Image.new("RGB", (W, H), (230, 235, 242))
    draw = ImageDraw.Draw(img, "RGBA")
    texture_paths = mm.parse_texture_symbols(ROOT)
    scale = 3.7 / radius
    y_base = min(v.y for tri in triangles for v in tri.vertices)
    y_scale = scale * 0.92
    cam = (5.2, 3.7, -7.2)
    target = (0, 0.65, 0.2)
    fwd = v_norm((target[0]-cam[0], target[1]-cam[1], target[2]-cam[2]))
    right = v_norm(v_cross(fwd, (0,1,0)))
    up = v_cross(right, fwd)
    focal = 690

    tex_avg_cache = {}
    polys = []
    for tri in triangles:
        pts = [((v.x - cx) * scale, (v.y - y_base) * y_scale + 0.05, (v.z - cz) * scale) for v in tri.vertices]
        center = tuple(sum(p[i] for p in pts)/3 for i in range(3))
        depth = sum((center[i]-cam[i])*fwd[i] for i in range(3))
        if depth <= 0:
            continue
        if tri.texture not in tex_avg_cache:
            tex_avg_cache[tri.texture] = tuple(int(c*255) for c in texture_average(texture_paths, tri.texture))
        color = tex_avg_cache[tri.texture]
        n = v_norm(v_cross(v_sub(pts[1], pts[0]), v_sub(pts[2], pts[0])))
        light = max(0.42, min(1.05, 0.70 + 0.35 * sum(n[i] * v_norm((-0.4, 0.9, -0.2))[i] for i in range(3))))
        poly = []
        ok = True
        for p in pts:
            rel = (p[0]-cam[0], p[1]-cam[1], p[2]-cam[2])
            z = sum(rel[i]*fwd[i] for i in range(3))
            if z <= 0.1:
                ok = False
                break
            x = sum(rel[i]*right[i] for i in range(3))
            y = sum(rel[i]*up[i] for i in range(3))
            poly.append((W/2 + focal*x/z, H/2 - focal*y/z))
        if ok:
            shaded = tuple(max(0, min(255, int(color[i] * light))) for i in range(3)) + (255,)
            polys.append((depth, poly, shaded))
    for _, poly, color in sorted(polys, key=lambda p: p[0], reverse=True):
        draw.polygon(poly, fill=color, outline=(0,0,0,22))
    path = OUT_DIR / "bob_original_stage_preview.png"
    img.save(path)
    print(f"Wrote {path}")


def main():
    all_tris = collect_bob_triangles()
    kept, info = choose_slice(all_tris)
    print(f"Collected {len(all_tris)} BOB terrain triangles; kept {len(kept)} near stage slice")
    write_gltf(kept, info)
    render_preview(kept, info)


if __name__ == "__main__":
    main()
