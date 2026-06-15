#!/usr/bin/env python3
"""Render composed top-down minimap drafts from SM64 level data.

This stitches the area display list, selected script-placed terrain objects,
collision special geometry, and movtex water/quicksand regions into a temporary
top-down scene. It is a build/calibration tool for minimap art, not a full
Fast3D or behavior renderer.
"""

from __future__ import annotations

import argparse
import ast
import math
import re
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageDraw


MAP_MIN = -8192
MAP_MAX = 8192
MAP_SIZE = 256
DEFAULT_SCALE = 4


SCREEN_ANCHORS: dict[tuple[str, str], tuple[float, float]] = {
    ("bbh", "1"): (130.55, 196.84),
    ("castle_inside", "2"): (104.71, 158.69),
    ("castle_courtyard", "1"): (120.83, 117.16),
    ("hmc", "1"): (16.11, 224.03),
    ("ssl", "1"): (130.93, 214.88),
    ("sl", "1"): (200.71, 124.69),
    ("wdw", "1"): (170.71, 122.69),
    ("thi", "1"): (12.71, 225.69),
    ("ttc", "1"): (140.77, 110.57),
    ("rr", "1"): (159.77, 148.72),
    ("vcutm", "1"): (30.71, 28.69),
    ("totwc", "1"): (61.97, 118.98),
    ("bitdw", "1"): (11.71, 175.69),
    ("bitfs", "1"): (9.71, 118.69),
    ("bits", "1"): (17.68, 118.74),
    ("ddd", "1"): (75.85, 119.74),
    ("wmotr", "1"): (121.50, 120.31),
    ("ttm", "1"): (122.71, 202.69),
    ("bowser_1", "1"): (120.00, 120.00),
    ("bowser_2", "1"): (120.00, 120.00),
    ("bowser_3", "1"): (120.00, 120.00),
    ("sa", "1"): (120.00, 120.00),
}


@dataclass(frozen=True)
class Vertex:
    x: int
    y: int
    z: int
    s: int
    t: int
    rgba: tuple[int, int, int, int]


@dataclass(frozen=True)
class Triangle:
    vertices: tuple[Vertex, Vertex, Vertex]
    texture: str | None
    avg_y: float
    normal_y: float


@dataclass(frozen=True)
class Transform:
    x: int = 0
    y: int = 0
    z: int = 0
    yaw_degrees: float = 0.0


@dataclass(frozen=True)
class PlacedGeo:
    geo: str
    transform: Transform
    source: str


@dataclass(frozen=True)
class MovtexRegion:
    name: str
    points: tuple[tuple[int, int], ...]
    color: tuple[int, int, int, int]
    texture: str | None = None
    avg_y: float | None = None


@dataclass(frozen=True)
class WaterBox:
    box_id: int
    x1: int
    z1: int
    x2: int
    z2: int
    y: int


@dataclass(frozen=True)
class MapMarker:
    kind: str
    x: int
    z: int


@dataclass
class RenderState:
    texture: str | None = None
    transform: Transform = Transform()
    vertex_cache: dict[int, Vertex] | None = None

    def __post_init__(self) -> None:
        if self.vertex_cache is None:
            self.vertex_cache = {}


def parse_int(value: str) -> int:
    value = value.strip()
    tree = ast.parse(value, mode="eval")

    def eval_node(node: ast.AST) -> int:
        if isinstance(node, ast.Expression):
            return eval_node(node.body)
        if isinstance(node, ast.Constant) and isinstance(node.value, int):
            return node.value
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
            return -eval_node(node.operand)
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.UAdd):
            return eval_node(node.operand)
        raise ValueError(f"unsupported integer expression: {value}")

    return eval_node(tree)


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//.*", "", text)


def split_args(args: str) -> list[str]:
    parts: list[str] = []
    depth = 0
    start = 0
    for index, char in enumerate(args):
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
        elif char == "," and depth == 0:
            parts.append(args[start:index].strip())
            start = index + 1
    tail = args[start:].strip()
    if tail:
        parts.append(tail)
    return parts


def find_level_area_model_files(root: Path, level: str, area: str) -> list[Path]:
    area_dir = root / "levels" / level / "areas" / area
    return sorted(area_dir.glob("*/model.inc.c"))


def find_level_model_files(root: Path, level: str) -> list[Path]:
    level_dir = root / "levels" / level
    return sorted(level_dir.glob("**/model.inc.c"))


def find_aux_model_files(root: Path) -> list[Path]:
    return [
        root / "actors" / "checkerboard_platform" / "model.inc.c",
    ]


def iter_level_areas(root: Path) -> list[tuple[str, str]]:
    areas: list[tuple[str, str]] = []
    for area_dir in sorted((root / "levels").glob("*/areas/*")):
        if area_dir.is_dir() and any(area_dir.glob("*/model.inc.c")):
            areas.append((area_dir.parents[1].name, area_dir.name))
    return areas


def parse_mario_start(root: Path, level: str) -> tuple[str, int, int, int, int] | None:
    script_path = root / "levels" / level / "script.c"
    if not script_path.exists():
        return None
    text = strip_comments(script_path.read_text())
    match = re.search(
        r"MARIO_POS\s*\(\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^)]+)\)",
        text,
    )
    if not match:
        return None
    area = str(parse_int(match.group(1)))
    yaw = parse_int(match.group(2))
    x = parse_int(match.group(3))
    y = parse_int(match.group(4))
    z = parse_int(match.group(5))
    return area, yaw, x, y, z


def parse_geo_display_lists(root: Path, level: str, area: str) -> list[str]:
    geo_path = root / "levels" / level / "areas" / area / "geo.inc.c"
    if not geo_path.exists():
        return []
    text = strip_comments(geo_path.read_text())
    return re.findall(r"GEO_DISPLAY_LIST\s*\(\s*[^,]+,\s*([A-Za-z0-9_]+)\s*\)", text)


