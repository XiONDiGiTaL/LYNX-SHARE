// --- food theme kits (core/ThemeKits.h): seed colours expanded by the palette engine ---

#include "core/ThemeKits.h"

TEST (themekits_food_kits_are_readable_and_unique)
{
    using namespace lynx::themes;
    const auto& kits = foodKits();
    CHECK (kits.size() >= 12 && kits.size() <= 16);

    std::set<std::string> ids, names, captions;
    for (const auto& k : kits)
    {
        CHECK (! k.id.empty() && ! k.name.empty() && ! k.hue.empty() && ! k.taste.empty());
        CHECK (ids.insert (k.id).second);   // no duplicate ids
        CHECK (names.insert (k.name).second);
        CHECK (captions.insert (caption (k)).second);
        CHECK (k.id != k.name);             // the id is the stable key, the name is the label
        CHECK (k.hue.find ('|') == std::string::npos && k.taste.find ('|') == std::string::npos);

        // the caption is what the theme page and the docs show under the badge: NAME | HUE | TASTE (upper case)
        std::string upper;
        for (const auto c : k.name)
            upper += (char) std::toupper ((unsigned char) c);
        CHECK (caption (k) == upper + " | " + k.hue + " | " + k.taste);
        for (const auto c : caption (k))
            CHECK (! std::islower ((unsigned char) c));

        // every kit keeps the relations the palette engine promises: readable panels and readable text on the accent
        const auto p = derivePalette (k.accent, k.background);
        CHECK (p.accent == (k.accent | 0xff000000u));
        CHECK (p.bg1 == (k.background | 0xff000000u));
        CHECK (contrastRatio (p.textPrimary, p.bg2) >= 7.0);
        CHECK (contrastRatio (p.onAccent, p.accent) >= 3.0);
        CHECK (relativeLuminance (p.accentHover) > relativeLuminance (p.accent));
        CHECK (relativeLuminance (p.accentPressed) < relativeLuminance (p.accent));

        // the fruit's colour is the theme's primary: selections, steps, the playhead and notes all come from it
        CHECK (p.selectionFill == ((k.accent & 0x00ffffffu) | 0x2e000000u));
        CHECK (p.stepNote != p.accent && p.noteHard != p.accent);

        // the leaf is a colour of its own (the badge's second colour), not the accent again
        CHECK (k.leaf != k.accent);
        CHECK ((k.leaf & 0xff000000u) == 0xff000000u && (k.accent & 0xff000000u) == 0xff000000u);
    }

    // a hue sweep: the kits really do cover the fruit colours instead of repeating one
    std::set<std::uint32_t> accents;
    for (const auto& k : kits)
        accents.insert (k.accent & 0x00ffffffu);
    CHECK (accents.size() == kits.size());

    // Kiwi is in, and its green is its own (not Lime's or Avocado's)
    const auto* kiwi = findKit ("kiwi");
    CHECK (kiwi != nullptr && kiwi->hue == "GREEN");
    CHECK (kiwi != nullptr && kiwi->accent != 0xffa6d81c && kiwi->accent != 0xff7fb53a);
}

TEST (themekits_workspace_is_the_fruit_in_its_deepest_surface)
{
    using namespace lynx::themes;
    for (const auto& k : foodKits())
    {
        // every kit names its workspace (not "automatic"), a darker surface of the same family than the background
        CHECK (k.workspace != 0);
        CHECK (relativeLuminance (k.workspace | 0xff000000u) < relativeLuminance (k.background | 0xff000000u));
        CHECK (relativeLuminance (k.workspace | 0xff000000u) < 0.05);

        const auto spec = specOf (k);
        CHECK (spec.workspace == k.workspace);
        CHECK (workspaceColour (spec) == (k.workspace | 0xff000000u));
        CHECK (spec.accent == k.accent && spec.background == k.background && spec.name == k.name);
        CHECK (! spec.isDefaultLook());
        CHECK (spec.image.empty() && spec.startupSound.empty()); // a kit never carries a workspace image or a sound
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

        // a theme is matched back to its kit, and only to that kit
        CHECK (matchKit (spec) == banana && kitFor (spec).id == banana->id);

        auto other = spec;
        other.background = 0xff000000;
        CHECK (matchKit (other) == nullptr);
    }

    // LYNX's own look is not a kit: it draws the pepper, in its own red
    CHECK (matchKit (Spec {}) == nullptr);
    const auto pepper = kitFor (Spec {});
    CHECK (pepper.id == "pepper" && pepper.name == "Pepper");
    CHECK (pepper.accent == defaultAccent && pepper.background == defaultBackground);
    CHECK (caption (pepper) == "PEPPER | RED | SPICE");

    // a hand-made theme keeps its own colours on the pepper
    auto custom = Spec {};
    custom.accent = 0xff3d8bfd;
    custom.background = 0xff12161d;
    custom.workspace = 0xff0b0f15;
    custom.name = "Ocean";
    CHECK (matchKit (custom) == nullptr);
    CHECK (kitFor (custom).accent == custom.accent && kitFor (custom).background == custom.background);
    CHECK (kitFor (custom).workspace == custom.workspace);

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
        CHECK (listed.name == kits[i].name && listed.accent == kits[i].accent && listed.workspace == kits[i].workspace);
        bananaListed = bananaListed || listed.name == "Banana";
    }
    CHECK (bananaListed);

    // in the order the theme page's gallery shows them: a hue sweep, red to violet, then the neutral
    CHECK (all.size() > kits.size() && all.back().name == "Coconut");
    CHECK (foodKits().front().name == "Cherry");
}
