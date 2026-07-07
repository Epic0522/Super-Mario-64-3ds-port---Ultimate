#!/usr/bin/env python3
"""Create a first-pass SM64 BOB-inspired 3D banner stage glTF.

This is a deliberately self-contained blockout/prototype asset:
it does not copy the BOB level mesh wholesale because that mesh is not
360-degree-safe for a HOME Menu banner. Instead it builds a sealed miniature
stage from BOB-themed elements: grass, paths, hills, cannon, boxes, Bob-omb,
Goomba, and a custom non-game-low-poly Mario placeholder with a looped run path.
"""

from __future__ import annotations

import json
import math
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "3ds/banner_3d/bob_stage"
OUT_BASE = OUT_DIR / "bob_stage"


def v_add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def v_sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def v_scale(a, s): return (a[0] * s, a[1] * s, a[2] * s)
def v_cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )
def v_norm(a):
    l = math.sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]) or 1.0
    return (a[0] / l, a[1] / l, a[2] / l)


@dataclass
class Material:
    name: str
    color: tuple[float, float, float, float]
    roughness: float = 0.65
    metallic: float = 0.0
    double_sided: bool = True


class MeshBuilder:
    def __init__(self):
        self.positions: list[tuple[float, float, float]] = []
        self.normals: list[tuple[float, float, float]] = []
        self.indices: list[int] = []

    def tri(self, a, b, c):
        n = v_norm(v_cross(v_sub(b, a), v_sub(c, a)))
        base = len(self.positions)
        self.positions.extend([a, b, c])
        self.normals.extend([n, n, n])
        self.indices.extend([base, base + 1, base + 2])

    def quad(self, a, b, c, d):
        self.tri(a, b, c)
        self.tri(a, c, d)


class Scene:
    def __init__(self):
        self.materials: list[Material] = []
        self.mat_index: dict[str, int] = {}
        self.meshes: list[tuple[str, MeshBuilder, int]] = []
        self.nodes: list[dict] = []
        self.animations: list[dict] = []
        self.extra_buffers: list[bytes] = []

    def mat(self, name, color, **kwargs) -> int:
        if name in self.mat_index:
            return self.mat_index[name]
        idx = len(self.materials)
        self.materials.append(Material(name, color, **kwargs))
        self.mat_index[name] = idx
        return idx

    def add_mesh_node(self, name: str, mesh: MeshBuilder, mat_name: str, translation=(0, 0, 0), rotation=None, scale=(1, 1, 1)) -> int:
        mesh_idx = len(self.meshes)
        mat_idx = self.mat_index[mat_name]
        self.meshes.append((name + "Mesh", mesh, mat_idx))
        node = {"name": name, "mesh": mesh_idx}
        if translation != (0, 0, 0):
            node["translation"] = list(translation)
        if rotation is not None:
            node["rotation"] = list(rotation)
        if scale != (1, 1, 1):
            node["scale"] = list(scale)
        self.nodes.append(node)
        return len(self.nodes) - 1


def box(sx, sy, sz) -> MeshBuilder:
    x, y, z = sx / 2, sy / 2, sz / 2
    m = MeshBuilder()
    p = {
        "lbf": (-x, -y, z), "rbf": (x, -y, z), "rtf": (x, y, z), "ltf": (-x, y, z),
        "lbb": (-x, -y, -z), "rbb": (x, -y, -z), "rtb": (x, y, -z), "ltb": (-x, y, -z),
    }
    m.quad(p["lbf"], p["rbf"], p["rtf"], p["ltf"])
    m.quad(p["rbb"], p["lbb"], p["ltb"], p["rtb"])
    m.quad(p["lbb"], p["lbf"], p["ltf"], p["ltb"])
    m.quad(p["rbf"], p["rbb"], p["rtb"], p["rtf"])
    m.quad(p["ltf"], p["rtf"], p["rtb"], p["ltb"])
    m.quad(p["lbb"], p["rbb"], p["rbf"], p["lbf"])
    return m


