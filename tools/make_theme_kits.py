#!/usr/bin/env python3
"""Writes docs/img/theme-kits.svg: the food theme kits, as Options > Theme draws them.

A kit is a seed pair for core/ThemePalette.h plus its food identity (core/ThemeKits.h): the accent, the leaf, the
workspace and the NAME | HUE | TASTE caption. This script draws each one as the badge the theme page shows - a dark
disc, a ring glowing in the kit's accent, the fruit on it - and the same fruit inside the food logo.

The fruit shapes below mirror src/ui/FoodGlyphs.cpp, which draws them in JUCE paths from the same unit space (0..1,
y down). Change a fruit there and here together; the numbers are the design, not a screenshot.

    python tools/make_theme_kits.py            # rewrites docs/img/theme-kits.svg
"""

import base64
import math
import os
import pathlib

# id, name, hue, taste, accent, leaf, background, workspace
KITS = [
    ("cherry", "Cherry", "RED", "BRIGHT", 0xD92B45, 0x4E7A3A, 0x160F11, 0x0A0708),
    ("watermelon", "Watermelon", "PINK", "JUICY", 0xFF5C73, 0x3F8A4F, 0x101614, 0x070908),
    ("tangerine", "Tangerine", "ORANGE", "ZEST", 0xFF9E1F, 0x4F8A38, 0x1A1209, 0x0C0804),
    ("banana", "Banana", "YELLOW", "SWEET", 0xF2C21B, 0x7FA83A, 0x17150D, 0x0B0A06),
    ("kiwi", "Kiwi", "GREEN", "TART", 0x63C46B, 0x6E8F3F, 0x101511, 0x070A08),
    ("lime", "Lime", "LIME", "SOUR", 0xA6D81C, 0x5E8C2A, 0x12150E, 0x080A06),
    ("avocado", "Avocado", "OLIVE", "CREAMY", 0x7FB53A, 0x4F7A26, 0x12140F, 0x080907),
    ("blueberry", "Blueberry", "BLUE", "BERRY", 0x4E6BE8, 0x5C8A4A, 0x0F1320, 0x080A11),
    ("eggplant", "Eggplant", "PURPLE", "RICH", 0x8F5BD6, 0x5E8A3A, 0x141019, 0x09070C),
    ("fig", "Fig", "INDIGO", "ANCIENT", 0x6D5AE8, 0x5E8A46, 0x121122, 0x08070F),
    ("dragonfruit", "Dragonfruit", "VIOLET", "EXOTIC", 0xFF4FA3, 0x4F9A5C, 0x170F18, 0x0A070B),
    ("lychee", "Lychee", "BLUSH", "FLORAL", 0xF2A0B5, 0x5B8C43, 0x161114, 0x0A0709),
    ("coconut", "Coconut", "CREAM", "MELLOW", 0xE4C9A0, 0x8A6A45, 0x16110D, 0x0A0705),
]

PEPPER = ("pepper", "Pepper", "RED", "SPICE", 0xE5383B, 0x7FA83A, 0x15161A, 0x0E0F12)

FONT = "-apple-system, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif"
MONO = "'Cascadia Mono', Consolas, 'SF Mono', Menlo, monospace"


