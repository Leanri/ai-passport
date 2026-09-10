#!/usr/bin/env python3
"""Convert the user-supplied Egg Catcher fox into compact LVGL sprites."""

from pathlib import Path

from PIL import Image, ImageOps


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/images/egg-catcher-fox-source.png"
OUTPUT_DIR = ROOT / "main/assets"
SPRITE_SIZE = 110
CONTENT_SIZE = 108
BASKET_FRONT_W = 27
BASKET_FRONT_H = 16
RIGHT_BASKET_FRONT_BOX = (80, 53, 107, 69)
LEFT_BASKET_FRONT_BOX = (3, 53, 30, 69)


def make_right_sprite() -> Image.Image:
    """Crop transparent margins and fit the original pose without smoothing."""
    source = Image.open(SOURCE).convert("RGBA")
    bounds = source.getchannel("A").getbbox()
    if bounds is None:
        raise ValueError(f"source has no visible pixels: {SOURCE}")

    cropped = source.crop(bounds)
    fitted = ImageOps.contain(
        cropped,
        (CONTENT_SIZE, CONTENT_SIZE),
        method=Image.Resampling.NEAREST,
    )
    sprite = Image.new("RGBA", (SPRITE_SIZE, SPRITE_SIZE), (0, 0, 0, 0))
    position = (
        (SPRITE_SIZE - fitted.width) // 2,
        (SPRITE_SIZE - fitted.height) // 2,
    )
    sprite.alpha_composite(fitted, position)
    return sprite


def write_lvgl_argb8888(image: Image.Image, path: Path) -> None:
    """Write LVGL ARGB8888 bytes in the BGRA order used on this target."""
    path.write_bytes(image.tobytes("raw", "BGRA"))
    expected = image.width * image.height * 4
    if path.stat().st_size != expected:
        raise ValueError(f"unexpected sprite size for {path}")


def main() -> None:
    right = make_right_sprite()
    left = ImageOps.mirror(right)
    write_lvgl_argb8888(left, OUTPUT_DIR / "egg_game_fox_left.argb8888")
    write_lvgl_argb8888(right, OUTPUT_DIR / "egg_game_fox_right.argb8888")

    # The full fox remains the back layer. These exact transparent crops add
    # only the hand and basket front when a caught egg sinks into the opening.
    right_front = right.crop(RIGHT_BASKET_FRONT_BOX)
    left_front = left.crop(LEFT_BASKET_FRONT_BOX)
    if right_front.size != (BASKET_FRONT_W, BASKET_FRONT_H):
        raise ValueError(f"unexpected right basket size: {right_front.size}")
    if left_front.size != (BASKET_FRONT_W, BASKET_FRONT_H):
        raise ValueError(f"unexpected left basket size: {left_front.size}")
    write_lvgl_argb8888(
        left_front, OUTPUT_DIR / "egg_game_basket_front_left.argb8888"
    )
    write_lvgl_argb8888(
        right_front, OUTPUT_DIR / "egg_game_basket_front_right.argb8888"
    )


if __name__ == "__main__":
    main()
