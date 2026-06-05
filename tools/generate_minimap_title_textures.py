#!/usr/bin/env python3
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
TEXTURE_DIR = ROOT / "src/minimap/textures"
SEGMENT2_DIR = ROOT / "textures/segment2"
INTRO_DIR = ROOT / "levels/intro"


def glyph_offset(ch):
    if "0" <= ch <= "9":
        return (ord(ch) - ord("0")) * 0x200
    if "A" <= ch <= "Z":
        return 0x1400 + (ord(ch) - ord("A")) * 0x200
    return None


def load_hud_glyph(ch):
    offset = glyph_offset(ch)
    if offset is None:
        return None
    return Image.open(SEGMENT2_DIR / f"segment2.{offset:05X}.rgba16.png").convert("RGBA")


def render_centered_hud_text(text, output):
    width, height = 256, 16
    advance = 12
    glyph_width = 16
    text_width = (len(text) - 1) * advance + glyph_width
    x = (width - text_width) // 2
    out = Image.new("RGBA", (width, height), (255, 255, 255, 0))

    for ch in text:
        glyph = load_hud_glyph(ch)
        if glyph is not None:
            out.alpha_composite(glyph, (x, 0))
        x += advance

    out.save(output)


def main():
    TEXTURE_DIR.mkdir(parents=True, exist_ok=True)
    Image.open(INTRO_DIR / "2_copyright.rgba16.png").convert("RGBA").save(
        TEXTURE_DIR / "title_1996_nintendo.png"
    )
    render_centered_hud_text("PRESS START", TEXTURE_DIR / "title_press_start.png")


if __name__ == "__main__":
    main()
