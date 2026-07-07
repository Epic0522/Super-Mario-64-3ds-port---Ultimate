#!/usr/bin/env python3
"""Export a Whomp's Fortress scene preview as OBJ for 3DS banner planning.

This is deliberately a preview/export tool, not a full runtime object renderer.
It composes the WF area geometry plus script-placed stage objects, chooses the
high-tower layout, and skips the ACT1 Whomp King boss.  The output OBJ is meant
for Xcode/Blender-style inspection before we squeeze the scene into a HOME menu
banner model.
"""

from __future__ import annotations

import argparse
import math
import re
import shutil
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import minimap_extract_textured as mm  # noqa: E402


OUT_DIR = ROOT / "3ds/banner_3d/wf_stage"
OUT_BASE = OUT_DIR / "wf_stage"


EXCLUDED_MODELS = {
    "MODEL_NONE",
    "MODEL_STAR",
    "MODEL_1UP",
    "MODEL_BUTTERFLY",
    "MODEL_BOBOMB_BUDDY",
    "MODEL_HOOT",
    "MODEL_BULLET_BILL",  # keep the cannon model, not a moving projectile
}

EXCLUDED_BEHAVIORS = {
    "bhvWhompKingBoss",
    "bhvStar",
    "bhvHiddenStar",
    "bhvHiddenRedCoinStar",
    "bhv1Up",
    "bhvButterfly",
    "bhvBobombBuddyOpensCannon",
    "bhvHoot",
    "bhvBulletBill",
}

PROBLEM_ANIMATED_MONSTERS: set[str] = set()

ACT1_ONLY = "ACT_1"

GLOBAL_GEO_HINTS = {
    "MODEL_THWOMP": "thwomp_geo",
    "MODEL_PIRANHA_PLANT": "piranha_plant_geo",
    "MODEL_WHOMP": "whomp_geo",
    "MODEL_WF_BUBBLY_TREE": "bubbly_tree_geo",
}


@dataclass(frozen=True)
class ObjTriangle:
    verts: tuple[tuple[float, float, float], tuple[float, float, float], tuple[float, float, float]]
    uvs: tuple[tuple[float, float], tuple[float, float], tuple[float, float]]
    normal: tuple[float, float, float]
    texture: str | None
    rgba: tuple[int, int, int, int]
    source: str


@dataclass(frozen=True)
class GeoTransform:
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0
    yaw_degrees: float = 0.0
    scale: float = 1.0


@dataclass
class ObjRenderState:
    texture: str | None = None
    rgba: tuple[int, int, int, int] | None = None
    transform: GeoTransform = GeoTransform()
    vertex_cache: dict[int, mm.Vertex] | None = None

    def __post_init__(self) -> None:
        if self.vertex_cache is None:
            self.vertex_cache = {}


@dataclass(frozen=True)
class MatrixTransform:
    matrix: tuple[tuple[float, float, float, float], ...]


@dataclass
class AnimationState:
    values: list[int]
    indices: list[int]
    frame: int
    anim_type: int
    pointer: int = 0


ANIM_FLAG_HOR_TRANS = 1 << 3
ANIM_FLAG_VERT_TRANS = 1 << 4
ANIM_FLAG_6 = 1 << 6
ANIM_TYPE_TRANSLATION = 1
ANIM_TYPE_VERTICAL_TRANSLATION = 2
ANIM_TYPE_LATERAL_TRANSLATION = 3
ANIM_TYPE_NO_TRANSLATION = 4
ANIM_TYPE_ROTATION = 5

ANIM_POSES = {
    "whomp_geo": (ROOT / "actors/whomp/anims/anim_060209EC.inc.c", 0),
    "piranha_plant_geo": (ROOT / "actors/piranha_plant/anims/anim_06017C38.inc.c", 0),
}

WF_CANNON_POS = (-1844.0, 1026.0, 3893.0)
WF_SIGNPOSTS = [
    (4200.0, 256.0, 5160.0, 315.0),
    (-2540.0, 2560.0, -900.0, 0.0),
    (1600.0, 2560.0, 2600.0, 90.0),
    (-2705.0, 2560.0, 59.0, 270.0),
    (3460.0, 2304.0, -40.0, 180.0),
    (-2932.0, 386.0, -157.0, 270.0),
    (4800.0, 256.0, 3000.0, 0.0),
    (2930.0, 1075.0, -3740.0, 90.0),
]
WF_SMALL_BOXES = [
    (4320.0, 256.0, 1880.0, 0.0),
    (-2940.0, 384.0, -1320.0, 0.0),
]
WF_BLUE_COIN_SWITCHES = [
    (-2500.0, 384.0, -250.0, 0.0),
]
WF_YELLOW_COINS = [
    (0.0, 2650.0, 2900.0, 0.0),
    (-500.0, 2650.0, 2900.0, 0.0),
    (250.0, 2650.0, 2800.0, 0.0),
    (-750.0, 2650.0, 2800.0, 0.0),
]
WF_COIN_LINES = [
    (3760.0, 960.0, 2740.0, 0.0),
    (-1400.0, 1160.0, 3900.0, 90.0),
    (1254.0, 2586.0, 2299.0, 90.0),
    (3396.0, 1380.0, 3280.0, 0.0),
]
WF_COIN_RINGS = [
    (-2500.0, 1795.0, -260.0, 0.0, False),
    (4611.0, 256.0, 141.0, 0.0, False),
    (1558.0, 922.0, 2329.0, 0.0, False),
    (3234.0, 3345.0, -1787.0, 0.0, True),
]
WF_COIN_ARROW = (1215.0, 3600.0, -2609.0, 135.0)
WF_BLUE_COINS = [
    (-2500.0, 450.0, -1150.0, 0.0),
    (-2500.0, 450.0, -900.0, 0.0),
    (-2500.0, 450.0, -650.0, 0.0),
    (-2500.0, 450.0, -1400.0, 0.0),
]
WF_RED_COINS = [
    (-250.0, 2650.0, 2970.0, 0.0),
    (1746.0, 3620.0, -3120.0, 0.0),
    (1277.0, 2600.0, 1350.0, 0.0),
    (1585.0, 2595.0, -80.0, 0.0),
    (3350.0, 3000.0, -1520.0, 0.0),
    (2700.0, 3600.0, -900.0, 0.0),
    (3770.0, 1380.0, 650.0, 0.0),
    (-270.0, 1720.0, 2250.0, 0.0),
]
WF_WATER_BOX = (-1023.0, 1024.0, 3226.0, 4096.0, 973.0)
WF_BANNER_MARIO_POS = (2600.0, 343.0, 5120.0)
WF_BANNER_MARIO_YAW = 90.0
WF_BANNER_MARIO_SCALE = 0.65

LIGHT_RGBA_BY_SYMBOL = {
    "mario_blue_lights_group": (0, 0, 255, 255),
    "mario_red_lights_group": (255, 0, 0, 255),
    "mario_white_lights_group": (255, 255, 255, 255),
    "mario_brown1_lights_group": (114, 28, 14, 255),
    "mario_beige_lights_group": (254, 193, 121, 255),
    "mario_brown2_lights_group": (115, 6, 0, 255),
}

MARIO_INHERITED_RGBA_BY_DL = {
    # These display lists rely on the graph/display-list state set by their
    # parent in the live engine.  The offline baker renders each collected
    # Mario display list independently, so seed the inherited light color.
    "mario_torso": LIGHT_RGBA_BY_SYMBOL["mario_blue_lights_group"],
    "mario_left_forearm_shared_dl": LIGHT_RGBA_BY_SYMBOL["mario_red_lights_group"],
    "mario_right_forearm_shared_dl": LIGHT_RGBA_BY_SYMBOL["mario_red_lights_group"],
    "mario_left_leg_shared_dl": LIGHT_RGBA_BY_SYMBOL["mario_blue_lights_group"],
    "mario_right_leg_shared_dl": LIGHT_RGBA_BY_SYMBOL["mario_blue_lights_group"],
}


def normal_of(points: tuple[tuple[float, float, float], tuple[float, float, float], tuple[float, float, float]]) -> tuple[float, float, float]:
    a, b, c = points
    ux, uy, uz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    vx, vy, vz = c[0] - a[0], c[1] - a[1], c[2] - a[2]
    nx = uy * vz - uz * vy
    ny = uz * vx - ux * vz
    nz = ux * vy - uy * vx
    length = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
    return nx / length, ny / length, nz / length


def mat_identity() -> tuple[tuple[float, float, float, float], ...]:
    return ((1.0, 0.0, 0.0, 0.0), (0.0, 1.0, 0.0, 0.0), (0.0, 0.0, 1.0, 0.0), (0.0, 0.0, 0.0, 1.0))


def mat_mul(a: tuple[tuple[float, float, float, float], ...], b: tuple[tuple[float, float, float, float], ...]) -> tuple[tuple[float, float, float, float], ...]:
    return tuple(tuple(sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)) for i in range(4))


def mat_translate(x: float, y: float, z: float) -> tuple[tuple[float, float, float, float], ...]:
    return ((1.0, 0.0, 0.0, x), (0.0, 1.0, 0.0, y), (0.0, 0.0, 1.0, z), (0.0, 0.0, 0.0, 1.0))


def mat_scale(scale: float) -> tuple[tuple[float, float, float, float], ...]:
    return ((scale, 0.0, 0.0, 0.0), (0.0, scale, 0.0, 0.0), (0.0, 0.0, scale, 0.0), (0.0, 0.0, 0.0, 1.0))


def mat_scale_xyz(x: float, y: float, z: float) -> tuple[tuple[float, float, float, float], ...]:
    return ((x, 0.0, 0.0, 0.0), (0.0, y, 0.0, 0.0), (0.0, 0.0, z, 0.0), (0.0, 0.0, 0.0, 1.0))


