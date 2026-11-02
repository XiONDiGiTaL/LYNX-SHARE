#!/usr/bin/env python3
"""Writes docs/img/theme-kits.svg: the food theme kits, as Options > Theme draws them.

A kit is a seed pair for core/ThemePalette.h plus its food identity (core/ThemeKits.h): the accent, the leaf, the
workspace and the NAME | HUE | TASTE caption. This script draws each one as the badge the theme page shows - a dark
disc, a ring glowing in the kit's accent, the fruit on it - and the same fruit inside the food logo.

The fruit shapes below mirror src/ui/FoodGlyphs.cpp, which draws them in JUCE paths from the same unit space (0..1,
y down). Change a fruit there and here together; the numbers are the design, not a screenshot.

    python tools/make_theme_kits.py            # rewrites docs/img/theme-kits.svg
"""

import math
import os

# id, name, hue, taste, accent, leaf, background, workspace
KITS = [
    ("cherry", "Cherry", "RED", "BRIGHT", 0xD92B45, 0x4E7A3A, 0x160F11, 0x0A0708),
    ("watermelon", "Watermelon", "PINK", "JUICY", 0xFF5C73, 0x3F8A4F, 0x101614, 0x070908),
    ("mango", "Mango", "ORANGE", "TROPICAL", 0xFF9E1F, 0x4F8A38, 0x1A1209, 0x0C0804),
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


def luminance(c):
    return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]


def readable_on(c):
    """White unless the colour is clearly light, as themes::readableTextOn does."""
    return (255, 255, 255) if luminance(c) < 148 else (17, 18, 20)


def css(value):
    """A colour as an SVG string: the marks pass (r, g, b) tuples, the page passes #RRGGBB."""
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


def stem(c, frm, ctrl, to, colour, width):
    return quad(c.p(*frm), c.p(*ctrl), c.p(*to)), colour, width * c.s()


# --- the fruits (same geometry as src/ui/FoodGlyphs.cpp) -----------------------------------------------------

def pepper(s, c, ink, leaf):
    pts = cubic(c.p(0.48, 0.22), c.p(0.74, 0.32), c.p(0.74, 0.64), c.p(0.58, 0.92))[:-1]
    pts += cubic(c.p(0.58, 0.92), c.p(0.54, 0.70), c.p(0.34, 0.56), c.p(0.28, 0.26))[:-1]
    s.polygon(pts, ink)
    s.ellipse(c.e(0.32, 0.22, 0.26, 0.16), ink)
    s.polyline(cubic(c.p(0.44, 0.34), c.p(0.52, 0.46), c.p(0.54, 0.62), c.p(0.52, 0.78)), brighter(ink, 0.32), 0.05 * c.s())
    pts_, colr, width = stem(c, (0.44, 0.24), (0.34, 0.14), (0.46, 0.06), leaf, 0.07)
    s.polyline(pts_, colr, width)
    s.ellipse(c.e(0.30, 0.20, 0.26, 0.08), leaf)


def banana(s, c, ink, leaf):
    cx, cy = c.p(0.50, 0.27)
    outer, inner = 0.56 * c.s(), 0.30 * c.s()
    a0, a1 = 0.78, 2.36
    pts = [(cx + outer * math.cos(a0 + (a1 - a0) * i / 24), cy + outer * math.sin(a0 + (a1 - a0) * i / 24))
           for i in range(25)]
    pts += [(cx + inner * math.cos(a1 + (a0 - a1) * i / 24), cy + inner * math.sin(a1 + (a0 - a1) * i / 24))
            for i in range(25)]
    s.polygon(pts, ink)
    mid = (outer + inner) / 2
    s.polyline([(cx + mid * math.cos(a0 + 0.12 + (a1 - a0 - 0.24) * i / 20),
                 cy + mid * math.sin(a0 + 0.12 + (a1 - a0 - 0.24) * i / 20)) for i in range(21)],
               darker(ink, 0.20), 0.014 * c.s())
    for a, colour, size in ((a0, leaf, 0.065), (a1, darker(ink, 0.55), 0.045)):
        s.circle(cx + 0.43 * c.s() * math.cos(a), cy + 0.43 * c.s() * math.sin(a), size * c.s() / 2, fill=colour)