def cylinder(radius=1.0, height=1.0, segments=48, cap_top=True, cap_bottom=True) -> MeshBuilder:
    m = MeshBuilder()
    y0, y1 = -height / 2, height / 2
    for i in range(segments):
        a0 = 2 * math.pi * i / segments
        a1 = 2 * math.pi * (i + 1) / segments
        p0 = (math.cos(a0) * radius, y0, math.sin(a0) * radius)
        p1 = (math.cos(a1) * radius, y0, math.sin(a1) * radius)
        p2 = (math.cos(a1) * radius, y1, math.sin(a1) * radius)
        p3 = (math.cos(a0) * radius, y1, math.sin(a0) * radius)
        m.quad(p0, p1, p2, p3)
        if cap_top:
            m.tri((0, y1, 0), p3, p2)
        if cap_bottom:
            m.tri((0, y0, 0), p1, p0)
    return m


def sphere(radius=1.0, rings=10, segments=20, squash=(1, 1, 1)) -> MeshBuilder:
    m = MeshBuilder()
    def pt(r, s):
        phi = math.pi * r / rings
        theta = 2 * math.pi * s / segments
        return (
            math.sin(phi) * math.cos(theta) * radius * squash[0],
            math.cos(phi) * radius * squash[1],
            math.sin(phi) * math.sin(theta) * radius * squash[2],
        )
    for r in range(rings):
        for s in range(segments):
            m.quad(pt(r, s), pt(r, s + 1), pt(r + 1, s + 1), pt(r + 1, s))
    return m


def cone(radius=1.0, height=1.0, segments=32) -> MeshBuilder:
    m = MeshBuilder()
    y0, y1 = -height / 2, height / 2
    tip = (0, y1, 0)
    for i in range(segments):
        a0 = 2 * math.pi * i / segments
        a1 = 2 * math.pi * (i + 1) / segments
        p0 = (math.cos(a0) * radius, y0, math.sin(a0) * radius)
        p1 = (math.cos(a1) * radius, y0, math.sin(a1) * radius)
        m.tri(p0, p1, tip)
        m.tri((0, y0, 0), p1, p0)
    return m


def low_hill(radius=1.0, height=0.8, segments=32) -> MeshBuilder:
    m = MeshBuilder()
    top = (0, height, 0)
    for i in range(segments):
        a0 = 2 * math.pi * i / segments
        a1 = 2 * math.pi * (i + 1) / segments
        p0 = (math.cos(a0) * radius, 0, math.sin(a0) * radius)
        p1 = (math.cos(a1) * radius, 0, math.sin(a1) * radius)
        mid = (math.cos((a0+a1)/2) * radius * 0.45, height * 0.62, math.sin((a0+a1)/2) * radius * 0.45)
        m.tri(p0, p1, mid)
        m.tri(p0, mid, top)
        m.tri((0, 0, 0), p1, p0)
    return m


def path_patch(length=4.0, width=0.9, curve=0.55, steps=12) -> MeshBuilder:
    m = MeshBuilder()
    for i in range(steps):
        t0 = i / steps
        t1 = (i + 1) / steps
        def edge(t, side):
            z = -length / 2 + t * length
            x = math.sin((t - 0.5) * math.pi) * curve + side * width * (0.45 + 0.08 * math.sin(t * math.pi))
            return (x, 0.012, z)
        m.quad(edge(t0, -1), edge(t0, 1), edge(t1, 1), edge(t1, -1))
    return m


def question_box() -> MeshBuilder:
    return box(0.55, 0.55, 0.55)