def parse_all_geo_display_lists(root: Path, level: str) -> dict[str, list[str]]:
    geos: dict[str, list[str]] = {}
    paths = [
        *sorted((root / "levels" / level).glob("**/geo.inc.c")),
        *sorted((root / "actors").glob("**/geo.inc.c")),
    ]
    for path in paths:
        text = strip_comments(path.read_text())
        for match in re.finditer(r"const\s+GeoLayout\s+([A-Za-z0-9_]+)\[\]\s*=\s*\{(.*?)\};", text, re.S):
            geos[match.group(1)] = re.findall(
                r"GEO_DISPLAY_LIST\s*\(\s*[^,]+,\s*([A-Za-z0-9_]+)\s*\)",
                match.group(2),
            )
    return geos


def parse_model_geo_map(root: Path, level: str) -> dict[str, str]:
    script_path = root / "levels" / level / "script.c"
    if not script_path.exists():
        return {}
    text = strip_comments(script_path.read_text())
    mapping: dict[str, str] = {}
    for match in re.finditer(r"LOAD_MODEL_FROM_GEO\s*\(\s*([^,]+),\s*([^)]+)\)", text):
        mapping[match.group(1).strip()] = match.group(2).strip()
    if level == "vcutm":
        mapping["MODEL_VCUTM_CHECKERBOARD_PLATFORM_SPAWNER"] = "checkerboard_platform_geo"
    return mapping


def should_include_object_model(model: str, behavior: str, include_dynamic: bool) -> bool:
    if model == "MODEL_NONE":
        return False
    if any(token in model for token in ("COIN", "STAR", "GOOMBA", "BOBOMB", "BUTTERFLY", "BULLET_BILL", "PIRANHA", "1UP")):
        return False
    if behavior in {"bhvStar", "bhvHiddenStar", "bhvHiddenRedCoinStar", "bhv1Up", "bhvButterfly"}:
        return False
    if model.startswith("MODEL_LEVEL_GEOMETRY_"):
        return True
    if behavior in {
        "bhvStaticObject",
        "bhvTower",
        "bhvTowerDoor",
        "bhvTowerPlatformGroup",
        "bhvBulletBillCannon",
        "bhvGiantPole",
        "bhvSmallBomp",
        "bhvLargeBomp",
        "bhvWfRotatingWoodenPlatform",
        "bhvWfSlidingPlatform",
        "bhvWfTumblingBridge",
        "bhvWfBreakableWallRight",
        "bhvWfBreakableWallLeft",
        "bhvKickableBoard",
        "bhvRotatingPlatform",
        "bhvPyramidTop",
        "bhvToxBox",
        "bhvGrindel",
        "bhvHorizontalGrindel",
        "bhvSpindel",
        "bhvSslMovingPyramidWall",
        "bhvPyramidElevator",
    }:
        return True
    if include_dynamic and (
        "_PLATFORM" in model
        or "_BRIDGE" in model
        or "_WALL" in model
        or "_DOOR" in model
        or "_POLE" in model
        or "_BOMP" in model
    ):
        return True
    return False


def parse_script_object_geos(root: Path, level: str, include_dynamic: bool) -> list[PlacedGeo]:
    script_path = root / "levels" / level / "script.c"
    if not script_path.exists():
        return []
    text = strip_comments(script_path.read_text())
    model_geos = parse_model_geo_map(root, level)
    placed: list[PlacedGeo] = []
    object_re = re.compile(r"\bOBJECT(?:_WITH_ACTS)?\s*\((.*?)\)", re.S)
    for match in object_re.finditer(text):
        args = split_args(match.group(1))
        if len(args) < 9:
            continue
        model = args[0].strip()
        behavior = args[8].strip()
        if not should_include_object_model(model, behavior, include_dynamic):
            continue
        geo = model_geos.get(model)
        if not geo:
            continue
        placed.append(
            PlacedGeo(
                geo,
                Transform(
                    parse_int(args[1]),
                    parse_int(args[2]),
                    parse_int(args[3]),
                    float(parse_int(args[5])),
                ),
                f"{model}/{behavior}",
            )
        )
    return placed


def special_model_name(preset: str) -> str | None:
    match = re.fullmatch(r"special_level_geo_([0-9A-Fa-f]+)", preset)
    if match:
        return f"MODEL_LEVEL_GEOMETRY_{match.group(1).upper()}"
    return None


def parse_collision_special_geos(root: Path, level: str, area: str) -> list[PlacedGeo]:
    collision_path = root / "levels" / level / "areas" / area / "collision.inc.c"
    if not collision_path.exists():
        return []
    text = strip_comments(collision_path.read_text())
    model_geos = parse_model_geo_map(root, level)
    placed: list[PlacedGeo] = []
    special_re = re.compile(r"\b(SPECIAL_OBJECT(?:_WITH_YAW)?)\s*\((.*?)\)", re.S)
    for match in special_re.finditer(text):
        macro = match.group(1)
        args = split_args(match.group(2))
        if len(args) < 4:
            continue
        model = special_model_name(args[0].strip())
        if not model:
            continue
        geo = model_geos.get(model)
        if not geo:
            continue
        yaw = float(parse_int(args[4]) * 360.0 / 256.0) if macro.endswith("_WITH_YAW") and len(args) >= 5 else 0.0
        placed.append(
            PlacedGeo(
                geo,
                Transform(parse_int(args[1]), parse_int(args[2]), parse_int(args[3]), yaw),
                args[0].strip(),
            )
        )
    return placed


def parse_collision_map_markers(root: Path, level: str, area: str) -> list[MapMarker]:
    collision_path = root / "levels" / level / "areas" / area / "collision.inc.c"
    if not collision_path.exists():
        return []
    text = strip_comments(collision_path.read_text())
    markers: list[MapMarker] = []
    special_re = re.compile(r"\bSPECIAL_OBJECT(?:_WITH_YAW)?\s*\((.*?)\)", re.S)
    for match in special_re.finditer(text):
        args = split_args(match.group(1))
        if len(args) < 4:
            continue
        preset = args[0].strip()
        if preset in {"special_bubble_tree", "special_spiky_tree", "special_snow_tree", "special_unknown_tree", "special_palm_tree"}:
            markers.append(MapMarker("tree", parse_int(args[1]), parse_int(args[3])))
    return markers


