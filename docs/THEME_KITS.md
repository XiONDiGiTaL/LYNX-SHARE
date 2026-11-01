# Theme kits (food themes)

`core/ThemeKits.h` adds a small, explicit layer above the palette engine in `core/ThemePalette.h`.

A kit is a *seed*, not a token table: an accent, a background and (optionally) a workspace colour. The existing
engine turns those two colours into all forty-odd tokens the UI draws with - elevations, hairlines, hover and pressed
shades, tinted selections, note colours and readable text - so a kit is two hex values and cannot drift out of
relation with the rest of the interface. This is the same idea as Kryvex's token system: name the intent, derive the
tokens, keep contrast measurable.

The kits that ship are foods, matching the fruit-and-vegetable logos FL Studio and LYNX SHARE both use. They arrive in
*Options > Theme* behind the built-in looks (LYNX, Ocean, Emerald, Amber, Violet, Graphite), because
`themes::builtInThemes()` appends `kitThemes()`; nothing else in the theme page had to change.

| Kit | Id | Accent | Background |
| --- | --- | --- | --- |
| Banana | `banana` | `#F2C21B` | `#17150D` |
| Eggplant | `eggplant` | `#8F5BD6` | `#141019` |
| Blueberry | `blueberry` | `#4E6BE8` | `#0F1320` |
| Lime | `lime` | `#A6D81C` | `#12150E` |
| Watermelon | `watermelon` | `#FF5C73` | `#101614` |
| Mango | `mango` | `#FF9E1F` | `#1A1209` |
| Cherry | `cherry` | `#D92B45` | `#160F11` |
| Avocado | `avocado` | `#7FB53A` | `#12140F` |
| Dragonfruit | `dragonfruit` | `#FF4FA3` | `#170F18` |
| Lychee | `lychee` | `#F2A0B5` | `#161114` |

Each background is tinted towards its fruit, and the light accents (Lychee, Banana, Lime) get dark text on the accent
automatically through `readableTextOn`.

## Adding a kit

One entry in `foodKits()` in `src/core/ThemeKits.cpp`:

```cpp
{ "coffee", "Coffee", 0xffb07d48, 0xff14100c },
```

`findKit("coffee")` then resolves it by id or by name (case-insensitive), so a preset can remember the stable id while
the menu shows the label.

## Guarantees the tests hold

`tests/ThemeKitsTests.inc` checks every kit, not just the one being edited:

* text on panels stays at a contrast ratio of at least 7:1, and text on the accent at least 3:1 (WCAG);
* `accentHover` is lighter and `accentPressed` darker than the accent, so the interaction states stay ordered;
* ids and names are unique and ids are not labels;
* the kits really do reach the theme picker, with LYNX still first (the default look).
