#pragma once

// Theme kits: a look described by two seed colours and expanded by the palette engine (core/ThemePalette.h) into
// every token of the UI. The idea comes from Kryvex's token system: a kit names the design intent and the engine
// derives elevations, hairlines, hover/pressed shades and readable text, so a new look is two hex values instead of a
// hand-written token table. The kits that ship with LYNX are foods, matching the fruit-and-veg logos both FL Studio
// and LYNX SHARE use: one per hue, so the preset list reads like a menu.
//
// A kit carries its own identity next to those seeds, because the theme page and the About window draw it: the hue and
// taste words of its caption ("BANANA | YELLOW | SWEET"), the colour of its leaf or skin, and the workspace colour
// behind the panels - a deep surface tinted towards the fruit, so the food colour reaches the whole window instead of
// only the accent. With those, the editor draws the kit's badge and logo in vector (ui/FoodGlyphs.h), which is why the
// core stays JUCE-free: the kit is data, the drawing is UI.

#include <cstdint>
#include <string>
#include <vector>

#include "core/ThemePalette.h"

namespace lynx::themes
{
struct Kit
{
    std::string id;              // "banana" (stable: a preset can remember it)
    std::string name;            // "Banana" (what Options > Theme lists)
    std::string hue;             // "YELLOW"  (the caption's second word)
    std::string taste;           // "SWEET"   (the caption's third word)
    std::uint32_t accent = defaultAccent;      // the fruit's own colour: selection, steps, playhead, notes
    std::uint32_t leaf = 0xff6e9e3e;           // the leaf, skin or rind: the badge's second colour
    std::uint32_t background = defaultBackground; // the dark grays the panels are built from
    std::uint32_t workspace = 0;  // 0 = automatic (the palette's deepest surface)

    bool operator== (const Kit&) const = default;
};

// The food kits, in menu order (a hue sweep, red to violet, then the neutrals): Cherry, Watermelon, Mango, Banana,
// Kiwi, Lime, Avocado, Blueberry, Eggplant, Fig, Dragonfruit, Lychee, Coconut. Accents are the fruit's own colour;
// backgrounds are the dark grays tinted towards it, and every workspace is a deeper surface of the same family.
const std::vector<Kit>& foodKits();

// The kit as a theme (what Options > Theme applies and a .lynxtheme file holds).
Spec specOf (const Kit& kit);

// The food kits as themes, in kit order.
std::vector<Spec> kitThemes();

// The kit by id ("banana") or by name ("Banana", case-insensitive); nullptr when there is none.
const Kit* findKit (const std::string& idOrName);

// The kit a theme belongs to (same accent and background), or nullptr for LYNX's own look and hand-made colours.
const Kit* matchKit (const Spec& spec);

// The look of a theme as a kit: the matching kit, else a "Pepper" kit in the theme's own colours (LYNX's red by
// default), so the badge and the logo have something to draw for every theme.
Kit kitFor (const Spec& spec);

// "BANANA | YELLOW | SWEET": the caption the theme page and the docs show under a badge.
std::string caption (const Kit& kit);
} // namespace lynx::themes
