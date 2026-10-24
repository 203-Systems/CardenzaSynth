#!/usr/bin/env python3
"""Turns CardenzaSimShot frame captures into GIFs: the 240x135 screen at 2x with the 4x14 key
lights drawn underneath. Usage: tools/groove-motion-gif.py <capture dir> <output dir>"""
import pathlib
import sys

from PIL import Image, ImageDraw

LABELS = [
    ["ESC", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "SUB"],
    ["TAB", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]", "ADD"],
    ["FN", "sh", "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "ENT"],
    ["CTL", "opt", "alt", "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/", "PLY"],
]
STRIP = [(255, 90, 54), (54, 214, 230), (255, 208, 48), (160, 112, 255)]


def read_leds(path):
    rows, master = [], 15
    for line in path.read_text().splitlines():
        parts = line.split()
        if parts and parts[0] == "brightness":
            master = int(parts[1])
        elif len(parts) == 14:
            rows.append([int(v) for v in parts])
    return rows, master


def keyboard(levels, master, width=480):
    pad, gap = 6, 3
    key_w = (width - pad * 2 - gap * 13) // 14
    key_h = 26
    image = Image.new("RGB", (width, pad * 2 + key_h * 4 + gap * 3), (14, 14, 13))
    draw = ImageDraw.Draw(image)
    for r in range(4):
        for c in range(14):
            x = pad + c * (key_w + gap)
            y = pad + r * (key_h + gap)
            level = levels[r][c] * master / 225.0 if levels else 0.0
            base = (34, 34, 32)
            glow = (255, 236, 196)
            fill = tuple(int(b + (g - b) * level) for b, g in zip(base, glow))
            outline = STRIP[r] if c == 0 else (70, 70, 66)
            draw.rounded_rectangle([x, y, x + key_w - 1, y + key_h - 1], radius=4, fill=fill, outline=outline)
            text = (12, 12, 12) if level > 0.55 else (120, 120, 114)
            draw.text((x + 4, y + 8), LABELS[r][c], fill=text)
    return image


def main():
    capture, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    names = {"a-boot": "1-boot", "b-scene": "2-scene-switch", "c-pages": "3-pages-tracks", "d-seq": "4-sequencer",
             "e-tape": "5-tape", "f-mix": "6-mix", "g-modal": "7-preset-browser", "h-endings": "8-endings",
             "i-maps": "9-hold-maps"}
    for section, name in names.items():
        files = sorted((capture / section).glob("*.png"))[::2]
        if not files:
            continue
        frames = []
        for path in files:
            screen = Image.open(path).convert("RGB").resize((480, 270), Image.NEAREST)
            levels, master = read_leds(path.with_suffix(".leds.txt")) if path.with_suffix(".leds.txt").exists() else ([], 15)
            keys = keyboard(levels, master)
            frame = Image.new("RGB", (480, 270 + keys.height), (14, 14, 13))
            frame.paste(screen, (0, 0))
            frame.paste(keys, (0, 270))
            frames.append(frame.quantize(colors=128, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE))
        target = out / f"{name}.gif"
        frames[0].save(target, save_all=True, append_images=frames[1:], duration=32, loop=0, disposal=1)
        print(target, len(frames), f"{target.stat().st_size // 1024} KB")


if __name__ == "__main__":
    main()
