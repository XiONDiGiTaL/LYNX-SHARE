#!/usr/bin/env python3
"""Cut the nine fruit marks out of the reference sheet (tools/reference/theme-fruits.png).

The sheet is the design: 3 x 3 badges, each a disc with a glowing ring and the fruit on it, with a caption under it.
The app draws the disc and the ring itself (in the kit's own colours), so only the fruit is wanted - and the sheet has
no transparency, so the fruit has to be keyed off the disc.

The disc is not a flat colour: it is a dark grey with a soft bloom in the ring's colour, brightest at the rim. That
background is (for our purposes) a function of the distance from the badge centre only, so it is measured per radius
as the median of the pixels on that radius, and the fruit is everything that departs from it. The median is safe
because the fruit covers a minority of any given circle.

    python tools/extract_fruit_marks.py            # writes docs/img/marks/<id>.png and src/ui/FoodMarkData.h
    python tools/extract_fruit_marks.py --preview  # also writes build/marks-preview.png to look at the cut-outs
"""

from __future__ import annotations

import pathlib
import sys

import numpy as np
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent.parent
SHEET = ROOT / "tools" / "reference" / "theme-fruits.png"
OUT_PNG = ROOT / "docs" / "img" / "marks"
OUT_HEADER = ROOT / "src" / "ui" / "FoodMarkData.h"

# the sheet, row by row: badge id (the kit id) -> centre, outer glow radius
BADGES = [
    (["pepper", "tangerine", "banana"], [(174, 179), (512, 178), (849, 179)], 132.0),
    (["kiwi", "blueberry", "eggplant"], [(174, 512), (512, 512), (849, 513)], 132.0),
    (["coconut", "fig", "dragonfruit"], [(173, 828), (515, 828), (852, 828)], 126.0),
]

SIZE = 224        # the cut-out is stored at this many pixels (the badge shows it at ~90, 2x on a retina screen)


def keep_largest(mask: Image.Image) -> Image.Image:
    """Drop the specks the key leaves behind (grain, the bloom at the rim): only the fruit's own blob survives."""
    from collections import deque

    filled = np.asarray(mask) > 127
    h, w = filled.shape
    seen = np.zeros_like(filled)
    best: list[tuple[int, int]] = []

    for sy in range(h):
        for sx in range(w):
            if not filled[sy, sx] or seen[sy, sx]:
                continue
            blob = []
            queue = deque([(sy, sx)])
            seen[sy, sx] = True
            while queue:
                y, x = queue.popleft()
                blob.append((y, x))
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        ny, nx = y + dy, x + dx
                        if 0 <= ny < h and 0 <= nx < w and filled[ny, nx] and not seen[ny, nx]:
                            seen[ny, nx] = True
                            queue.append((ny, nx))
            if len(blob) > len(best):
                best = blob

    out = np.zeros_like(filled)
    for y, x in best:
        out[y, x] = True
    return Image.fromarray(out.astype(np.uint8) * 255, "L")


def largest_blob(mask: Image.Image) -> Image.Image:
    """Keep the silhouette the fruit makes: close the gaps, fill its interior, drop whatever else survives."""
    from PIL import ImageDraw, ImageFilter

    closed = mask.filter(ImageFilter.MaxFilter(7)).filter(ImageFilter.MinFilter(7))
    closed = keep_largest(closed)

    # anything the fruit encloses is fruit (the darkest shading is not the disc)
    holes = closed.point(lambda v: 255 - v)
    ImageDraw.floodfill(holes, (0, 0), 128)
    filled = (np.asarray(closed) > 127) | (np.asarray(holes) == 255)
    closed = Image.fromarray(filled.astype(np.uint8) * 255, "L")

    return closed