def parse_collision_water_boxes(root: Path, level: str, area: str) -> list[WaterBox]:
    collision_path = root / "levels" / level / "areas" / area / "collision.inc.c"
    if not collision_path.exists():
        return []
    text = strip_comments(collision_path.read_text())
    boxes: list[WaterBox] = []
    for match in re.finditer(r"\bCOL_WATER_BOX\s*\((.*?)\)", text, re.S):
        args = split_args(match.group(1))
        if len(args) < 6:
            continue
        boxes.append(
            WaterBox(
                parse_int(args[0]),
                parse_int(args[1]),
                parse_int(args[2]),
                parse_int(args[3]),
                parse_int(args[4]),
                parse_int(args[5]),
            )
        )
    return boxes


def match_water_box_height(points: list[tuple[int, int]], water_boxes: list[WaterBox]) -> int | None:
    if len(points) < 4:
        return None
    min_x = min(x for x, _z in points)
    max_x = max(x for x, _z in points)
    min_z = min(z for _x, z in points)
    max_z = max(z for _x, z in points)
    for box in water_boxes:
        if (
            min(box.x1, box.x2) == min_x
            and max(box.x1, box.x2) == max_x
            and min(box.z1, box.z2) == min_z
            and max(box.z1, box.z2) == max_z
        ):
            return box.y
    return None


def convex_hull(points: list[tuple[int, int]]) -> list[tuple[int, int]]:
    unique = sorted(set(points))
    if len(unique) <= 2:
        return unique

    def cross(o: tuple[int, int], a: tuple[int, int], b: tuple[int, int]) -> int:
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    lower: list[tuple[int, int]] = []
    for point in unique:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], point) <= 0:
            lower.pop()
        lower.append(point)
    upper: list[tuple[int, int]] = []
    for point in reversed(unique):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], point) <= 0:
            upper.pop()
        upper.append(point)
    return lower[:-1] + upper[:-1]


def parse_movtex_regions(root: Path, level: str, area: str, include_water: bool = False) -> list[MovtexRegion]:
    movtex_path = root / "levels" / level / "areas" / area / "movtext.inc.c"
    if not movtex_path.exists():
        return []
    text = strip_comments(movtex_path.read_text())
    regions: list[MovtexRegion] = []
    gfx_lists = parse_display_lists(text)
    water_boxes = parse_collision_water_boxes(root, level, area)
    array_re = re.compile(r"(?:static\s+)?Movtex\s+([A-Za-z0-9_]+)\[\]\s*=\s*\{(.*?)\};", re.S)
    for match in array_re.finditer(text):
        name = match.group(1)
        body = match.group(2)
        lower_name = name.lower()
        if "mist" in lower_name:
            continue
        is_water = "water" in lower_name and "sand" not in lower_name
        if is_water and not include_water:
            continue
        if not any(tag in lower_name for tag in ("quicksand", "sand", "water")):
            continue
        texture = movtex_texture_name(name)

        box_points = [
            (parse_int(box_match.group(1)), parse_int(box_match.group(2)))
            for box_match in re.finditer(r"MOV_TEX_4_BOX_TRIS\s*\(\s*([^,]+),\s*([^)]+)\)", body)
        ]
        if len(box_points) >= 4:
            color = (81, 138, 204, 170) if is_water else (204, 160, 73, 185)
            regions.append(MovtexRegion(name, tuple(box_points), color, texture, match_water_box_height(box_points, water_boxes)))
            continue

        rot_vertices = [
            (parse_int(rot_match.group(1)), parse_int(rot_match.group(2)), parse_int(rot_match.group(3)))
            for rot_match in re.finditer(r"MOV_TEX_ROT_TRIS\s*\(\s*([^,]+),\s*([^,]+),\s*([^,]+),", body)
        ]
        rot_points = [(x, z) for x, _y, z in rot_vertices]
        if len(rot_points) >= 3:
            color = (204, 160, 73, 185) if "sand" in lower_name or "quicksand" in lower_name else (81, 138, 204, 170)
            display_list_name = re.sub(r"_movtex_tris_", "_dl_", name)
            display_list = gfx_lists.get(display_list_name)
            if display_list:
                for indices in parse_triangle_indices(display_list):
                    if all(0 <= index < len(rot_points) for index in indices):
                        avg_y = sum(rot_vertices[index][1] for index in indices) / 3.0
                        regions.append(MovtexRegion(name, tuple(rot_points[index] for index in indices), color, texture, avg_y))
            else:
                hull = convex_hull(rot_points)
                if len(hull) >= 3:
                    avg_y = sum(y for _x, y, _z in rot_vertices) / len(rot_vertices)
                    regions.append(MovtexRegion(name, tuple(hull), color, texture, avg_y))
    return regions


def movtex_texture_name(name: str) -> str | None:
    lower_name = name.lower()
    if "pyramid" in lower_name and ("sand" in lower_name or "quicksand" in lower_name):
        return "ssl_pyramid_sand"
    if "quicksand" in lower_name or "sand" in lower_name:
        return "ssl_quicksand"
    return None


def transform_point_2d(x: int, z: int, transform: Transform) -> tuple[int, int]:
    angle = math.radians(transform.yaw_degrees)
    cos_y = math.cos(angle)
    sin_y = math.sin(angle)
    return (
        round(x * cos_y + z * sin_y + transform.x),
        round(-x * sin_y + z * cos_y + transform.z),
    )