def sm64_angle(value: int) -> float:
    return (value if value < 0x8000 else value - 0x10000) * math.tau / 65536.0


def mat_rotate_x(angle: float) -> tuple[tuple[float, float, float, float], ...]:
    c, s = math.cos(angle), math.sin(angle)
    return ((1.0, 0.0, 0.0, 0.0), (0.0, c, -s, 0.0), (0.0, s, c, 0.0), (0.0, 0.0, 0.0, 1.0))


def mat_rotate_y(angle: float) -> tuple[tuple[float, float, float, float], ...]:
    c, s = math.cos(angle), math.sin(angle)
    return ((c, 0.0, s, 0.0), (0.0, 1.0, 0.0, 0.0), (-s, 0.0, c, 0.0), (0.0, 0.0, 0.0, 1.0))


def mat_rotate_z(angle: float) -> tuple[tuple[float, float, float, float], ...]:
    c, s = math.cos(angle), math.sin(angle)
    return ((c, -s, 0.0, 0.0), (s, c, 0.0, 0.0), (0.0, 0.0, 1.0, 0.0), (0.0, 0.0, 0.0, 1.0))


def mat_rotate_xyz(rotation: tuple[int, int, int]) -> tuple[tuple[float, float, float, float], ...]:
    rx, ry, rz = (sm64_angle(v & 0xFFFF) for v in rotation)
    sx, cx = math.sin(rx), math.cos(rx)
    sy, cy = math.sin(ry), math.cos(ry)
    sz, cz = math.sin(rz), math.cos(rz)
    # SM64's mtxf_rotate_xyz_and_translate() is written in the engine's matrix
    # layout.  The banner baker uses column-vector transforms, so use the
    # transpose of the engine's 3x3 rotation block here.  This keeps baked Mario
    # skeleton poses from having limbs/head attached with the wrong basis.
    return (
        (cy * cz, sx * sy * cz - cx * sz, cx * sy * cz + sx * sz, 0.0),
        (cy * sz, sx * sy * sz + cx * cz, cx * sy * sz - sx * cz, 0.0),
        (-sy, sx * cy, cx * cy, 0.0),
        (0.0, 0.0, 0.0, 1.0),
    )


def mat_rotate_x_degrees(degrees: float) -> tuple[tuple[float, float, float, float], ...]:
    return mat_rotate_x(math.radians(degrees))


def mat_rotate_y_degrees(degrees: float) -> tuple[tuple[float, float, float, float], ...]:
    return mat_rotate_y(math.radians(degrees))


def mat_rotate_z_degrees(degrees: float) -> tuple[tuple[float, float, float, float], ...]:
    return mat_rotate_z(math.radians(degrees))


def mat_transform_point(matrix: tuple[tuple[float, float, float, float], ...], vertex: mm.Vertex) -> mm.Vertex:
    x = matrix[0][0] * vertex.x + matrix[0][1] * vertex.y + matrix[0][2] * vertex.z + matrix[0][3]
    y = matrix[1][0] * vertex.x + matrix[1][1] * vertex.y + matrix[1][2] * vertex.z + matrix[1][3]
    z = matrix[2][0] * vertex.x + matrix[2][1] * vertex.y + matrix[2][2] * vertex.z + matrix[2][3]
    return mm.Vertex(round(x), round(y), round(z), vertex.s, vertex.t, vertex.rgba)


def sm64_angle_to_degrees(angle: int) -> float:
    return angle * 360.0 / 65536.0


def matrix_uniform_scale(matrix: tuple[tuple[float, float, float, float], ...]) -> float:
    # Approximate inherited scale from the first basis column.  WF actor
    # billboards only need uniform scale preservation; their camera-facing
    # rotation is baked separately for OBJ.
    return math.sqrt(matrix[0][0] * matrix[0][0] + matrix[1][0] * matrix[1][0] + matrix[2][0] * matrix[2][0]) or 1.0


def bake_billboard_matrix(matrix: tuple[tuple[float, float, float, float], ...]) -> tuple[tuple[float, float, float, float], ...]:
    scale = matrix_uniform_scale(matrix)
    x, y, z = matrix[0][3], matrix[1][3], matrix[2][3]
    # Freeze N64 billboard quads as world-facing XY cards.  This keeps Whomp's
    # hand sprites visible in OBJ/Xcode/CGFX previews instead of relying on the
    # runtime camera-facing billboard node.
    return mat_mul(mat_translate(x, y, z), mat_scale(scale))


def bake_whomp_hand_billboard_matrix(matrix: tuple[tuple[float, float, float, float], ...]) -> tuple[tuple[float, float, float, float], ...]:
    # The original hand billboard is tiny because it was designed to face the
    # active camera at gameplay scale.  The HOME banner is viewed farther away,
    # so bake it larger while preserving inherited actor scale.
    scale = matrix_uniform_scale(matrix) * 1.85
    x, y, z = matrix[0][3], matrix[1][3], matrix[2][3]
    return mat_mul(mat_translate(x, y, z), mat_scale(scale))


def texture_symbols(root: Path) -> dict[str, Path]:
    symbols = mm.parse_texture_symbols(root)
    texture_re = re.compile(
        r"(?:ALIGNED8\s+)?(?:static\s+)?const\s+u8\s+([A-Za-z0-9_]+)\[\]\s*=\s*\{\s*"
        r"#include\s+\"([^\"]+?)(?:\.inc\.c)?\"\s*\};",
        re.S,
    )
    for path in sorted((root / "actors").glob("**/model.inc.c")):
        text = mm.strip_comments(path.read_text())
        for match in texture_re.finditer(text):
            png = root / f"{match.group(2)}.png"
            if png.exists():
                symbols[match.group(1)] = png
    water_png = root / "textures/water/waterbox_water.rgba16.png"
    if water_png.exists():
        symbols["texture_waterbox_water"] = water_png
    # The decomp symbol includes the waterbox texture from bin/segment2.c; some
    # checkouts only have the generated PNG under textures/skyboxes/water.
    for candidate in [
        root / "textures/water/jrb_textures.00000.rgba16.png",
        root / "textures/water/jrb_textures.00800.rgba16.png",
    ]:
        if candidate.exists():
            symbols.setdefault("texture_waterbox_water", candidate)
            break
    add_banner_water_texture(symbols)
    add_tinted_coin_textures(symbols)
    return symbols


def add_banner_water_texture(symbols: dict[str, Path]) -> None:
    src = symbols.get("texture_waterbox_water")
    if not src or not src.exists():
        return
    gen_dir = OUT_DIR / "generated_textures"
    gen_dir.mkdir(parents=True, exist_ok=True)
    image = Image.open(src).convert("RGBA")
    out = Image.new("RGBA", image.size)
    out_pixels = out.load()
    pixels = image.load()
    for y in range(image.height):
        for x in range(image.width):
            r, g, b, a = pixels[x, y]
            # Bake WF's movtex water alpha into the preview texture.  Keep a
            # little saturation boost so it still reads as water in tiny HOME
            # banner renders after blending with stone underneath.
            out_pixels[x, y] = (
                min(255, int(r * 1.08)),
                min(255, int(g * 1.08)),
                min(255, int(b * 1.12)),
                min(a, 0x78),
            )
    out_path = gen_dir / "banner_wf_water_alpha.png"
    out.save(out_path)
    symbols["texture_banner_wf_water_alpha"] = out_path


def add_tinted_coin_textures(symbols: dict[str, Path]) -> None:
    tints = {
        # Banner coins need to survive OBJ/Xcode/CGFX preview without the
        # game's vertex-color + IA modulation path, so bake a brighter,
        # slightly glow-like color directly into the texture.
        "yellow": (255, 238, 48),
        "red": (255, 78, 64),
        "blue": (72, 154, 255),
    }
    coin_textures = [
        "coin_seg3_texture_03005780",
        "coin_seg3_texture_03005F80",
        "coin_seg3_texture_03006780",
        "coin_seg3_texture_03006F80",
    ]
    gen_dir = OUT_DIR / "generated_textures"
    gen_dir.mkdir(parents=True, exist_ok=True)
    for base_name in coin_textures:
        src = symbols.get(base_name)
        if not src or not src.exists():
            continue
        image = Image.open(src).convert("RGBA")
        alpha_glow = image.getchannel("A").filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.GaussianBlur(1.1))
        pixels = image.load()
        glow_pixels = alpha_glow.load()
        for tint_name, tint in tints.items():
            out = Image.new("RGBA", image.size)
            out_pixels = out.load()
            for y in range(image.height):
                for x in range(image.width):
                    r, g, b, a = pixels[x, y]
                    base_intensity = max(r, g, b) / 255.0
                    # Lift midtones, then add a small halo from the alpha mask.
                    # This makes coins read as colored sparkle points on the
                    # HOME menu without turning them into giant bloom blobs.
                    intensity = min(1.0, (base_intensity ** 0.62) * 1.45)
                    glow = max(0, glow_pixels[x, y] - a) / 255.0
                    alpha = max(a, int(glow * 96))
                    glow_boost = glow * 0.55
                    out_pixels[x, y] = (
                        min(255, int(tint[0] * (intensity + glow_boost))),
                        min(255, int(tint[1] * (intensity + glow_boost))),
                        min(255, int(tint[2] * (intensity + glow_boost))),
                        alpha,
                    )
            out_path = gen_dir / f"banner_glow_{tint_name}_{base_name}.png"
            out.save(out_path)
            symbols[f"banner_glow_{tint_name}_{base_name}"] = out_path