def kiwi(s, c, ink, leaf):
    skin = mix(ink, (0x6B, 0x4A, 0x2A), 0.62)
    s.ellipse(c.e(0.09, 0.16, 0.76, 0.70), skin)
    s.ellipse(c.e(0.17, 0.24, 0.60, 0.54), brighter(ink, 0.10))
    s.ellipse(c.e(0.41, 0.42, 0.14, 0.14), brighter(ink, 0.55))
    for i in range(10):
        a = 2 * math.pi * i / 10
        s.ellipse(c.e(0.47 + 0.17 * math.cos(a) - 0.017, 0.49 + 0.16 * math.sin(a) - 0.017, 0.034, 0.034), darker(ink, 0.78))
    s.ellipse(c.e(0.44, 0.11, 0.13, 0.09), leaf)
    s.polygon(leaf_shape(c, (0.51, 0.14), (0.78, 0.05), 0.075), leaf)


def blueberry(s, c, ink, leaf):
    deep = darker(ink, 0.35)

    def berry(x, y, d, colour):
        s.ellipse(c.e(x, y, d, d), colour)
        s.ellipse(c.e(x + d * 0.20, y + d * 0.15, d * 0.28, d * 0.20), brighter(colour, 0.35))
        s.ellipse(c.e(x + d * 0.40, y + d * 0.38, d * 0.16, d * 0.14), darker(colour, 0.6))

    berry(0.08, 0.42, 0.38, deep)
    berry(0.54, 0.44, 0.36, deep)
    berry(0.28, 0.28, 0.40, ink)
    s.polygon(leaf_shape(c, (0.50, 0.34), (0.76, 0.12), 0.05), leaf)
    s.polygon(leaf_shape(c, (0.50, 0.34), (0.28, 0.12), 0.045), leaf)


def eggplant(s, c, ink, leaf):
    pts = cubic(c.p(0.50, 0.20), c.p(0.70, 0.32), c.p(0.74, 0.74), c.p(0.50, 0.94))[:-1]
    pts += cubic(c.p(0.50, 0.94), c.p(0.26, 0.74), c.p(0.30, 0.32), c.p(0.50, 0.20))[:-1]
    s.polygon(pts, ink)
    s.polyline(cubic(c.p(0.38, 0.42), c.p(0.32, 0.56), c.p(0.35, 0.72), c.p(0.42, 0.82)), brighter(ink, 0.38), 0.05 * c.s())
    s.polygon(leaf_shape(c, (0.50, 0.30), (0.24, 0.16), 0.085), leaf)
    s.polygon(leaf_shape(c, (0.50, 0.30), (0.76, 0.16), 0.085), leaf)
    s.polygon(leaf_shape(c, (0.50, 0.30), (0.50, 0.10), 0.075), leaf)
    pts_, colr, width = stem(c, (0.50, 0.22), (0.55, 0.12), (0.58, 0.05), leaf, 0.055)
    s.polyline(pts_, colr, width)


def lime(s, c, ink, leaf):
    s.ellipse(c.e(0.14, 0.22, 0.66, 0.60), ink)
    s.ellipse(c.e(0.34, 0.40, 0.26, 0.24), brighter(ink, 0.34))
    for i in range(6):
        a = 2 * math.pi * i / 6
        s.polyline([c.p(0.47, 0.52), c.p(0.47 + 0.10 * math.cos(a), 0.52 + 0.10 * math.sin(a))], brighter(ink, 0.10), 0.015 * c.s())
    s.ellipse(c.e(0.24, 0.30, 0.20, 0.10), brighter(ink, 0.45))
    s.polygon(leaf_shape(c, (0.62, 0.24), (0.86, 0.10), 0.055), leaf)
    pts_, colr, width = stem(c, (0.62, 0.24), (0.58, 0.18), (0.52, 0.16), leaf, 0.04)
    s.polyline(pts_, colr, width)


