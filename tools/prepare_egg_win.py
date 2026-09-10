#!/usr/bin/env python3
"""Prepare the user-supplied full-screen Egg Catcher victory artwork."""

from pathlib import Path

from PIL import Image, ImageOps


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/images/egg-catcher-win-source.png"
OUTPUT = ROOT / "main/assets/egg_game_win.rgb565"
SCREEN_SIZE = (240, 320)


def write_rgb565le(image: Image.Image, path: Path) -> None:
    """Write tightly packed little-endian RGB565 pixels for LVGL."""
    output = bytearray(image.width * image.height * 2)
    offset = 0
    pixels = image.load()
    for y in range(image.height):
        for x in range(image.width):
            red, green, blue = pixels[x, y]
            pixel = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
            output[offset] = pixel & 0xFF
            output[offset + 1] = pixel >> 8
            offset += 2
    path.write_bytes(output)


def main() -> None:
    source = Image.open(SOURCE).convert("RGB")
    prepared = ImageOps.fit(
        source,
        SCREEN_SIZE,
        method=Image.Resampling.LANCZOS,
        centering=(0.5, 0.5),
    )
    write_rgb565le(prepared, OUTPUT)
    expected = SCREEN_SIZE[0] * SCREEN_SIZE[1] * 2
    if OUTPUT.stat().st_size != expected:
        raise ValueError(f"unexpected victory image size: {OUTPUT.stat().st_size}")


if __name__ == "__main__":
    main()