def all_model_text(root: Path) -> str:
    wf_paths = set((root / "levels" / "wf").glob("**/model.inc.c"))
    # Some WF chunks, notably the high tower, are split as 1.inc.c/2.inc.c
    # beside their geo.inc.c instead of using the usual model.inc.c filename.
    wf_paths.update(path for path in (root / "levels" / "wf").glob("**/[0-9]*.inc.c") if path.name != "geo.inc.c")
    paths = [
        *sorted(wf_paths),
        *sorted((root / "actors").glob("**/model.inc.c")),
    ]
    return "\n".join(mm.strip_comments(path.read_text()) for path in paths)


def all_geo_bodies(root: Path) -> dict[str, str]:
    geos: dict[str, str] = {}
    paths = [
        *sorted((root / "levels" / "wf").glob("**/geo.inc.c")),
        *sorted((root / "actors").glob("**/geo.inc.c")),
    ]
    for path in paths:
        text = mm.strip_comments(path.read_text())
        for match in re.finditer(r"const\s+GeoLayout\s+([A-Za-z0-9_]+)\[\]\s*=\s*\{(.*?)\};", text, re.S):
            geos[match.group(1)] = match.group(2)
    return geos


def base_transform(transform: mm.Transform) -> GeoTransform:
    return GeoTransform(float(transform.x), float(transform.y), float(transform.z), transform.yaw_degrees, 1.0)


def compose_transform(parent: GeoTransform, dx: int, dy: int, dz: int, scale: float | None = None) -> GeoTransform:
    angle = math.radians(parent.yaw_degrees)
    cos_y = math.cos(angle)
    sin_y = math.sin(angle)
    local_scale = parent.scale
    child_scale = parent.scale if scale is None else parent.scale * scale
    return GeoTransform(
        (dx * cos_y + dz * sin_y) * local_scale + parent.x,
        parent.y + dy * local_scale,
        (-dx * sin_y + dz * cos_y) * local_scale + parent.z,
        parent.yaw_degrees,
        child_scale,
    )


def geo_display_entries(
    bodies: dict[str, str],
    geo: str,
    base: GeoTransform,
    visited: set[str] | None = None,
) -> list[tuple[str, GeoTransform]]:
    """Return display lists with a simple bind-pose transform.

    This handles the subset used by WF and common actors: GEO_DISPLAY_LIST,
    GEO_ANIMATED_PART and GEO_TRANSLATE_NODE with OPEN/CLOSE scoping.  Rotation,
    billboarding and animation frames are intentionally ignored for the banner
    preview, but local translations are preserved so actors do not explode into
    loose parts.
    """
    if visited is None:
        visited = set()
    if geo in visited or geo not in bodies:
        return []
    visited.add(geo)

    entries: list[tuple[str, GeoTransform]] = []
    current = base
    stack: list[GeoTransform] = []
    pending_child: GeoTransform | None = None
    switch_depth = 0
    switch_taken_depths: set[int] = set()

    macro_re = re.compile(
        r"\b(GEO_DISPLAY_LIST|GEO_ANIMATED_PART|GEO_TRANSLATE_NODE|GEO_SCALE|GEO_OPEN_NODE|GEO_CLOSE_NODE|GEO_BRANCH|GEO_SWITCH_CASE|GEO_BILLBOARD)\s*\((.*?)\)|\b(GEO_OPEN_NODE|GEO_CLOSE_NODE)\s*\(\s*\)",
        re.S,
    )
    for match in macro_re.finditer(bodies[geo]):
        macro = match.group(1) or match.group(3)
        args = mm.split_args(match.group(2) or "")
        if macro == "GEO_SWITCH_CASE":
            switch_depth = 1
            switch_taken_depths.clear()
            continue
        if switch_depth and macro not in {"GEO_OPEN_NODE", "GEO_CLOSE_NODE"}:
            if switch_depth in switch_taken_depths:
                continue
            switch_taken_depths.add(switch_depth)
        if macro == "GEO_DISPLAY_LIST" and len(args) >= 2:
            entries.append((args[1].strip(), current))
        elif macro == "GEO_ANIMATED_PART" and len(args) >= 5:
            part_tf = compose_transform(current, mm.parse_int(args[1]), mm.parse_int(args[2]), mm.parse_int(args[3]))
            dl = args[4].strip()
            if dl != "NULL":
                entries.append((dl, part_tf))
            pending_child = part_tf
        elif macro == "GEO_TRANSLATE_NODE" and len(args) >= 4:
            pending_child = compose_transform(current, mm.parse_int(args[1]), mm.parse_int(args[2]), mm.parse_int(args[3]))
        elif macro == "GEO_SCALE" and len(args) >= 2:
            # Geo scale values are fixed point against 0x10000.
            pending_child = compose_transform(current, 0, 0, 0, mm.parse_int(args[1]) / 65536.0)
        elif macro == "GEO_BRANCH" and len(args) >= 2:
            entries.extend(geo_display_entries(bodies, args[1].strip(), current, visited.copy()))
        elif macro == "GEO_OPEN_NODE":
            stack.append(current)
            if pending_child is not None:
                current = pending_child
                pending_child = None
            if switch_depth:
                switch_depth += 1
        elif macro == "GEO_CLOSE_NODE":
            if stack:
                current = stack.pop()
            if switch_depth:
                switch_taken_depths.discard(switch_depth)
                switch_depth -= 1
            pending_child = None
    return entries


def transform_vertex_obj(vertex: mm.Vertex, transform: GeoTransform) -> mm.Vertex:
    angle = math.radians(transform.yaw_degrees)
    cos_y = math.cos(angle)
    sin_y = math.sin(angle)
    sx = vertex.x * transform.scale
    sy = vertex.y * transform.scale
    sz = vertex.z * transform.scale
    x = sx * cos_y + sz * sin_y + transform.x
    z = -sx * sin_y + sz * cos_y + transform.z
    y = sy + transform.y
    return mm.Vertex(round(x), round(y), round(z), vertex.s, vertex.t, vertex.rgba)


def append_obj_triangle(state: ObjRenderState, triangles: list[mm.Triangle], indices: tuple[int, int, int]) -> None:
    if state.vertex_cache is None or any(index not in state.vertex_cache for index in indices):
        return
    verts = tuple(state.vertex_cache[index] for index in indices)
    if state.rgba is not None and state.texture is None:
        verts = tuple(mm.Vertex(v.x, v.y, v.z, v.s, v.t, state.rgba) for v in verts)
    normal_y = mm.triangle_normal_y(*verts)
    avg_y = sum(vertex.y for vertex in verts) / 3.0
    triangles.append(mm.Triangle(verts, state.texture, avg_y, normal_y))


def apply_light_macro(state: ObjRenderState, args: list[str]) -> None:
    if len(args) < 2 or args[1].strip() != "1":
        return
    match = re.search(r"&\s*([A-Za-z0-9_]+)\.l", args[0])
    if match and match.group(1) in LIGHT_RGBA_BY_SYMBOL:
        state.rgba = LIGHT_RGBA_BY_SYMBOL[match.group(1)]


def run_display_list_obj(
    name: str,
    display_lists: dict[str, str],
    vertices: dict[str, list[mm.Vertex]],
    state: ObjRenderState,
    triangles: list[mm.Triangle],
    visited: set[str] | None = None,
) -> None:
    if visited is None:
        visited = set()
    if name in visited or name not in display_lists:
        return
    visited.add(name)

    body = display_lists[name]
    macro_re = re.compile(r"\b(gsDPSetTextureImage|gsDPSetCombineMode|gsSPTexture|gsSPLight|gsSPVertex|gsSP2Triangles|gsSP1Triangle|gsSPDisplayList|gsSPBranchList)\s*\((.*?)\)", re.S)
    for match in macro_re.finditer(body):
        macro = match.group(1)
        args = mm.split_args(match.group(2))
        if macro == "gsDPSetTextureImage" and len(args) >= 4:
            state.texture = args[3]
        elif macro == "gsSPLight":
            apply_light_macro(state, args)
        elif macro == "gsDPSetCombineMode" and args and "G_CC_SHADE" in args[0]:
            state.texture = None
        elif macro == "gsSPTexture" and args and args[-1].strip() == "G_OFF":
            state.texture = None
        elif macro == "gsSPVertex" and len(args) >= 3:
            array_name = args[0]
            count = mm.parse_int(args[1])
            start_index = mm.parse_int(args[2])
            source = vertices.get(array_name, [])
            for offset, vertex in enumerate(source[:count]):
                state.vertex_cache[start_index + offset] = transform_vertex_obj(vertex, state.transform)
        elif macro in {"gsSPDisplayList", "gsSPBranchList"} and args:
            run_display_list_obj(args[0], display_lists, vertices, state, triangles, visited.copy())
        elif macro == "gsSP1Triangle" and len(args) >= 3:
            append_obj_triangle(state, triangles, (mm.parse_int(args[0]), mm.parse_int(args[1]), mm.parse_int(args[2])))
        elif macro == "gsSP2Triangles" and len(args) >= 7:
            append_obj_triangle(state, triangles, (mm.parse_int(args[0]), mm.parse_int(args[1]), mm.parse_int(args[2])))
            append_obj_triangle(state, triangles, (mm.parse_int(args[4]), mm.parse_int(args[5]), mm.parse_int(args[6])))


def parse_c_ints(body: str) -> list[int]:
    values: list[int] = []
    for token in re.findall(r"0x[0-9A-Fa-f]+|-?\d+", body):
        value = int(token, 16) if token.lower().startswith("0x") else int(token)
        if value >= 0x8000 and token.lower().startswith("0x"):
            value -= 0x10000
        values.append(value)
    return values


def parse_c_uints(body: str) -> list[int]:
    values: list[int] = []
    for token in re.findall(r"0x[0-9A-Fa-f]+|-?\d+", body):
        value = int(token, 16) if token.lower().startswith("0x") else int(token)
        values.append(value & 0xFFFF)
    return values