def watermelon(s, c, ink, leaf):
    x0, y0 = c.p(0.50, 0.46)
    r = 0.40 * c.s()
    s.pieslice((x0 - r * 1.10, y0 - r * 1.10, x0 + r * 1.10, y0 + r * 1.10), 0, 180, leaf)
    s.pieslice((x0 - r, y0 - r, x0 + r, y0 + r), 0, 180, ink)
    for i in range(5):
        a = math.pi * (0.18 + 0.16 * i)
        d = 0.15 + (0.09 if i % 2 == 0 else 0.0)
        s.ellipse(c.e(0.50 + d * math.cos(a) - 0.022, 0.50 + d * math.sin(a) - 0.030, 0.044, 0.060), darker(ink, 0.62))


def mango(s, c, ink, leaf):
    box = c.e(0.11, 0.27, 0.74, 0.50)
    ang = -0.42
    rx, ry = (box[2] - box[0]) / 2, (box[3] - box[1]) / 2
    mx, my = (box[0] + box[2]) / 2, (box[1] + box[3]) / 2
    pts = []
    for i in range(60):
        t = 2 * math.pi * i / 60
        x, y = rx * math.cos(t), ry * math.sin(t)
        pts.append((mx + x * math.cos(ang) - y * math.sin(ang), my + x * math.sin(ang) + y * math.cos(ang)))
    s.polygon(pts, ink)
    s.ellipse(c.e(0.22, 0.31, 0.34, 0.20), brighter(ink, 0.18))
    s.polygon(leaf_shape(c, (0.58, 0.26), (0.80, 0.08), 0.055), leaf)
    pts_, colr, width = stem(c, (0.58, 0.26), (0.56, 0.18), (0.50, 0.14), leaf, 0.04)
    s.polyline(pts_, colr, width)


def cherry(s, c, ink, leaf):
    s.polyline(cubic(c.p(0.50, 0.16), c.p(0.36, 0.30), c.p(0.30, 0.40), c.p(0.30, 0.52)), leaf, 0.042 * c.s())
    s.polyline(cubic(c.p(0.50, 0.16), c.p(0.60, 0.32), c.p(0.68, 0.42), c.p(0.68, 0.54)), leaf, 0.042 * c.s())
    for x, y in ((0.10, 0.54), (0.54, 0.56)):
        s.ellipse(c.e(x, y, 0.34, 0.34), ink)
        s.ellipse(c.e(x + 0.06, y + 0.06, 0.10, 0.08), brighter(ink, 0.40))
    s.polygon(leaf_shape(c, (0.52, 0.18), (0.80, 0.08), 0.06), leaf)


def avocado(s, c, ink, leaf):
    s.ellipse(c.e(0.20, 0.38, 0.60, 0.52), ink)
    s.ellipse(c.e(0.32, 0.14, 0.36, 0.44), ink)
    s.ellipse(c.e(0.25, 0.42, 0.50, 0.42), brighter(ink, 0.28))
    s.ellipse(c.e(0.35, 0.19, 0.30, 0.36), brighter(ink, 0.28))
    s.ellipse(c.e(0.39, 0.56, 0.24, 0.22), mix(ink, (0x8A, 0x5A, 0x2B), 0.80))
    s.polygon(leaf_shape(c, (0.66, 0.24), (0.90, 0.06), 0.055), leaf)


def dragonfruit(s, c, ink, leaf):
    for i in range(10):
        a = 2 * math.pi * i / 10
        x, y = 0.50 + 0.29 * math.cos(a), 0.50 + 0.28 * math.sin(a)
        nx, ny = math.cos(a), math.sin(a)
        s.polygon([c.p(x - 0.055 * ny, y + 0.055 * nx), c.p(x + 0.115 * nx, y + 0.115 * ny),
                   c.p(x + 0.055 * ny, y - 0.055 * nx)], leaf)
    s.ellipse(c.e(0.20, 0.26, 0.60, 0.52), ink)
    s.ellipse(c.e(0.31, 0.35, 0.38, 0.34), brighter(ink, 0.72))
    for i in range(12):
        a = 2 * math.pi * i / 12
        d = 0.12 if i % 2 == 0 else 0.075
        s.ellipse(c.e(0.50 + d * math.cos(a) - 0.014, 0.52 + d * math.sin(a) - 0.014, 0.028, 0.028), darker(ink, 0.55))


