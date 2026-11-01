// --- food theme kits (core/ThemeKits.h): seed colours expanded by the palette engine ---

#include "core/ThemeKits.h"

TEST (themekits_food_kits_are_readable_and_unique)
{
    using namespace lynx::themes;
    const auto& kits = foodKits();
    CHECK (kits.size() >= 5 && kits.size() <= 12);

    std::set<std::string> ids, names;
    for (const auto& k : kits)
    {
        CHECK (! k.id.empty() && ! k.name.empty());
        CHECK (ids.insert (k.id).second);   // no duplicate ids
        CHECK (names.insert (k.name).second);
        CHECK (k.id != k.name);             // the id is the stable key, the name is the label

        // every kit keeps the relations the palette engine promises: readable panels and readable text on the accent
        const auto p = derivePalette (k.accent, k.background);
        CHECK (p.accent == (k.accent | 0xff000000u));
        CHECK (p.bg1 == (k.background | 0xff000000u));
        CHECK (contrastRatio (p.textPrimary, p.bg2) >= 7.0);
        CHECK (contrastRatio (p.onAccent, p.accent) >= 3.0);
        CHECK (relativeLuminance (p.accentHover) > relativeLuminance (p.accent));
        CHECK (relativeLuminance (p.accentPressed) < relativeLuminance (p.accent));
    }
}

TEST (themekits_lookup_and_spec)
{
    using namespace lynx::themes;
    const auto* banana = findKit ("banana");
    CHECK (banana != nullptr && banana->name == "Banana");
    CHECK (findKit ("Banana") == banana);   // by name
    CHECK (findKit ("BANANA") == banana);   // case-insensitive
    CHECK (findKit ("dragonfruit") != nullptr);
    CHECK (findKit ("") == nullptr && findKit ("banana split") == nullptr);

    if (banana != nullptr)
    {
        const auto spec = specOf (*banana);
        CHECK (spec.name == "Banana" && spec.accent == banana->accent && spec.background == banana->background);
        CHECK (! spec.isDefaultLook());
        CHECK (! spec.image.empty() == false); // a kit never carries a workspace image
    }

    // the kits reach Options > Theme behind the built-in looks, and LYNX stays the first (default) entry
    const auto all = builtInThemes();
    const auto kits = kitThemes();
    CHECK (all.size() >= 6 + kits.size());
    CHECK (all[0].accent == defaultAccent && all[0].background == defaultBackground);
    CHECK (kits.size() == foodKits().size());
    bool bananaListed = false;
    for (std::size_t i = 0; i < kits.size() && i < all.size(); ++i)
    {
        const auto& listed = all[all.size() - kits.size() + i];
        CHECK (listed.name == kits[i].name && listed.accent == kits[i].accent);
        bananaListed = bananaListed || listed.name == "Banana";
    }
    CHECK (bananaListed);
}