def parse_animation(path: Path, frame: int) -> AnimationState:
    text = mm.strip_comments(path.read_text())
    values_match = re.search(r"static\s+const\s+s16\s+([A-Za-z0-9_]+)\[\]\s*=\s*\{(.*?)\};", text, re.S)
    indices_match = re.search(r"static\s+const\s+u16\s+([A-Za-z0-9_]+)\[\]\s*=\s*\{(.*?)\};", text, re.S)
    struct_match = re.search(r"static\s+const\s+struct\s+Animation\s+[A-Za-z0-9_]+\s*=\s*\{(.*?)\};", text, re.S)
    if not values_match or not indices_match or not struct_match:
        raise ValueError(f"could not parse animation {path}")
    values = parse_c_ints(values_match.group(2))
    indices = parse_c_uints(indices_match.group(2))
    fields = mm.split_args(struct_match.group(1))
    flags = mm.parse_int(fields[0]) if fields else 0
    loop_end = mm.parse_int(fields[4]) if len(fields) > 4 else frame + 1
    frame = max(0, min(frame, max(0, loop_end - 1)))
    if flags & ANIM_FLAG_HOR_TRANS:
        anim_type = ANIM_TYPE_VERTICAL_TRANSLATION
    elif flags & ANIM_FLAG_VERT_TRANS:
        anim_type = ANIM_TYPE_LATERAL_TRANSLATION
    elif flags & ANIM_FLAG_6:
        anim_type = ANIM_TYPE_NO_TRANSLATION
    else:
        anim_type = ANIM_TYPE_TRANSLATION
    return AnimationState(values, indices, frame, anim_type)


def parse_named_mario_animation(path: Path, anim_name: str, frame: int) -> AnimationState:
    """Parse generated build/us_3ds/assets/mario_anim_data.c animation data."""
    text = mm.strip_comments(path.read_text())
    table_match = re.search(r"\bgMarioAnims\s*=\s*\{", text)
    if not table_match:
        raise ValueError(f"could not parse Mario animation anim_{anim_name} from {path}")

    # The generated 3DS build does not emit standalone initializers for Mario's
    # animation arrays.  Instead, each animation is packed into the big
    # gMarioAnims object as:
    #   { struct Animation referencing anim_XX_values/indices },
    #   { anim_XX_indices payload },
    #   { anim_XX_values payload },
    # so scan second-level initializer blocks and use the two payload blocks
    # immediately after the matching struct block.
    blocks: list[str] = []
    depth = 0
    block_start: int | None = None
    start = text.index("{", table_match.start())
    for i in range(start, len(text)):
        ch = text[i]
        if ch == "{":
            if depth == 1:
                block_start = i + 1
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 1 and block_start is not None:
                blocks.append(text[block_start:i])
                block_start = None
            elif depth == 0:
                break

    needle_values = f"anim_{anim_name}_values"
    needle_indices = f"anim_{anim_name}_indices"
    struct_index = next(
        (
            i
            for i, block in enumerate(blocks)
            if needle_values in block and needle_indices in block
        ),
        None,
    )
    if struct_index is None or struct_index + 2 >= len(blocks):
        raise ValueError(f"could not parse Mario animation anim_{anim_name} from {path}")

    fields = mm.split_args(blocks[struct_index])
    indices = parse_c_uints(blocks[struct_index + 1])
    values = parse_c_ints(blocks[struct_index + 2])
    flags = mm.parse_int(fields[0]) if fields else 0
    loop_end = mm.parse_int(fields[4]) if len(fields) > 4 else frame + 1
    frame = max(0, min(frame, max(0, loop_end - 1)))
    if flags & ANIM_FLAG_HOR_TRANS:
        anim_type = ANIM_TYPE_VERTICAL_TRANSLATION
    elif flags & ANIM_FLAG_VERT_TRANS:
        anim_type = ANIM_TYPE_LATERAL_TRANSLATION
    elif flags & ANIM_FLAG_6:
        anim_type = ANIM_TYPE_NO_TRANSLATION
    else:
        anim_type = ANIM_TYPE_TRANSLATION
    return AnimationState(values, indices, frame, anim_type)


def retrieve_anim_value(anim: AnimationState) -> int:
    if anim.pointer + 1 >= len(anim.indices):
        return 0
    length = anim.indices[anim.pointer]
    offset = anim.indices[anim.pointer + 1]
    anim.pointer += 2
    index = offset + (anim.frame if anim.frame < length else max(0, length - 1))
    if 0 <= index < len(anim.values):
        return anim.values[index]
    return 0


def anim_part_transform(anim: AnimationState, base_translation: tuple[int, int, int]) -> tuple[tuple[float, float, float], tuple[int, int, int]]:
    tx, ty, tz = (float(v) for v in base_translation)
    rotation = [0, 0, 0]

    if anim.anim_type == ANIM_TYPE_TRANSLATION:
        tx += retrieve_anim_value(anim)
        ty += retrieve_anim_value(anim)
        tz += retrieve_anim_value(anim)
        anim.anim_type = ANIM_TYPE_ROTATION
    elif anim.anim_type == ANIM_TYPE_LATERAL_TRANSLATION:
        tx += retrieve_anim_value(anim)
        anim.pointer += 2
        tz += retrieve_anim_value(anim)
        anim.anim_type = ANIM_TYPE_ROTATION
    elif anim.anim_type == ANIM_TYPE_VERTICAL_TRANSLATION:
        anim.pointer += 2
        ty += retrieve_anim_value(anim)
        anim.pointer += 2
        anim.anim_type = ANIM_TYPE_ROTATION
    elif anim.anim_type == ANIM_TYPE_NO_TRANSLATION:
        anim.pointer += 6
        anim.anim_type = ANIM_TYPE_ROTATION

    if anim.anim_type == ANIM_TYPE_ROTATION:
        rotation[0] = retrieve_anim_value(anim)
        rotation[1] = retrieve_anim_value(anim)
        rotation[2] = retrieve_anim_value(anim)
    return (tx, ty, tz), (rotation[0], rotation[1], rotation[2])


def base_matrix(transform: mm.Transform) -> MatrixTransform:
    angle = math.radians(transform.yaw_degrees)
    matrix = mat_mul(mat_translate(transform.x, transform.y, transform.z), mat_rotate_y(angle))
    return MatrixTransform(matrix)


def run_display_list_matrix(
    name: str,
    display_lists: dict[str, str],
    vertices: dict[str, list[mm.Vertex]],
    matrix: MatrixTransform,
    triangles: list[mm.Triangle],
    state: ObjRenderState | None = None,
    visited: set[str] | None = None,
) -> None:
    if visited is None:
        visited = set()
    if name in visited or name not in display_lists:
        return
    visited.add(name)
    if state is None:
        state = ObjRenderState(transform=GeoTransform())
    body = display_lists[name]
    macro_re = re.compile(r"\b(gsDPSetTextureImage|gsDPSetCombineMode|gsSPTexture|gsSPLight|gsSPVertex|gsSP2Triangles|gsSP1Triangle|gsSPDisplayList|gsSPBranchList)\s*\((.*?)\)", re.S)
    for match in macro_re.finditer(body):
        macro = match.group(1)
        args = mm.split_args(match.group(2))
        if macro == "gsDPSetTextureImage" and len(args) >= 4:
            state.texture = args[3]
        elif macro == "gsSPLight":
            apply_light_macro(state, args)
        elif macro == "gsDPSetCombineMode" and args and "G_CC_SHADE" in args[0]:
            state.texture = None
        elif macro == "gsSPTexture" and args and args[-1].strip() == "G_OFF":
            state.texture = None
        elif macro == "gsSPVertex" and len(args) >= 3:
            source = vertices.get(args[0], [])
            count = mm.parse_int(args[1])
            start_index = mm.parse_int(args[2])
            for offset, vertex in enumerate(source[:count]):
                state.vertex_cache[start_index + offset] = mat_transform_point(matrix.matrix, vertex)
        elif macro in {"gsSPDisplayList", "gsSPBranchList"} and args:
            run_display_list_matrix(args[0], display_lists, vertices, matrix, triangles, state, visited.copy())
        elif macro == "gsSP1Triangle" and len(args) >= 3:
            append_obj_triangle(state, triangles, (mm.parse_int(args[0]), mm.parse_int(args[1]), mm.parse_int(args[2])))
        elif macro == "gsSP2Triangles" and len(args) >= 7:
            append_obj_triangle(state, triangles, (mm.parse_int(args[0]), mm.parse_int(args[1]), mm.parse_int(args[2])))
            append_obj_triangle(state, triangles, (mm.parse_int(args[4]), mm.parse_int(args[5]), mm.parse_int(args[6])))