def parse_named_movtex_light_regions(root: Path, level: str, movtex_name: str, display_list_name: str) -> list[MovtexRegion]:
    text = "\n".join(strip_comments(path.read_text()) for path in sorted((root / "levels" / level).glob("**/model.inc.c")))
    array_match = re.search(rf"(?:static\s+)?Movtex\s+{re.escape(movtex_name)}\[\]\s*=\s*\{{(.*?)\}};", text, re.S)
    if not array_match:
        return []
    vertices = [
        (parse_int(point_match.group(1)), parse_int(point_match.group(2)), parse_int(point_match.group(3)))
        for point_match in re.finditer(r"MOV_TEX_LIGHT_TRIS\s*\(\s*([^,]+),\s*([^,]+),\s*([^,]+),", array_match.group(1))
    ]
    points = [(x, z) for x, _y, z in vertices]
    display_list = parse_display_lists(text).get(display_list_name)
    if not display_list:
        return []
    texture = movtex_texture_name(movtex_name)
    regions: list[MovtexRegion] = []
    for indices in parse_triangle_indices(display_list):
        if all(0 <= index < len(points) for index in indices):
            avg_y = sum(vertices[index][1] for index in indices) / 3.0
            regions.append(MovtexRegion(movtex_name, tuple(points[index] for index in indices), (204, 160, 73, 220), texture, avg_y))
    return regions


def parse_special_movtex_regions(root: Path, level: str, area: str) -> list[MovtexRegion]:
    regions: list[MovtexRegion] = []
    for placed in parse_collision_special_geos(root, level, area):
        if placed.source == "special_level_geo_03":
            local_regions = parse_named_movtex_light_regions(root, level, "ssl_movtex_tris_quicksand_pit", "ssl_dl_quicksand_pit")
        elif placed.source == "special_level_geo_04":
            local_regions = parse_named_movtex_light_regions(root, level, "ssl_movtex_tris_pyramid_quicksand_pit", "ssl_dl_quicksand_pit")
        else:
            continue
        for region in local_regions:
            regions.append(
                MovtexRegion(
                    region.name,
                    tuple(transform_point_2d(x, z, placed.transform) for x, z in region.points),
                    region.color,
                    region.texture,
                    None if region.avg_y is None else region.avg_y + placed.transform.y,
                )
            )
    return regions


def parse_triangle_indices(display_list: str) -> list[tuple[int, int, int]]:
    triangles: list[tuple[int, int, int]] = []
    macro_re = re.compile(r"\b(gsSP2Triangles|gsSP1Triangle)\s*\((.*?)\)", re.S)
    for match in macro_re.finditer(display_list):
        macro = match.group(1)
        args = split_args(match.group(2))
        if macro == "gsSP1Triangle" and len(args) >= 3:
            triangles.append((parse_int(args[0]), parse_int(args[1]), parse_int(args[2])))
        elif macro == "gsSP2Triangles" and len(args) >= 7:
            triangles.append((parse_int(args[0]), parse_int(args[1]), parse_int(args[2])))
            triangles.append((parse_int(args[4]), parse_int(args[5]), parse_int(args[6])))
    return triangles


def parse_vertices(text: str) -> dict[str, list[Vertex]]:
    arrays: dict[str, list[Vertex]] = {}
    array_re = re.compile(r"static\s+const\s+Vtx\s+([A-Za-z0-9_]+)\[\]\s*=\s*\{(.*?)\};", re.S)
    vertex_re = re.compile(
        r"\{\{\{\s*([^,]+),\s*([^,]+),\s*([^}]+)\}\s*,\s*[^,]+,\s*"
        r"\{\s*([^,]+),\s*([^}]+)\}\s*,\s*"
        r"\{\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^}]+)\}\s*\}\}"
    )
    for array_match in array_re.finditer(text):
        name = array_match.group(1)
        body = array_match.group(2)
        vertices: list[Vertex] = []
        for match in vertex_re.finditer(body):
            rgba = tuple(parse_int(match.group(i)) & 0xFF for i in range(6, 10))
            vertices.append(
                Vertex(
                    parse_int(match.group(1)),
                    parse_int(match.group(2)),
                    parse_int(match.group(3)),
                    parse_int(match.group(4)),
                    parse_int(match.group(5)),
                    rgba,  # type: ignore[arg-type]
                )
            )
        arrays[name] = vertices
    return arrays


def parse_display_lists(text: str) -> dict[str, str]:
    display_lists: dict[str, str] = {}
    dl_re = re.compile(r"(?:static\s+const|const)\s+Gfx\s+([A-Za-z0-9_]+)\[\]\s*=\s*\{(.*?)\};", re.S)
    for match in dl_re.finditer(text):
        display_lists[match.group(1)] = match.group(2)
    return display_lists


def parse_texture_symbols(root: Path) -> dict[str, Path]:
    symbols: dict[str, Path] = {}
    texture_re = re.compile(
        r"(?:ALIGNED8\s+)?(?:static\s+)?const\s+u8\s+([A-Za-z0-9_]+)\[\]\s*=\s*\{\s*"
        r"#include\s+\"([^\"]+?)(?:\.inc\.c)?\"\s*\};",
        re.S,
    )
    paths = [
        *sorted((root / "levels").glob("*/texture.inc.c")),
        *sorted((root / "levels").glob("**/model.inc.c")),
        *sorted((root / "bin").glob("*.c")),
    ]
    for path in paths:
        text = strip_comments(path.read_text())
        for match in texture_re.finditer(text):
            include_path = match.group(2)
            png = root / f"{include_path}.png"
            if png.exists():
                symbols[match.group(1)] = png
    return symbols


def collect_level_geometry(
    root: Path,
    level: str,
    area: str,
    include_level_models: bool,
) -> tuple[dict[str, list[Vertex]], dict[str, str], list[str], dict[str, list[str]]]:
    texts: list[str] = []
    model_files = find_level_model_files(root, level) if include_level_models else find_level_area_model_files(root, level, area)
    if include_level_models:
        model_files.extend(path for path in find_aux_model_files(root) if path.exists())
    for path in model_files:
        texts.append(strip_comments(path.read_text()))
    text = "\n".join(texts)
    return parse_vertices(text), parse_display_lists(text), parse_geo_display_lists(root, level, area), parse_all_geo_display_lists(root, level)


