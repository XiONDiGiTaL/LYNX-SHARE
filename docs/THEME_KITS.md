# Theme kits (food themes)

`core/ThemeKits.h` adds a small, explicit layer above the palette engine in `core/ThemePalette.h`.

A kit is a *seed*, not a token table: an accent, a background and a workspace, expanded by the existing engine into all
forty-odd tokens the UI draws with - elevations, hairlines, hover and pressed shades, tinted selections, note colours and
readable text. A kit is therefore a handful of hex values, and it cannot drift out of relation with the rest of the
interface. This is the same idea as Kryvex's token system: name the intent, derive the tokens, keep contrast measurable.

The kits that ship are foods, matching the fruit-and-vegetable logos FL Studio and LYNX SHARE both use. Each one is also
an identity the interface draws, because a name in a dropdown does not say "Banana":

| Kit | Id | Caption | Accent | Leaf | Background | Workspace |
| --- | --- | --- | --- | --- | --- | --- |
| Cherry | `cherry` | `CHERRY \| RED \| BRIGHT` | `#D92B45` | `#4E7A3A` | `#160F11` | `#0A0708` |
| Watermelon | `watermelon` | `WATERMELON \| PINK \| JUICY` | `#FF5C73` | `#3F8A4F` | `#101614` | `#070908` |
| Tangerine | `tangerine` | `TANGERINE \| ORANGE \| ZEST` | `#FF9E1F` | `#4F8A38` | `#1A1209` | `#0C0804` |
| Banana | `banana` | `BANANA \| YELLOW \| SWEET` | `#F2C21B` | `#7FA83A` | `#17150D` | `#0B0A06` |
| Kiwi | `kiwi` | `KIWI \| GREEN \| TART` | `#63C46B` | `#6E8F3F` | `#101511` | `#070A08` |
| Lime | `lime` | `LIME \| LIME \| SOUR` | `#A6D81C` | `#5E8C2A` | `#12150E` | `#080A06` |
| Avocado | `avocado` | `AVOCADO \| OLIVE \| CREAMY` | `#7FB53A` | `#4F7A26` | `#12140F` | `#080907` |
| Blueberry | `blueberry` | `BLUEBERRY \| BLUE \| BERRY` | `#4E6BE8` | `#5C8A4A` | `#0F1320` | `#080A11` |
| Eggplant | `eggplant` | `EGGPLANT \| PURPLE \| RICH` | `#8F5BD6` | `#5E8A3A` | `#141019` | `#09070C` |
| Fig | `fig` | `FIG \| INDIGO \| ANCIENT` | `#6D5AE8` | `#5E8A46` | `#121122` | `#08070F` |
| Dragonfruit | `dragonfruit` | `DRAGONFRUIT \| VIOLET \| EXOTIC` | `#FF4FA3` | `#4F9A5C` | `#170F18` | `#0A070B` |
| Lychee | `lychee` | `LYCHEE \| BLUSH \| FLORAL` | `#F2A0B5` | `#5B8C43` | `#161114` | `#0A0709` |
| Coconut | `coconut` | `COCONUT \| CREAM \| MELLOW` | `#E4C9A0` | `#8A6A45` | `#16110D` | `#0A0705` |

The order is a hue sweep (red, pink, orange, yellow, green, lime, olive, blue, purple, indigo, violet, blush) that ends
on the neutral, so the gallery in *Options > Theme* reads like a menu.

Each background is the app's dark gray tinted towards its fruit, and each workspace is a deeper surface of the same
family - so the fruit reaches the backdrop behind the panels, not only the accent. The leaf (or skin, or rind) is the
badge's second colour. Light accents (Banana, Kiwi, Lime, Coconut) get dark ink on the accent automatically through
`readableTextOn`.

## Where the kits appear

- **Options > Theme** opens with a hero strip: the logo of the theme in use, its name, its caption and its three seeds
  (accent, background, workspace).
- **The food gallery** under the theme presets draws every kit as the badge the themes were designed around: a dark disc,
  a ring glowing in the kit's accent, the fruit on it, the name and `HUE | TASTE` under it. Clicking one applies the
  kit - accent, background, workspace and name - like a preset. The dropdown above still lists the six built-in looks,
  the thirteen kits and your own `.lynxtheme` presets.