def geo_display_entries_with_anim(
    bodies: dict[str, str],
    geo: str,
    base: MatrixTransform,
    anim: AnimationState,
    visited: set[str] | None = None,
) -> list[tuple[str, MatrixTransform]]:
    if visited is None:
        visited = set()
    if geo in visited or geo not in bodies:
        return []
    visited.add(geo)
    if geo == "mario_geo_face_and_wings":
        return [("mario_cap_on_eyes_front", base)]
    if geo == "mario_geo_left_hand":
        trans, rot = anim_part_transform(anim, (60, 0, 0))
        matrix = mat_mul(base.matrix, mat_mul(mat_translate(*trans), mat_rotate_xyz(rot)))
        return [("mario_left_hand_open", MatrixTransform(matrix))]
    if geo == "mario_geo_right_hand":
        trans, rot = anim_part_transform(anim, (60, 0, 0))
        matrix = mat_mul(base.matrix, mat_mul(mat_translate(*trans), mat_rotate_xyz(rot)))
        return [("mario_right_hand_peace", MatrixTransform(matrix))]
    entries: list[tuple[str, MatrixTransform]] = []
    current = base
    stack: list[MatrixTransform] = []
    pending_child: MatrixTransform | None = None
    billboard_active = False
    macro_re = re.compile(
        r"\b(GEO_DISPLAY_LIST|GEO_ANIMATED_PART|GEO_TRANSLATE_NODE|GEO_SCALE|GEO_OPEN_NODE|GEO_CLOSE_NODE|GEO_BRANCH|GEO_BILLBOARD|GEO_DEATH_RAGDOLL_LIMB)\s*\((.*?)\)|\b(GEO_OPEN_NODE|GEO_CLOSE_NODE)\s*\(\s*\)",
        re.S,
    )
    for match in macro_re.finditer(bodies[geo]):
        macro = match.group(1) or match.group(3)
        args = mm.split_args(match.group(2) or "")
        if macro == "GEO_DISPLAY_LIST" and len(args) >= 2:
            dl = args[1].strip()
            if billboard_active and dl in {"whomp_seg6_dl_0601FBC0", "whomp_seg6_dl_0601FCA8"}:
                entries.append((dl, MatrixTransform(bake_whomp_hand_billboard_matrix(current.matrix))))
            else:
                entries.append((dl, current))
        elif macro == "GEO_ANIMATED_PART" and len(args) >= 5:
            trans, rot = anim_part_transform(anim, (mm.parse_int(args[1]), mm.parse_int(args[2]), mm.parse_int(args[3])))
            local = mat_mul(mat_translate(*trans), mat_rotate_xyz(rot))
            part = MatrixTransform(mat_mul(current.matrix, local))
            dl = args[4].strip()
            if dl != "NULL":
                if billboard_active and dl in {"whomp_seg6_dl_0601FBC0", "whomp_seg6_dl_0601FCA8"}:
                    entries.append((dl, MatrixTransform(bake_whomp_hand_billboard_matrix(current.matrix))))
                else:
                    entries.append((dl, part))
            pending_child = part
        elif macro == "GEO_TRANSLATE_NODE" and len(args) >= 4:
            local = mat_translate(mm.parse_int(args[1]), mm.parse_int(args[2]), mm.parse_int(args[3]))
            pending_child = MatrixTransform(mat_mul(current.matrix, local))
        elif macro == "GEO_SCALE" and len(args) >= 2:
            pending_child = MatrixTransform(mat_mul(current.matrix, mat_scale(mm.parse_int(args[1]) / 65536.0)))
        elif macro == "GEO_BILLBOARD":
            pending_child = MatrixTransform(bake_billboard_matrix(current.matrix))
            billboard_active = True
        elif macro == "GEO_DEATH_RAGDOLL_LIMB":
            # In this repo the Mario geo source uses a helper macro:
            #   GEO_ASM(...), GEO_ROTATION_NODE(identity), GEO_OPEN_NODE()
            # The offline parser sees the macro call but not the preprocessor
            # expansion, so explicitly mirror the identity open node here.
            stack.append(current)
        elif macro == "GEO_BRANCH" and len(args) >= 2:
            entries.extend(geo_display_entries_with_anim(bodies, args[1].strip(), current, anim, visited.copy()))
        elif macro == "GEO_OPEN_NODE":
            stack.append(current)
            if pending_child is not None:
                current = pending_child
                pending_child = None
        elif macro == "GEO_CLOSE_NODE":
            if stack:
                current = stack.pop()
            billboard_active = False
            pending_child = None
    return entries


def add_banner_mario(
    geos: dict[str, str],
    display_lists: dict[str, str],
    vertices: dict[str, list[mm.Vertex]],
    raw: list[tuple[str, mm.Triangle]],
) -> None:
    # Bake Mario's course-clear star dance final pose (not the mid-dance
    # celebration frames) as a static banner prop.  Right hand resolves to the
    # official peace-sign display list through geo_display_entries_with_anim.
    anim = parse_named_mario_animation(ROOT / "build/us_3ds/assets/mario_anim_data.c", "CD", 62)
    x, y, z = WF_BANNER_MARIO_POS
    base = mat_mul(
        mat_translate(x, y, z),
        mat_mul(mat_rotate_y_degrees(WF_BANNER_MARIO_YAW), mat_scale(WF_BANNER_MARIO_SCALE)),
    )
    for dl, matrix in geo_display_entries_with_anim(geos, "mario_geo_body", MatrixTransform(base), anim):
        tris: list[mm.Triangle] = []
        state = ObjRenderState(rgba=MARIO_INHERITED_RGBA_BY_DL.get(dl))
        run_display_list_matrix(dl, display_lists, vertices, matrix, tris, state=state)
        raw.extend(("banner_mario_star_dance", tri) for tri in tris)


def model_geo_map(root: Path) -> dict[str, str]:
    mapping = mm.parse_model_geo_map(root, "wf")
    for script in (root / "levels" / "scripts.c", root / "levels" / "wf" / "script.c"):
        text = mm.strip_comments(script.read_text())
        for match in re.finditer(r"LOAD_MODEL_FROM_GEO\s*\(\s*([^,]+),\s*([^)]+)\)", text):
            mapping.setdefault(match.group(1).strip(), match.group(2).strip())
    mapping.update(GLOBAL_GEO_HINTS)
    return mapping


def parse_wf_objects(root: Path, include_problem_monsters: bool = True) -> list[mm.PlacedGeo]:
    text = mm.strip_comments((root / "levels" / "wf" / "script.c").read_text())
    mapping = model_geo_map(root)
    placed: list[mm.PlacedGeo] = []
    for match in re.finditer(r"\bOBJECT(?:_WITH_ACTS)?\s*\((.*?)\)", text, re.S):
        args = mm.split_args(match.group(1))
        if len(args) < 9:
            continue
        model = args[0].strip()
        behavior = args[8].strip()
        acts = args[9].strip() if len(args) >= 10 else "ALL_ACTS"
        if behavior in EXCLUDED_BEHAVIORS or model in EXCLUDED_MODELS:
            continue
        if not include_problem_monsters and model in PROBLEM_ANIMATED_MONSTERS:
            continue
        if behavior == "bhvWhompKingBoss" or (model == "MODEL_WHOMP" and ACT1_ONLY in acts):
            continue
        geo = mapping.get(model)
        if not geo:
            continue
        x = mm.parse_int(args[1])
        y = mm.parse_int(args[2])
        z = mm.parse_int(args[3])
        yaw = float(mm.parse_int(args[5]))
        # Freeze push-out/sliding WF mechanisms in a readable banner pose.  The
        # original objects animate from a retracted state; in HOME menu preview
        # that makes the front path look empty, so use the midpoint of their
        # official travel ranges.
        if behavior in {"bhvSmallBomp", "bhvLargeBomp"}:
            x = 3580
            if behavior == "bhvSmallBomp":
                yaw -= 90.0
        elif behavior == "bhvWfSlidingPlatform":
            x = x + 2 + 255
        # High tower is selected by keeping ACT2+ tower pieces while excluding
        # the ACT1 boss above.  Other ACT2+ scenery is useful for this preview.
        placed.append(
            mm.PlacedGeo(
                geo=geo,
                transform=mm.Transform(
                    x,
                    y,
                    z,
                    yaw,
                ),
                source=f"{model}/{behavior}",
            )
        )
    return placed


def parse_specials(root: Path) -> list[mm.PlacedGeo]:
    placed = mm.parse_collision_special_geos(root, "wf", "1")
    return placed


