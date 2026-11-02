#include "ui/FoodGlyphs.h"

#include <cmath>

namespace lynx::food
{
namespace
{
using juce::Colour;
using juce::Graphics;
using juce::Path;
using juce::Point;
using juce::Rectangle;

Colour opaque (std::uint32_t argb)
{
    return Colour (argb | 0xff000000u);
}

// A mark is drawn in a unit square (0..1 on both axes) and scaled to the area it goes in, so the same shape works for
// a 16 px icon and a 128 px badge.
struct Canvas
{
    Rectangle<float> area;

    Point<float> p (float x, float y) const { return { area.getX() + x * area.getWidth(), area.getY() + y * area.getHeight() }; }
    Rectangle<float> e (float x, float y, float w, float h) const
    {
        const auto topLeft = p (x, y);
        return { topLeft.x, topLeft.y, w * area.getWidth(), h * area.getHeight() };
    }
    float s() const { return area.getWidth(); }

    juce::PathStrokeType stroke (float width) const
    {
        return juce::PathStrokeType (juce::jmax (0.6f, s() * width), juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    }
};

// A pointed leaf: two quadratics between `base` and `tip`, bulging both ways.
Path leafShape (Point<float> base, Point<float> tip, float width)
{
    Path path;
    const auto along = tip - base;
    const auto length = juce::jmax (1.0f, along.getDistanceFromOrigin());
    const auto normal = Point<float> (-along.y, along.x) / length;
    const auto mid = base + along * 0.5f;
    path.startNewSubPath (base);
    path.quadraticTo (mid + normal * width, tip);
    path.quadraticTo (mid - normal * width * 0.7f, base);
    path.closeSubPath();
    return path;
}

// A stem: a single bend, drawn as a rounded stroke.
void stem (Graphics& g, const Canvas& c, Point<float> from, Point<float> control, Point<float> to, Colour colour, float width)
{
    Path path;
    path.startNewSubPath (from);
    path.quadraticTo (control, to);
    g.setColour (colour);
    g.strokePath (path, c.stroke (width));
}

// --- the fruits ------------------------------------------------------------------------------------------------

// LYNX's own mark: the pepper the logo and the splash have always used.
void pepperMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    Path body;
    body.startNewSubPath (c.p (0.46f, 0.24f));
    body.cubicTo (c.p (0.66f, 0.34f), c.p (0.68f, 0.66f), c.p (0.58f, 0.92f));
    body.cubicTo (c.p (0.52f, 0.72f), c.p (0.38f, 0.56f), c.p (0.34f, 0.30f));
    body.closeSubPath();
    g.setColour (ink);
    g.fillPath (body);
    g.fillEllipse (c.e (0.32f, 0.22f, 0.26f, 0.16f)); // the shoulder

    g.setColour (ink.interpolatedWith (juce::Colours::white, 0.32f));
    Path shine;
    shine.startNewSubPath (c.p (0.44f, 0.34f));
    shine.cubicTo (c.p (0.52f, 0.46f), c.p (0.54f, 0.62f), c.p (0.52f, 0.78f));
    g.strokePath (shine, c.stroke (0.05f));

    stem (g, c, c.p (0.44f, 0.24f), c.p (0.34f, 0.14f), c.p (0.46f, 0.06f), leaf, 0.07f);
    g.setColour (leaf);
    g.fillEllipse (c.e (0.30f, 0.20f, 0.26f, 0.08f)); // the green cap
}

void bananaMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    // a crescent between two arcs: the peel's outer curve and the inner one, joined at both ends
    const auto centre = c.p (0.50f, 0.27f);
    const auto outer = c.s() * 0.56f, inner = c.s() * 0.30f;
    constexpr float from = 0.78f, to = 2.36f;

    Path body;
    body.addCentredArc (centre.x, centre.y, outer, outer, 0.0f, from, to, true);
    body.addCentredArc (centre.x, centre.y, inner, inner, 0.0f, to, from, false);
    body.closeSubPath();
    g.setColour (ink);
    g.fillPath (body);

