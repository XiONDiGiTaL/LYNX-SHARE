#pragma once

// Theme kits: a look described by two seed colours and expanded by the palette engine (core/ThemePalette.h) into
// every token of the UI. The idea comes from Kryvex's token system: a kit names the design intent and the engine
// derives elevations, hairlines, hover/pressed shades and readable text, so a new look is two hex values instead of a
// hand-written token table. The kits that ship with LYNX are foods, matching the fruit-and-veg logos both FL Studio
// and LYNX SHARE use: one per hue, so the preset list reads like a menu.

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
    std::uint32_t accent = defaultAccent;
    std::uint32_t background = defaultBackground;
    std::uint32_t workspace = 0; // 0 = automatic (the palette's deepest surface)

    bool operator== (const Kit&) const = default;
};

// The food kits, in menu order: Banana, Eggplant, Blueberry, Lime, Watermelon, Mango, Cherry, Avocado, Dragonfruit,
// Lychee. Accents are the fruit's own colour; backgrounds are tinted towards it, dark enough for the derived text.
const std::vector<Kit>& foodKits();

// The kit as a theme (what Options > Theme applies and a .lynxtheme file holds).
Spec specOf (const Kit& kit);

// The food kits as themes, in kit order.
std::vector<Spec> kitThemes();

// The kit by id ("banana") or by name ("Banana", case-insensitive); nullptr when there is none.
const Kit* findKit (const std::string& idOrName);
} // namespace lynx::themes