def avg_rgba(vertices: tuple[mm.Vertex, mm.Vertex, mm.Vertex]) -> tuple[int, int, int, int]:
    return tuple(sum(v.rgba[i] for v in vertices) // 3 for i in range(4))  # type: ignore[return-value]


def should_skip_banner_triangle(source: str, tri: mm.Triangle) -> bool:
    # Residual shadow decal from WF's omitted bubbly tree.  The tree itself is
    # not part of the banner scene, so keep its baked floor shadow out too.
    if source == "special_level_geo_0E" and tri.texture == "grass_0900B000":
        cx = sum(v.x for v in tri.vertices) / 3.0
        cy = sum(v.y for v in tri.vertices) / 3.0
        cz = sum(v.z for v in tri.vertices) / 3.0
        if 2300.0 <= cx <= 2800.0 and cy < 320.0 and 4350.0 <= cz <= 4850.0:
            return True
    return False


def remap_banner_texture(source: str, texture: str | None) -> str | None:
    if source == "banner_cannon_barrel":
        return None
    if texture is None:
        return None
    if source.startswith("banner_blue_coin_switch"):
        return texture
    if source.startswith("banner_yellow_coin"):
        return f"banner_glow_yellow_{texture}"
    if source.startswith("banner_red_coin"):
        return f"banner_glow_red_{texture}"
    if source.startswith("banner_blue_coin"):
        return f"banner_glow_blue_{texture}"
    return texture


def adjusted_banner_vertices(source: str, tri: mm.Triangle) -> tuple[tuple[float, float, float], tuple[float, float, float], tuple[float, float, float]]:
    verts = tuple((float(v.x), float(v.y), float(v.z)) for v in tri.vertices)
    # WF uses semi-transparent yellow marker triangles as surface decals.  They
    # are nearly coplanar with floors/walls and flicker badly after OBJ/CGFX
    # conversion, so lift them slightly along their own normal.
    if source == "area" and tri.texture is None and avg_rgba(tri.vertices) == (255, 255, 0, 128) and abs(tri.normal_y) > 0.82:
        n = normal_of(verts)
        return tuple((x + n[0] * 18.0, y + n[1] * 18.0, z + n[2] * 18.0) for x, y, z in verts)  # type: ignore[return-value]
    return verts


def add_banner_cannon(
    display_lists: dict[str, str],
    vertices: dict[str, list[mm.Vertex]],
    raw: list[tuple[str, mm.Triangle]],
) -> None:
    # WF has a macro_cannon_closed at this position.  For the banner we bake a
    # visible cannon barrel protruding from the hole, but skip Bob-omb Buddy
    # because it is a complex/billboard-heavy actor and reads poorly at HOME
    # banner scale.
    x, y, z = WF_CANNON_POS
    base_matrix_tf = mat_mul(mat_translate(x, y + 28.0, z), mat_scale(1.15))
    barrel_matrix = mat_mul(
        mat_translate(x, y + 88.0, z - 18.0),
        mat_mul(mat_rotate_x_degrees(-52.0), mat_scale(1.12)),
    )
    for source, dl, matrix in [
        ("banner_cannon_base", "cannon_base_seg8_dl_080057F8", base_matrix_tf),
        ("banner_cannon_barrel", "cannon_barrel_seg8_dl_08006660", barrel_matrix),
    ]:
        tris: list[mm.Triangle] = []
        run_display_list_matrix(dl, display_lists, vertices, MatrixTransform(matrix), tris)
        raw.extend((source, tri) for tri in tris)


def add_static_geo_instance(
    source: str,
    geo: str,
    transform: mm.Transform,
    geos: dict[str, str],
    display_lists: dict[str, str],
    vertices: dict[str, list[mm.Vertex]],
    raw: list[tuple[str, mm.Triangle]],
    scale: float = 1.0,
) -> None:
    base = base_transform(transform)
    if scale != 1.0:
        base = GeoTransform(base.x, base.y, base.z, base.yaw_degrees, base.scale * scale)
    for dl, local_transform in geo_display_entries(geos, geo, base):
        tris: list[mm.Triangle] = []
        run_display_list_obj(dl, display_lists, vertices, ObjRenderState(transform=local_transform), tris)
        raw.extend((source, tri) for tri in tris)


def add_banner_macro_objects(
    geos: dict[str, str],
    display_lists: dict[str, str],
    vertices: dict[str, list[mm.Vertex]],
    raw: list[tuple[str, mm.Triangle]],
) -> None:
    for index, (x, y, z, yaw) in enumerate(WF_SIGNPOSTS):
        add_static_geo_instance(
            f"banner_signpost_{index}",
            "wooden_signpost_geo",
            mm.Transform(round(x), round(y), round(z), yaw),
            geos,
            display_lists,
            vertices,
            raw,
        )
    for index, (x, y, z, yaw) in enumerate(WF_SMALL_BOXES):
        # bhvBreakableBoxSmall sets animState=1 for the cork/wooden breakable
        # texture, not the crazy/jumping-box texture selected by the first
        # branch of breakable_box_small_geo.  Use a banner-readable 0.75 scale
        # instead of runtime 0.4 so the prop does not disappear beside coins.
        matrix = mat_mul(
            mat_translate(x, y, z),
            mat_mul(mat_rotate_y_degrees(yaw), mat_scale(0.75)),
        )
        tris: list[mm.Triangle] = []
        run_display_list_matrix("breakable_box_seg8_dl_08012D48", display_lists, vertices, MatrixTransform(matrix), tris)
        raw.extend((f"banner_small_breakable_box_{index}", tri) for tri in tris)
    for index, (x, y, z, yaw) in enumerate(WF_BLUE_COIN_SWITCHES):
        add_static_geo_instance(
            f"banner_blue_coin_switch_{index}",
            "blue_coin_switch_geo",
            mm.Transform(round(x), round(y), round(z), yaw),
            geos,
            display_lists,
            vertices,
            raw,
            scale=3.0,
        )


def add_geo_matrix_instance(
    source: str,
    geo: str,
    matrix: tuple[tuple[float, float, float, float], ...],
    geos: dict[str, str],
    display_lists: dict[str, str],
    vertices: dict[str, list[mm.Vertex]],
    raw: list[tuple[str, mm.Triangle]],
) -> None:
    for dl, local_transform in geo_display_entries(geos, geo, GeoTransform()):
        local_matrix = mat_mul(
            mat_translate(local_transform.x, local_transform.y, local_transform.z),
            mat_mul(mat_rotate_y_degrees(local_transform.yaw_degrees), mat_scale(local_transform.scale)),
        )
        tris: list[mm.Triangle] = []
        run_display_list_matrix(dl, display_lists, vertices, MatrixTransform(mat_mul(matrix, local_matrix)), tris)
        raw.extend((source, tri) for tri in tris)


def add_banner_dynamic_platforms(
    geos: dict[str, str],
    display_lists: dict[str, str],
    vertices: dict[str, list[mm.Vertex]],
    raw: list[tuple[str, mm.Triangle]],
) -> None:
    # bhvTowerPlatformGroup is a MODEL_NONE spawner in WF script.c.  Bake the
    # child platforms it creates and freeze moving ones halfway through their
    # official travel distance so the banner reads as an active mechanism.
    tower_base_x, tower_base_y, tower_base_z = 0.0, 3483.0 + 300.0, 0.0
    tower_radius = 704.0
    tower_slide_half = 190.0
    tower_platforms = [
        ("wf_geo_000B10", False),
        ("wf_geo_000B10", True),
        ("wf_geo_000B10", False),
        ("wf_geo_000B10", True),
        ("wf_geo_000B10", False),
        ("wf_geo_000B10", True),
        ("wf_geo_000B10", False),
        ("wf_geo_000B60", False),
    ]
    for index, (geo, sliding) in enumerate(tower_platforms):
        yaw = sm64_angle_to_degrees(index * 0x2000)
        angle = math.radians(yaw)
        x = tower_base_x + tower_radius * math.sin(angle)
        y = tower_base_y + 100.0 * index
        z = tower_base_z + tower_radius * math.cos(angle)
        if sliding:
            x -= tower_slide_half * math.sin(angle)
            z -= tower_slide_half * math.cos(angle)
        if geo == "wf_geo_000B60":
            y += 350.0
        add_static_geo_instance(
            f"banner_tower_platform_{index}",
            geo,
            mm.Transform(round(x), round(y), round(z), yaw),
            geos,
            display_lists,
            vertices,
            raw,
        )

    # bhvCheckerboardElevatorGroup spawns two scaled checkerboard platforms.
    # Use its type-0 official scale and freeze both at the midpoint of their
    # vertical/elevator travel.
    parent_x, parent_y, parent_z, parent_yaw = 1035.0, 2880.0, -900.0, 45.0
    for index, rel_z in enumerate((-145.0, 145.0)):
        # Freeze the paired elevators in opposite phases: same offset amount,
        # one above the base and one below it, so the banner reads as an
        # active alternating mechanism instead of duplicated platforms.
        x, _y, z = relative_coin_pos(parent_x, parent_y, parent_z, parent_yaw, 0.0, 0.0, rel_z)
        y = parent_y + (325.0 if index == 0 else -325.0)
        matrix = mat_mul(
            mat_translate(x, y, z),
            mat_mul(mat_rotate_y_degrees(parent_yaw), mat_scale_xyz(0.7, 1.5, 0.7)),
        )
        add_geo_matrix_instance(
            f"banner_checkerboard_elevator_{index}",
            "checkerboard_platform_geo",
            matrix,
            geos,
            display_lists,
            vertices,
            raw,
        )
def add_coin_instance(
    source: str,
    dl: str,
    x: float,
    y: float,
    z: float,
    yaw: float,
    display_lists: dict[str, str],
    vertices: dict[str, list[mm.Vertex]],
    raw: list[tuple[str, mm.Triangle]],
    scale: float = 1.0,
) -> None:
    # Coins are alpha quads in the original game.  For banner use we freeze one
    # visible angle and rotate it a little so the HOME model has colored points
    # without relying on coin spin animation.
    matrix = mat_mul(
        mat_translate(x, y, z),
        mat_mul(mat_rotate_y_degrees(yaw + 28.0), mat_scale(scale)),
    )
    tris: list[mm.Triangle] = []
    run_display_list_matrix(dl, display_lists, vertices, MatrixTransform(matrix), tris)
    raw.extend((source, tri) for tri in tris)


def is_banner_coin_source(source: str) -> bool:
    return (
        source.startswith("banner_yellow_coin")
        or source.startswith("banner_red_coin")
        or (source.startswith("banner_blue_coin_") and not source.startswith("banner_blue_coin_switch"))
    )


def make_backface(tri: ObjTriangle) -> ObjTriangle:
    verts = (tri.verts[2], tri.verts[1], tri.verts[0])
    uvs = (tri.uvs[2], tri.uvs[1], tri.uvs[0])
    return ObjTriangle(verts, uvs, normal_of(verts), tri.texture, tri.rgba, tri.source)


def triangle_with_rgba(tri: mm.Triangle, rgba: tuple[int, int, int, int]) -> mm.Triangle:
    vertices = tuple(mm.Vertex(v.x, v.y, v.z, v.s, v.t, rgba) for v in tri.vertices)
    return mm.Triangle(vertices, tri.texture, tri.avg_y, tri.normal_y)


def barycentric_xz(x: float, z: float, verts: tuple[mm.Vertex, mm.Vertex, mm.Vertex]) -> tuple[float, float, float] | None:
    x0, z0 = verts[0].x, verts[0].z
    x1, z1 = verts[1].x, verts[1].z
    x2, z2 = verts[2].x, verts[2].z
    denom = (z1 - z2) * (x0 - x2) + (x2 - x1) * (z0 - z2)
    if abs(denom) < 1e-6:
        return None
    w0 = ((z1 - z2) * (x - x2) + (x2 - x1) * (z - z2)) / denom
    w1 = ((z2 - z0) * (x - x2) + (x0 - x2) * (z - z2)) / denom
    w2 = 1.0 - w0 - w1
    if w0 < -0.001 or w1 < -0.001 or w2 < -0.001:
        return None
    return w0, w1, w2


def terrain_floor_height(raw: list[tuple[str, mm.Triangle]], x: float, y: float, z: float) -> float:
    best: float | None = None
    max_probe_y = y + 360.0
    for source, tri in raw:
        if source.startswith("banner_"):
            continue
        if source.startswith("MODEL_PIRANHA") or source.startswith("MODEL_WHOMP") or source.startswith("MODEL_THWOMP"):
            continue
        if tri.normal_y < 0.35:
            continue
        weights = barycentric_xz(x, z, tri.vertices)
        if weights is None:
            continue
        height = sum(tri.vertices[i].y * weights[i] for i in range(3))
        if height > max_probe_y:
            continue
        if best is None or height > best:
            best = height
    return y if best is None else best


def relative_coin_pos(x: float, y: float, z: float, yaw: float, rel_x: float, rel_y: float, rel_z: float) -> tuple[float, float, float]:
    angle = math.radians(yaw)
    cos_y = math.cos(angle)
    sin_y = math.sin(angle)
    return (
        x + rel_x * cos_y + rel_z * sin_y,
        y + rel_y,
        z - rel_x * sin_y + rel_z * cos_y,
    )


def add_banner_coins(
    display_lists: dict[str, str],
    vertices: dict[str, list[mm.Vertex]],
    raw: list[tuple[str, mm.Triangle]],
) -> None:
    floor_raw = list(raw)

    def add_yellow(x: float, y: float, z: float, yaw: float, label: str, snap_to_floor: bool = False) -> None:
        if snap_to_floor:
            y = terrain_floor_height(floor_raw, x, y + 300.0, z)
        add_coin_instance(label, "coin_seg3_dl_03007828", x, y, z, yaw, display_lists, vertices, raw, scale=1.12)

    def add_blue(x: float, y: float, z: float, yaw: float, label: str, snap_to_floor: bool = False) -> None:
        if snap_to_floor:
            y = terrain_floor_height(floor_raw, x, y + 300.0, z)
        # Hidden blue coins are very easy to lose in the tiny HOME banner, so
        # make them a touch larger than normal coins while keeping original
        # placement.
        add_coin_instance(label, "coin_seg3_dl_030078C8", x, y, z, yaw, display_lists, vertices, raw, scale=1.28)

    def add_red(x: float, y: float, z: float, yaw: float, label: str) -> None:
        add_coin_instance(label, "coin_seg3_dl_03007968", x, y, z, yaw, display_lists, vertices, raw, scale=1.12)

    for index, (x, y, z, yaw) in enumerate(WF_YELLOW_COINS):
        add_yellow(x, y, z, yaw, f"banner_yellow_coin_{index}")
    for line_index, (x, y, z, yaw) in enumerate(WF_COIN_LINES):
        for i in range(5):
            wx, wy, wz = relative_coin_pos(x, y, z, yaw, 0.0, 0.0, 160.0 * (i - 2))
            add_yellow(wx, wy, wz, yaw, f"banner_yellow_coin_line_{line_index}_{i}", snap_to_floor=True)
    for ring_index, (x, y, z, yaw, flying) in enumerate(WF_COIN_RINGS):
        for i in range(8):
            angle = i << 13
            rel_x = math.sin(angle * math.tau / 65536.0) * 300.0
            rel_z = math.cos(angle * math.tau / 65536.0) * 300.0
            wx, wy, wz = relative_coin_pos(x, y, z, yaw, rel_x, 0.0, rel_z)
            add_yellow(wx, wy, wz, yaw + i * 45.0, f"banner_yellow_coin_ring_{ring_index}_{i}", snap_to_floor=not flying)
    ax, ay, az, ayaw = WF_COIN_ARROW
    arrow_offsets = [(0, -150), (0, -50), (0, 50), (0, 150), (-50, 100), (-100, 50), (50, 100), (100, 50)]
    for i, (ox, oz) in enumerate(arrow_offsets):
        wx, wy, wz = relative_coin_pos(ax, ay, az, ayaw, ox, 0.0, oz)
        add_yellow(wx, wy, wz, ayaw, f"banner_yellow_coin_arrow_{i}", snap_to_floor=True)
    for index, (x, y, z, yaw) in enumerate(WF_BLUE_COINS):
        add_blue(x, y, z, yaw, f"banner_blue_coin_{index}", snap_to_floor=True)
    for index, (x, y, z, yaw) in enumerate(WF_RED_COINS):
        add_red(x, y, z, yaw, f"banner_red_coin_{index}")


def add_banner_water(raw: list[tuple[str, mm.Triangle]]) -> None:
    x1, z1, x2, z2, y = WF_WATER_BOX
    y += 4.0
    verts = (
        mm.Vertex(round(x1), round(y), round(z1), 0, 0, (120, 170, 255, 0x78)),
        mm.Vertex(round(x1), round(y), round(z2), 0, 32 * 64, (120, 170, 255, 0x78)),
        mm.Vertex(round(x2), round(y), round(z2), 32 * 64, 32 * 64, (120, 170, 255, 0x78)),
        mm.Vertex(round(x2), round(y), round(z1), 32 * 64, 0, (120, 170, 255, 0x78)),
    )
    for tri_verts in ((verts[0], verts[1], verts[2]), (verts[0], verts[2], verts[3])):
        raw.append(("banner_wf_water", mm.Triangle(tri_verts, "texture_banner_wf_water_alpha", y, 1.0)))


def collect_triangles(root: Path, include_problem_monsters: bool = True) -> list[ObjTriangle]:
    text = all_model_text(root)
    vertices = mm.parse_vertices(text)
    display_lists = mm.parse_display_lists(text)
    geos = all_geo_bodies(root)
    raw: list[tuple[str, mm.Triangle]] = []

    area_dls = mm.parse_geo_display_lists(root, "wf", "1")
    for dl in area_dls:
        tris: list[mm.Triangle] = []
        mm.run_display_list(dl, display_lists, vertices, mm.RenderState(), tris)
        raw.extend(("area", tri) for tri in tris)

    for placed in [*parse_wf_objects(root, include_problem_monsters), *parse_specials(root)]:
        anim_pose = ANIM_POSES.get(placed.geo)
        if anim_pose:
            anim_path, frame = anim_pose
            anim = parse_animation(anim_path, frame)
            for dl, matrix in geo_display_entries_with_anim(geos, placed.geo, base_matrix(placed.transform), anim):
                tris: list[mm.Triangle] = []
                run_display_list_matrix(dl, display_lists, vertices, matrix, tris)
                raw.extend((placed.source, tri) for tri in tris)
        else:
            for dl, transform in geo_display_entries(geos, placed.geo, base_transform(placed.transform)):
                tris: list[mm.Triangle] = []
                run_display_list_obj(dl, display_lists, vertices, ObjRenderState(transform=transform), tris)
                raw.extend((placed.source, tri) for tri in tris)

    add_banner_cannon(display_lists, vertices, raw)
    add_banner_macro_objects(geos, display_lists, vertices, raw)
    add_banner_dynamic_platforms(geos, display_lists, vertices, raw)
    add_banner_coins(display_lists, vertices, raw)
    add_banner_water(raw)

    out: list[ObjTriangle] = []
    for source, tri in raw:
        if should_skip_banner_triangle(source, tri):
            continue
        verts = adjusted_banner_vertices(source, tri)
        uvs = tuple((float(v.s), float(v.t)) for v in tri.vertices)
        rgba = avg_rgba(tri.vertices)
        if source == "banner_cannon_barrel":
            rgba = (30, 28, 82, 255)
        obj_tri = ObjTriangle(verts, uvs, normal_of(verts), remap_banner_texture(source, tri.texture), rgba, source)
        out.append(obj_tri)
        # Coins are flat alpha cards.  Bake a reversed copy so the banner still
        # shows colored coin faces when HOME rotation views them from behind.
        if is_banner_coin_source(source):
            out.append(make_backface(obj_tri))
    return out


def normalized_triangles(triangles: list[ObjTriangle], target_width: float) -> list[ObjTriangle]:
    all_verts = [v for tri in triangles for v in tri.verts]
    min_x = min(v[0] for v in all_verts)
    max_x = max(v[0] for v in all_verts)
    min_y = min(v[1] for v in all_verts)
    min_z = min(v[2] for v in all_verts)
    max_z = max(v[2] for v in all_verts)
    cx = (min_x + max_x) / 2.0
    cz = (min_z + max_z) / 2.0
    scale = target_width / max(max_x - min_x, max_z - min_z, 1.0)

    out: list[ObjTriangle] = []
    for tri in triangles:
        verts = tuple(((x - cx) * scale, (y - min_y) * scale, (z - cz) * scale) for x, y, z in tri.verts)
        out.append(ObjTriangle(verts, tri.uvs, normal_of(verts), tri.texture, tri.rgba, tri.source))
    return out


def material_name(texture: str | None, rgba: tuple[int, int, int, int], source: str) -> str:
    if texture:
        return f"tex_{re.sub(r'[^A-Za-z0-9_]+', '_', texture)}"
    r, g, b, _a = rgba
    bucket = (r // 24 * 24, g // 24 * 24, b // 24 * 24)
    hint = re.sub(r"[^A-Za-z0-9_]+", "_", source.split("/")[0])[:28]
    return f"flat_{hint}_{bucket[0]}_{bucket[1]}_{bucket[2]}"


def texture_size(texture: str | None, symbols: dict[str, Path]) -> tuple[int, int]:
    if texture and texture in symbols:
        with Image.open(symbols[texture]) as image:
            return image.size
    return 32, 32


def uv_fixed_divisor(texture: str | None) -> float:
    # Coin display lists enable gsSPTexture(0x8000, 0x8000), i.e. half texture
    # scale.  The source UVs reach 1984 for a 32px texture; without this extra
    # factor OBJ viewers repeat the image as a 2x2 sheet of coins.
    if texture and "coin_seg3_texture" in texture:
        return 64.0
    return 32.0


def write_obj(triangles: list[ObjTriangle], symbols: dict[str, Path]) -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    tex_dir = OUT_DIR / "textures"
    tex_dir.mkdir(exist_ok=True)

    mats: dict[str, tuple[str | None, tuple[int, int, int, int]]] = {}
    for tri in triangles:
        mats.setdefault(material_name(tri.texture, tri.rgba, tri.source), (tri.texture, tri.rgba))

    mtl_lines = ["# WF banner preview materials"]
    copied: dict[str, str] = {}
    for name, (texture, rgba) in sorted(mats.items()):
        r, g, b, a = rgba
        alpha = a / 255.0
        mtl_lines.extend([
            f"newmtl {name}",
            f"Kd {r / 255:.4f} {g / 255:.4f} {b / 255:.4f}",
            "Ka 0.1800 0.1800 0.1800",
            "Ks 0.0000 0.0000 0.0000",
        ])
        if alpha < 0.999:
            mtl_lines.extend([f"d {alpha:.4f}", f"Tr {1.0 - alpha:.4f}"])
        if texture and texture in symbols:
            src = symbols[texture]
            dst_name = copied.get(texture)
            if dst_name is None:
                dst_name = f"{texture}.png"
                shutil.copyfile(src, tex_dir / dst_name)
                copied[texture] = dst_name
            mtl_lines.append(f"map_Kd textures/{dst_name}")
        mtl_lines.append("")
    (OUT_BASE.with_suffix(".mtl")).write_text("\n".join(mtl_lines))

    lines = ["# Whomp's Fortress high-tower banner preview", "mtllib wf_stage.mtl"]
    vertex_index = 1
    uv_index = 1
    normal_index = 1
    last_mat = None
    for tri in triangles:
        mat = material_name(tri.texture, tri.rgba, tri.source)
        if mat != last_mat:
            lines.append(f"usemtl {mat}")
            last_mat = mat
        tex_w, tex_h = texture_size(tri.texture, symbols)
        divisor = uv_fixed_divisor(tri.texture)
        for v in tri.verts:
            lines.append(f"v {v[0]:.6f} {v[1]:.6f} {v[2]:.6f}")
        for uv in tri.uvs:
            u = (uv[0] / divisor) / tex_w
            v = 1.0 - (uv[1] / divisor) / tex_h
            lines.append(f"vt {u:.6f} {v:.6f}")
        lines.append(f"vn {tri.normal[0]:.6f} {tri.normal[1]:.6f} {tri.normal[2]:.6f}")
        face = " ".join(f"{vertex_index + i}/{uv_index + i}/{normal_index}" for i in range(3))
        lines.append(f"f {face}")
        vertex_index += 3
        uv_index += 3
        normal_index += 1
    OUT_BASE.with_suffix(".obj").write_text("\n".join(lines))


def average_color(texture: str | None, rgba: tuple[int, int, int, int], symbols: dict[str, Path]) -> tuple[int, int, int]:
    if texture and texture in symbols:
        with Image.open(symbols[texture]).convert("RGBA") as image:
            return image.resize((1, 1), Image.Resampling.BOX).getpixel((0, 0))[:3]
    r, g, b, _a = rgba
    return r, g, b


def barycentric(px: float, py: float, pts: list[tuple[float, float]]) -> tuple[float, float, float] | None:
    (x0, y0), (x1, y1), (x2, y2) = pts
    denom = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
    if abs(denom) < 1e-6:
        return None
    w0 = ((y1 - y2) * (px - x2) + (x2 - x1) * (py - y2)) / denom
    w1 = ((y2 - y0) * (px - x2) + (x0 - x2) * (py - y2)) / denom
    w2 = 1.0 - w0 - w1
    if w0 < -0.001 or w1 < -0.001 or w2 < -0.001:
        return None
    return w0, w1, w2


def texture_cache(symbols: dict[str, Path]) -> dict[str, Image.Image]:
    cache: dict[str, Image.Image] = {}
    for name, path in symbols.items():
        try:
            cache[name] = Image.open(path).convert("RGBA")
        except OSError:
            continue
    return cache


def sample_triangle_color(
    tri: ObjTriangle,
    weights: tuple[float, float, float],
    textures: dict[str, Image.Image],
) -> tuple[int, int, int, int]:
    if tri.texture and tri.texture in textures:
        texture = textures[tri.texture]
        tex_w, tex_h = texture.size
        divisor = uv_fixed_divisor(tri.texture)
        s = sum(tri.uvs[i][0] * weights[i] for i in range(3)) / divisor
        t = sum(tri.uvs[i][1] * weights[i] for i in range(3)) / divisor
        x = int(math.floor(s)) % tex_w
        y = int(math.floor(t)) % tex_h
        return texture.getpixel((x, y))
    return tri.rgba


def draw_textured_triangle(
    image: Image.Image,
    depth: list[float],
    tri: ObjTriangle,
    pts2d: list[tuple[float, float]],
    pts3d: list[tuple[float, float, float]],
    textures: dict[str, Image.Image],
) -> None:
    width, height = image.size
    min_x = max(0, int(math.floor(min(p[0] for p in pts2d))))
    max_x = min(width - 1, int(math.ceil(max(p[0] for p in pts2d))))
    min_y = max(0, int(math.floor(min(p[1] for p in pts2d))))
    max_y = min(height - 1, int(math.ceil(max(p[1] for p in pts2d))))
    if min_x > max_x or min_y > max_y:
        return
    pixels = image.load()
    light = 0.46 + 0.54 * max(0.0, tri.normal[1])
    for y in range(min_y, max_y + 1):
        for x in range(min_x, max_x + 1):
            weights = barycentric(x + 0.5, y + 0.5, pts2d)
            if weights is None:
                continue
            z = sum(pts3d[i][2] * weights[i] for i in range(3))
            index = y * width + x
            if z < depth[index]:
                continue
            r, g, b, a = sample_triangle_color(tri, weights, textures)
            if a < 16:
                continue
            pixels[x, y] = (
                int(r * light),
                int(g * light),
                int(b * light),
                255,
            )
            depth[index] = z


def write_preview(
    triangles: list[ObjTriangle],
    symbols: dict[str, Path],
    yaw: float = -35.0,
    pitch: float = 24.0,
    draw_wire: bool = False,
) -> None:
    size = 1200
    image = Image.new("RGBA", (size, size), (245, 248, 250, 255))
    textures = texture_cache(symbols)
    yaw_r = math.radians(yaw)
    pitch_r = math.radians(pitch)
    cy, sy = math.cos(yaw_r), math.sin(yaw_r)
    cp, sp = math.cos(pitch_r), math.sin(pitch_r)

    def project(p: tuple[float, float, float]) -> tuple[float, float, float]:
        x, y, z = p
        rx = x * cy + z * sy
        rz = -x * sy + z * cy
        ry = y * cp - rz * sp
        depth = y * sp + rz * cp
        return rx, ry, depth

    projected = []
    for tri in triangles:
        pts = [project(v) for v in tri.verts]
        projected.append((sum(p[2] for p in pts) / 3.0, pts, tri))

    xs = [p[0] for _d, pts, _tri in projected for p in pts]
    ys = [p[1] for _d, pts, _tri in projected for p in pts]
    min_x, max_x = min(xs), max(xs)
    min_y, max_y = min(ys), max(ys)
    scale = 0.86 * size / max(max_x - min_x, max_y - min_y, 1.0)
    off_x = size / 2.0 - (min_x + max_x) / 2.0 * scale
    off_y = size / 2.0 + (min_y + max_y) / 2.0 * scale

    depth_buffer = [-1.0e9] * (size * size)
    for _depth, pts, tri in sorted(projected, key=lambda item: item[0]):
        pts2d = [(p[0] * scale + off_x, -p[1] * scale + off_y) for p in pts]
        draw_textured_triangle(image, depth_buffer, tri, pts2d, pts, textures)
    if draw_wire:
        # Soft wire hint only on top, so we can see broken geometry without
        # hiding the actual texture read.
        draw = ImageDraw.Draw(image, "RGBA")
        for _depth, pts, _tri in sorted(projected, key=lambda item: item[0]):
            poly = [(p[0] * scale + off_x, -p[1] * scale + off_y) for p in pts]
            draw.line([*poly, poly[0]], fill=(0, 0, 0, 24), width=1)
    image.save(OUT_BASE.with_name(f"{OUT_BASE.name}_preview.png"))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target-width", type=float, default=9.0, help="normalized OBJ XZ width")
    parser.add_argument("--skip-problem-monsters", action="store_true", help="skip animated piranha plants/small whomps for debugging")
    parser.add_argument("--wire", action="store_true", help="draw a faint triangle wire overlay in the preview")
    args = parser.parse_args()

    raw = collect_triangles(ROOT, not args.skip_problem_monsters)
    normalized = normalized_triangles(raw, args.target_width)
    symbols = texture_symbols(ROOT)
    write_obj(normalized, symbols)
    write_preview(normalized, symbols, draw_wire=args.wire)

    sources = defaultdict(int)
    for tri in raw:
        sources[tri.source] += 1
    print(f"wrote {OUT_BASE.with_suffix('.obj')}")
    print(f"triangles: {len(raw)}")
    print(f"sources: {len(sources)}")
    for source, count in sorted(sources.items()):
        print(f"  {source}: {count}")


if __name__ == "__main__":
    main()
