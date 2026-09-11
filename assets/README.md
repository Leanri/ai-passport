<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, and sound effects, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

Store reusable font files and generated font sources in `fonts/`.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

## Images

Store reusable source images and generated display assets in `images/`.

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

### Egg Catcher fox

- Source: `images/egg-catcher-fox-source.png`, a 911 × 927 RGBA image supplied by the fork owner for use in this personal fork; no broader redistribution license is claimed.
- Basket reference: `images/egg-catcher-basket-front-reference.png`, the matching 157 × 98 RGBA crop supplied by the fork owner under the same personal-fork terms. Its opaque white background is reference-only and is never embedded in the firmware.
- Integration: `tools/prepare_egg_fox.py` removes the edge-connected white backdrop and the enclosed white gap between the forearms, crops the resulting alpha bounds, fits the artwork inside a 110 × 110 canvas with nearest-neighbor sampling, mirrors the left pose, and writes LVGL ARGB8888/BGRA bytes to `main/assets/egg_game_fox_{left,right}.argb8888`. It verifies the supplied basket crop against the source and extracts matching transparent 19 × 11 basket-front layers from the generated poses as `main/assets/egg_game_basket_front_{left,right}.argb8888`.
- Regeneration: install Pillow and run `python tools/prepare_egg_fox.py` from the repository root.

### Egg Catcher victory screen

- Source: `images/egg-catcher-win-source.png`, a 1086 × 1448 RGB image supplied by the fork owner for use in this personal fork; no broader redistribution license is claimed.
- Integration: `tools/prepare_egg_win.py` center-fits the artwork to the 240 × 320 display and writes little-endian RGB565 pixels to `main/assets/egg_game_win.rgb565`. The game displays it after the hundredth catch.
- Regeneration: install Pillow and run `python tools/prepare_egg_win.py` from the repository root.

### Egg Catcher cover

- Source: `images/egg-catcher-cover-source.png`, a 1086 × 1448 RGB image supplied by the fork owner for use in this personal fork; no broader redistribution license is claimed.
- Integration: `tools/prepare_egg_cover.py` center-fits the artwork to the 240 × 320 display and writes little-endian RGB565 pixels to `main/assets/egg_game_cover.rgb565`. The game shows it behind the Kids/Adults selector before play begins.
- Regeneration: install Pillow and run `python tools/prepare_egg_cover.py` from the repository root.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.
