#!/usr/bin/env python3
"""Generate rough minimap drafts from SM64 collision files.

This is a calibration tool, not final art. It projects collision triangles onto
the X/Z plane so the generated PNG can be compared with Mario's bottom-screen
marker in-game.
"""

from __future__ import annotations

import argparse
import ast
import re
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageDraw


MAP_MIN = -8192
MAP_MAX = 8192
MAP_SIZE = 256
DEFAULT_SCALE = 4


@dataclass(frozen=True)
class Vertex:
    x: int
    y: int
    z: int


@dataclass(frozen=True)
class Triangle:
    surface: str
    vertices: tuple[int, int, int]
    avg_y: float
    normal_y: float


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


def parse_collision(path: Path) -> tuple[list[Vertex], list[Triangle]]:
    text = strip_comments(path.read_text())
    macro_re = re.compile(r"\b(COL_VERTEX|COL_TRI_INIT|COL_TRI|COL_TRI_SPECIAL)\s*\(([^)]*)\)")

    vertices: list[Vertex] = []
    triangles: list[Triangle] = []
    current_surface = "SURFACE_DEFAULT"

    for match in macro_re.finditer(text):
        name = match.group(1)
        args = [arg.strip() for arg in match.group(2).split(",")]

        if name == "COL_VERTEX":
            if len(args) != 3:
                continue
            vertices.append(Vertex(parse_int(args[0]), parse_int(args[1]), parse_int(args[2])))
        elif name == "COL_TRI_INIT":
            if args:
                current_surface = args[0]
        elif name in {"COL_TRI", "COL_TRI_SPECIAL"}:
            if len(args) < 3:
                continue
            indices = tuple(parse_int(arg) for arg in args[:3])
            if any(index < 0 or index >= len(vertices) for index in indices):
                continue
            avg_y = sum(vertices[index].y for index in indices) / 3.0
            normal_y = triangle_normal_y(*(vertices[index] for index in indices))
            triangles.append(Triangle(current_surface, indices, avg_y, normal_y))

    return vertices, triangles


def triangle_normal_y(a: Vertex, b: Vertex, c: Vertex) -> float:
    ux, uy, uz = b.x - a.x, b.y - a.y, b.z - a.z
    vx, vy, vz = c.x - a.x, c.y - a.y, c.z - a.z
    nx = uy * vz - uz * vy
    ny = uz * vx - ux * vz
    nz = ux * vy - uy * vx
    length = (nx * nx + ny * ny + nz * nz) ** 0.5
    if length == 0:
        return 0.0
    return ny / length


def should_render_triangle(triangle: Triangle, include_walls: bool, include_death_planes: bool, min_normal_y: float) -> bool:
    if not include_death_planes and triangle.surface == "SURFACE_DEATH_PLANE":
        return False
    if include_walls:
        return True
    return abs(triangle.normal_y) >= min_normal_y


def palette_for_level(level: str) -> str:
    if level in {"bob", "castle_grounds", "castle_courtyard"}:
        return "grass"
    if level in {"ccm", "sl"}:
        return "snow"
    if level in {"ssl"}:
        return "sand"
    if level in {"jrb", "ddd", "sa"}:
        return "water"
    if level in {"lll", "bitfs", "bits", "bitdw", "bowser_1", "bowser_2", "bowser_3"}:
        return "lava"
    if level in {"bbh", "hmc", "cotmc", "wmotr"}:
        return "dark"
    return "stone"


def surface_color(surface: str, avg_y: float, palette: str) -> tuple[int, int, int, int]:
    if "BURNING" in surface or "LAVA" in surface:
        return (225, 55, 30, 235)
    if "WATER" in surface or "QUICKSAND" in surface:
        return (66, 142, 214, 210)
    if "SLIPPERY" in surface or "ICE" in surface or "SNOW" in surface:
        return (170, 205, 228, 235)
    if "DEATH" in surface or "HARD" in surface:
        return (165, 85, 85, 225)

    if palette == "grass":
        shade = max(85, min(210, int(132 + avg_y / 55)))
        return (max(25, shade - 95), shade, max(32, shade - 105), 240)
    if palette == "snow":
        shade = max(145, min(235, int(185 + avg_y / 75)))
        return (shade, min(255, shade + 8), min(255, shade + 20), 240)
    if palette == "sand":
        shade = max(130, min(225, int(172 + avg_y / 65)))
        return (shade, max(110, shade - 28), max(70, shade - 68), 240)
    if palette == "water":
        shade = max(100, min(210, int(145 + avg_y / 70)))
        return (max(80, shade - 35), max(90, shade - 20), shade, 238)
    if palette == "lava":
        shade = max(90, min(205, int(125 + avg_y / 60)))
        return (shade, max(65, shade - 45), max(55, shade - 55), 238)
    if palette == "dark":
        shade = max(75, min(185, int(112 + avg_y / 55)))
        return (shade, shade, min(210, shade + 8), 238)

    if "NOISE" in surface:
        shade = max(92, min(205, int(125 + avg_y / 55)))
        return (shade, min(255, shade + 8), shade, 235)
    shade = max(86, min(215, int(126 + avg_y / 45)))
    return (shade, min(255, shade + 12), shade, 235)