    // the peel's crease (a ring between the two arcs) and the ends
    g.setColour (ink.interpolatedWith (juce::Colours::black, 0.20f));
    Path crease;
    crease.addCentredArc (centre.x, centre.y, (outer + inner) * 0.5f, (outer + inner) * 0.5f, 0.0f, from + 0.12f, to - 0.12f, true);
    g.strokePath (crease, c.stroke (0.014f));

    const auto leftEnd = c.p (0.5f + 0.43f * std::cos (from), 0.27f + 0.43f * std::sin (from));
    const auto rightEnd = c.p (0.5f + 0.43f * std::cos (to), 0.27f + 0.43f * std::sin (to));
    g.setColour (ink.interpolatedWith (juce::Colours::black, 0.55f));
    g.fillEllipse (juce::Rectangle<float> (0.045f * c.s(), 0.045f * c.s()).withCentre (rightEnd));
    g.setColour (leaf);
    g.fillEllipse (juce::Rectangle<float> (0.065f * c.s(), 0.065f * c.s()).withCentre (leftEnd)); // the cut stalk
}

void kiwiMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    const auto skin = ink.interpolatedWith (Colour (0xff6b4a2a), 0.62f);

    g.setColour (skin);
    g.fillEllipse (c.e (0.09f, 0.16f, 0.76f, 0.70f)); // the whole fruit, skin showing round the cut
    g.setColour (ink.interpolatedWith (juce::Colours::white, 0.10f));
    g.fillEllipse (c.e (0.17f, 0.24f, 0.60f, 0.54f)); // the cut face
    g.setColour (ink.interpolatedWith (juce::Colours::white, 0.55f));
    g.fillEllipse (c.e (0.41f, 0.42f, 0.14f, 0.14f)); // the core

    g.setColour (ink.interpolatedWith (juce::Colours::black, 0.78f));
    for (int i = 0; i < 10; ++i)
    {
        const auto a = juce::MathConstants<float>::twoPi * (float) i / 10.0f;
        g.fillEllipse (c.e (0.47f + 0.17f * std::cos (a) - 0.017f, 0.49f + 0.16f * std::sin (a) - 0.017f, 0.034f, 0.034f));
    }

    // the cut stalk and a leaf
    g.setColour (leaf);
    g.fillEllipse (c.e (0.44f, 0.11f, 0.13f, 0.09f));
    g.fillPath (leafShape (c.p (0.51f, 0.14f), c.p (0.78f, 0.05f), c.s() * 0.075f));
}

void blueberryMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    const auto deep = ink.interpolatedWith (juce::Colours::black, 0.35f);
    const auto berry = [&] (float x, float y, float d, Colour colour) {
        g.setColour (colour);
        g.fillEllipse (c.e (x, y, d, d));
        g.setColour (colour.interpolatedWith (juce::Colours::white, 0.35f));
        g.fillEllipse (c.e (x + d * 0.20f, y + d * 0.15f, d * 0.28f, d * 0.20f));
        g.setColour (colour.interpolatedWith (juce::Colours::black, 0.6f));
        g.fillEllipse (c.e (x + d * 0.40f, y + d * 0.38f, d * 0.16f, d * 0.14f)); // the calyx
    };
    berry (0.08f, 0.42f, 0.38f, deep);
    berry (0.54f, 0.44f, 0.36f, deep);
    berry (0.28f, 0.28f, 0.40f, ink);

    g.setColour (leaf);
    g.fillPath (leafShape (c.p (0.50f, 0.34f), c.p (0.76f, 0.12f), c.s() * 0.05f));
    g.fillPath (leafShape (c.p (0.50f, 0.34f), c.p (0.28f, 0.12f), c.s() * 0.045f));
}

void eggplantMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    Path body;
    body.startNewSubPath (c.p (0.50f, 0.20f));
    body.cubicTo (c.p (0.70f, 0.32f), c.p (0.74f, 0.74f), c.p (0.50f, 0.94f));
    body.cubicTo (c.p (0.26f, 0.74f), c.p (0.30f, 0.32f), c.p (0.50f, 0.20f));
    body.closeSubPath();
    g.setColour (ink);
    g.fillPath (body);

    g.setColour (ink.interpolatedWith (juce::Colours::white, 0.38f));
    Path shine;
    shine.startNewSubPath (c.p (0.38f, 0.42f));
    shine.cubicTo (c.p (0.32f, 0.56f), c.p (0.35f, 0.72f), c.p (0.42f, 0.82f));
    g.strokePath (shine, c.stroke (0.05f));

    g.setColour (leaf);
    g.fillPath (leafShape (c.p (0.50f, 0.30f), c.p (0.24f, 0.16f), c.s() * 0.085f)); // the sepals
    g.fillPath (leafShape (c.p (0.50f, 0.30f), c.p (0.76f, 0.16f), c.s() * 0.085f));
    g.fillPath (leafShape (c.p (0.50f, 0.30f), c.p (0.50f, 0.10f), c.s() * 0.075f));
    stem (g, c, c.p (0.50f, 0.22f), c.p (0.55f, 0.12f), c.p (0.58f, 0.05f), leaf, 0.055f);
}

void limeMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    g.setColour (ink);
    g.fillEllipse (c.e (0.14f, 0.22f, 0.66f, 0.60f));

    g.setColour (ink.interpolatedWith (juce::Colours::white, 0.34f));
    g.fillEllipse (c.e (0.34f, 0.40f, 0.26f, 0.24f)); // the cut face
    g.setColour (ink.interpolatedWith (juce::Colours::white, 0.10f));
    for (int i = 0; i < 6; ++i)
    {
        const auto a = juce::MathConstants<float>::twoPi * (float) i / 6.0f;
        g.drawLine (c.p (0.47f, 0.52f).x, c.p (0.47f, 0.52f).y, c.p (0.47f + 0.10f * std::cos (a), 0.52f + 0.10f * std::sin (a)).x,
                    c.p (0.47f + 0.10f * std::cos (a), 0.52f + 0.10f * std::sin (a)).y, juce::jmax (1.0f, c.s() * 0.015f));
    }

    g.setColour (ink.interpolatedWith (juce::Colours::white, 0.45f));
    g.fillEllipse (c.e (0.24f, 0.30f, 0.20f, 0.10f)); // the shine
    g.setColour (leaf);
    g.fillPath (leafShape (c.p (0.62f, 0.24f), c.p (0.86f, 0.10f), c.s() * 0.055f));
    stem (g, c, c.p (0.62f, 0.24f), c.p (0.58f, 0.18f), c.p (0.52f, 0.16f), leaf, 0.04f);
}

void watermelonMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    const auto centre = c.p (0.50f, 0.46f);
    const auto r = c.s() * 0.40f;

    Path rind;
    rind.addPieSegment (centre.x - r * 1.10f, centre.y - r * 1.10f, r * 2.2f, r * 2.2f, 0.0f, juce::MathConstants<float>::pi, 0.0f);
    g.setColour (leaf);
    g.fillPath (rind);

    Path flesh;
    flesh.addPieSegment (centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 0.0f, juce::MathConstants<float>::pi, 0.0f);
    g.setColour (ink);
    g.fillPath (flesh);

    g.setColour (ink.interpolatedWith (juce::Colours::black, 0.62f));
    for (int i = 0; i < 5; ++i)
    {
        const auto a = juce::MathConstants<float>::pi * (0.18f + 0.16f * (float) i);
        const auto d = 0.15f + 0.09f * ((i % 2) == 0 ? 1.0f : 0.0f);
        g.fillEllipse (c.e (0.50f + d * std::cos (a) - 0.022f, 0.50f + d * std::sin (a) - 0.030f, 0.044f, 0.060f));
    }
}

void mangoMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    Path body;
    body.addEllipse (c.e (0.11f, 0.27f, 0.74f, 0.50f));
    const auto centre = c.p (0.50f, 0.50f);
    body.applyTransform (juce::AffineTransform::rotation (-0.42f, centre.x, centre.y));
    g.setColour (ink);
    g.fillPath (body);

    // a lighter top-left cheek: the mango's own gradient, not a spot
    g.setColour (ink.interpolatedWith (juce::Colours::white, 0.18f));
    g.fillEllipse (c.e (0.22f, 0.31f, 0.34f, 0.20f));

    g.setColour (leaf);
    g.fillPath (leafShape (c.p (0.58f, 0.26f), c.p (0.80f, 0.08f), c.s() * 0.055f));
    stem (g, c, c.p (0.58f, 0.26f), c.p (0.56f, 0.18f), c.p (0.50f, 0.14f), leaf, 0.04f);
}

void cherryMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    g.setColour (leaf);
    Path stems;
    stems.startNewSubPath (c.p (0.50f, 0.16f));
    stems.cubicTo (c.p (0.36f, 0.30f), c.p (0.30f, 0.40f), c.p (0.30f, 0.52f));
    stems.startNewSubPath (c.p (0.50f, 0.16f));
    stems.cubicTo (c.p (0.60f, 0.32f), c.p (0.68f, 0.42f), c.p (0.68f, 0.54f));
    g.strokePath (stems, c.stroke (0.042f));

    const auto cherry = [&] (float x, float y) {
        g.setColour (ink);
        g.fillEllipse (c.e (x, y, 0.34f, 0.34f));
        g.setColour (ink.interpolatedWith (juce::Colours::white, 0.40f));
        g.fillEllipse (c.e (x + 0.06f, y + 0.06f, 0.10f, 0.08f));
    };
    cherry (0.10f, 0.54f);
    cherry (0.54f, 0.56f);

    g.setColour (leaf);
    g.fillPath (leafShape (c.p (0.52f, 0.18f), c.p (0.80f, 0.08f), c.s() * 0.06f));
}

void avocadoMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    Path body;
    body.addEllipse (c.e (0.20f, 0.38f, 0.60f, 0.52f));
    body.addEllipse (c.e (0.32f, 0.14f, 0.36f, 0.44f));
    g.setColour (ink);
    g.fillPath (body);

    Path flesh;
    flesh.addEllipse (c.e (0.25f, 0.42f, 0.50f, 0.42f));
    flesh.addEllipse (c.e (0.35f, 0.19f, 0.30f, 0.36f));
    g.setColour (ink.interpolatedWith (juce::Colours::white, 0.28f));
    g.fillPath (flesh);

    g.setColour (ink.interpolatedWith (Colour (0xff8a5a2b), 0.80f));
    g.fillEllipse (c.e (0.39f, 0.56f, 0.24f, 0.22f)); // the stone

    g.setColour (leaf);
    g.fillPath (leafShape (c.p (0.66f, 0.24f), c.p (0.90f, 0.06f), c.s() * 0.055f));
}

void dragonfruitMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    g.setColour (leaf);
    for (int i = 0; i < 10; ++i)
    {
        const auto a = juce::MathConstants<float>::twoPi * (float) i / 10.0f;
        const auto x = 0.50f + 0.29f * std::cos (a);
        const auto y = 0.50f + 0.28f * std::sin (a);
        const auto nx = std::cos (a), ny = std::sin (a);
        Path scale;
        scale.startNewSubPath (c.p (x - 0.055f * ny, y + 0.055f * nx));
        scale.lineTo (c.p (x + 0.115f * nx, y + 0.115f * ny));
        scale.lineTo (c.p (x + 0.055f * ny, y - 0.055f * nx));
        scale.closeSubPath();
        g.fillPath (scale);
    }

    g.setColour (ink);
    g.fillEllipse (c.e (0.20f, 0.26f, 0.60f, 0.52f));
    g.setColour (ink.interpolatedWith (juce::Colours::white, 0.72f));
    g.fillEllipse (c.e (0.31f, 0.35f, 0.38f, 0.34f)); // the white flesh

    g.setColour (ink.interpolatedWith (juce::Colours::black, 0.55f));
    for (int i = 0; i < 12; ++i)
    {
        const auto a = juce::MathConstants<float>::twoPi * (float) i / 12.0f;
        const auto d = (i % 2) == 0 ? 0.12f : 0.075f;
        g.fillEllipse (c.e (0.50f + d * std::cos (a) - 0.014f, 0.52f + d * std::sin (a) - 0.014f, 0.028f, 0.028f));
    }
}

void lycheeMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    g.setColour (ink);
    g.fillEllipse (c.e (0.14f, 0.26f, 0.66f, 0.60f));

    g.setColour (ink.interpolatedWith (juce::Colours::black, 0.32f));
    for (int i = 0; i < 12; ++i)
    {
        const auto a = juce::MathConstants<float>::twoPi * (float) i / 12.0f;
        g.fillEllipse (c.e (0.47f + 0.29f * std::cos (a) - 0.025f, 0.56f + 0.27f * std::sin (a) - 0.025f, 0.050f, 0.050f));
    }
    g.setColour (ink.interpolatedWith (juce::Colours::white, 0.45f));
    g.fillEllipse (c.e (0.30f, 0.36f, 0.22f, 0.14f)); // the shine

    g.setColour (leaf);
    g.fillPath (leafShape (c.p (0.56f, 0.28f), c.p (0.84f, 0.10f), c.s() * 0.055f));
    stem (g, c, c.p (0.56f, 0.28f), c.p (0.54f, 0.20f), c.p (0.48f, 0.16f), leaf, 0.042f);
}

void figMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    Path body;
    body.startNewSubPath (c.p (0.50f, 0.22f));
    body.cubicTo (c.p (0.84f, 0.34f), c.p (0.88f, 0.80f), c.p (0.50f, 0.94f));
    body.cubicTo (c.p (0.12f, 0.80f), c.p (0.16f, 0.34f), c.p (0.50f, 0.22f));
    body.closeSubPath();
    g.setColour (ink);
    g.fillPath (body);

    g.setColour (ink.interpolatedWith (juce::Colours::white, 0.62f));
    g.fillEllipse (c.e (0.33f, 0.38f, 0.34f, 0.40f)); // the cut: pale flesh
    g.setColour (ink.interpolatedWith (juce::Colours::black, 0.55f));
    for (int i = 0; i < 9; ++i)
    {
        const auto a = juce::MathConstants<float>::twoPi * (float) i / 9.0f;
        g.fillEllipse (c.e (0.50f + 0.10f * std::cos (a) - 0.013f, 0.58f + 0.13f * std::sin (a) - 0.013f, 0.026f, 0.026f));
    }

    stem (g, c, c.p (0.50f, 0.24f), c.p (0.46f, 0.14f), c.p (0.52f, 0.06f), leaf, 0.055f);
    g.setColour (leaf);
    g.fillPath (leafShape (c.p (0.52f, 0.14f), c.p (0.76f, 0.06f), c.s() * 0.055f));
}

void coconutMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    // a whole coconut: the hairy shell with its three eyes, the way a market coconut is drawn
    const auto husk = ink.interpolatedWith (Colour (0xff6b4a2a), 0.55f);
    g.setColour (husk);
    g.fillEllipse (c.e (0.10f, 0.20f, 0.78f, 0.74f));
    g.setColour (husk.interpolatedWith (juce::Colours::white, 0.18f));
    g.fillEllipse (c.e (0.20f, 0.30f, 0.34f, 0.26f)); // the shine
    g.setColour (husk.interpolatedWith (juce::Colours::black, 0.60f));
    for (int i = 0; i < 3; ++i)
        g.fillEllipse (c.e (0.31f + 0.11f * (float) i, 0.36f, 0.075f, 0.075f)); // the eyes

    g.setColour (leaf);
    g.fillPath (leafShape (c.p (0.78f, 0.24f), c.p (0.96f, 0.10f), c.s() * 0.06f));
}