def build_scene() -> tuple[Scene, int]:
    s = Scene()
    for name, color in [
        ("grass", (0.48, 0.92, 0.30, 1)),
        ("grass_dark", (0.17, 0.58, 0.19, 1)),
        ("dirt", (0.57, 0.33, 0.14, 1)),
        ("road", (0.78, 0.62, 0.36, 1)),
        ("stone", (0.55, 0.54, 0.50, 1)),
        ("cannon", (0.08, 0.08, 0.09, 1)),
        ("box", (0.62, 0.36, 0.16, 1)),
        ("qbox", (1.0, 0.76, 0.12, 1)),
        ("bobomb_black", (0.02, 0.025, 0.04, 1)),
        ("bobomb_blue", (0.07, 0.16, 0.68, 1)),
        ("goomba", (0.57, 0.29, 0.11, 1)),
        ("goomba_face", (0.86, 0.68, 0.46, 1)),
        ("white", (0.96, 0.94, 0.85, 1)),
        ("red", (0.92, 0.04, 0.03, 1)),
        ("blue", (0.05, 0.12, 0.72, 1)),
        ("skin", (1.0, 0.71, 0.50, 1)),
        ("brown", (0.36, 0.16, 0.07, 1)),
        ("castle", (0.82, 0.70, 0.58, 1)),
        ("roof", (0.63, 0.16, 0.14, 1)),
        ("tree", (0.09, 0.50, 0.15, 1)),
    ]:
        s.mat(name, color)

    stage_root = {"name": "COMMON", "children": []}
    s.nodes.append(stage_root)
    common_id = 0

    def add_child(node_id):
        s.nodes[common_id].setdefault("children", []).append(node_id)

    # Rotating circular stage area.
    add_child(s.add_mesh_node("grass_round_stage", cylinder(4.0, 0.32, 72), "grass", translation=(0, 0, 0)))
    add_child(s.add_mesh_node("dirt_side_band", cylinder(4.02, 0.46, 48, cap_top=False, cap_bottom=False), "dirt", translation=(0, -0.08, 0)))
    add_child(s.add_mesh_node("bob_birth_path", path_patch(6.0, 1.0, 0.35, 18), "road", translation=(0, 0.18, -0.25)))

    # Raised stone platform inspired by BOB's blocky ledges.
    add_child(s.add_mesh_node("raised_stone_platform", box(1.25, 0.35, 1.05), "stone", translation=(-1.8, 0.46, 0.35), rotation=(0, math.sin(0.18), 0, math.cos(0.18))))

    # BOB-style hills and background castle silhouette, made 3D and sealed.
    add_child(s.add_mesh_node("rear_hill_left", low_hill(1.25, 0.95, 28), "grass_dark", translation=(-2.4, 0.18, 2.05), scale=(1.25, 1.0, 0.8)))
    add_child(s.add_mesh_node("rear_hill_right", low_hill(1.0, 0.78, 28), "grass_dark", translation=(2.35, 0.18, 2.25), scale=(1.05, 1.0, 0.7)))
    add_child(s.add_mesh_node("castle_body", box(1.75, 1.25, 0.78), "castle", translation=(0.0, 1.02, 2.72)))
    add_child(s.add_mesh_node("castle_tower_l", box(0.52, 1.75, 0.58), "castle", translation=(-1.08, 1.22, 2.72)))
    add_child(s.add_mesh_node("castle_tower_r", box(0.52, 1.75, 0.58), "castle", translation=(1.08, 1.22, 2.72)))
    add_child(s.add_mesh_node("castle_roof_l", cone(0.39, 0.55, 4), "roof", translation=(-1.08, 2.36, 2.72), rotation=(0, math.sin(math.pi/4), 0, math.cos(math.pi/4))))
    add_child(s.add_mesh_node("castle_roof_r", cone(0.39, 0.55, 4), "roof", translation=(1.08, 2.36, 2.72), rotation=(0, math.sin(math.pi/4), 0, math.cos(math.pi/4))))
    add_child(s.add_mesh_node("castle_roof_mid", box(1.85, 0.25, 0.78), "roof", translation=(0, 1.78, 2.72)))

    # Static BOB props.
    add_child(s.add_mesh_node("cannon_base", cylinder(0.32, 0.35, 24), "cannon", translation=(2.25, 0.44, -0.55)))
    add_child(s.add_mesh_node("cannon_barrel", cylinder(0.18, 0.95, 20), "cannon", translation=(2.25, 0.86, -0.55), rotation=(math.sin(math.pi/4), 0, 0, math.cos(math.pi/4)), scale=(1, 1, 1)))
    add_child(s.add_mesh_node("breakable_box", box(0.55, 0.55, 0.55), "box", translation=(1.15, 0.62, 1.05), rotation=(0, math.sin(0.35), 0, math.cos(0.35))))
    add_child(s.add_mesh_node("question_box", question_box(), "qbox", translation=(-0.65, 1.15, -1.7), rotation=(0, math.sin(-0.2), 0, math.cos(-0.2))))

    # Bob-omb: body, fuse, feet.
    add_child(s.add_mesh_node("bobomb_body", sphere(0.34, 9, 18), "bobomb_black", translation=(-2.55, 0.72, -0.95)))
    add_child(s.add_mesh_node("bobomb_fuse", cylinder(0.035, 0.42, 10), "brown", translation=(-2.55, 1.12, -0.95)))
    add_child(s.add_mesh_node("bobomb_key", box(0.38, 0.10, 0.12), "bobomb_blue", translation=(-2.15, 0.92, -0.95)))
    add_child(s.add_mesh_node("bobomb_foot_l", sphere(0.11, 6, 12, squash=(1.5, 0.45, 0.9)), "white", translation=(-2.78, 0.36, -0.78)))
    add_child(s.add_mesh_node("bobomb_foot_r", sphere(0.11, 6, 12, squash=(1.5, 0.45, 0.9)), "white", translation=(-2.34, 0.36, -0.78)))

    # Goomba.
    add_child(s.add_mesh_node("goomba_body", sphere(0.32, 9, 18, squash=(1.05, 0.75, 0.95)), "goomba", translation=(2.25, 0.67, 1.05)))
    add_child(s.add_mesh_node("goomba_face_patch", box(0.38, 0.28, 0.03), "goomba_face", translation=(2.25, 0.70, 0.72)))
    add_child(s.add_mesh_node("goomba_foot_l", sphere(0.15, 6, 12, squash=(1.45, 0.35, 0.8)), "brown", translation=(2.02, 0.34, 0.9)))
    add_child(s.add_mesh_node("goomba_foot_r", sphere(0.15, 6, 12, squash=(1.45, 0.35, 0.8)), "brown", translation=(2.48, 0.34, 0.9)))

    # A tiny tree pair for 360-degree edge dressing.
    for x, z, sc in [(-3.15, 0.9, 0.8), (3.15, 0.05, 0.75)]:
        add_child(s.add_mesh_node(f"tree_trunk_{x}", cylinder(0.08, 0.65, 10), "brown", translation=(x, 0.6, z)))
        add_child(s.add_mesh_node(f"tree_top_{x}", sphere(0.38 * sc, 7, 14, squash=(1, 1.15, 1)), "tree", translation=(x, 1.08, z)))

    # Custom non-game-low-poly Mario placeholder, animated as a single runner node.
    runner_root = {"name": "Runner_Mario_Custom", "translation": [0.0, 0.35, -1.85], "children": []}
    s.nodes.append(runner_root)
    runner_id = len(s.nodes) - 1
    s.nodes[common_id].setdefault("children", []).append(runner_id)

    def add_runner_part(name, mesh, mat, translation, scale=(1,1,1), rotation=None):
        node_id = s.add_mesh_node(name, mesh, mat, translation=translation, scale=scale, rotation=rotation)
        s.nodes[runner_id].setdefault("children", []).append(node_id)

    add_runner_part("mario_body", box(0.28, 0.48, 0.20), "blue", (0, 0.55, 0))
    add_runner_part("mario_head", sphere(0.18, 7, 14), "skin", (0, 0.95, 0))
    add_runner_part("mario_cap", sphere(0.19, 5, 14, squash=(1.08, 0.45, 1.0)), "red", (0, 1.08, -0.02))
    add_runner_part("mario_arm_l", box(0.09, 0.35, 0.09), "red", (-0.22, 0.58, 0.02), rotation=(0, 0, math.sin(0.45), math.cos(0.45)))
    add_runner_part("mario_arm_r", box(0.09, 0.35, 0.09), "red", (0.22, 0.58, -0.02), rotation=(0, 0, math.sin(-0.45), math.cos(-0.45)))
    add_runner_part("mario_leg_l", box(0.10, 0.34, 0.10), "blue", (-0.09, 0.20, 0.03), rotation=(math.sin(0.30), 0, 0, math.cos(0.30)))
    add_runner_part("mario_leg_r", box(0.10, 0.34, 0.10), "blue", (0.09, 0.20, -0.03), rotation=(math.sin(-0.30), 0, 0, math.cos(-0.30)))

    add_mario_run_animation(s, runner_id)
    return s, common_id