def world_to_pixel(x: int, z: int, bounds: tuple[int, int, int, int], size: int) -> tuple[float, float]:
    min_x, max_x, min_z, max_z = bounds
    span_x = max(1, max_x - min_x)
    span_z = max(1, max_z - min_z)
    px = (x - min_x) * (size - 1) / span_x
    py = (max_z - z) * (size - 1) / span_z
    return px, py


def compute_bounds(vertices: list[Vertex], fit: bool, padding: int) -> tuple[int, int, int, int]:
    if not fit or not vertices:
        return (MAP_MIN, MAP_MAX, MAP_MIN, MAP_MAX)

    min_x = min(v.x for v in vertices) - padding
    max_x = max(v.x for v in vertices) + padding
    min_z = min(v.z for v in vertices) - padding
    max_z = max(v.z for v in vertices) + padding

    span = max(max_x - min_x, max_z - min_z, 1)
    mid_x = (min_x + max_x) // 2
    mid_z = (min_z + max_z) // 2
    half = span // 2
    return (mid_x - half, mid_x + half, mid_z - half, mid_z + half)


def draw_grid(draw: ImageDraw.ImageDraw, size: int) -> None:
    for i in range(0, size + 1, 16):
        color = (32, 44, 54, 255)
        width = 1
        if i in (0, size // 2, size):
            color = (74, 132, 154, 255)
            width = 2
        draw.line([(i, 0), (i, size)], fill=color, width=width)
        draw.line([(0, i), (size, i)], fill=color, width=width)

    c = size // 2
    draw.ellipse((c - 8, c - 8, c + 8, c + 8), outline=(255, 180, 64, 255), width=2)
    draw.line([(c, c - 16), (c, c + 16)], fill=(255, 180, 64, 255), width=1)
    draw.line([(c - 16, c), (c + 16, c)], fill=(255, 180, 64, 255), width=1)


def render(
    vertices: list[Vertex],
    triangles: list[Triangle],
    bounds: tuple[int, int, int, int],
    size: int,
    include_walls: bool,
    include_death_planes: bool,
    min_normal_y: float,
    style: str,
    grid: bool,
    scale: int,
    palette: str,
    outline_alpha: int,
) -> Image.Image:
    render_size = size * scale
    if style == "debug":
        background = (7, 10, 14, 255)
    elif style == "black":
        background = (0, 0, 0, 255)
    else:
        background = (0, 0, 0, 0)

    img = Image.new("RGBA", (render_size, render_size), background)
    draw = ImageDraw.Draw(img, "RGBA")
    if grid or style == "debug":
        draw_grid(draw, render_size)

    visible_triangles = [
        tri for tri in triangles
        if should_render_triangle(tri, include_walls, include_death_planes, min_normal_y)
    ]

    for tri in sorted(visible_triangles, key=lambda triangle: triangle.avg_y):
        pts = [world_to_pixel(vertices[index].x, vertices[index].z, bounds, render_size) for index in tri.vertices]
        color = surface_color(tri.surface, tri.avg_y, palette)
        draw.polygon(pts, fill=color)
        if outline_alpha > 0:
            outline = (15, 18, 20, outline_alpha) if style != "debug" else (20, 26, 30, 180)
            draw.line([pts[0], pts[1], pts[2], pts[0]], fill=outline, width=max(1, scale))

    if style == "debug" or grid:
        draw.rectangle((0, 0, render_size - 1, render_size - 1), outline=(120, 190, 210, 255), width=max(2, scale * 2))

    if scale > 1:
        return img.resize((size, size), Image.Resampling.LANCZOS)
    return img


def iter_area_collision_files(root: Path) -> list[Path]:
    levels_dir = root / "levels"
    return sorted(levels_dir.glob("*/areas/*/collision.inc.c"))


def default_collision_path(root: Path, level: str, area: int) -> Path:
    return root / "levels" / level / "areas" / str(area) / "collision.inc.c"


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--collision", type=Path, help="collision.inc.c file to project")
    parser.add_argument("--level", help="level folder name, e.g. bob or wf")
    parser.add_argument("--area", type=int, default=1, help="area index when --level is used")
    parser.add_argument("--output", type=Path, help="output PNG path")
    parser.add_argument("--all", action="store_true", help="render every levels/*/areas/*/collision.inc.c file")
    parser.add_argument("--output-dir", type=Path, help="directory for --all outputs")
    parser.add_argument("--size", type=int, default=MAP_SIZE, help="output image size in pixels")
    parser.add_argument("--fit", action="store_true", help="fit bounds to geometry instead of SM64 -8192..8192 world")
    parser.add_argument("--padding", type=int, default=512, help="world-unit padding for --fit")
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="repository root")
    parser.add_argument("--include-walls", action="store_true", help="include near-vertical collision triangles")
    parser.add_argument("--include-death-planes", action="store_true", help="include SURFACE_DEATH_PLANE triangles")
    parser.add_argument("--min-normal-y", type=float, default=0.25, help="minimum absolute Y normal for floor-like triangles")
    parser.add_argument("--style", choices=("original", "black", "debug"), default="original", help="output visual style")
    parser.add_argument("--palette", choices=("auto", "grass", "snow", "sand", "water", "lava", "dark", "stone"), default="auto")
    parser.add_argument("--grid", action="store_true", help="overlay calibration grid")
    parser.add_argument("--scale", type=int, default=DEFAULT_SCALE, help="internal supersampling scale")
    parser.add_argument("--outline-alpha", type=int, default=70, help="internal triangle outline alpha, 0-255")
    return parser


