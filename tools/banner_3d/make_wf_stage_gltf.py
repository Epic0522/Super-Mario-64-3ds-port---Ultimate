#!/usr/bin/env python3
"""Build the Whomp's Fortress HOME-menu banner glTF from the OBJ-preview scene."""

from __future__ import annotations

import json
import hashlib
import math
import os
import shutil
import struct
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "banner_3d"))

import export_wf_stage as wf  # noqa: E402


OUT_DIR = ROOT / "3ds/banner_3d/wf_stage"
OUT_BASE = OUT_DIR / "wf_stage"


def pad4(blob: bytearray) -> None:
    while len(blob) % 4:
        blob.append(0)


def vec_sub(a: tuple[float, float, float], b: tuple[float, float, float]) -> tuple[float, float, float]:
    return a[0] - b[0], a[1] - b[1], a[2] - b[2]


def vec_dot(a: tuple[float, float, float], b: tuple[float, float, float]) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def vec_cross(a: tuple[float, float, float], b: tuple[float, float, float]) -> tuple[float, float, float]:
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def vec_norm(v: tuple[float, float, float]) -> tuple[float, float, float]:
    length = math.sqrt(vec_dot(v, v)) or 1.0
    return v[0] / length, v[1] / length, v[2] / length


def quat_from_matrix(m: tuple[tuple[float, float, float], tuple[float, float, float], tuple[float, float, float]]) -> tuple[float, float, float, float]:
    # Matrix is row-major. Return glTF quaternion [x, y, z, w].
    m00, m01, m02 = m[0]
    m10, m11, m12 = m[1]
    m20, m21, m22 = m[2]
    trace = m00 + m11 + m22
    if trace > 0:
        s = math.sqrt(trace + 1.0) * 2.0
        return ((m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s, 0.25 * s)
    if m00 > m11 and m00 > m22:
        s = math.sqrt(1.0 + m00 - m11 - m22) * 2.0
        return (0.25 * s, (m01 + m10) / s, (m02 + m20) / s, (m21 - m12) / s)
    if m11 > m22:
        s = math.sqrt(1.0 + m11 - m00 - m22) * 2.0
        return ((m01 + m10) / s, 0.25 * s, (m12 + m21) / s, (m02 - m20) / s)
    s = math.sqrt(1.0 + m22 - m00 - m11) * 2.0
    return ((m02 + m20) / s, (m12 + m21) / s, 0.25 * s, (m10 - m01) / s)


def camera_look_at_quaternion(
    position: tuple[float, float, float],
    target: tuple[float, float, float],
) -> tuple[float, float, float, float]:
    forward = vec_norm(vec_sub(target, position))
    world_up = (0.0, 1.0, 0.0)
    z_axis = (-forward[0], -forward[1], -forward[2])
    x_axis = vec_norm(vec_cross(world_up, z_axis))
    y_axis = vec_cross(z_axis, x_axis)
    # Columns are the camera local axes in world space. Convert to row-major.
    matrix = (
        (x_axis[0], y_axis[0], z_axis[0]),
        (x_axis[1], y_axis[1], z_axis[1]),
        (x_axis[2], y_axis[2], z_axis[2]),
    )
    return quat_from_matrix(matrix)


def texture_size(texture: str | None, symbols: dict[str, Path]) -> tuple[int, int]:
    return wf.texture_size(texture, symbols)


def decimate_for_crash_test(triangles: list[wf.ObjTriangle], max_triangles: int) -> list[wf.ObjTriangle]:
    if max_triangles <= 0 or len(triangles) <= max_triangles:
        return triangles

    # Keep at least one representative for every texture/material call so this
    # test isolates geometry pressure instead of accidentally removing texture
    # state, camera state, or material setup from the CGFX.
    buckets: dict[str, list[wf.ObjTriangle]] = {}
    for tri in triangles:
        key = tri.texture or wf.material_name(tri.texture, tri.rgba, tri.source)
        buckets.setdefault(key, []).append(tri)

    selected: list[wf.ObjTriangle] = [items[len(items) // 2] for items in buckets.values()]
    selected_ids = {id(tri) for tri in selected}
    remaining_budget = max(0, max_triangles - len(selected))
    if remaining_budget == 0:
        return selected[:max_triangles]

    rest = [tri for tri in triangles if id(tri) not in selected_ids]
    if not rest:
        return selected
    step = max(1, len(rest) / remaining_budget)
    for i in range(remaining_budget):
        selected.append(rest[min(len(rest) - 1, int(i * step))])
    return selected


def make_screen_quad(
    *,
    name: str,
    center: tuple[float, float, float],
    size: tuple[float, float],
    rgba: tuple[int, int, int, int],
    texture: str | None = None,
) -> list[wf.ObjTriangle]:
    """Create a camera-facing flat banner element in HOME-menu view space."""
    cx, cy, cz = center
    width, height = size
    x0, x1 = cx - width / 2.0, cx + width / 2.0
    y0, y1 = cy - height / 2.0, cy + height / 2.0
    verts = (
        (x0, y0, cz),
        (x1, y0, cz),
        (x1, y1, cz),
        (x0, y1, cz),
    )
    if texture == "texture_banner_sm64_logo":
        # ObjTriangle UVs follow SM64's fixed-point convention and are later
        # converted as (s / 32) / texture_width.  The title logo is a synthetic
        # screen quad, so feed full-texture fixed-point coordinates here instead
        # of normalized 0..1 UVs.
        tex_w, tex_h = 256.0, 128.0
        # The banner source image is stored with the opposite V convention from
        # the generated glTF preview path.  Flip V so the readable side is not
        # upside-down; see logo_uv_contact.png for the checked variants.
        uvs = ((0.0, 32.0 * tex_h), (32.0 * tex_w, 32.0 * tex_h), (32.0 * tex_w, 0.0), (0.0, 0.0))
    else:
        uvs = ((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))
    return [
        wf.ObjTriangle((verts[0], verts[1], verts[2]), (uvs[0], uvs[1], uvs[2]), (0.0, 0.0, 1.0), texture, rgba, name),
        wf.ObjTriangle((verts[0], verts[2], verts[3]), (uvs[0], uvs[2], uvs[3]), (0.0, 0.0, 1.0), texture, rgba, name),
    ]


def banner_layout_triangles() -> list[wf.ObjTriangle]:
    """Build the screen-facing title logo plane."""
    layout: list[wf.ObjTriangle] = []
    layout += make_screen_quad(
        name="name",
        center=(0.0, 0.55, 12.0),
        size=(15.0, 7.5),
        rgba=(255, 255, 255, 255),
        texture="texture_banner_sm64_logo",
    )
    return layout


def parent_node_for_source(source: str) -> int:
    return 0


def is_logo_triangle(tri: wf.ObjTriangle) -> bool:
    return tri.source == "name"


def keep_banner_detail_triangle(tri: wf.ObjTriangle) -> bool:
    # Preserve full topology for terrain and solid props.  Only omit high-cost
    # tiny/banner-fragile decorations; never randomly decimate triangles.
    return True


def is_mask_texture(texture: str | None) -> bool:
    return texture in {
        "texture_banner_sm64_logo",
    } or (
        texture is not None
        and texture.startswith("banner_glow_")
        and "coin_seg3_texture" in texture
    )


def is_mario_head_alpha_texture(texture: str | None) -> bool:
    return texture in {
        "mario_texture_m_logo",
        "mario_texture_eyes_front",
        "mario_texture_hair_sideburn",
        "mario_texture_mustache",
    }


MARIO_HEAD_ALPHA_BACKGROUNDS = {
    "mario_texture_m_logo": (255, 0, 0, 255),
    "mario_texture_eyes_front": (254, 193, 121, 255),
    "mario_texture_hair_sideburn": (254, 193, 121, 255),
    "mario_texture_mustache": (254, 193, 121, 255),
}

MARIO_HEAD_ALPHA_OUTPUT_NAMES = {
    "mario_texture_m_logo": "mario_texture_m_logo_bgcap",
    "mario_texture_eyes_front": "mario_texture_eyes_front_bgface",
    "mario_texture_hair_sideburn": "mario_texture_hair_sideburn_bgface",
    "mario_texture_mustache": "mario_texture_mustache_bgface",
}


def make_cgfx_texture_copy(texture: str, source: Path) -> str:
    tex_dir = OUT_DIR / "textures_cgfx"
    tex_dir.mkdir(exist_ok=True)
    output_texture_name = MARIO_HEAD_ALPHA_OUTPUT_NAMES.get(texture, texture)
    if texture == "texture_banner_sm64_logo":
        output_texture_name = f"{texture}_{hashlib.sha1(source.read_bytes()).hexdigest()[:8]}"
    dst_name = f"textures_cgfx/{output_texture_name}.png"
    dst = OUT_DIR / dst_name
    image = Image.open(source).convert("RGBA")
    if texture == "texture_banner_sm64_logo":
        max_dim = 256
    elif is_mario_head_alpha_texture(texture):
        max_dim = 32
    else:
        max_dim = 16
    if max(image.size) > max_dim:
        scale = max_dim / max(image.size)
        new_size = (max(1, round(image.width * scale)), max(1, round(image.height * scale)))
        image = image.resize(new_size, Image.Resampling.NEAREST)
    if texture in MARIO_HEAD_ALPHA_BACKGROUNDS and image.getchannel("A").getextrema() != (255, 255):
        opaque = Image.new("RGBA", image.size, MARIO_HEAD_ALPHA_BACKGROUNDS[texture])
        opaque.alpha_composite(image)
        image = opaque
    # Keep the title logo alpha like official banners, but keep other formerly
    # translucent materials flattened until each one is tested separately.
    if not is_mask_texture(texture) and image.getchannel("A").getextrema() != (255, 255):
        opaque = Image.new("RGBA", image.size, (255, 255, 255, 255))
        opaque.alpha_composite(image)
        image = opaque
    image.save(dst)
    return dst_name


def main() -> None:
    # Rebuild OBJ/texture preview side effects first so the glTF uses the latest
    # texture copies and the user can still inspect the same OBJ in Xcode.
    symbols = wf.texture_symbols(wf.ROOT)
    symbols["texture_banner_sm64_logo"] = ROOT / "3ds/banner.png"
    target_width = float(os.environ.get("WF_BANNER_TARGET_WIDTH", "30.0"))
    y_offset = float(os.environ.get("WF_BANNER_Y_OFFSET", "-9.75"))
    triangles = wf.normalized_triangles(wf.collect_triangles(wf.ROOT), target_width)
    triangles = [tri for tri in triangles if keep_banner_detail_triangle(tri)]
    if os.environ.get("WF_BANNER_SAMPLE_STRUCTURE", "0") != "0":
        triangles = [
            tri for tri in triangles
            if tri.source in {"area", "special_level_geo_03", "special_level_geo_04", "special_level_geo_05", "special_level_geo_06", "special_level_geo_07", "special_level_geo_08", "special_level_geo_09", "special_level_geo_0A", "special_level_geo_0B", "special_level_geo_0C", "special_level_geo_0D", "special_level_geo_0E"}
        ]
    if y_offset:
        triangles = [
            wf.ObjTriangle(
                tuple((x, y + y_offset, z) for x, y, z in tri.verts),
                tri.uvs,
                wf.normal_of(tuple((x, y + y_offset, z) for x, y, z in tri.verts)),
                tri.texture,
                tri.rgba,
                tri.source,
            )
            for tri in triangles
        ]
    crash_test_max = int(os.environ.get("WF_BANNER_MAX_TRIS", "0") or "0")
    triangles = decimate_for_crash_test(triangles, crash_test_max)
    if os.environ.get("WF_BANNER_LAYOUT_GUIDES", "1") != "0":
        triangles = triangles + banner_layout_triangles()
    wf.write_obj(triangles, symbols)
    wf.write_preview(triangles, symbols)

    blob = bytearray()
    buffer_views: list[dict] = []
    accessors: list[dict] = []
    meshes: list[dict] = []
    materials: list[dict] = []
    images: list[dict] = []
    textures: list[dict] = []
    nodes: list[dict] = [{"name": "COMMON", "children": []}]

    material_map: dict[tuple[str | None, tuple[int, int, int, int]], int] = {}
    image_map: dict[str, int] = {}

    def add_view(data: bytes, target: int | None = None) -> int:
        offset = len(blob)
        blob.extend(data)
        pad4(blob)
        view = {"buffer": 0, "byteOffset": offset, "byteLength": len(data)}
        if target is not None:
            view["target"] = target
        buffer_views.append(view)
        return len(buffer_views) - 1

    def add_accessor(view: int, component_type: int, count: int, typ: str, minv=None, maxv=None) -> int:
        accessor = {"bufferView": view, "componentType": component_type, "count": count, "type": typ}
        if minv is not None:
            accessor["min"] = list(minv)
        if maxv is not None:
            accessor["max"] = list(maxv)
        accessors.append(accessor)
        return len(accessors) - 1

    def add_material(texture: str | None, rgba: tuple[int, int, int, int], source_hint: str) -> int:
        key = (texture, (0, 0, 0, 255) if texture else rgba)
        if key in material_map:
            return material_map[key]
        r, g, b, a = rgba
        mat: dict = {
            "name": wf.material_name(texture, rgba, source_hint),
            "doubleSided": (
                source_hint.startswith("banner_")
                or texture == "texture_banner_sm64_logo"
                or is_mario_head_alpha_texture(texture)
                or (texture is not None and "coin_seg3_texture" in texture)
            ),
            "pbrMetallicRoughness": {
                "baseColorFactor": [1.0, 1.0, 1.0, 1.0],
                "roughnessFactor": 0.72,
                "metallicFactor": 0.0,
            },
        }
        if texture and texture in symbols:
            if texture not in image_map:
                dst_name = make_cgfx_texture_copy(texture, symbols[texture])
                images.append({"uri": dst_name})
                image_map[texture] = len(images) - 1
                textures.append({"source": image_map[texture], "sampler": 0})
            mat["pbrMetallicRoughness"]["baseColorTexture"] = {"index": image_map[texture]}
            mat["pbrMetallicRoughness"]["baseColorFactor"] = [1.0, 1.0, 1.0, 1.0]
        if is_mask_texture(texture):
            mat["alphaMode"] = "MASK"
            mat["alphaCutoff"] = 0.5
        material_map[key] = len(materials)
        materials.append(mat)
        return material_map[key]

    grouped: dict[tuple[str, int], list[wf.ObjTriangle]] = {}
    for tri in triangles:
        mat_id = add_material(tri.texture, tri.rgba, tri.source)
        mesh_group = "name" if is_logo_triangle(tri) else "wf_stage"
        grouped.setdefault((mesh_group, mat_id), []).append(tri)

    mesh_primitives: dict[str, list[dict]] = {"wf_stage": [], "name": []}

    for (mesh_group, mat_id), tris in grouped.items():
        include_vertex_color = tris[0].texture is None
        positions: list[tuple[float, float, float]] = []
        normals: list[tuple[float, float, float]] = []
        uvs: list[tuple[float, float]] = []
        colors: list[tuple[float, float, float, float]] = []
        indices: list[int] = []
        vertex_map: dict[tuple, int] = {}
        for tri in tris:
            tex_w, tex_h = texture_size(tri.texture, symbols)
            divisor = wf.uv_fixed_divisor(tri.texture)
            for i, pos in enumerate(tri.verts):
                s, t = tri.uvs[i]
                uv = ((s / divisor) / tex_w, (t / divisor) / tex_h)
                color = (tri.rgba[0] / 255.0, tri.rgba[1] / 255.0, tri.rgba[2] / 255.0, 1.0)
                key = (
                    tuple(round(v, 6) for v in pos),
                    tuple(round(v, 6) for v in tri.normal),
                    tuple(round(v, 6) for v in uv),
                    tuple(round(v, 6) for v in color) if include_vertex_color else (),
                )
                index = vertex_map.get(key)
                if index is None:
                    index = len(positions)
                    vertex_map[key] = index
                    positions.append(pos)
                    normals.append(tri.normal)
                    uvs.append(uv)
                    if include_vertex_color:
                        colors.append(color)
                indices.append(index)

        pos_data = b"".join(struct.pack("<3f", *p) for p in positions)
        nrm_data = b"".join(struct.pack("<3f", *n) for n in normals)
        uv_data = b"".join(struct.pack("<2f", *uv) for uv in uvs)
        idx_data = b"".join(struct.pack("<H", i) for i in indices)

        pv = add_view(pos_data, 34962)
        nv = add_view(nrm_data, 34962)
        tv = add_view(uv_data, 34962)
        iv = add_view(idx_data, 34963)
        pa = add_accessor(
            pv,
            5126,
            len(positions),
            "VEC3",
            [min(p[i] for p in positions) for i in range(3)],
            [max(p[i] for p in positions) for i in range(3)],
        )
        na = add_accessor(nv, 5126, len(normals), "VEC3")
        ta = add_accessor(tv, 5126, len(uvs), "VEC2")
        ia = add_accessor(iv, 5123, len(indices), "SCALAR")
        attributes = {"POSITION": pa, "NORMAL": na, "TEXCOORD_0": ta}
        if include_vertex_color:
            color_data = b"".join(struct.pack("<4f", *color) for color in colors)
            cv = add_view(color_data, 34962)
            ca = add_accessor(cv, 5126, len(colors), "VEC4")
            attributes["COLOR_0"] = ca

        mesh_primitives[mesh_group].append({
            "attributes": attributes,
            "indices": ia,
            "material": mat_id,
            "mode": 4,
        })

    for node_name in ("wf_stage", "name"):
        primitives = mesh_primitives[node_name]
        if not primitives:
            continue
        mesh_id = len(meshes)
        meshes.append({"name": node_name, "primitives": primitives})
        nodes.append({"name": node_name, "mesh": mesh_id})
        nodes[0]["children"].append(len(nodes) - 1)

    camera_position = (0.0, 1.0, 44.786)
    camera_node = len(nodes)
    nodes.append({
        "name": "Banner Camera",
        "camera": 0,
        "translation": list(camera_position),
    })

    gltf = {
        "asset": {"version": "2.0", "generator": "make_wf_stage_gltf.py"},
        "scene": 0,
        "scenes": [{"nodes": [0, camera_node]}],
        "nodes": nodes,
        "cameras": [{
            "name": "Banner Camera",
            "type": "perspective",
            "perspective": {
                "aspectRatio": 1.66666666667,
                "yfov": 0.523599,
                "znear": 26.5,
                "zfar": 1000.0,
            },
        }],
        "meshes": meshes,
        "materials": materials,
        "images": images,
        "textures": textures,
        "samplers": [{"magFilter": 9728, "minFilter": 9728, "wrapS": 10497, "wrapT": 10497}],
        "buffers": [{"uri": OUT_BASE.with_suffix(".bin").name, "byteLength": len(blob)}],
        "bufferViews": buffer_views,
        "accessors": accessors,
    }
    OUT_BASE.with_suffix(".bin").write_bytes(blob)
    OUT_BASE.with_suffix(".gltf").write_text(json.dumps(gltf, indent=2) + "\n")
    print(f"wrote {OUT_BASE.with_suffix('.gltf')}")
    print(f"triangles: {len(triangles)}")
    print(f"materials: {len(materials)}")
    print(f"target_width: {target_width}, y_offset: {y_offset}")
    print(f"camera: position={camera_position}, star-test defaults, transparent materials disabled")


if __name__ == "__main__":
    main()
