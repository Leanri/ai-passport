#!/usr/bin/env python3
"""Restore crisp ground texture behind the Egg Catcher character."""

from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/images/egg-catcher-background-source.png"
OUTPUT = ROOT / "main/assets/egg_game_background.rgb565"
PREVIEW = ROOT / "build/egg-game-background-restored.png"


def restore_ground_texture(image: Image.Image) -> Image.Image:
    """Replace the former flat erase with neighboring pixel-art texture."""
    source = image.convert("RGB")
    restored = source.copy()

    # The lower path is intact on both sides of the old character. Stretch
    # those real pixel-art strips toward the center, meeting beneath the fox.
    # This keeps the existing palette and authored dirt marks instead of
    # inventing a smooth fill or adding random noise.
    top = 242
    bottom = 320
    left_strip = source.crop((45, 270, 78, bottom))
    right_strip = source.crop((162, 270, 195, bottom))
    left_fill = left_strip.resize((65, bottom - top), Image.Resampling.NEAREST)
    right_fill = right_strip.resize((65, bottom - top), Image.Resampling.NEAREST)
    texture = source.copy()
    texture.paste(left_fill, (55, top))
    texture.paste(right_fill, (120, top))

    mask = Image.new("L", source.size, 0)
    mask_draw = ImageDraw.Draw(mask)
    mask_draw.polygon(((74, top), (166, top), (186, bottom), (54, bottom)), fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(3.0))
    restored = Image.composite(texture, restored, mask)

    return restored


def write_rgb565le(image: Image.Image, path: Path) -> None:
    payload = bytearray()
    for red, green, blue in image.getdata():
        pixel = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
        payload.extend((pixel & 0xFF, pixel >> 8))
    path.write_bytes(payload)


def main() -> None:
    source = Image.open(SOURCE).convert("RGB")
    if source.size != (240, 320):
        raise ValueError(f"unexpected background size: {source.size}")
    restored = restore_ground_texture(source)
    write_rgb565le(restored, OUTPUT)
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    restored.save(PREVIEW)


if __name__ == "__main__":
    main()