def rgb(v):
    return (v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF


def hexc(v):
    return "#%06X" % (v & 0xFFFFFF)


def mix(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


def darker(c, t=0.3):
    return mix(c, (0, 0, 0), t)


def brighter(c, t=0.3):
    return mix(c, (255, 255, 255), t)


def col(c, alpha=1.0):
    return "rgb(%d,%d,%d)" % c if alpha >= 1.0 else "rgba(%d,%d,%d,%.3f)" % (c[0], c[1], c[2], alpha)


def css(value):
    """A colour as an SVG string: the marks pass (r, g, b) or (r, g, b, a) tuples, the page passes #RRGGBB."""
    if isinstance(value, tuple) and len(value) == 4:
        return "rgba(%d,%d,%d,%.3f)" % (value[0], value[1], value[2], value[3] / 255.0)
    return col(value) if isinstance(value, tuple) else value


class Svg:
    """The drawing calls the marks use, emitted as SVG."""

    def __init__(self):
        self.parts = []

    def ellipse(self, box, fill):
        x0, y0, x1, y1 = box
        self.parts.append('<ellipse cx="%.2f" cy="%.2f" rx="%.2f" ry="%.2f" fill="%s"/>'
                          % ((x0 + x1) / 2, (y0 + y1) / 2, (x1 - x0) / 2, (y1 - y0) / 2, css(fill)))

    def rect(self, box, fill=None, rx=0.0, stroke=None, width=1.0):
        x0, y0, x1, y1 = box
        attrs = 'x="%.2f" y="%.2f" width="%.2f" height="%.2f" rx="%.2f"' % (x0, y0, x1 - x0, y1 - y0, rx)
        attrs += ' fill="%s"' % (css(fill) if fill is not None else "none")
        if stroke is not None:
            attrs += ' stroke="%s" stroke-width="%.2f"' % (css(stroke), width)
        self.parts.append("<rect %s/>" % attrs)

    def polygon(self, pts, fill):
        d = " ".join("%.2f,%.2f" % p for p in pts)
        self.parts.append('<polygon points="%s" fill="%s"/>' % (d, css(fill)))

    def polyline(self, pts, stroke, width, opacity=1.0, cap="round"):
        d = " ".join("%.2f,%.2f" % p for p in pts)
        alpha = "" if opacity >= 1.0 else ' stroke-opacity="%.3f"' % opacity
        self.parts.append('<polyline points="%s" fill="none" stroke="%s" stroke-width="%.2f" stroke-linecap="%s" '
                          'stroke-linejoin="round"%s/>' % (d, css(stroke), width, cap, alpha))

    def circle(self, cx, cy, r, fill=None, stroke=None, width=1.0, opacity=1.0):
        attrs = 'cx="%.2f" cy="%.2f" r="%.2f"' % (cx, cy, r)
        attrs += ' fill="%s"' % (css(fill) if fill is not None else "none")
        if stroke is not None:
            attrs += ' stroke="%s" stroke-width="%.2f"' % (css(stroke), width)
        if opacity < 1.0:
            attrs += ' stroke-opacity="%.3f"' % opacity
        self.parts.append("<circle %s/>" % attrs)

    def pieslice(self, box, a0_deg, a1_deg, fill, steps=64):
        x0, y0, x1, y1 = box
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        rx, ry = (x1 - x0) / 2, (y1 - y0) / 2
        pts = [(cx, cy)]
        for i in range(steps + 1):
            a = math.radians(a0_deg + (a1_deg - a0_deg) * i / steps)
            pts.append((cx + rx * math.cos(a), cy + ry * math.sin(a)))
        self.polygon(pts, fill)

    def text(self, x, y, text, size, fill, weight="600", family=FONT, anchor="middle", spacing=None):
        extra = ' letter-spacing="%.2f"' % spacing if spacing else ""
        self.parts.append('<text x="%.2f" y="%.2f" text-anchor="%s" font-family="%s" font-size="%.1f" '
                          'font-weight="%s" fill="%s"%s>%s</text>'
                          % (x, y, anchor, family, size, weight, fill, extra, text))

    def defs_gradient(self, gid, top, bottom, x1=0.5, y1=0.0, x2=0.5, y2=1.0):
        self.parts.append(
            '<linearGradient id="%s" x1="%.2f" y1="%.2f" x2="%.2f" y2="%.2f">'
            '<stop offset="0" stop-color="%s"/><stop offset="1" stop-color="%s"/></linearGradient>'
            % (gid, x1, y1, x2, y2, css(top), css(bottom)))

    def defs_radial(self, gid, stops, cx=0.30, cy=0.20, r=1.05):
        """A radial gradient in object-bounding-box units, as FoodGlyphs.cpp's `solid` shades a fruit."""
        body = "".join('<stop offset="%.2f" stop-color="%s"/>' % (o, css(colour)) for o, colour in stops)
        self.parts.append('<radialGradient id="%s" cx="%.2f" cy="%.2f" r="%.2f">%s</radialGradient>'
                          % (gid, cx, cy, r, body))

    def art(self, box, path):
        x0, y0, x1, y1 = box
        data = base64.b64encode(pathlib.Path(path).read_bytes()).decode("ascii")
        self.parts.append('<image x="%.2f" y="%.2f" width="%.2f" height="%.2f" preserveAspectRatio="xMidYMid meet" '
                          'href="data:image/png;base64,%s"/>' % (x0, y0, x1 - x0, y1 - y0, data))

    def render(self):
        return "\n".join(self.parts)


class Pil:
    """The same calls, rasterised with Pillow, so the layout can be checked without an SVG viewer (--png). Gradients
    become their mid colour: enough to see the shapes, not a second renderer to keep in step."""

    def __init__(self, width, height):
        from PIL import Image, ImageDraw
        self.image = Image.new("RGB", (int(width), int(height)), (11, 11, 12))
        self.d = ImageDraw.Draw(self.image, "RGBA")
        self.gradients = {}

    def _colour(self, value, opacity=1.0):
        if isinstance(value, str) and value.startswith("url(#"):
            top, bottom = self.gradients.get(value[5:-1], ((128, 128, 128), (64, 64, 64)))
            value = tuple((top[i] + bottom[i]) // 2 for i in range(3))
        if not isinstance(value, tuple):
            return value
        if len(value) == 4:
            return value
        return value + (int(round(255 * max(0.0, min(1.0, opacity)))),) if opacity < 1.0 else value

    def ellipse(self, box, fill):
        self.d.ellipse(box, fill=self._colour(fill))

    def rect(self, box, fill=None, rx=0.0, stroke=None, width=1.0):
        if fill is not None:
            self.d.rounded_rectangle(box, radius=rx, fill=self._colour(fill))
        if stroke is not None:
            self.d.rounded_rectangle(box, radius=rx, outline=self._colour(stroke), width=max(1, int(round(width))))

    def polygon(self, pts, fill):
        self.d.polygon([tuple(p) for p in pts], fill=self._colour(fill))

    def polyline(self, pts, stroke, width, opacity=1.0, cap="round"):
        self.d.line([tuple(p) for p in pts], fill=self._colour(stroke, opacity), width=max(1, int(round(width))), joint="curve")

    def circle(self, cx, cy, r, fill=None, stroke=None, width=1.0, opacity=1.0):
        box = (cx - r, cy - r, cx + r, cy + r)
        if fill is not None:
            self.d.ellipse(box, fill=self._colour(fill))
        if stroke is not None:
            self.d.ellipse(box, outline=self._colour(stroke, opacity), width=max(1, int(round(width))))

    def pieslice(self, box, a0_deg, a1_deg, fill, steps=64):
        x0, y0, x1, y1 = box
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        rx, ry = (x1 - x0) / 2, (y1 - y0) / 2
        pts = [(cx, cy)]
        for i in range(steps + 1):
            a = math.radians(a0_deg + (a1_deg - a0_deg) * i / steps)
            pts.append((cx + rx * math.cos(a), cy + ry * math.sin(a)))
        self.polygon(pts, fill)

    def defs_gradient(self, gid, top, bottom, x1=0.5, y1=0.0, x2=0.5, y2=1.0):
        self.gradients[gid] = (top, bottom)

    def defs_radial(self, gid, stops, cx=0.30, cy=0.20, r=1.05):
        # flattened to the gradient's own ends: the shapes stay visible, the shading does not
        self.gradients[gid] = (stops[0][1][:3], stops[-1][1][:3])

    def art(self, box, path):
        from PIL import Image
        art = Image.open(path).convert("RGBA")
        art = art.resize((max(1, int(round(box[2] - box[0]))), max(1, int(round(box[3] - box[1])))), Image.LANCZOS)
        self.image.paste(art, (int(round(box[0])), int(round(box[1]))), art)

    def text(self, x, y, body, size, fill, weight="600", family=FONT, anchor="middle", spacing=None):
        from PIL import ImageFont
        font = None
        for name in ("segoeuib.ttf", "arialbd.ttf", "ariblk.ttf", "DejaVuSans-Bold.ttf"):
            try:
                font = ImageFont.truetype(name, int(round(size)))
                break
            except OSError:
                continue
        if font is None:
            font = ImageFont.load_default()
        anchors = {"middle": "ms", "start": "ls", "end": "rs"}
        self.d.text((x, y), body, font=font, fill=self._colour(fill), anchor=anchors.get(anchor, "ms"))

    def render(self):
        return self.image


class Canvas:
    """The unit-space canvas the marks are drawn on (mirrors FoodGlyphs.cpp)."""

    def __init__(self, svg, area):
        self.d, self.area = svg, area

    def p(self, x, y):
        ax, ay, w = self.area
        return ax + x * w, ay + y * w

    def e(self, x, y, w, h):
        ax, ay, ww = self.area
        x0, y0 = self.p(x, y)
        return x0, y0, x0 + w * ww, y0 + h * ww

    def s(self):
        return self.area[2]


def quad(p0, p1, p2, n=20):
    return [((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0],
             (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1]) for t in [i / n for i in range(n + 1)]]


def cubic(p0, p1, p2, p3, n=24):
    return [((1 - t) ** 3 * p0[0] + 3 * (1 - t) ** 2 * t * p1[0] + 3 * (1 - t) * t * t * p2[0] + t ** 3 * p3[0],
             (1 - t) ** 3 * p0[1] + 3 * (1 - t) ** 2 * t * p1[1] + 3 * (1 - t) * t * t * p2[1] + t ** 3 * p3[1])
            for t in [i / n for i in range(n + 1)]]


def leaf_shape(c, base, tip, width):
    b, t = c.p(*base), c.p(*tip)
    dx, dy = t[0] - b[0], t[1] - b[1]
    length = max(1e-3, math.hypot(dx, dy))
    nx, ny = -dy / length, dx / length
    mx, my = (b[0] + t[0]) / 2, (b[1] + t[1]) / 2
    w = width * c.s()
    return quad(b, (mx + nx * w, my + ny * w), t) + quad(t, (mx - nx * w * 0.7, my - ny * w * 0.7), b)


# --- the shading every mark shares (FoodGlyphs.cpp's solid / gloss / contactShadow / paintLeaf) -------------------

def lift(c, t=0.3):
    return mix(c, (255, 255, 255), t)


def sink(c, t=0.3):
    return mix(c, (0, 0, 0), t)


def volume_stops(base, depth=1.0):
    return [(0.0, lift(base, 0.24 * depth)), (0.44, base), (0.80, sink(base, 0.20 * depth)), (1.0, sink(base, 0.40 * depth))]


def grad(s, base, depth=1.0):
    gid = "v%02X%02X%02X%02X" % (base[0], base[1], base[2], round(depth * 100))
    s.defs_radial(gid, volume_stops(base, depth))
    return "url(#%s)" % gid


def solid_poly(s, pts, base, depth=1.0):
    s.polygon(pts, grad(s, base, depth))


def solid_ellipse(s, c, x, y, w, h, base, depth=1.0):
    s.ellipse(c.e(x, y, w, h), grad(s, base, depth))


def gloss(s, c, x, y, w, h, base, alpha=0.40):
    box = c.e(x, y, w, h)
    s.ellipse(box, lift(base, 0.55) + (int(170 * alpha * 1.1),))
    dx, dy = (box[2] - box[0]) * 0.20, (box[3] - box[1]) * 0.22
    s.ellipse((box[0] + dx, box[1] + dy, box[2] - dx * 1.2, box[3] - dy * 1.6), lift(base, 0.80) + (int(210 * alpha * 1.2),))


def contact(s, c, x, y, w, h, alpha=0.18):
    s.ellipse(c.e(x, y, w, h), (0, 0, 0, int(255 * alpha)))


def leaf_shape_px(c, b, t, width):
    dx, dy = t[0] - b[0], t[1] - b[1]
    length = max(1e-3, math.hypot(dx, dy))
    nx, ny = -dy / length, dx / length
    mx, my = (b[0] + t[0]) / 2, (b[1] + t[1]) / 2
    w = width * c.s()
    return quad(b, (mx + nx * w, my + ny * w), t) + quad(t, (mx - nx * w * 0.7, my - ny * w * 0.7), b)


def paint_leaf_px(s, c, b, t, width, colour):
    gid = "leaf%02X%02X%02X" % colour
    s.defs_gradient(gid, lift(colour, 0.30), sink(colour, 0.28), x1=0.0, y1=0.0, x2=1.0, y2=1.0)
    s.polygon(leaf_shape_px(c, b, t, width), "url(#%s)" % gid)
    s.polyline([(b[0] + (t[0] - b[0]) * 0.12, b[1] + (t[1] - b[1]) * 0.12),
                (b[0] + (t[0] - b[0]) * 0.84, b[1] + (t[1] - b[1]) * 0.84)],
               sink(colour, 0.34) + (140,), 0.010 * c.s())


def paint_leaf(s, c, base, tip, width, colour):
    paint_leaf_px(s, c, c.p(*base), c.p(*tip), width, colour)


def rot_pt(c, p, about, angle):
    """A point of the mark after a turn about `about`: pixel space (FoodGlyphs.cpp's rotAbout)."""
    o = c.p(*about)
    x, y = c.p(*p)[0] - o[0], c.p(*p)[1] - o[1]
    return o[0] + x * math.cos(angle) - y * math.sin(angle), o[1] + x * math.sin(angle) + y * math.cos(angle)


def rotate_pts(pts, pivot, angle):
    ca, sa = math.cos(angle), math.sin(angle)
    return [(pivot[0] + (x - pivot[0]) * ca - (y - pivot[1]) * sa,
             pivot[1] + (x - pivot[0]) * sa + (y - pivot[1]) * ca) for x, y in pts]


def to_unit(c, px):
    ax, ay, w = c.area
    return (px[0] - ax) / w, (px[1] - ay) / w


def stem_poly(s, c, frm, ctrl, to, colour, width):
    pts = quad(c.p(*frm), c.p(*ctrl), c.p(*to))
    s.polyline(pts, colour, width * c.s())
    s.polyline(pts, lift(colour, 0.30) + (115,), width * 0.4 * c.s())


def arc_pts(c, centre, radius, a0, a1, n=28):
    return [(c.p(centre[0] + radius * math.cos(a0 + (a1 - a0) * i / n),
                 centre[1] + radius * math.sin(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]


def tube(s, c, frm, ctrl, to, colour, width):
    """FoodGlyphs.cpp's tube: a banana, shaded across its width, round at both ends."""
    pts = quad(c.p(*frm), c.p(*ctrl), c.p(*to), 30)
    gid = "tube%02X%02X%02X" % colour
    s.defs_gradient(gid, lift(colour, 0.30), sink(colour, 0.34), x1=0.0, y1=0.0, x2=0.0, y2=1.0)
    s.polyline(pts, "url(#%s)" % gid, width * c.s(), cap="round")


# --- the fruits (same geometry as src/ui/FoodGlyphs.cpp) -----------------------------------------------------

def pepper(s, c, ink, leafc):
    contact(s, c, 0.20, 0.84, 0.58, 0.12, 0.14)

    # the pod: full at the shoulder, tapering to the tip
    pts = cubic(c.p(0.47, 0.23), c.p(0.74, 0.30), c.p(0.74, 0.64), c.p(0.58, 0.94))[:-1]
    pts += cubic(c.p(0.58, 0.94), c.p(0.48, 0.72), c.p(0.32, 0.54), c.p(0.30, 0.27))[:-1]
    solid_poly(s, pts, ink, 1.05)
    solid_ellipse(s, c, 0.29, 0.19, 0.33, 0.18, ink, 0.9)

    shine = cubic(c.p(0.38, 0.31), c.p(0.35, 0.48), c.p(0.39, 0.68), c.p(0.47, 0.84))
    s.polyline(shine, (255, 255, 255, 92), 0.040 * c.s())
    gloss(s, c, 0.33, 0.25, 0.18, 0.13, ink, 0.42)

    # the green cap and the stem curling away from it
    solid_ellipse(s, c, 0.29, 0.18, 0.32, 0.10, leafc, 0.7)
    gloss(s, c, 0.34, 0.19, 0.16, 0.05, leafc, 0.28)
    stalk = cubic(c.p(0.45, 0.20), c.p(0.51, 0.11), c.p(0.62, 0.09), c.p(0.62, 0.02))
    s.polyline(stalk, leafc, 0.055 * c.s())
    s.polyline(stalk, lift(leafc, 0.32) + (128,), 0.022 * c.s())


def banana(s, c, ink, leafc):
    contact(s, c, 0.16, 0.80, 0.68, 0.12, 0.14)
    cream = lift(ink, 0.88)

    # the green stalk the hand hangs from, laid down first so the fruit covers all but its end
    s.rect(c.e(0.79, 0.17, 0.17, 0.30), fill=leafc, rx=0.070 * c.s())
    s.rect(c.e(0.87, 0.17, 0.09, 0.30), fill=sink(leafc, 0.32), rx=0.035 * c.s())

    # a hand of three, the front one lit: the bunch is cut at the right, so the tips point down and left
    tips = [(0.26, 0.62), (0.32, 0.74), (0.40, 0.86)]
    tube(s, c, (0.86, 0.18), (0.38, 0.16), tips[0], sink(ink, 0.34), 0.150)
    tube(s, c, (0.88, 0.30), (0.40, 0.30), tips[1], sink(ink, 0.16), 0.160)
    tube(s, c, (0.90, 0.44), (0.46, 0.44), tips[2], ink, 0.170)

    # the pale flesh at the two front tips, where the bananas were cut
    for tx, ty in tips[1:]:
        x, y = c.p(tx, ty)
        s.circle(x, y, 0.060 * c.s(), fill=sink(ink, 0.50))
        s.circle(x, y, 0.041 * c.s(), fill=cream)

    # the peel's ridge along the front fruit
    s.polyline(quad(c.p(0.80, 0.43), c.p(0.46, 0.44), c.p(0.38, 0.80), 30), sink(ink, 0.30) + (90,), 0.010 * c.s())


def kiwi(s, c, ink, leafc):
    skin = mix(ink, (0x7A, 0x52, 0x30), 0.78)
    centre = (0.41, 0.59)

    contact(s, c, 0.10, 0.80, 0.80, 0.12, 0.14)

    # the whole fruit behind, on the right: the skin and its fuzz
    solid_ellipse(s, c, 0.55, 0.11, 0.45, 0.51, skin, 0.9)
    for i in range(18):
        a = 2.39996 * i
        d = 0.05 + 0.14 * math.fmod(0.11 * i, 1.0)
        s.ellipse(c.e(0.775 + d * math.cos(a) - 0.007, 0.365 + d * math.sin(a) - 0.007, 0.014, 0.014), sink(skin, 0.34) + (77,))
    gloss(s, c, 0.60, 0.17, 0.18, 0.14, skin, 0.30)
    paint_leaf(s, c, (0.66, 0.20), (0.94, 0.03), 0.050, leafc)

    # the cut half in front: skin, flesh, pale core
    solid_ellipse(s, c, 0.06, 0.24, 0.70, 0.70, skin, 1.0)
    solid_ellipse(s, c, 0.11, 0.29, 0.60, 0.60, lift(ink, 0.12), 0.55)

    for i in range(18):
        a = 2 * math.pi * i / 18
        s.polyline([c.p(centre[0] + 0.055 * math.cos(a), centre[1] + 0.055 * math.sin(a)),
                    c.p(centre[0] + 0.255 * math.cos(a), centre[1] + 0.255 * math.sin(a))],
                   lift(ink, 0.46) + (128,), 0.012 * c.s())

    for i in range(14):
        a = 2 * math.pi * (i + 0.35) / 14
        s.ellipse(c.e(centre[0] + 0.155 * math.cos(a) - 0.012, centre[1] + 0.155 * math.sin(a) - 0.010, 0.024, 0.020), sink(ink, 0.90))
    solid_ellipse(s, c, centre[0] - 0.052, centre[1] - 0.046, 0.104, 0.092, lift(ink, 0.82), 0.4)

    # the stalk where it was cut from the vine
    s.ellipse(c.e(0.30, 0.19, 0.10, 0.07), sink(skin, 0.30))
    paint_leaf(s, c, (0.34, 0.22), (0.06, 0.03), 0.050, leafc)


def blueberry(s, c, ink, leafc):
    deep = sink(ink, 0.32)

    def berry(x, y, d, colour):
        contact(s, c, x + d * 0.02, y + d * 0.66, d * 0.94, d * 0.30, 0.16)
        solid_ellipse(s, c, x, y, d, d, colour, 1.0)
        gloss(s, c, x + d * 0.14, y + d * 0.08, d * 0.44, d * 0.30, colour, 0.50)

        # the blossom end: a small dark well ringed by its five teeth
        cx, cy = x + d * 0.52, y + d * 0.44
        s.ellipse(c.e(cx - d * 0.070, cy - d * 0.055, d * 0.140, d * 0.110), sink(colour, 0.62))
        for i in range(5):
            a = 2 * math.pi * i / 5 - 1.2
            s.polyline([c.p(cx + d * 0.050 * math.cos(a), cy + d * 0.045 * math.sin(a)),
                        c.p(cx + d * 0.115 * math.cos(a), cy + d * 0.100 * math.sin(a))],
                       sink(colour, 0.38) + (217,), 0.013 * c.s())

    berry(0.05, 0.45, 0.36, deep)
    berry(0.57, 0.48, 0.34, sink(ink, 0.46))
    berry(0.25, 0.25, 0.42, ink)

    stem_poly(s, c, (0.50, 0.33), (0.50, 0.27), (0.52, 0.21), leafc, 0.028)
    paint_leaf(s, c, (0.51, 0.24), (0.86, 0.09), 0.100, leafc)
    paint_leaf(s, c, (0.48, 0.24), (0.14, 0.07), 0.092, leafc)


def eggplant(s, c, ink, leafc):
    contact(s, c, 0.16, 0.84, 0.62, 0.12, 0.14)

    about = (0.50, 0.55)
    tilt = 0.26  # the lean of the fruit in the reference sheet
    pivot = c.p(*about)

    pts = cubic(c.p(0.50, 0.18), c.p(0.74, 0.30), c.p(0.78, 0.72), c.p(0.52, 0.92))[:-1]
    pts += cubic(c.p(0.52, 0.92), c.p(0.28, 0.80), c.p(0.21, 0.46), c.p(0.39, 0.26))[:-1]
    pts += cubic(c.p(0.39, 0.26), c.p(0.42, 0.22), c.p(0.46, 0.19), c.p(0.50, 0.18))[:-1]
    solid_poly(s, rotate_pts(pts, pivot, tilt), ink, 1.10)

    shine = rotate_pts(cubic(c.p(0.38, 0.34), c.p(0.33, 0.48), c.p(0.34, 0.64), c.p(0.40, 0.76)), pivot, tilt)
    s.polyline(shine, (255, 255, 255, 77), 0.052 * c.s())
    s.polyline(shine, (255, 255, 255, 41), 0.100 * c.s())
    gx, gy = to_unit(c, rot_pt(c, (0.40, 0.33), about, tilt))
    gloss(s, c, gx - 0.07, gy - 0.05, 0.14, 0.10, ink, 0.34)

    # the calyx: pointed sepals folding over the shoulders, and the stalk
    for frm, to, width in (((0.46, 0.26), (0.25, 0.17), 0.062), ((0.50, 0.25), (0.75, 0.18), 0.062),
                           ((0.45, 0.27), (0.33, 0.38), 0.040), ((0.53, 0.26), (0.66, 0.38), 0.040)):
        paint_leaf_px(s, c, rot_pt(c, frm, about, tilt), rot_pt(c, to, about, tilt), width, leafc)

    stalk = quad(rot_pt(c, (0.50, 0.22), about, tilt), rot_pt(c, (0.55, 0.13), about, tilt),
                 rot_pt(c, (0.59, 0.06), about, tilt))
    s.polyline(stalk, leafc, 0.050 * c.s())
    s.polyline(stalk, lift(leafc, 0.30) + (115,), 0.020 * c.s())

    kx, ky = to_unit(c, rot_pt(c, (0.50, 0.23), about, tilt))
    s.ellipse(c.e(kx - 0.05, ky - 0.035, 0.10, 0.07), sink(leafc, 0.45))


def lime(s, c, ink, leafc):
    contact(s, c, 0.14, 0.74, 0.72, 0.16, 0.14)

    # a lime cut across: the rind, the pale pith, then the flesh in segments
    solid_ellipse(s, c, 0.13, 0.18, 0.72, 0.68, sink(ink, 0.40), 1.0)
    solid_ellipse(s, c, 0.17, 0.22, 0.64, 0.60, lift(ink, 0.80), 0.25)
    solid_ellipse(s, c, 0.21, 0.26, 0.56, 0.52, lift(ink, 0.22), 0.35)

    centre = (0.49, 0.52)
    for i in range(8):
        a = 2 * math.pi * i / 8 + 0.18
        s.polyline([c.p(centre[0] + 0.045 * math.cos(a), centre[1] + 0.045 * math.sin(a)),
                    c.p(centre[0] + 0.245 * math.cos(a), centre[1] + 0.235 * math.sin(a))],
                   lift(ink, 0.72) + (217,), 0.014 * c.s())
    solid_ellipse(s, c, centre[0] - 0.040, centre[1] - 0.036, 0.080, 0.072, lift(ink, 0.62), 0.3)

    gloss(s, c, 0.20, 0.24, 0.22, 0.14, ink, 0.30)
    paint_leaf(s, c, (0.62, 0.22), (0.90, 0.06), 0.055, leafc)
    stem_poly(s, c, (0.62, 0.22), (0.58, 0.16), (0.52, 0.14), leafc, 0.036)


def watermelon(s, c, ink, leafc):
    contact(s, c, 0.14, 0.86, 0.72, 0.10, 0.10)

    # a cut face seen head-on (as in FoodGlyphs.cpp): the rind, the pale line under it, the flesh
    x, y, size = 0.07, 0.08, 0.86
    solid_ellipse(s, c, x, y, size, size, sink(leafc, 0.18), 1.05)
    solid_ellipse(s, c, x + 0.035, y + 0.035, size - 0.07, size - 0.07, lift(leafc, 0.80), 0.25)
    solid_ellipse(s, c, x + 0.065, y + 0.065, size - 0.13, size - 0.13, ink, 0.75)

    cx, cy = x + size / 2, y + size / 2

    def seed(angle, dist, w, h):
        sx, sy = cx + dist * math.cos(angle), cy + dist * math.sin(angle)
        turn = angle + math.pi / 2
        pts = [(sx + (w / 2) * math.cos(t) * math.cos(turn) - (h / 2) * math.sin(t) * math.sin(turn),
                sy + (w / 2) * math.cos(t) * math.sin(turn) + (h / 2) * math.sin(t) * math.cos(turn))
               for t in [i * math.tau / 24 for i in range(24)]]
        s.polygon([c.p(px, py) for px, py in pts], sink(ink, 0.72))

    for i in range(8):
        seed(math.tau * i / 8 + 0.35, 0.285, 0.040, 0.058)
    for i in range(5):
        seed(math.tau * i / 5 + 0.95, 0.150, 0.032, 0.048)

    gloss(s, c, 0.21, 0.22, 0.34, 0.18, ink, 0.24)


def mango(s, c, ink, leafc):
    contact(s, c, 0.12, 0.72, 0.76, 0.16, 0.14)

    pts = cubic(c.p(0.66, 0.16), c.p(0.92, 0.28), c.p(0.90, 0.66), c.p(0.62, 0.80))[:-1]
    pts += cubic(c.p(0.62, 0.80), c.p(0.34, 0.94), c.p(0.06, 0.70), c.p(0.14, 0.46))[:-1]
    pts += cubic(c.p(0.14, 0.46), c.p(0.22, 0.24), c.p(0.44, 0.10), c.p(0.66, 0.16))[:-1]
    solid_poly(s, pts, ink, 1.05)

    gloss(s, c, 0.28, 0.28, 0.30, 0.24, ink, 0.34)
    s.polyline(cubic(c.p(0.66, 0.76), c.p(0.78, 0.60), c.p(0.76, 0.40), c.p(0.68, 0.26)), sink(ink, 0.30) + (90,), 0.055 * c.s())

    paint_leaf(s, c, (0.72, 0.15), (0.96, 0.03), 0.048, leafc)
    stem_poly(s, c, (0.70, 0.16), (0.66, 0.10), (0.62, 0.07), leafc, 0.034)


def cherry(s, c, ink, leafc):
    contact(s, c, 0.10, 0.78, 0.76, 0.14, 0.14)

    stem_poly(s, c, (0.50, 0.12), (0.34, 0.28), (0.30, 0.56), leafc, 0.034)
    stem_poly(s, c, (0.50, 0.12), (0.62, 0.32), (0.68, 0.60), leafc, 0.034)

    def fruit(x, y, d, colour):
        contact(s, c, x + d * 0.04, y + d * 0.68, d * 0.92, d * 0.28, 0.16)
        solid_ellipse(s, c, x, y, d, d, colour, 1.0)
        gloss(s, c, x + d * 0.18, y + d * 0.12, d * 0.30, d * 0.20, colour, 0.60)

    fruit(0.46, 0.56, 0.34, sink(ink, 0.30))
    fruit(0.10, 0.52, 0.38, ink)

    s.ellipse(c.e(0.465, 0.10, 0.07, 0.05), sink(leafc, 0.30))
    paint_leaf(s, c, (0.52, 0.13), (0.82, 0.05), 0.055, leafc)


def avocado(s, c, ink, leafc):
    contact(s, c, 0.18, 0.82, 0.62, 0.14, 0.14)

    # the half: dark skin, pale flesh, one stone
    skin = cubic(c.p(0.50, 0.09), c.p(0.67, 0.09), c.p(0.74, 0.28), c.p(0.76, 0.46))[:-1]
    skin += cubic(c.p(0.76, 0.46), c.p(0.79, 0.68), c.p(0.69, 0.90), c.p(0.50, 0.92))[:-1]
    skin += cubic(c.p(0.50, 0.92), c.p(0.31, 0.90), c.p(0.21, 0.68), c.p(0.24, 0.46))[:-1]
    skin += cubic(c.p(0.24, 0.46), c.p(0.26, 0.28), c.p(0.33, 0.09), c.p(0.50, 0.09))[:-1]
    solid_poly(s, skin, sink(leafc, 0.32), 1.0)

    flesh = cubic(c.p(0.50, 0.16), c.p(0.63, 0.16), c.p(0.68, 0.31), c.p(0.70, 0.46))[:-1]
    flesh += cubic(c.p(0.70, 0.46), c.p(0.72, 0.64), c.p(0.64, 0.84), c.p(0.50, 0.86))[:-1]
    flesh += cubic(c.p(0.50, 0.86), c.p(0.36, 0.84), c.p(0.28, 0.64), c.p(0.30, 0.46))[:-1]
    flesh += cubic(c.p(0.30, 0.46), c.p(0.32, 0.31), c.p(0.37, 0.16), c.p(0.50, 0.16))[:-1]
    solid_poly(s, flesh, lift(ink, 0.28), 0.55)

    solid_ellipse(s, c, 0.38, 0.53, 0.25, 0.27, mix(ink, (0x8A, 0x5A, 0x2B), 0.78), 0.9)
    gloss(s, c, 0.42, 0.57, 0.09, 0.07, mix(ink, (0x8A, 0x5A, 0x2B), 0.78), 0.45)

    paint_leaf(s, c, (0.66, 0.22), (0.92, 0.05), 0.050, leafc)


def dragonfruit(s, c, ink, leafc):
    contact(s, c, 0.16, 0.80, 0.66, 0.12, 0.14)

    solid_ellipse(s, c, 0.13, 0.23, 0.74, 0.64, ink, 1.0)

    # the green scales curling off the skin
    centre = (0.50, 0.55)
    for i in range(7):
        a = 2 * math.pi * i / 7 - 1.9
        base = (centre[0] + 0.22 * math.cos(a), centre[1] + 0.19 * math.sin(a))
        tip = (centre[0] + 0.365 * math.cos(a), centre[1] + 0.335 * math.sin(a))
        paint_leaf(s, c, base, tip, 0.034, leafc)

    # the crown of leaves at the top
    paint_leaf(s, c, (0.43, 0.26), (0.33, 0.03), 0.042, leafc)
    paint_leaf(s, c, (0.51, 0.25), (0.58, 0.02), 0.040, leafc)
    paint_leaf(s, c, (0.47, 0.25), (0.45, 0.01), 0.036, leafc)

    # the cut face at the front: white flesh, its speckle of seeds, the pink rim round it
    solid_ellipse(s, c, 0.28, 0.36, 0.44, 0.44, lift(ink, 0.55), 0.55)
    solid_ellipse(s, c, 0.31, 0.39, 0.38, 0.38, lift(ink, 0.90), 0.15)
    for i in range(21):
        a = 2.39996 * i
        d = 0.012 + 0.140 * math.sqrt(i / 21.0)
        s.ellipse(c.e(0.50 + d * math.cos(a) - 0.007, 0.58 + d * math.sin(a) - 0.007, 0.014, 0.014), sink(ink, 0.55))


def lychee(s, c, ink, leafc):
    contact(s, c, 0.12, 0.78, 0.72, 0.16, 0.14)

    solid_ellipse(s, c, 0.12, 0.22, 0.72, 0.70, ink, 1.0)

    # the bumps: the fruit's skin is all of them, so they carry the shape
    for i in range(22):
        a = 2.39996 * i
        d = 0.30 * math.sqrt((i + 0.4) / 22.0)
        x, y = 0.48 + d * math.cos(a), 0.57 + d * math.sin(a) * 0.92
        s.ellipse(c.e(x - 0.038, y - 0.032, 0.076, 0.064), sink(ink, 0.26))
        s.ellipse(c.e(x - 0.022, y - 0.024, 0.030, 0.022), lift(ink, 0.30) + (140,))
    gloss(s, c, 0.20, 0.28, 0.24, 0.16, ink, 0.40)

    stem_poly(s, c, (0.54, 0.26), (0.54, 0.18), (0.48, 0.13), leafc, 0.034)
    paint_leaf(s, c, (0.56, 0.27), (0.86, 0.08), 0.050, leafc)


def fig(s, c, ink, leafc):
    contact(s, c, 0.16, 0.84, 0.68, 0.12, 0.14)

    # a squat fig: as wide as it is tall, with a flat crown and a round base
    skin = cubic(c.p(0.50, 0.20), c.p(0.72, 0.20), c.p(0.82, 0.40), c.p(0.78, 0.62))[:-1]
    skin += cubic(c.p(0.78, 0.62), c.p(0.74, 0.84), c.p(0.62, 0.92), c.p(0.50, 0.92))[:-1]
    skin += cubic(c.p(0.50, 0.92), c.p(0.38, 0.92), c.p(0.26, 0.84), c.p(0.22, 0.62))[:-1]
    skin += cubic(c.p(0.22, 0.62), c.p(0.18, 0.40), c.p(0.28, 0.20), c.p(0.50, 0.20))[:-1]
    solid_poly(s, skin, ink, 1.05)
    gloss(s, c, 0.23, 0.33, 0.14, 0.20, ink, 0.30)

    # cut open: the pale rim under the skin, the red interior and its seeds
    solid_ellipse(s, c, 0.24, 0.28, 0.54, 0.56, lift(ink, 0.60), 0.4)

    flesh = mix(ink, (0xC8, 0x30, 0x5A), 0.72)
    solid_ellipse(s, c, 0.27, 0.31, 0.48, 0.50, flesh, 0.7)

    for i in range(18):
        a = 2.39996 * i
        d = 0.020 + 0.150 * math.sqrt((i + 0.5) / 18.0)
        s.ellipse(c.e(0.51 + d * math.cos(a) - 0.008, 0.56 + d * math.sin(a) - 0.007, 0.016, 0.014), lift(flesh, 0.55))

    stem_poly(s, c, (0.50, 0.21), (0.46, 0.13), (0.52, 0.06), leafc, 0.046)
    paint_leaf(s, c, (0.52, 0.14), (0.82, 0.05), 0.050, leafc)


def coconut(s, c, ink, leafc):
    husk = mix(ink, (0x6B, 0x4A, 0x2A), 0.62)
    contact(s, c, 0.08, 0.76, 0.86, 0.16, 0.14)

    # the whole nut: husk, fibres and the three eyes
    solid_ellipse(s, c, 0.05, 0.16, 0.62, 0.64, husk, 1.0)
    for i in range(9):
        a = 2 * math.pi * i / 9
        s.polyline([c.p(0.36 + 0.10 * math.cos(a), 0.46 + 0.12 * math.sin(a)),
                    c.p(0.36 + 0.26 * math.cos(a), 0.46 + 0.30 * math.sin(a))], sink(husk, 0.30) + (90,), 0.012 * c.s())
    for i in range(3):
        s.ellipse(c.e(0.26 + 0.075 * i, 0.30, 0.058, 0.058), sink(husk, 0.62))
    gloss(s, c, 0.10, 0.21, 0.22, 0.14, husk, 0.32)

    # the half beside it, split open on its white meat
    solid_ellipse(s, c, 0.46, 0.44, 0.50, 0.50, husk, 1.0)
    solid_ellipse(s, c, 0.505, 0.485, 0.41, 0.41, lift(ink, 0.62), 0.35)
    solid_ellipse(s, c, 0.535, 0.515, 0.35, 0.35, lift(ink, 0.14), 0.45)
    gloss(s, c, 0.56, 0.53, 0.16, 0.12, lift(ink, 0.14), 0.35)

    paint_leaf(s, c, (0.64, 0.24), (0.92, 0.08), 0.055, leafc)


MARKS = dict(pepper=pepper, banana=banana, kiwi=kiwi, blueberry=blueberry, eggplant=eggplant, lime=lime,
             watermelon=watermelon, cherry=cherry, avocado=avocado, dragonfruit=dragonfruit,
             lychee=lychee, fig=fig, coconut=coconut)

# The fruit art cut from the reference sheet by tools/extract_fruit_marks.py: the same PNGs ui/FoodMarkData.h embeds,
# so the picture here is the app's mark rather than a redrawing of it. Only the kits this sheet has no art for fall back
# to the vector shapes above.
MARK_DIR = pathlib.Path(__file__).resolve().parent.parent / "docs" / "img" / "marks"


def draw_mark(s, key, x, y, size, ink, leafc):
    art = MARK_DIR / (key + ".png")
    if art.exists():
        s.art((x, y, x + size, y + size), str(art))
    else:
        MARKS.get(key, pepper)(s, Canvas(s, (x, y, size)), ink, leafc)


def badge(s, kit, cx, cy, size, selected=False):
    key, name, hue, taste, accent, leafc, bg, ws = kit
    ink, leaf, bg_rgb = rgb(accent), rgb(leafc), rgb(bg)
    r = size / 2

    # the disc: the kit's background, lighter at the top
    gid = "disc-" + key
    s.defs_gradient(gid, brighter(bg_rgb, 0.12), darker(bg_rgb, 0.45))
    s.circle(cx, cy, r, fill="url(#%s)" % gid)

    # the ring and its halo
    s.circle(cx, cy, r + r * 0.09, stroke=col(ink), width=r * 0.14, opacity=0.10 if selected else 0.05)
    s.circle(cx, cy, r + r * 0.035, stroke=col(ink), width=r * 0.06, opacity=0.20 if selected else 0.10)
    s.circle(cx, cy, r - 1, stroke=col(ink), width=max(2.0, r * 0.09) if selected else max(1.2, r * 0.044),
             opacity=0.95 if selected else 0.62)

    inner = size * 0.94
    draw_mark(s, key, cx - inner / 2, cy - inner / 2, inner, ink, leaf)


def logo(s, kit, x, y, size):
    # FoodGlyphs.cpp's paintLogo: the kit's own dark surface, the accent as its edge and a halo outside it, and the
    # fruit in its fruit colours - so the banana stays yellow instead of turning into a silhouette.
    key, name, hue, taste, accent, leafc, bg, ws = kit
    accent_rgb, leaf, bg_rgb = rgb(accent), rgb(leafc), rgb(bg)
    radius = size * 0.22
    gid = "logo-" + key
    s.defs_gradient(gid, brighter(bg_rgb, 0.16), darker(bg_rgb, 0.55))
    s.rect((x, y, x + size, y + size), fill="url(#%s)" % gid, rx=radius)
    # a blended halo and edge, because neither backend strokes with an opacity
    pad = size * 0.035
    s.rect((x - pad, y - pad, x + size + pad, y + size + pad), rx=radius + pad,
           stroke=mix(bg_rgb, accent_rgb, 0.20), width=max(1.0, size * 0.012))
    s.rect((x, y, x + size, y + size), rx=radius, stroke=mix(bg_rgb, accent_rgb, 0.85), width=max(1.0, size * 0.016))
    inner = size * 0.80
    draw_mark(s, key, x + (size - inner) / 2, y + (size - inner) / 2, inner, accent_rgb, leaf)


CARD_W, CARD_H, COLS = 158, 196, 7
ROWS = (len(KITS) + COLS - 1) // COLS
LOGO_ROW_H = 116
WIDTH = CARD_W * COLS
HEIGHT = 54 + ROWS * CARD_H + 44 + LOGO_ROW_H + 14


def build(s):
    """Draws the page into a backend (Svg for the file, Pil for the --png preview)."""
    logos = KITS + [PEPPER]
    s.rect((0, 0, WIDTH, HEIGHT), fill="#0b0b0c")
    s.text(24, 34, "FOOD THEME KITS", 15, "#eceef2", weight="700", anchor="start", spacing=2.4)
    s.text(WIDTH - 24, 34, "OPTIONS \u203A THEME", 10, "#60656f", weight="600", anchor="end", spacing=1.6)

    badge_size = 112
    for i, kit in enumerate(KITS):
        key, name, hue, taste, accent, leafc, bg, ws = kit
        x0 = (i % COLS) * CARD_W
        y0 = 54 + (i // COLS) * CARD_H
        badge(s, kit, x0 + CARD_W / 2, y0 + badge_size / 2 + 6, badge_size, selected=(i == 3))
        s.text(x0 + CARD_W / 2, y0 + badge_size + 30, name.upper(), 11.5,
               "#eceef2" if i == 3 else hexc(accent), weight="700", spacing=1.0)
        s.text(x0 + CARD_W / 2, y0 + badge_size + 46, "%s | %s" % (hue, taste), 9.5, "#8b9099", weight="500",
               family=MONO, spacing=0.6)

    strip_y = 54 + ROWS * CARD_H + 16
    s.text(24, strip_y + 8, "THE FOOD LOGO", 10, "#60656f", weight="700", anchor="start", spacing=1.8)
    logo_size, gap = 60, 14
    for i, kit in enumerate(logos):
        logo(s, kit, 24 + i * (logo_size + gap), strip_y + 26, logo_size)

    s.text(WIDTH - 24, HEIGHT - 12, "the same fruit on the logo's tile, edged in each theme's accent", 9, "#4a4e57",
           weight="500", anchor="end")
    return s


def svg_document():
    return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d" '
            'role="img" aria-label="The thirteen food theme kits, each as a badge with its fruit, and the food logo in '
            'every theme\'s colour">\n' % (WIDTH, HEIGHT, WIDTH, HEIGHT)) + build(Svg()).render() + "\n</svg>\n"


def main():
    import sys
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    if "--png" in sys.argv:  # a quick look without an SVG viewer (gradients flattened)
        index = sys.argv.index("--png")
        path = sys.argv[index + 1] if index + 1 < len(sys.argv) else "theme-kits.png"
        build(Pil(WIDTH, HEIGHT)).render().save(path)
        print("wrote", path)
        return

    out = os.path.join(root, "docs", "img", "theme-kits.svg")
    svg = svg_document()
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write(svg)
    print("wrote", os.path.relpath(out, root), "(%d bytes)" % len(svg.encode("utf-8")))


if __name__ == "__main__":
    main()