- **The logo** (`ui/FoodGlyphs.h`) is the same fruit on a rounded square of the kit's own dark surface, with the accent
  as the edge and a halo outside it - the badge's idea in a square, so the fruit keeps its colours (a yellow banana, a
  blue blueberry) instead of turning into a silhouette. It is the mark in the About window, next to the theme's name
  and caption, and the object the start-up splash drops. LYNX's own look and hand-made colours (Ocean, Emerald, a custom
  accent) show the pepper instead, in their own accent: `kitFor(spec)` always returns something to draw.

## The art

The fruit is the art on the reference sheet, `tools/reference/theme-fruits.png` - a 3 x 3 grid of the nine fruits, each on
a disc with a glowing ring and its `NAME | HUE | TASTE` caption. The badge only supplies that disc and ring, so
`tools/extract_fruit_marks.py` cuts the fruit out of it: it keys the disc's neutral grey and its soft shadow off the
pixels, keeps the fruit's own silhouette (closing the gaps and filling what it encloses, so the dark shading survives),
feathers the edge and writes one transparent PNG per id - `docs/img/marks/<id>.png`, 224 px, which is the size the
badges are drawn at on a retina screen. The same script regenerates `src/ui/FoodMarkData.h`, the byte arrays the binary
embeds, so the app carries the design and reads nothing from disk.

Cherry, Watermelon, Lime, Avocado and Lychee are not on the sheet: they still draw with `juce::Path` from a unit square
(0..1, y down) in `src/ui/FoodGlyphs.cpp`, shaded as volumes (a light from the upper left, a gloss, a cut face where one
belongs). A missing PNG is not an error - the vector mark of the same id is the fallback - and an id with neither draws
LYNX's pepper.

The art is used as drawn: the badge's ring glows in the kit's accent and the disc is the kit's background, but the
fruit itself is the sheet's, shading and all - the light from the upper left, the glosses, the cut faces. The vector
fallback follows the same rules for the fruits that have no art: each is a volume (`solid`: light high on the left, the
body colour through the middle, the far edge sinking into shadow) with a gloss where the light lands, a cut face with
pith and seeds where one belongs, the stone for the avocado, the three eyes and the white meat for the coconut. Those
parts are placed from the kit's colours - the accent is the fruit, the leaf is the stalk, the skin and the calyx - so a
hand-made theme still draws its own pepper in its own accent.

## Adding a kit

One entry in `foodKits()` in `src/core/ThemeKits.cpp`:

```cpp
{ "coffee", "Coffee", "BROWN", "BITTER", 0xffb07d48, 0xff6d4b2c, 0xff14100c, 0xff0a0705 },
```

`findKit ("coffee")` then resolves it by id or by name (case-insensitive), so a preset can remember the stable id while
the menu shows the label. The id also picks the badge and the logo mark: an id without a mark draws LYNX's pepper until
it has one, and `kitFor` keeps every theme drawable.

To give the new id a mark, either add its badge to the sheet table in `tools/extract_fruit_marks.py` and run it (it
writes `docs/img/marks/<id>.png` and the header), or add a function to `src/ui/FoodGlyphs.cpp` and a branch in
`markById` (the fruit shapes are the design: keep them in the unit square). `tools/make_theme_kits.py` picks up either,
as it embeds the PNG when one exists and calls the vector shape otherwise.

## Guarantees the tests hold

`tests/ThemeKitsTests.inc` checks every kit, not just the one being edited:

- text on panels stays at a contrast ratio of at least 7:1, and text on the accent at least 3:1 (WCAG);
- `accentHover` is lighter and `accentPressed` darker than the accent, so the interaction states stay ordered;
- the accent really is the theme's primary: `selectionFill` and the note colours are derived from it;
- the workspace is named (not "automatic"), darker than the background, and is what `workspaceColour` returns;
- ids, names, captions and accents are unique, and every caption is `NAME | HUE | TASTE` in upper case;
- the leaf is its own colour, never the accent again;
- `matchKit` finds a kit by its accent and background and nothing else, `kitFor` gives a pepper for anything else;
- the kits reach the theme picker in gallery order, with LYNX still first (the default look) and Coconut last.

## The picture in the README

`docs/img/theme-kits.svg` is generated, not drawn by hand:

```sh
python tools/make_theme_kits.py             # rewrites the SVG
python tools/make_theme_kits.py --png out.png   # a quick preview (gradients flattened)
```

The script holds the kit table and embeds the same mark PNGs `FoodMarkData.h` carries, so the sheet cannot drift from
the badges; for the kits without art it calls the same unit-space vector geometry as `FoodGlyphs.cpp`. Re-run
`tools/extract_fruit_marks.py` first whenever the art changes, then this script, so the header, the SVG and the theme
page stay in step.
