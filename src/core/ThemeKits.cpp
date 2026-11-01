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
} // namespace

const std::vector<Kit>& foodKits()
{
    static const std::vector<Kit> all {
        { "banana", "Banana", 0xfff2c21b, 0xff17150d },
        { "eggplant", "Eggplant", 0xff8f5bd6, 0xff141019 },
        { "blueberry", "Blueberry", 0xff4e6be8, 0xff0f1320 },
        { "lime", "Lime", 0xffa6d81c, 0xff12150e },
        { "watermelon", "Watermelon", 0xffff5c73, 0xff101614 },
        { "mango", "Mango", 0xffff9e1f, 0xff1a1209 },
        { "cherry", "Cherry", 0xffd92b45, 0xff160f11 },
        { "avocado", "Avocado", 0xff7fb53a, 0xff12140f },
        { "dragonfruit", "Dragonfruit", 0xffff4fa3, 0xff170f18 },
        { "lychee", "Lychee", 0xfff2a0b5, 0xff161114 },
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
} // namespace lynx::themes