def add_mario_run_animation(scene: Scene, runner_id: int):
    # Elliptical loop around the front/middle of the stage, deliberately small
    # so the HOME Menu's own COMMON rotation can still read as the main motion.
    times = [0, 0.8, 1.6, 2.4, 3.2, 4.0]
    pts = []
    rots = []
    for t in times:
        a = 2 * math.pi * t / times[-1]
        x = math.sin(a) * 1.05
        z = -1.15 + math.cos(a) * 0.55
        pts.append((x, 0.35, z))
        yaw = a + math.pi / 2
        rots.append((0, math.sin(yaw / 2), 0, math.cos(yaw / 2)))
    scene.animations.append({"node": runner_id, "times": times, "translations": pts, "rotations": rots})


def pad4(blob: bytearray):
    while len(blob) % 4:
        blob.append(0)


def write_gltf(scene: Scene):
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    blob = bytearray()
    buffer_views = []
    accessors = []
    meshes = []

    def add_view(data: bytes, target: int | None = None):
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
        if minv is not None: acc["min"] = list(minv)
        if maxv is not None: acc["max"] = list(maxv)
        accessors.append(acc)
        return len(accessors) - 1

    for mesh_name, mesh, mat_idx in scene.meshes:
        pos_data = b"".join(struct.pack("<3f", *p) for p in mesh.positions)
        nrm_data = b"".join(struct.pack("<3f", *n) for n in mesh.normals)
        idx_data = b"".join(struct.pack("<H", i) for i in mesh.indices)
        pv = add_view(pos_data, 34962)
        nv = add_view(nrm_data, 34962)
        iv = add_view(idx_data, 34963)
        pa = add_accessor(pv, 5126, len(mesh.positions), "VEC3",
                          [min(p[i] for p in mesh.positions) for i in range(3)],
                          [max(p[i] for p in mesh.positions) for i in range(3)])
        na = add_accessor(nv, 5126, len(mesh.normals), "VEC3")
        ia = add_accessor(iv, 5123, len(mesh.indices), "SCALAR")
        meshes.append({
            "name": mesh_name,
            "primitives": [{"attributes": {"POSITION": pa, "NORMAL": na}, "indices": ia, "material": mat_idx, "mode": 4}]
        })

    animations = []
    for anim in scene.animations:
        time_data = b"".join(struct.pack("<f", t) for t in anim["times"])
        trans_data = b"".join(struct.pack("<3f", *p) for p in anim["translations"])
        rot_data = b"".join(struct.pack("<4f", *r) for r in anim["rotations"])
        tv = add_view(time_data)
        trv = add_view(trans_data)
        rv = add_view(rot_data)
        ta = add_accessor(tv, 5126, len(anim["times"]), "SCALAR", [min(anim["times"])], [max(anim["times"])])
        tra = add_accessor(trv, 5126, len(anim["translations"]), "VEC3")
        ra = add_accessor(rv, 5126, len(anim["rotations"]), "VEC4")
        animations.append({
            "name": "MarioRunLoop",
            "samplers": [
                {"input": ta, "output": tra, "interpolation": "LINEAR"},
                {"input": ta, "output": ra, "interpolation": "LINEAR"},
            ],
            "channels": [
                {"sampler": 0, "target": {"node": anim["node"], "path": "translation"}},
                {"sampler": 1, "target": {"node": anim["node"], "path": "rotation"}},
            ],
        })

    gltf = {
        "asset": {"version": "2.0", "generator": "make_bob_stage_gltf.py"},
        "scene": 0,
        "scenes": [{"nodes": [0, len(scene.nodes)]}],
        "nodes": scene.nodes + [{
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
        "materials": [{
            "name": m.name,
            "doubleSided": m.double_sided,
            "pbrMetallicRoughness": {
                "baseColorFactor": list(m.color),
                "roughnessFactor": m.roughness,
                "metallicFactor": m.metallic,
            },
        } for m in scene.materials],
        "animations": animations,
        "buffers": [{"uri": OUT_BASE.with_suffix(".bin").name, "byteLength": len(blob)}],
        "bufferViews": buffer_views,
        "accessors": accessors,
    }
    OUT_BASE.with_suffix(".bin").write_bytes(blob)
    OUT_BASE.with_suffix(".gltf").write_text(json.dumps(gltf, indent=2) + "\n")
    print(f"Wrote {OUT_BASE.with_suffix('.gltf')} with {len(scene.meshes)} meshes, {sum(len(m.positions) for _,m,_ in scene.meshes)} vertices")


# Tiny software preview renderer: enough to judge composition without Blender.
def render_preview(scene: Scene):
    W, H = 1200, 720
    img = Image.new("RGB", (W, H), (226, 232, 238))
    draw = ImageDraw.Draw(img, "RGBA")
    cam = (6.8, 4.3, -8.2)
    target = (0, 0.8, 0.25)
    fwd = v_norm(v_sub(target, cam))
    right = v_norm(v_cross(fwd, (0, 1, 0)))
    up = v_cross(right, fwd)
    f = 670

    materials = {i: tuple(int(c * 255) for c in m.color) for i, m in enumerate(scene.materials)}

    world_nodes = []
    def walk(node_id, parent_t=(0,0,0)):
        n = scene.nodes[node_id]
        t = tuple(n.get("translation", (0,0,0)))
        wt = v_add(parent_t, t)
        if "mesh" in n:
            world_nodes.append((n["mesh"], wt))
        for c in n.get("children", []):
            walk(c, wt)
    walk(0)

    tris = []
    for mesh_idx, trans in world_nodes:
        _, mesh, mat_idx = scene.meshes[mesh_idx]
        col = materials[mat_idx]
        for i in range(0, len(mesh.indices), 3):
            pts = [v_add(mesh.positions[mesh.indices[i+j]], trans) for j in range(3)]
            center = tuple(sum(p[k] for p in pts)/3 for k in range(3))
            depth = sum((center[k]-cam[k])*fwd[k] for k in range(3))
            if depth <= 0.1:
                continue
            n = v_norm(v_cross(v_sub(pts[1], pts[0]), v_sub(pts[2], pts[0])))
            light = max(0.25, min(1.0, 0.55 + 0.45 * sum(n[k]*v_norm((-0.35,0.9,-0.25))[k] for k in range(3))))
            poly = []
            ok = True
            for p in pts:
                rel = v_sub(p, cam)
                x = sum(rel[k]*right[k] for k in range(3))
                y = sum(rel[k]*up[k] for k in range(3))
                z = sum(rel[k]*fwd[k] for k in range(3))
                if z <= 0.1:
                    ok = False
                    break
                poly.append((W/2 + f*x/z, H/2 - f*y/z))
            if ok:
                shaded = tuple(int(c * light) for c in col[:3]) + (255,)
                tris.append((depth, poly, shaded))
    for _, poly, col in sorted(tris, key=lambda x: x[0], reverse=True):
        draw.polygon(poly, fill=col, outline=(0,0,0,28))

    img.save(OUT_DIR / "bob_stage_preview.png")
    print(f"Wrote {OUT_DIR / 'bob_stage_preview.png'}")


def main():
    scene, _ = build_scene()
    write_gltf(scene)
    render_preview(scene)


if __name__ == "__main__":
    main()
