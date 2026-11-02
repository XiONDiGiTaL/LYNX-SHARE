#pragma once

#include <juce_graphics/juce_graphics.h>

#include "core/ThemeKits.h"

// The food themes drawn: every kit has a vector mark (a banana, a kiwi, an eggplant...), shown on the badge the theme
// page presents - a dark disc with a glowing ring in the kit's accent, as the food themes were designed - and inside
// LYNX SHARE's logo, a rounded square in the kit's colours. Nothing is loaded from disk: the marks are paths, so they
// take the theme's accent and leaf colours and stay crisp at every size. A theme that is not a kit shows the pepper,
// LYNX's own fruit.
namespace lynx::food
{
// The kit's fruit in `ink` (its body) and `leaf` (leaves, stems, skins), fitted into `area` (centred, square).
void paintMark (juce::Graphics&, juce::Rectangle<float> area, const themes::Kit&, juce::Colour ink, juce::Colour leaf);

// The kit's badge: a disc in the kit's background, a ring glowing in its accent, the fruit on it. `selected` brightens
// and thickens the ring. `area` is square; the disc fills it.
void paintBadge (juce::Graphics&, juce::Rectangle<float> area, const themes::Kit&, bool selected);

// LYNX SHARE's food logo: a rounded square in the kit's own dark surface, edged and haloed in its accent, with the
// fruit on it in the colours the badge uses. `area` is square.
void paintLogo (juce::Graphics&, juce::Rectangle<float> area, const themes::Kit&);
} // namespace lynx::food
