#!/usr/bin/env python3
"""Convert the user-supplied Egg Catcher fox into compact LVGL sprites."""

from collections import deque
from pathlib import Path

from PIL import Image, ImageChops, ImageOps


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/images/egg-catcher-fox-source.png"
BASKET_SOURCE = ROOT / "assets/images/egg-catcher-basket-front-reference.png"
OUTPUT_DIR = ROOT / "main/assets"
SPRITE_SIZE = 110
CONTENT_SIZE = 108
BASKET_SOURCE_BOX = (665, 550, 822, 648)
BASKET_FRONT_W = 19
BASKET_FRONT_H = 11
RIGHT_BASKET_FRONT_BOX = (82, 66, 101, 77)
LEFT_BASKET_FRONT_BOX = (9, 66, 28, 77)


def remove_connected_white_background(source: Image.Image) -> Image.Image:
    """Remove only near-white pixels connected to an outer image edge."""
    image = source.convert("RGBA")
    width, height = image.size
    pixels = image.load()
    candidate = bytearray(width * height)
    background = bytearray(width * height)
    pending: deque[tuple[int, int]] = deque()

    for y in range(height):
        row = y * width
        for x in range(width):
            red, green, blue, alpha = pixels[x, y]
            candidate[row + x] = (
                alpha > 0
                and min(red, green, blue) >= 210
                and max(red, green, blue) - min(red, green, blue) <= 38
            )

    def add_if_background(x: int, y: int) -> None:
        index = y * width + x
        if candidate[index] and not background[index]:
            background[index] = 1
            pending.append((x, y))

    for x in range(width):
        add_if_background(x, 0)
        add_if_background(x, height - 1)
    for y in range(height):
        add_if_background(0, y)
        add_if_background(width - 1, y)

    while pending:
        x, y = pending.popleft()
        if x > 0:
            add_if_background(x - 1, y)
        if x + 1 < width:
            add_if_background(x + 1, y)
        if y > 0:
            add_if_background(x, y - 1)
        if y + 1 < height:
            add_if_background(x, y + 1)

    alpha = image.getchannel("A")
    alpha_pixels = alpha.load()
    for y in range(height):
        row = y * width
        for x in range(width):
            if background[row + x]:
                alpha_pixels[x, y] = 0
    image.putalpha(alpha)
    return image


def verify_basket_reference(source: Image.Image) -> None:
    """Keep the supplied front layer tied to the exact source artwork."""
    reference = Image.open(BASKET_SOURCE).convert("RGBA")
    source_crop = source.convert("RGBA").crop(BASKET_SOURCE_BOX)
    if source_crop.size != reference.size or ImageChops.difference(
        source_crop, reference
    ).getbbox() is not None:
        raise ValueError("basket reference no longer matches the fox source")


def make_right_sprite() -> Image.Image:
    """Clear the supplied white backdrop and fit the pose without smoothing."""
    original = Image.open(SOURCE).convert("RGBA")
    verify_basket_reference(original)
    source = remove_connected_white_background(original)
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