def cut(sheet: np.ndarray, cx: float, cy: float, radius: float) -> Image.Image:
    # The ring sits at r = 117 on every badge (measured on the sheet), so the fruit - which reaches r = 114 - is cut
    # just inside it; the crop itself is wider, so a fruit that fills the disc is never clipped by the square.
    inner = radius * 0.98
    clip = 116.0
    y0, y1 = int(cy - inner), int(cy + inner) + 1
    x0, x1 = int(cx - inner), int(cx + inner) + 1
    crop = sheet[y0:y1, x0:x1]

    h, w, _ = crop.shape
    yy, xx = np.mgrid[0:h, 0:w]
    dist = np.hypot(xx + 0.5 - (cx - x0), yy + 0.5 - (cy - y0))

    flat = crop.max(axis=2) - crop.min(axis=2)
    lum = crop.mean(axis=2)

    # The fruit is either coloured (its body) or bright (cut flesh, coconut milk, banana ends). The soft grey shadow
    # the art paints behind a fruit is neither: it is neutral and mid-dark, so it stays out of the cut-out and the
    # badge's own disc shows there instead.
    seed = ((flat > 22) | (lum > 96)) & (dist < clip)

    mask = largest_blob(Image.fromarray((seed * 255).astype(np.uint8), "L"))

    # a hair of erosion, so the dark rim the art was composited on is not in the cut-out
    from PIL import ImageFilter

    mask = mask.filter(ImageFilter.MinFilter(3)).filter(ImageFilter.GaussianBlur(0.9))
    alpha = np.asarray(mask).astype(float) / 255.0

    rgba = np.dstack([crop, alpha * 255.0]).astype(np.uint8)
    image = Image.fromarray(rgba, "RGBA")

    box = image.getchannel("A").point(lambda v: 255 if v > 12 else 0).getbbox()
    if box is None:
        raise SystemExit("nothing found in a badge cell")
    image = image.crop(box)

    # square, so the mark keeps its proportions whatever the area it is drawn in
    side = max(image.size)
    square = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    square.paste(image, ((side - image.width) // 2, (side - image.height) // 2))
    return square.resize((SIZE, SIZE), Image.LANCZOS)


def main() -> int:
    sheet = np.asarray(Image.open(SHEET).convert("RGB")).astype(float)
    OUT_PNG.mkdir(parents=True, exist_ok=True)

    marks: list[tuple[str, bytes]] = []
    for ids, centres, radius in BADGES:
        for name, (cx, cy) in zip(ids, centres):
            image = cut(sheet, cx, cy, radius)
            path = OUT_PNG / f"{name}.png"
            image.save(path, optimize=True)
            marks.append((name, path.read_bytes()))
            print(f"{name:12s} {image.width}x{image.height}  {path.relative_to(ROOT)}  {len(marks[-1][1]) // 1024} KB")

    write_header(marks)

    if "--preview" in sys.argv:
        preview = Image.new("RGBA", (SIZE * 3 + 40, SIZE * 3 + 40), (26, 26, 28, 255))
        for i, (name, _) in enumerate(marks):
            tile = Image.open(OUT_PNG / f"{name}.png")
            preview.alpha_composite(tile, (10 + (i % 3) * (SIZE + 10), 10 + (i // 3) * (SIZE + 10)))
        out = ROOT / "build" / "marks-preview.png"
        out.parent.mkdir(exist_ok=True)
        preview.save(out)
        print(f"preview -> {out.relative_to(ROOT)}")

    return 0


def write_header(marks: list[tuple[str, bytes]]) -> None:
    lines = [
        "#pragma once",
        "",
        "// The food theme marks: the fruit art from tools/reference/theme-fruits.png, cut out by",
        "// tools/extract_fruit_marks.py and embedded here as 224 px PNGs with transparency. ui/FoodGlyphs.cpp draws",
        "// these in the badge and the logo; a kit without art here falls back to the vector marks in the same file.",
        "",
        "namespace lynx::foodmarks",
        "{",
        "struct Mark",
        "{",
        "    const char* id;",
        "    const unsigned char* png;",
        "    int size;",
        "};",
        "",
    ]

    for name, data in marks:
        body = ",".join(str(b) for b in data)
        lines.append(f"inline constexpr unsigned char {name}[] = {{")
        # 26 values a line keeps the file readable without exploding it
        values = body.split(",")
        for i in range(0, len(values), 26):
            lines.append(",".join(values[i:i + 26]) + ",")
        lines.append("};")
        lines.append(f"inline constexpr int {name}Size = {len(data)};")
        lines.append("")

    lines.append("inline constexpr Mark marks[] = {")
    for name, _ in marks:
        lines.append(f'    {{ "{name}", {name}, {name}Size }},')
    lines += [
        "};",
        "} // namespace lynx::foodmarks",
        "",
    ]

    OUT_HEADER.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {OUT_HEADER.relative_to(ROOT)} ({OUT_HEADER.stat().st_size // 1024} KB)")


if __name__ == "__main__":
    raise SystemExit(main())
