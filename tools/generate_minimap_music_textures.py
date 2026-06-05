#!/usr/bin/env python3
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
TEXTURE_DIR = ROOT / "src/minimap/textures"
FONT_DIR = ROOT / "textures/segment2"

SPECIAL_CHARS = {
    " ": 0x9E,
    ".": 0x6E,
    ",": 0x6F,
    "'": 0x3E,
    "-": 0x9F,
    "&": 0xE5,
    "!": 0xF2,
    "?": 0xF4,
}

FONT_OFFSETS = {}
for i in range(10):
    FONT_OFFSETS[str(i)] = 0x5900 + i * 0x40
for i, ch in enumerate("ABCDEFGHIJKLMNOPQRSTUVWXYZ"):
    FONT_OFFSETS[ch] = 0x5B80 + i * 0x40
for i, ch in enumerate("abcdefghijklmnopqrstuvwxyz"):
    FONT_OFFSETS[ch] = 0x6200 + i * 0x40
FONT_OFFSETS.update({
    "'": 0x6B80,
    ".": 0x6A80,
    ",": 0x6B40,
    "-": 0x6A40,
    "&": 0x6D80,
    "!": 0x68C0,
    "?": 0x6BC0,
})

CHAR_WIDTHS = {
    " ": 5,
    "'": 3,
    ",": 5,
    "-": 6,
    ".": 4,
    "0": 7, "1": 7, "2": 7, "3": 7, "4": 7,
    "5": 7, "6": 7, "7": 7, "8": 7, "9": 7,
}
for ch, width in zip("ABCDEFGHIJKLMNOPQRSTUVWXYZ",
                     (6, 6, 6, 6, 6, 6, 5, 6, 6, 5, 8, 8, 6,
                      6, 6, 6, 6, 5, 6, 6, 8, 7, 6, 6, 6, 5)):
    CHAR_WIDTHS[ch] = width
for ch, width in zip("abcdefghijklmnopqrstuvwxyz",
                     (6, 5, 5, 6, 5, 5, 6, 5, 4, 5, 5, 3, 7,
                      5, 5, 5, 6, 5, 5, 5, 5, 5, 7, 7, 5, 5)):
    CHAR_WIDTHS[ch] = width

TRACKS = {
    "music_silence.png": "Silence",
    "music_victory.png": "Course Clear",
    "music_title_theme.png": "Title Theme",
    "music_main_theme.png": "Super Mario 64 Main Theme",
    "music_inside_castle.png": "Inside the Castle Walls",
    "music_dire_dire_docks.png": "Dire, Dire Docks",
    "music_lethal_lava_land.png": "Lethal Lava Land",
    "music_bowser_battle.png": "Koopa's Theme",
    "music_snow_mountain.png": "Snow Mountain",
    "music_slider.png": "Slider",
    "music_haunted_house.png": "Haunted House",
    "music_piranha_lullaby.png": "Piranha Plant's Lullaby",
    "music_hazy_maze_cave.png": "Cave Dungeon",
    "music_star_select.png": "Star Select",
    "music_powerful_mario.png": "Powerful Mario",
    "music_metal_mario.png": "Metallic Mario",
    "music_koopa_message.png": "Koopa's Message",
    "music_koopas_road.png": "Koopa's Road",
    "music_high_score.png": "High Score",
    "music_merry_go_round.png": "Merry-Go-Round",
    "music_race_fanfare.png": "Race Fanfare",
    "music_star_spawn.png": "Power Star",
    "music_boss_battle.png": "Stage Boss",
    "music_key_clear.png": "Koopa Clear",
    "music_endless_stairs.png": "Looping Steps",
    "music_final_bowser.png": "Ultimate Koopa",
    "music_staff_roll.png": "Staff Roll",
    "music_puzzle_solved.png": "Correct Solution",
    "music_toad_message.png": "Toad's Message",
    "music_peach_message.png": "Peach's Message",
    "music_opening.png": "Opening",
    "music_ending.png": "Ending Demo",
    "music_file_select.png": "File Select",
    "music_lakitu.png": "Lakitu",
}


def dialog_index(ch):
    if "0" <= ch <= "9":
        return ord(ch) - ord("0")
    if "A" <= ch <= "Z":
        return ord(ch) - ord("A") + 0x0A
    if "a" <= ch <= "z":
        return ord(ch) - ord("a") + 0x24
    return SPECIAL_CHARS.get(ch, 0x9E)


def load_glyph(ch):
    offset = FONT_OFFSETS.get(ch)
    if offset is None:
        return None
    path = FONT_DIR / f"font_graphics.{offset:05X}.ia4.png"
    glyph = Image.open(path).convert("RGBA")
    glyph = glyph.transpose(Image.Transpose.TRANSVERSE)
    if glyph.height != 16:
        glyph = glyph.resize((glyph.width, 16), Image.Resampling.NEAREST)
    shaped = Image.new("RGBA", glyph.size, (255, 255, 255, 0))
    shaped.putalpha(glyph.getchannel("A"))
    return shaped


def render_note():
    note = Image.new("RGBA", (16, 16), (255, 255, 255, 0))
    pixels = note.load()
    # Pixel music note drawn in the same white IA-style silhouette as dialog text.
    rows = (
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        ".........####...",
        ".....####...#...",
        ".....#......#...",
        ".....#......#...",
        ".....#...####...",
        "..####..#####...",
        ".#####..####....",
        ".####...........",
        "................",
        "................",
    )
    for y, row in enumerate(rows):
        for x, value in enumerate(row):
            if value == "#":
                pixels[x, y] = (255, 255, 255, 255)
    return note


def render_track(path, title, note):
    old = Image.open(path).convert("RGBA")
    canvas_w, canvas_h = old.size
    out = Image.new("RGBA", old.size, (255, 255, 255, 0))

    note_gap = 0
    right_pad = 4
    letter_gap = 0
    space_width = CHAR_WIDTHS[" "]
    glyphs = []
    text_width = 0
    for ch in title:
        if ch == " ":
            glyphs.append((" ", None))
            text_width += space_width
            continue
        glyph = load_glyph(ch)
        glyphs.append((ch, glyph))
        if glyph is not None:
            text_width += CHAR_WIDTHS.get(ch, glyph.width) + letter_gap

    max_text_width = canvas_w - right_pad - note.width - note_gap
    if text_width > max_text_width:
        letter_gap = 0
        space_width = 4
        text_width = 0
        for ch, glyph in glyphs:
            if glyph is None:
                text_width += space_width
            else:
                text_width += CHAR_WIDTHS.get(ch, glyph.width) + letter_gap

    total_width = note.width + note_gap + text_width
    x = max(0, canvas_w - right_pad - total_width)

    out.alpha_composite(note, (x, 0))
    x += note.width + note_gap

    for ch, glyph in glyphs:
        if glyph is None:
            x += space_width
        else:
            out.alpha_composite(glyph, (x, 0))
            x += CHAR_WIDTHS.get(ch, glyph.width) + letter_gap

    bbox = out.getchannel("A").getbbox()
    if bbox is not None and bbox[2] > canvas_w - right_pad:
        shifted = Image.new("RGBA", old.size, (255, 255, 255, 0))
        shifted.alpha_composite(out, (canvas_w - right_pad - bbox[2], 0))
        out = shifted

    out.save(path)
    if out.size != (canvas_w, canvas_h):
        raise RuntimeError(f"{path} changed size")


def main():
    note = render_note()
    for filename, title in TRACKS.items():
        path = TEXTURE_DIR / filename
        if path.exists():
            render_track(path, title, note)


if __name__ == "__main__":
    main()