void markById (const std::string& id, Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    if (id == "banana")
        bananaMark (g, c, ink, leaf);
    else if (id == "kiwi")
        kiwiMark (g, c, ink, leaf);
    else if (id == "blueberry")
        blueberryMark (g, c, ink, leaf);
    else if (id == "eggplant")
        eggplantMark (g, c, ink, leaf);
    else if (id == "lime")
        limeMark (g, c, ink, leaf);
    else if (id == "watermelon")
        watermelonMark (g, c, ink, leaf);
    else if (id == "mango")
        mangoMark (g, c, ink, leaf);
    else if (id == "cherry")
        cherryMark (g, c, ink, leaf);
    else if (id == "avocado")
        avocadoMark (g, c, ink, leaf);
    else if (id == "dragonfruit")
        dragonfruitMark (g, c, ink, leaf);
    else if (id == "lychee")
        lycheeMark (g, c, ink, leaf);
    else if (id == "fig")
        figMark (g, c, ink, leaf);
    else if (id == "coconut")
        coconutMark (g, c, ink, leaf);
    else
        pepperMark (g, c, ink, leaf);
}

Rectangle<float> squareIn (Rectangle<float> area)
{
    const auto size = juce::jmin (area.getWidth(), area.getHeight());
    return { area.getCentreX() - size * 0.5f, area.getCentreY() - size * 0.5f, size, size };
}
} // namespace

void paintMark (Graphics& g, Rectangle<float> area, const themes::Kit& kit, Colour ink, Colour leaf)
{
    const auto square = squareIn (area);
    if (square.getWidth() <= 0.0f)
        return;
    markById (kit.id, g, Canvas { square }, ink, leaf);
}

void paintBadge (Graphics& g, Rectangle<float> area, const themes::Kit& kit, bool selected)
{
    auto disc = squareIn (area);
    if (disc.getWidth() <= 0.0f)
        return;

    const auto background = opaque (kit.background);
    const auto accent = opaque (kit.accent);
    const auto leaf = opaque (kit.leaf);
    const auto radius = disc.getWidth() * 0.5f;

    // the disc: the kit's background, lighter at the top like the panels it comes from
    g.setGradientFill (juce::ColourGradient (background.brighter (0.12f), disc.getCentreX(), disc.getY(), background.darker (0.45f), disc.getCentreX(),
                                            disc.getBottom(), false));
    g.fillEllipse (disc);

    // the ring: a soft halo outside it, the line in the kit's accent, a hairline inside
    g.setColour (accent.withAlpha (selected ? 0.10f : 0.05f));
    g.drawEllipse (disc.expanded (radius * 0.09f), radius * 0.07f);
    g.setColour (accent.withAlpha (selected ? 0.20f : 0.10f));
    g.drawEllipse (disc.expanded (radius * 0.035f), radius * 0.03f);
    g.setColour (accent.withAlpha (selected ? 0.95f : 0.62f));
    g.drawEllipse (disc.reduced (1.0f), selected ? juce::jmax (2.0f, radius * 0.045f) : juce::jmax (1.2f, radius * 0.022f));

    paintMark (g, disc.reduced (radius * 0.34f), kit, accent, leaf);
}

void paintLogo (Graphics& g, Rectangle<float> area, const themes::Kit& kit)
{
    const auto square = squareIn (area);
    if (square.getWidth() <= 0.0f)
        return;

    const auto accent = opaque (kit.accent);
    const auto ink = Colour (themes::readableTextOn (kit.accent));
    const auto corner = square.getWidth() * 0.22f;

    Path body;
    body.addRoundedRectangle (square, corner);
    g.setGradientFill (juce::ColourGradient (accent.brighter (0.14f), square.getX(), square.getY(), accent.darker (0.26f), square.getX(),
                                            square.getBottom(), false));
    g.fillPath (body);

    // a soft light from the top left, and the edge
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.18f), square.getX(), square.getY(), juce::Colours::transparentWhite,
                                            square.getRight(), square.getBottom(), false));
    g.fillPath (body);
    g.setColour (accent.darker (0.42f));
    g.strokePath (body, juce::PathStrokeType (juce::jmax (1.0f, square.getWidth() * 0.014f)));

    paintMark (g, square.reduced (square.getWidth() * 0.20f), kit, ink, ink.withAlpha (0.62f));
}
} // namespace lynx::food