def triangle_normal_y(a: Vertex, b: Vertex, c: Vertex) -> float:
    ux, uy, uz = b.x - a.x, b.y - a.y, b.z - a.z
    vx, vy, vz = c.x - a.x, c.y - a.y, c.z - a.z
    nx = uy * vz - uz * vy
    ny = uz * vx - ux * vz
    nz = ux * vy - uy * vx
    length = math.sqrt(nx * nx + ny * ny + nz * nz)
    if length == 0:
        return 0.0
    return ny / length


def run_display_list(
    name: str,
    display_lists: dict[str, str],
    vertices: dict[str, list[Vertex]],
    state: RenderState,
    triangles: list[Triangle],
    visited: set[str] | None = None,
) -> None:
    if visited is None:
        visited = set()
    if name in visited or name not in display_lists:
        return
    visited.add(name)

    body = display_lists[name]
    macro_re = re.compile(r"\b(gsDPSetTextureImage|gsSPVertex|gsSP2Triangles|gsSP1Triangle|gsSPDisplayList)\s*\((.*?)\)", re.S)
    for match in macro_re.finditer(body):
        macro = match.group(1)
        args = split_args(match.group(2))
        if macro == "gsDPSetTextureImage" and len(args) >= 4:
            state.texture = args[3]
        elif macro == "gsSPVertex" and len(args) >= 3:
            array_name = args[0]
            count = parse_int(args[1])
            start_index = parse_int(args[2])
            source = vertices.get(array_name, [])
            for offset, vertex in enumerate(source[:count]):
                state.vertex_cache[start_index + offset] = transform_vertex(vertex, state.transform)
        elif macro == "gsSPDisplayList" and args:
            run_display_list(args[0], display_lists, vertices, state, triangles, visited.copy())
        elif macro == "gsSP1Triangle" and len(args) >= 3:
            append_triangle(state, triangles, (parse_int(args[0]), parse_int(args[1]), parse_int(args[2])))
        elif macro == "gsSP2Triangles" and len(args) >= 7:
            append_triangle(state, triangles, (parse_int(args[0]), parse_int(args[1]), parse_int(args[2])))
            append_triangle(state, triangles, (parse_int(args[4]), parse_int(args[5]), parse_int(args[6])))


def transform_vertex(vertex: Vertex, transform: Transform) -> Vertex:
    if transform == Transform():
        return vertex
    angle = math.radians(transform.yaw_degrees)
    cos_y = math.cos(angle)
    sin_y = math.sin(angle)
    x = vertex.x * cos_y + vertex.z * sin_y + transform.x
    z = -vertex.x * sin_y + vertex.z * cos_y + transform.z
    y = vertex.y + transform.y
    return Vertex(round(x), round(y), round(z), vertex.s, vertex.t, vertex.rgba)


def append_triangle(state: RenderState, triangles: list[Triangle], indices: tuple[int, int, int]) -> None:
    if state.vertex_cache is None or any(index not in state.vertex_cache for index in indices):
        return
    verts = tuple(state.vertex_cache[index] for index in indices)
    normal_y = triangle_normal_y(*verts)
    avg_y = sum(vertex.y for vertex in verts) / 3.0
    triangles.append(Triangle(verts, state.texture, avg_y, normal_y))


def compute_bounds(triangles: list[Triangle], fit: bool, padding: int) -> tuple[int, int, int, int]:
    if not fit or not triangles:
        return (MAP_MIN, MAP_MAX, MAP_MIN, MAP_MAX)
    all_vertices = [vertex for triangle in triangles for vertex in triangle.vertices]
    min_x = min(vertex.x for vertex in all_vertices) - padding
    max_x = max(vertex.x for vertex in all_vertices) + padding
    min_z = min(vertex.z for vertex in all_vertices) - padding
    max_z = max(vertex.z for vertex in all_vertices) + padding
    span = max(max_x - min_x, max_z - min_z, 1)
    mid_x = (min_x + max_x) // 2
    mid_z = (min_z + max_z) // 2
    half = span // 2
    return (mid_x - half, mid_x + half, mid_z - half, mid_z + half)


def world_to_pixel(x: float, z: float, bounds: tuple[int, int, int, int], size: int) -> tuple[float, float]:
    min_x, max_x, min_z, max_z = bounds
    px = (x - min_x) * (size - 1) / max(1, max_x - min_x)
    py = (z - min_z) * (size - 1) / max(1, max_z - min_z)
    return px, py


def apply_screen_anchor_bounds(
    level: str,
    area: str,
    bounds: tuple[int, int, int, int],
    mario_start: tuple[str, int, int, int, int] | None,
    size: int,
) -> tuple[int, int, int, int]:
    anchor = SCREEN_ANCHORS.get((level, area))
    if anchor is None or mario_start is None or mario_start[0] != area:
        return bounds

    _area, _yaw, mario_x, _mario_y, mario_z = mario_start
    min_x, max_x, min_z, max_z = bounds
    span_x = max_x - min_x
    span_z = max_z - min_z
    desired_x = anchor[0] * (size - 1) / 240.0
    desired_y = anchor[1] * (size - 1) / 240.0
    new_min_x = round(mario_x - desired_x * span_x / max(1, size - 1))
    new_min_z = round(mario_z - desired_y * span_z / max(1, size - 1))
    return (new_min_x, new_min_x + span_x, new_min_z, new_min_z + span_z)


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


def sample_texture(texture: Image.Image, s: float, t: float) -> tuple[int, int, int, int]:
    width, height = texture.size
    x = int(math.floor(s / 32.0)) % width
    y = int(math.floor(t / 32.0)) % height
    return texture.getpixel((x, y))


