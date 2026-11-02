#include "core/ThemeKits.h"

#include <algorithm>
#include <cctype>

namespace lynx::themes
{
namespace
{
std::string lowered (const std::string& text)
{
    std::string out;
    out.reserve (text.size());
    for (auto c : text)
        out += (char) std::tolower ((unsigned char) c);
    return out;
}

std::string uppered (const std::string& text)
{
    std::string out;
    out.reserve (text.size());
    for (auto c : text)
        out += (char) std::toupper ((unsigned char) c);
    return out;
}
} // namespace

const std::vector<Kit>& foodKits()
{
    // id, name, hue, taste, accent, leaf, background, workspace
    static const std::vector<Kit> all {
        { "cherry", "Cherry", "RED", "BRIGHT", 0xffd92b45, 0xff4e7a3a, 0xff160f11, 0xff0a0708 },
        { "watermelon", "Watermelon", "PINK", "JUICY", 0xffff5c73, 0xff3f8a4f, 0xff101614, 0xff070908 },
        { "mango", "Mango", "ORANGE", "TROPICAL", 0xffff9e1f, 0xff4f8a38, 0xff1a1209, 0xff0c0804 },
        { "banana", "Banana", "YELLOW", "SWEET", 0xfff2c21b, 0xff7fa83a, 0xff17150d, 0xff0b0a06 },
        { "kiwi", "Kiwi", "GREEN", "TART", 0xff63c46b, 0xff6e8f3f, 0xff101511, 0xff070a08 },
        { "lime", "Lime", "LIME", "SOUR", 0xffa6d81c, 0xff5e8c2a, 0xff12150e, 0xff080a06 },
        { "avocado", "Avocado", "OLIVE", "CREAMY", 0xff7fb53a, 0xff4f7a26, 0xff12140f, 0xff080907 },
        { "blueberry", "Blueberry", "BLUE", "BERRY", 0xff4e6be8, 0xff5c8a4a, 0xff0f1320, 0xff080a11 },
        { "eggplant", "Eggplant", "PURPLE", "RICH", 0xff8f5bd6, 0xff5e8a3a, 0xff141019, 0xff09070c },
        { "fig", "Fig", "INDIGO", "ANCIENT", 0xff6d5ae8, 0xff5e8a46, 0xff121122, 0xff08070f },
        { "dragonfruit", "Dragonfruit", "VIOLET", "EXOTIC", 0xffff4fa3, 0xff4f9a5c, 0xff170f18, 0xff0a070b },
        { "lychee", "Lychee", "BLUSH", "FLORAL", 0xfff2a0b5, 0xff5b8c43, 0xff161114, 0xff0a0709 },
        { "coconut", "Coconut", "CREAM", "MELLOW", 0xffe4c9a0, 0xff8a6a45, 0xff16110d, 0xff0a0705 },
    };
    return all;
}

Spec specOf (const Kit& kit)
{
    Spec s;
    s.name = kit.name;
    s.accent = kit.accent;
    s.background = kit.background;
    s.workspace = kit.workspace;
    return s;
}

std::vector<Spec> kitThemes()
{
    std::vector<Spec> out;
    out.reserve (foodKits().size());
    for (const auto& kit : foodKits())
        out.push_back (specOf (kit));
    return out;
}

const Kit* findKit (const std::string& idOrName)
{
    const auto wanted = lowered (idOrName);
    for (const auto& kit : foodKits())
        if (lowered (kit.id) == wanted || lowered (kit.name) == wanted)
            return &kit;
    return nullptr;
}

const Kit* matchKit (const Spec& spec)
{
    for (const auto& kit : foodKits())
        if (spec.accent == (kit.accent | 0xff000000u) && spec.background == (kit.background | 0xff000000u))
            return &kit;
    return nullptr;
}

Kit kitFor (const Spec& spec)
{
    if (const auto* kit = matchKit (spec))
        return *kit;

    // not a kit: the pepper, in the theme's own accent and background (LYNX's look keeps the red one)
    Kit pepper;
    pepper.id = "pepper";
    pepper.name = "Pepper";
    pepper.hue = "RED";
    pepper.taste = "SPICE";
    pepper.accent = spec.accent | 0xff000000u;
    pepper.leaf = 0xff6e9e3e;
    pepper.background = spec.background | 0xff000000u;
    pepper.workspace = spec.workspace;
    return pepper;
}

std::string caption (const Kit& kit)
{
    return uppered (kit.name) + " | " + kit.hue + " | " + kit.taste;
}
} // namespace lynx::themes