def output_path_for(args: argparse.Namespace, root: Path, collision_path: Path) -> Path:
    if args.output:
        output_path = args.output
    elif args.level:
        output_path = root / "minimap_work" / "collision" / f"{args.level}_{args.area}.png"
    else:
        level = collision_path.parts[-4]
        area = collision_path.parts[-2]
        output_dir = args.output_dir or root / "minimap_work" / "collision"
        output_path = output_dir / f"{level}_{area}.png"
    return output_path


def render_collision(args: argparse.Namespace, root: Path, collision_path: Path, output_path: Path) -> None:
    vertices, triangles = parse_collision(collision_path)
    if not vertices or not triangles:
        raise SystemExit(f"no collision geometry parsed from {collision_path}")

    bounds = compute_bounds(vertices, args.fit, args.padding)
    level = collision_path.parts[-4]
    palette = palette_for_level(level) if args.palette == "auto" else args.palette
    img = render(
        vertices,
        triangles,
        bounds,
        args.size,
        args.include_walls,
        args.include_death_planes,
        args.min_normal_y,
        args.style,
        args.grid,
        max(1, args.scale),
        palette,
        max(0, min(255, args.outline_alpha)),
    )
    output_path.parent.mkdir(parents=True, exist_ok=True)
    img.save(output_path)

    print(f"wrote {output_path}")
    print(f"vertices={len(vertices)} triangles={len(triangles)} bounds={bounds}")


def main() -> None:
    args = build_parser().parse_args()
    root = args.root.resolve()

    if args.all:
        collision_paths = iter_area_collision_files(root)
        if not collision_paths:
            raise SystemExit("no area collision files found")
        for collision_path in collision_paths:
            render_collision(args, root, collision_path, output_path_for(args, root, collision_path))
        return

    if args.collision:
        collision_path = args.collision.resolve()
    elif args.level:
        collision_path = default_collision_path(root, args.level, args.area)
    else:
        raise SystemExit("provide --collision, --level, or --all")

    if not collision_path.exists():
        raise SystemExit(f"collision file not found: {collision_path}")

    render_collision(args, root, collision_path, output_path_for(args, root, collision_path))


if __name__ == "__main__":
    main()