def fallback_color(triangle: Triangle) -> tuple[int, int, int, int]:
    r = sum(vertex.rgba[0] for vertex in triangle.vertices) // 3
    g = sum(vertex.rgba[1] for vertex in triangle.vertices) // 3
    b = sum(vertex.rgba[2] for vertex in triangle.vertices) // 3
    if max(r, g, b) <= 0x80:
        r = int(90 + 90 * max(0.0, abs(triangle.normal_y)))
        g = int(100 + 90 * max(0.0, abs(triangle.normal_y)))
        b = int(92 + 70 * max(0.0, abs(triangle.normal_y)))
    return (r, g, b, 255)


def projected_area_xz(triangle: Triangle) -> float:
    pts = [(vertex.x, vertex.z) for vertex in triangle.vertices]
    area2 = abs(
        sum(
            pts[index][0] * pts[(index + 1) % 3][1]
            - pts[(index + 1) % 3][0] * pts[index][1]
            for index in range(3)
        )
    )
    return area2 / 2.0


def should_skip_for_topdown(level: str, triangle: Triangle) -> bool:
    area = projected_area_xz(triangle)
    y = triangle.avg_y
    ny = triangle.normal_y
    if level == "vcutm":
        return y > 6000.0 and abs(ny) > 0.95 and area > 500000.0
    if level == "jrb":
        return ny < -0.90 and area > 1000000.0
    if level == "cotmc":
        return ny < -0.70
    if level == "ssl":
        # Pyramid interior: remove upward-facing pyramid slopes at Y>700
        return y > 700.0 and ny > 0.3 and area > 100000.0
    if level == "wdw":
        # WDW town: remove overhead platforms/roofs at Y>3500
        return y > 3500.0 and abs(ny) > 0.3 and area > 100000.0
    if level == "ttc":
        # Tick Tock Clock: remove ceiling at Y>7500
        return y > 7500.0 and ny < -0.3 and area > 100000.0
    if level == "bbh":
        # Big Boo's Haunt: remove mansion roof/ceiling at Y>3000
        return y > 3000.0 and area > 200000.0
    if level == "hmc":
        # Hazy Maze Cave: remove cave ceiling/roof at Y>2700
        return y > 2700.0 and abs(ny) > 0.3 and area > 200000.0
    if level == "pss":
        # Princess's Secret Slide: remove slide ceiling at Y>5500
        return y > 5500.0 and abs(ny) > 0.3 and area > 200000.0
    if level == "sl":
        # Snowman's Land: remove snowman/igloo roof at Y>4500
        return y > 4500.0 and ny > 0.3 and area > 200000.0
    if level == "totwc":
        # Tower of the Wing Cap: remove ceiling at Y>2000
        return y > 2000.0 and ny < -0.3 and area > 200000.0
    return False