def lychee(s, c, ink, leaf):
    s.ellipse(c.e(0.14, 0.26, 0.66, 0.60), ink)
    for i in range(12):
        a = 2 * math.pi * i / 12
        s.ellipse(c.e(0.47 + 0.29 * math.cos(a) - 0.025, 0.56 + 0.27 * math.sin(a) - 0.025, 0.050, 0.050), darker(ink, 0.32))
    s.ellipse(c.e(0.30, 0.36, 0.22, 0.14), brighter(ink, 0.45))
    s.polygon(leaf_shape(c, (0.56, 0.28), (0.84, 0.10), 0.055), leaf)
    pts_, colr, width = stem(c, (0.56, 0.28), (0.54, 0.20), (0.48, 0.16), leaf, 0.042)
    s.polyline(pts_, colr, width)


def fig(s, c, ink, leaf):
    pts = cubic(c.p(0.50, 0.22), c.p(0.84, 0.34), c.p(0.88, 0.80), c.p(0.50, 0.94))[:-1]
    pts += cubic(c.p(0.50, 0.94), c.p(0.12, 0.80), c.p(0.16, 0.34), c.p(0.50, 0.22))[:-1]
    s.polygon(pts, ink)
    s.ellipse(c.e(0.33, 0.38, 0.34, 0.40), brighter(ink, 0.62))
    for i in range(9):
        a = 2 * math.pi * i / 9
        s.ellipse(c.e(0.50 + 0.10 * math.cos(a) - 0.013, 0.58 + 0.13 * math.sin(a) - 0.013, 0.026, 0.026), darker(ink, 0.55))
    pts_, colr, width = stem(c, (0.50, 0.24), (0.46, 0.14), (0.52, 0.06), leaf, 0.055)
    s.polyline(pts_, colr, width)
    s.polygon(leaf_shape(c, (0.52, 0.14), (0.76, 0.06), 0.055), leaf)


def coconut(s, c, ink, leaf):
    husk = mix(ink, (0x6B, 0x4A, 0x2A), 0.55)
    s.ellipse(c.e(0.10, 0.20, 0.78, 0.74), husk)
    s.ellipse(c.e(0.20, 0.30, 0.34, 0.26), brighter(husk, 0.18))
    for i in range(3):
        s.ellipse(c.e(0.31 + 0.11 * i, 0.36, 0.075, 0.075), darker(husk, 0.60))
    s.polygon(leaf_shape(c, (0.78, 0.24), (0.96, 0.10), 0.06), leaf)


MARKS = dict(pepper=pepper, banana=banana, kiwi=kiwi, blueberry=blueberry, eggplant=eggplant, lime=lime,
             watermelon=watermelon, mango=mango, cherry=cherry, avocado=avocado, dragonfruit=dragonfruit,
             lychee=lychee, fig=fig, coconut=coconut)


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

    inner = size * 0.66
    MARKS.get(key, pepper)(s, Canvas(s, (cx - inner / 2, cy - inner / 2, inner)), ink, leaf)


def logo(s, kit, x, y, size):
    key, name, hue, taste, accent, leafc, bg, ws = kit
    accent_rgb = rgb(accent)
    ink = readable_on(accent_rgb)
    radius = size * 0.22
    gid = "logo-" + key
    s.defs_gradient(gid, brighter(accent_rgb, 0.14), darker(accent_rgb, 0.26))
    s.rect((x, y, x + size, y + size), fill="url(#%s)" % gid, rx=radius)
    s.rect((x, y, x + size, y + size), rx=radius, stroke=darker(accent_rgb, 0.42), width=size * 0.014)
    inner = size * 0.60
    MARKS.get(key, pepper)(s, Canvas(s, (x + (size - inner) / 2, y + (size - inner) / 2, inner)), ink,
                           mix(ink, accent_rgb, 0.38))


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

    s.text(WIDTH - 24, HEIGHT - 12, "the same fruit in the logo, in each theme's accent", 9, "#4a4e57",
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