def draw_grid(draw: ImageDraw.ImageDraw, size: int) -> None:
    for i in range(0, size + 1, 16):
        color = (32, 44, 54, 210)
        width = 1
        if i in (0, size // 2, size):
            color = (74, 132, 154, 245)
            width = 2
        draw.line([(i, 0), (i, size)], fill=color, width=width)
        draw.line([(0, i), (size, i)], fill=color, width=width)
    c = size // 2
    draw.ellipse((c - 8, c - 8, c + 8, c + 8), outline=(255, 180, 64, 255), width=2)
    draw.line([(c, c - 16), (c, c + 16)], fill=(255, 180, 64, 255), width=1)
    draw.line([(c - 16, c), (c + 16, c)], fill=(255, 180, 64, 255), width=1)


def draw_mario_start_marker(image: Image.Image, x: int, z: int, yaw: int) -> None:
    draw = ImageDraw.Draw(image, "RGBA")
    width, height = image.size
    px = max(0.0, min(float(width), (x + 8192.0) * width / 16384.0))
    py = max(0.0, min(float(height), (z + 8192.0) * height / 16384.0))
    radius = max(4, width // 64)
    draw.ellipse((px - radius, py - radius, px + radius, py + radius), fill=(255, 64, 64, 225), outline=(255, 255, 255, 255), width=2)

    angle = yaw / 65536.0 * math.tau
    tip = (px + math.sin(angle) * radius * 2.4, py - math.cos(angle) * radius * 2.4)
    left = (px + math.sin(angle + 2.55) * radius * 1.4, py - math.cos(angle + 2.55) * radius * 1.4)
    right = (px + math.sin(angle - 2.55) * radius * 1.4, py - math.cos(angle - 2.55) * radius * 1.4)
    draw.polygon([tip, left, right], fill=(255, 210, 64, 235), outline=(70, 36, 0, 255))


def ztest_pass(region: MovtexRegion, depth: list[float] | None, index: int, bias: float = 4.0) -> bool:
    if depth is None or region.avg_y is None:
        return True
    return region.avg_y >= depth[index] - bias


def blend_pixel(dst: tuple[int, int, int, int], src: tuple[int, int, int, int]) -> tuple[int, int, int, int]:
    a = src[3] / 255.0
    return (
        int(src[0] * a + dst[0] * (1.0 - a)),
        int(src[1] * a + dst[1] * (1.0 - a)),
        int(src[2] * a + dst[2] * (1.0 - a)),
        max(dst[3], src[3]),
    )


def draw_movtex_regions(
    image: Image.Image,
    regions: list[MovtexRegion],
    texture_paths: dict[str, Path],
    bounds: tuple[int, int, int, int],
    depth: list[float] | None,
) -> None:
    if not regions:
        return
    width, height = image.size
    textures: dict[str, Image.Image] = {}
    for region in regions:
        pts = [world_to_pixel(x, z, bounds, width) for x, z in region.points]
        if region.texture and region.texture in texture_paths:
            if region.texture not in textures:
                textures[region.texture] = Image.open(texture_paths[region.texture]).convert("RGBA")
            draw_textured_polygon(image, region, pts, textures[region.texture], bounds, depth)
        else:
            draw_flat_polygon(image, region, pts, depth)


def draw_flat_polygon(
    image: Image.Image,
    region: MovtexRegion,
    pts: list[tuple[float, float]],
    depth: list[float] | None,
) -> None:
    width, height = image.size
    min_px = max(0, int(math.floor(min(point[0] for point in pts))))
    max_px = min(width - 1, int(math.ceil(max(point[0] for point in pts))))
    min_py = max(0, int(math.floor(min(point[1] for point in pts))))
    max_py = min(height - 1, int(math.ceil(max(point[1] for point in pts))))
    if min_px > max_px or min_py > max_py:
        return

    mask = Image.new("L", image.size, 0)
    ImageDraw.Draw(mask).polygon(pts, fill=region.color[3])
    mask_pixels = mask.load()
    image_pixels = image.load()
    drew = False
    for py in range(min_py, max_py + 1):
        for px in range(min_px, max_px + 1):
            alpha = mask_pixels[px, py]
            if alpha == 0:
                continue
            index = py * width + px
            if not ztest_pass(region, depth, index):
                continue
            image_pixels[px, py] = blend_pixel(image_pixels[px, py], (*region.color[:3], alpha))
            if depth is not None and region.avg_y is not None:
                depth[index] = max(depth[index], region.avg_y)
            drew = True


def draw_map_markers(image: Image.Image, markers: list[MapMarker]) -> None:
    if not markers:
        return
    draw = ImageDraw.Draw(image, "RGBA")
    width, _height = image.size
    bounds = (MAP_MIN, MAP_MAX, MAP_MIN, MAP_MAX)
    for marker in markers:
        px, py = world_to_pixel(marker.x, marker.z, bounds, width)
        if marker.kind == "tree":
            r = max(5, width // 44)
            draw.ellipse((px - r, py - r, px + r, py + r), fill=(20, 92, 44, 230), outline=(8, 42, 21, 240), width=1)
            draw.ellipse((px - r * 0.55, py - r * 0.55, px + r * 0.55, py + r * 0.55), fill=(51, 135, 58, 225))
            draw.rectangle((px - 1, py + r * 0.15, px + 1, py + r * 1.25), fill=(104, 68, 36, 230))


def draw_textured_polygon(
    image: Image.Image,
    region: MovtexRegion,
    pts: list[tuple[float, float]],
    texture: Image.Image,
    bounds: tuple[int, int, int, int],
    depth: list[float] | None,
) -> None:
    width, height = image.size
    min_px = max(0, int(math.floor(min(point[0] for point in pts))))
    max_px = min(width - 1, int(math.ceil(max(point[0] for point in pts))))
    min_py = max(0, int(math.floor(min(point[1] for point in pts))))
    max_py = min(height - 1, int(math.ceil(max(point[1] for point in pts))))
    if min_px > max_px or min_py > max_py:
        return

    mask = Image.new("L", image.size, 0)
    ImageDraw.Draw(mask).polygon(pts, fill=region.color[3])
    mask_pixels = mask.load()
    image_pixels = image.load()
    tex_w, tex_h = texture.size
    min_x, max_x, min_z, max_z = bounds
    drew = False

    for py in range(min_py, max_py + 1):
        for px in range(min_px, max_px + 1):
            alpha = mask_pixels[px, py]
            if alpha == 0:
                continue
            index = py * width + px
            if not ztest_pass(region, depth, index):
                continue
            world_x = min_x + px * (max_x - min_x) / max(1, width - 1)
            world_z = min_z + py * (max_z - min_z) / max(1, height - 1)
            tx = int(math.floor(world_x / 64.0)) % tex_w
            ty = int(math.floor(world_z / 64.0)) % tex_h
            sr, sg, sb, sa = texture.getpixel((tx, ty))
            tint = region.color
            src = (
                sr * tint[0] // 255,
                sg * tint[1] // 255,
                sb * tint[2] // 255,
                min(sa, alpha),
            )
            image_pixels[px, py] = blend_pixel(image_pixels[px, py], src)
            if depth is not None and region.avg_y is not None:
                depth[index] = max(depth[index], region.avg_y)
            drew = True


def downsample_depth(zbuf: list[float], render_size: int, size: int, scale: int) -> list[float]:
    if scale <= 1:
        return zbuf
    depth = [-10**12.0] * (size * size)
    for py in range(size):
        for px in range(size):
            max_y = -10**12.0
            for sy in range(py * scale, min((py + 1) * scale, render_size)):
                start = sy * render_size + px * scale
                for value in zbuf[start : start + min(scale, render_size - px * scale)]:
                    if value > max_y:
                        max_y = value
            depth[py * size + px] = max_y
    return depth


def render(
    level: str,
    triangles: list[Triangle],
    texture_paths: dict[str, Path],
    bounds: tuple[int, int, int, int],
    size: int,
    scale: int,
    min_normal_y: float,
    include_walls: bool,
) -> tuple[Image.Image, list[float]]:
    render_size = size * scale
    img = Image.new("RGBA", (render_size, render_size), (0, 0, 0, 0))
    zbuf = [-10**12.0] * (render_size * render_size)
    pixels = img.load()
    textures: dict[str, Image.Image] = {}

    for triangle in sorted(triangles, key=lambda tri: tri.avg_y):
        if should_skip_for_topdown(level, triangle):
            continue
        if not include_walls and abs(triangle.normal_y) < min_normal_y:
            continue
        pts = [world_to_pixel(vertex.x, vertex.z, bounds, render_size) for vertex in triangle.vertices]
        min_px = max(0, int(math.floor(min(point[0] for point in pts))))
        max_px = min(render_size - 1, int(math.ceil(max(point[0] for point in pts))))
        min_py = max(0, int(math.floor(min(point[1] for point in pts))))
        max_py = min(render_size - 1, int(math.ceil(max(point[1] for point in pts))))
        if min_px > max_px or min_py > max_py:
            continue

        texture = None
        if triangle.texture and triangle.texture in texture_paths:
            if triangle.texture not in textures:
                textures[triangle.texture] = Image.open(texture_paths[triangle.texture]).convert("RGBA")
            texture = textures[triangle.texture]
        solid = fallback_color(triangle)
        shade = 0.72 + 0.28 * max(0.0, abs(triangle.normal_y))

        for py in range(min_py, max_py + 1):
            for px in range(min_px, max_px + 1):
                weights = barycentric(px + 0.5, py + 0.5, pts)
                if weights is None:
                    continue
                w0, w1, w2 = weights
                y = sum(w * vertex.y for w, vertex in zip(weights, triangle.vertices))
                z_index = py * render_size + px
                if y < zbuf[z_index]:
                    continue
                zbuf[z_index] = y
                if texture is not None:
                    s = w0 * triangle.vertices[0].s + w1 * triangle.vertices[1].s + w2 * triangle.vertices[2].s
                    t = w0 * triangle.vertices[0].t + w1 * triangle.vertices[1].t + w2 * triangle.vertices[2].t
                    color = sample_texture(texture, s, t)
                    pixels[px, py] = (
                        int(color[0] * shade),
                        int(color[1] * shade),
                        int(color[2] * shade),
                        color[3],
                    )
                else:
                    pixels[px, py] = solid

    depth = downsample_depth(zbuf, render_size, size, scale)
    if scale > 1:
        return img.resize((size, size), Image.Resampling.LANCZOS), depth
    return img, depth


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--level")
    parser.add_argument("--area", default="1")
    parser.add_argument("--all", action="store_true")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("minimap_work/textured"))
    parser.add_argument("--size", type=int, default=MAP_SIZE)
    parser.add_argument("--scale", type=int, default=DEFAULT_SCALE)
    parser.add_argument("--fit", action="store_true")
    parser.add_argument("--fit-padding", type=int, default=256)
    parser.add_argument("--include-walls", action="store_true")
    parser.add_argument("--min-normal-y", type=float, default=0.45)
    parser.add_argument("--grid", action="store_true")
    parser.add_argument("--mark-mario-start", action="store_true")
    parser.add_argument("--no-screen-anchor-calibration", action="store_true", help="do not align the render bounds to captured 3DS minimap screenshots")
    parser.add_argument("--compose-map", action="store_true", help="enable the usual full-map composition layers")
    parser.add_argument("--include-objects", action="store_true", help="compose script-placed terrain-like model objects")
    parser.add_argument("--include-special-objects", action="store_true", help="compose level geometry listed in collision special objects")
    parser.add_argument("--include-dynamic-objects", action="store_true", help="include broader platform/bridge/door object heuristics")
    parser.add_argument("--include-movtex", action="store_true", help="overlay moving texture regions such as water and quicksand")
    parser.add_argument("--include-water-movtex", action="store_true", help="also overlay movtex water surfaces; compose-map skips them by default")
    return parser


def render_level_area(args: argparse.Namespace, root: Path, level: str, area: str, texture_paths: dict[str, Path], output: Path) -> str:
    include_level_models = args.include_objects or args.include_special_objects
    vertices, display_lists, roots, geo_display_lists = collect_level_geometry(root, level, area, include_level_models)
    if not roots:
        roots = [name for name in display_lists if not name.startswith("static")]

    triangles: list[Triangle] = []
    state = RenderState(transform=Transform())
    for display_list in roots:
        run_display_list(display_list, display_lists, vertices, state, triangles)

    placed_geos: list[PlacedGeo] = []
    if args.include_objects:
        placed_geos.extend(parse_script_object_geos(root, level, args.include_dynamic_objects))
    if args.include_special_objects:
        placed_geos.extend(parse_collision_special_geos(root, level, area))

    for placed_geo in placed_geos:
        for display_list in geo_display_lists.get(placed_geo.geo, []):
            run_display_list(
                display_list,
                display_lists,
                vertices,
                RenderState(transform=placed_geo.transform),
                triangles,
            )

    mario_start = parse_mario_start(root, level)
    bounds = compute_bounds(triangles, args.fit, args.fit_padding)
    if not args.no_screen_anchor_calibration and not args.fit:
        bounds = apply_screen_anchor_bounds(level, area, bounds, mario_start, args.size)
    image, depth = render(
        level,
        triangles,
        texture_paths,
        bounds,
        args.size,
        args.scale,
        args.min_normal_y,
        args.include_walls,
    )
    movtex_regions = []
    if args.include_movtex:
        movtex_regions.extend(parse_movtex_regions(root, level, area, args.include_water_movtex))
        movtex_regions.extend(parse_special_movtex_regions(root, level, area))
    draw_movtex_regions(image, movtex_regions, texture_paths, bounds, depth)
    if args.grid:
        draw_grid(ImageDraw.Draw(image, "RGBA"), args.size)
    map_markers = parse_collision_map_markers(root, level, area) if args.include_special_objects else []
    draw_map_markers(image, map_markers)
    if args.mark_mario_start and mario_start is not None and mario_start[0] == area:
        _, yaw, x, _y, z = mario_start
        draw_mario_start_marker(image, x, z, yaw)
    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output)
    object_note = f", {len(placed_geos)} placed geos" if placed_geos else ""
    movtex_note = f", {len(movtex_regions)} movtex regions" if movtex_regions else ""
    marker_note = f", {len(map_markers)} markers" if map_markers else ""
    return f"rendered {level} area {area}: {len(triangles)} triangles{object_note}{movtex_note}{marker_note} -> {output}"


def main() -> None:
    args = build_arg_parser().parse_args()
    if args.compose_map:
        args.include_objects = True
        args.include_special_objects = True
        args.include_dynamic_objects = True
        args.include_movtex = True
    root = args.root.resolve()
    texture_paths = parse_texture_symbols(root)

    if args.all:
        for level, area in iter_level_areas(root):
            output = args.output_dir / f"{level}_area{area}_textured.png"
            print(render_level_area(args, root, level, area, texture_paths, output))
        return

    if not args.level:
        raise SystemExit("--level is required unless --all is used")
    output = args.output or (args.output_dir / f"{args.level}_area{args.area}_textured.png")
    print(render_level_area(args, root, args.level, args.area, texture_paths, output))


if __name__ == "__main__":
    main()
