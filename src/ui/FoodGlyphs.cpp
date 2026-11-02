#include "ui/FoodGlyphs.h"

#include "ui/FoodMarkData.h"

#include <cmath>
#include <map>

namespace lynx::food
{
namespace
{
using juce::Colour;
using juce::Graphics;
using juce::Line;
using juce::Path;
using juce::Point;
using juce::Rectangle;

Colour opaque (std::uint32_t argb)
{
    return Colour (argb | 0xff000000u);
}

// Every mark is lit from the upper left, like the badges it sits on: `lift` is the side facing the light, `sink` the
// side turning away from it, and the two are all the shading below needs.
Colour lift (Colour c, float amount) { return c.interpolatedWith (juce::Colours::white, amount); }
Colour sink (Colour c, float amount) { return c.interpolatedWith (juce::Colours::black, amount); }
Colour mix (Colour a, Colour b, float t) { return a.interpolatedWith (b, t); }

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

Rectangle<float> squareIn (Rectangle<float> area)
{
    const auto size = juce::jmin (area.getWidth(), area.getHeight());
    return { area.getCentreX() - size * 0.5f, area.getCentreY() - size * 0.5f, size, size };
}

// A pointed leaf: two quadratics between `base` and `tip`, bulging both ways (pixel space).
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

// --- shading ---------------------------------------------------------------------------------------------------

// The rounded volume every mark shares: the light high on the left, the body colour through the middle, the far edge
// sinking into shadow. The gradient runs in the shape's own bounds, so one call shades a 16 px icon and a badge alike.
void solid (Graphics& g, const Path& shape, Colour base, float depth = 1.0f)
{
    const auto b = shape.getBounds();
    if (b.isEmpty())
        return;
    juce::ColourGradient grad (lift (base, 0.24f * depth), b.getX() + b.getWidth() * 0.30f, b.getY() + b.getHeight() * 0.20f,
                               sink (base, 0.40f * depth), b.getRight(), b.getBottom(), true);
    grad.addColour (0.44, base);
    grad.addColour (0.80, sink (base, 0.20f * depth));
    g.setGradientFill (grad);
    g.fillPath (shape);
}

// The same for the round fruits, which are most of them.
void solidEllipse (Graphics& g, const Canvas& c, float x, float y, float w, float h, Colour base, float depth = 1.0f)
{
    Path shape;
    shape.addEllipse (c.e (x, y, w, h));
    solid (g, shape, base, depth);
}

// The highlight a smooth fruit has: white fading out, sitting where the light comes from.
void gloss (Graphics& g, const Canvas& c, float x, float y, float w, float h, float alpha = 0.55f)
{
    if (alpha <= 0.0f)
        return;
    const auto box = c.e (x, y, w, h);
    juce::ColourGradient grad (juce::Colours::white.withAlpha (alpha), box.getCentreX(), box.getCentreY(),
                               juce::Colours::white.withAlpha (0.0f), box.getRight(), box.getBottom(), true);
    grad.addColour (0.5, juce::Colours::white.withAlpha (alpha * 0.45f));
    g.setGradientFill (grad);
    g.fillEllipse (box);
}

// The shadow a fruit casts on the badge, so it sits on the disc instead of floating over it.
void contactShadow (Graphics& g, const Canvas& c, float x, float y, float w, float h, float alpha = 0.18f)
{
    const auto box = c.e (x, y, w, h);
    juce::ColourGradient grad (juce::Colours::black.withAlpha (alpha), box.getCentreX(), box.getCentreY(),
                               juce::Colours::black.withAlpha (0.0f), box.getRight(), box.getBottom(), true);
    g.setGradientFill (grad);
    g.fillEllipse (box);
}

// A leaf, sepal or stalk: the leaf colour, lighter along its ridge, with the ridge drawn in.
void paintLeafPx (Graphics& g, const Canvas& c, Point<float> from, Point<float> to, float width, Colour colour)
{
    const auto shape = leafShape (from, to, width * c.s());
    const auto b = shape.getBounds();
    juce::ColourGradient grad (lift (colour, 0.30f), b.getX(), b.getY(), sink (colour, 0.28f), b.getRight(), b.getBottom(), false);
    g.setGradientFill (grad);
    g.fillPath (shape);

    g.setColour (sink (colour, 0.34f).withAlpha (0.55f));
    Path rib;
    rib.startNewSubPath (from + (to - from) * 0.12f);
    rib.lineTo (from + (to - from) * 0.84f);
    g.strokePath (rib, c.stroke (0.010f));
}

void paintLeaf (Graphics& g, const Canvas& c, Point<float> base, Point<float> tip, float width, Colour colour)
{
    paintLeafPx (g, c, c.p (base.x, base.y), c.p (tip.x, tip.y), width, colour);
}

// A point of the mark after a turn about `about`, for the fruits that lean (the eggplant): pixel space.
Point<float> rotAbout (const Canvas& c, Point<float> p, Point<float> about, float angle)
{
    const auto o = c.p (about.x, about.y);
    const auto v = c.p (p.x, p.y) - o;
    return o + Point<float> (v.x * std::cos (angle) - v.y * std::sin (angle), v.x * std::sin (angle) + v.y * std::cos (angle));
}

Point<float> toUnit (const Canvas& c, Point<float> px)
{
    return { (px.x - c.area.getX()) / c.s(), (px.y - c.area.getY()) / c.s() };
}

// A stem: a single bend, drawn as a rounded stroke.
void stem (Graphics& g, const Canvas& c, Point<float> from, Point<float> control, Point<float> to, Colour colour, float width)
{
    Path path;
    path.startNewSubPath (c.p (from.x, from.y));
    path.quadraticTo (c.p (control.x, control.y), c.p (to.x, to.y));
    g.setColour (colour);
    g.strokePath (path, c.stroke (width));
    g.setColour (lift (colour, 0.30f).withAlpha (0.45f)); // the lit side of the stalk
    g.strokePath (path, c.stroke (width * 0.4f));
}

// A banana of a bunch: a thick tube along a bend, shaded across its width, round at both ends. Three of these stacked
// read as a hand of bananas at badge size, where a single thin crescent read as a bow.
void tube (Graphics& g, const Canvas& c, Point<float> from, Point<float> control, Point<float> to, Colour base, float width)
{
    Path path;
    path.startNewSubPath (c.p (from.x, from.y));
    path.quadraticTo (c.p (control.x, control.y), c.p (to.x, to.y));

    const auto box = path.getBounds();
    juce::ColourGradient grad (lift (base, 0.30f), box.getX(), box.getY(), sink (base, 0.34f), box.getX(), box.getBottom(), false);
    grad.addColour (0.55, base);
    g.setGradientFill (grad);
    g.strokePath (path, juce::PathStrokeType (width * c.s(), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// --- the fruits ------------------------------------------------------------------------------------------------

// LYNX's own mark: the pepper the logo and the splash have always used.
void pepperMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    contactShadow (g, c, 0.20f, 0.84f, 0.58f, 0.12f, 0.14f);

    // the pod: full at the shoulder, tapering to the tip
    Path body;
    body.startNewSubPath (c.p (0.47f, 0.23f));
    body.cubicTo (c.p (0.74f, 0.30f), c.p (0.74f, 0.64f), c.p (0.58f, 0.94f));
    body.cubicTo (c.p (0.48f, 0.72f), c.p (0.32f, 0.54f), c.p (0.30f, 0.27f));
    body.closeSubPath();
    solid (g, body, ink, 1.05f);

    Path shoulder; // the round shoulder that gives the pod its width
    shoulder.addEllipse (c.e (0.29f, 0.19f, 0.33f, 0.18f));
    solid (g, shoulder, ink, 0.9f);

    // the gloss down the lit side of the pod
    g.setColour (juce::Colours::white.withAlpha (0.36f));
    Path shine;
    shine.startNewSubPath (c.p (0.38f, 0.31f));
    shine.cubicTo (c.p (0.35f, 0.48f), c.p (0.39f, 0.68f), c.p (0.47f, 0.84f));
    g.strokePath (shine, c.stroke (0.040f));
    gloss (g, c, 0.33f, 0.25f, 0.18f, 0.13f, 0.42f);

    // the green cap and the stem curling away from it
    Path cap;
    cap.addEllipse (c.e (0.29f, 0.18f, 0.32f, 0.10f));
    solid (g, cap, leaf, 0.7f);
    gloss (g, c, 0.34f, 0.19f, 0.16f, 0.05f, 0.28f);
    Path stalk;
    stalk.startNewSubPath (c.p (0.45f, 0.20f));
    stalk.cubicTo (c.p (0.51f, 0.11f), c.p (0.62f, 0.09f), c.p (0.62f, 0.02f));
    g.setColour (leaf);
    g.strokePath (stalk, c.stroke (0.055f));
    g.setColour (lift (leaf, 0.32f).withAlpha (0.5f));
    g.strokePath (stalk, c.stroke (0.022f));
}

void bananaMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    contactShadow (g, c, 0.16f, 0.80f, 0.68f, 0.12f, 0.16f);
    const auto cream = lift (ink, 0.88f);

    // the green stalk the hand hangs from, laid down first so the fruit covers all but its end
    g.setColour (leaf);
    g.fillRoundedRectangle (c.e (0.79f, 0.17f, 0.17f, 0.30f), 0.070f * c.s());
    g.setColour (sink (leaf, 0.32f));
    g.fillRoundedRectangle (c.e (0.87f, 0.17f, 0.09f, 0.30f), 0.035f * c.s());

    // a hand of three, the front one lit: the bunch is cut at the right, so the tips point down and left
    const Point<float> tips[] = { { 0.26f, 0.62f }, { 0.32f, 0.74f }, { 0.40f, 0.86f } };
    tube (g, c, { 0.86f, 0.18f }, { 0.38f, 0.16f }, tips[0], sink (ink, 0.34f), 0.150f);
    tube (g, c, { 0.88f, 0.30f }, { 0.40f, 0.30f }, tips[1], sink (ink, 0.16f), 0.160f);
    tube (g, c, { 0.90f, 0.44f }, { 0.46f, 0.44f }, tips[2], ink, 0.170f);

    // the pale flesh at the two front tips, where the bananas were cut
    for (int i = 1; i < 3; ++i)
    {
        const auto tip = c.p (tips[i].x, tips[i].y);
        g.setColour (sink (ink, 0.50f));
        g.fillEllipse (Rectangle<float> (0.120f * c.s(), 0.120f * c.s()).withCentre (tip));
        g.setColour (cream);
        g.fillEllipse (Rectangle<float> (0.082f * c.s(), 0.082f * c.s()).withCentre (tip));
    }

    // the peel's ridges along the front fruit
    g.setColour (sink (ink, 0.30f).withAlpha (0.35f));
    Path ridge;
    ridge.startNewSubPath (c.p (0.80f, 0.43f));
    ridge.quadraticTo (c.p (0.46f, 0.44f), c.p (0.38f, 0.80f));
    g.strokePath (ridge, c.stroke (0.010f));
    gloss (g, c, 0.48f, 0.34f, 0.26f, 0.12f, 0.18f);
}

void kiwiMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    const auto skin = mix (ink, Colour (0xff7a5230), 0.78f); // the brown, fuzzy skin
    const auto centre = Point<float> { 0.41f, 0.59f };

    contactShadow (g, c, 0.10f, 0.80f, 0.80f, 0.12f, 0.14f);

    // the whole fruit behind, on the right: the skin and its fuzz
    solidEllipse (g, c, 0.55f, 0.11f, 0.45f, 0.51f, skin, 0.9f);
    g.setColour (sink (skin, 0.34f).withAlpha (0.30f));
    for (int i = 0; i < 18; ++i)
    {
        const auto a = 2.39996f * (float) i; // the golden angle: an even scatter, not a grid
        const auto d = 0.05f + 0.14f * std::fmod (0.11f * (float) i, 1.0f);
        g.fillEllipse (c.e (0.775f + d * std::cos (a) - 0.007f, 0.365f + d * std::sin (a) - 0.007f, 0.014f, 0.014f));
    }
    gloss (g, c, 0.60f, 0.17f, 0.18f, 0.14f, 0.30f);
    paintLeaf (g, c, { 0.66f, 0.20f }, { 0.94f, 0.03f }, 0.050f, leaf);

    // the cut half in front: skin, flesh, pale core
    solidEllipse (g, c, 0.06f, 0.24f, 0.70f, 0.70f, skin, 1.0f);
    solidEllipse (g, c, 0.11f, 0.29f, 0.60f, 0.60f, lift (ink, 0.12f), 0.55f);

    // the rays of the flesh running out from the core
    g.setColour (lift (ink, 0.46f).withAlpha (0.50f));
    for (int i = 0; i < 18; ++i)
    {
        const auto a = juce::MathConstants<float>::twoPi * (float) i / 18.0f;
        g.drawLine (Line<float> (c.p (centre.x + 0.055f * std::cos (a), centre.y + 0.055f * std::sin (a)),
                                 c.p (centre.x + 0.255f * std::cos (a), centre.y + 0.255f * std::sin (a))),
                    c.stroke (0.012f).getStrokeThickness());
    }

    // the seed ring, and the pale core inside it
    g.setColour (sink (ink, 0.90f));
    for (int i = 0; i < 14; ++i)
    {
        const auto a = juce::MathConstants<float>::twoPi * ((float) i + 0.35f) / 14.0f;
        g.fillEllipse (c.e (centre.x + 0.155f * std::cos (a) - 0.012f, centre.y + 0.155f * std::sin (a) - 0.010f, 0.024f, 0.020f));
    }
    solidEllipse (g, c, centre.x - 0.052f, centre.y - 0.046f, 0.104f, 0.092f, lift (ink, 0.82f), 0.4f);

    // the stalk where it was cut from the vine
    g.setColour (sink (skin, 0.30f));
    g.fillEllipse (c.e (0.30f, 0.19f, 0.10f, 0.07f));
    paintLeaf (g, c, { 0.34f, 0.22f }, { 0.06f, 0.03f }, 0.050f, leaf);
}

void blueberryMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    const auto deep = sink (ink, 0.32f);

    const auto berry = [&] (float x, float y, float d, Colour colour) {
        contactShadow (g, c, x + d * 0.02f, y + d * 0.66f, d * 0.94f, d * 0.30f, 0.16f);
        solidEllipse (g, c, x, y, d, d, colour, 1.0f);
        gloss (g, c, x + d * 0.14f, y + d * 0.08f, d * 0.44f, d * 0.30f, 0.50f);

        // the blossom end: a small dark well ringed by its five teeth
        const auto cx = x + d * 0.52f, cy = y + d * 0.44f;
        g.setColour (sink (colour, 0.62f));
        g.fillEllipse (c.e (cx - d * 0.070f, cy - d * 0.055f, d * 0.140f, d * 0.110f));
        g.setColour (sink (colour, 0.38f));
        for (int i = 0; i < 5; ++i)
        {
            const auto a = juce::MathConstants<float>::twoPi * (float) i / 5.0f - 1.2f;
            g.drawLine (Line<float> (c.p (cx + d * 0.050f * std::cos (a), cy + d * 0.045f * std::sin (a)),
                                     c.p (cx + d * 0.115f * std::cos (a), cy + d * 0.100f * std::sin (a))),
                        c.stroke (0.013f).getStrokeThickness());
        }
    };

    berry (0.05f, 0.45f, 0.36f, deep);
    berry (0.57f, 0.48f, 0.34f, sink (ink, 0.46f));
    berry (0.25f, 0.25f, 0.42f, ink);

    // the two leaves on the crown
    stem (g, c, { 0.50f, 0.33f }, { 0.50f, 0.27f }, { 0.52f, 0.21f }, leaf, 0.028f);
    paintLeaf (g, c, { 0.51f, 0.24f }, { 0.86f, 0.09f }, 0.100f, leaf);
    paintLeaf (g, c, { 0.48f, 0.24f }, { 0.14f, 0.07f }, 0.092f, leaf);
}

void eggplantMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    contactShadow (g, c, 0.16f, 0.84f, 0.62f, 0.12f, 0.14f);

    const auto about = Point<float> { 0.50f, 0.55f };
    constexpr float tilt = 0.26f; // the lean of the fruit in the reference sheet
    const auto pivot = c.p (about.x, about.y);

    Path body;
    body.startNewSubPath (c.p (0.50f, 0.18f));
    body.cubicTo (c.p (0.74f, 0.30f), c.p (0.78f, 0.72f), c.p (0.52f, 0.92f));
    body.cubicTo (c.p (0.28f, 0.80f), c.p (0.21f, 0.46f), c.p (0.39f, 0.26f));
    body.cubicTo (c.p (0.42f, 0.22f), c.p (0.46f, 0.19f), c.p (0.50f, 0.18f));
    body.closeSubPath();
    body.applyTransform (juce::AffineTransform::rotation (tilt, pivot.x, pivot.y));
    solid (g, body, ink, 1.10f);

    // the long gloss down the lit flank, and the small one near the shoulder
    Path shine;
    shine.startNewSubPath (c.p (0.38f, 0.34f));
    shine.cubicTo (c.p (0.33f, 0.48f), c.p (0.34f, 0.64f), c.p (0.40f, 0.76f));
    shine.applyTransform (juce::AffineTransform::rotation (tilt, pivot.x, pivot.y));
    g.setColour (juce::Colours::white.withAlpha (0.30f));
    g.strokePath (shine, c.stroke (0.052f));
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.strokePath (shine, c.stroke (0.100f));
    const auto gleam = toUnit (c, rotAbout (c, { 0.40f, 0.33f }, about, tilt));
    gloss (g, c, gleam.x - 0.07f, gleam.y - 0.05f, 0.14f, 0.10f, 0.34f);

    // the calyx: pointed sepals folding over the shoulders, and the stalk
    const auto sepal = [&] (Point<float> from, Point<float> to, float width) {
        paintLeafPx (g, c, rotAbout (c, from, about, tilt), rotAbout (c, to, about, tilt), width, leaf);
    };
    sepal ({ 0.46f, 0.26f }, { 0.25f, 0.17f }, 0.062f);
    sepal ({ 0.50f, 0.25f }, { 0.75f, 0.18f }, 0.062f);
    sepal ({ 0.45f, 0.27f }, { 0.33f, 0.38f }, 0.040f);
    sepal ({ 0.53f, 0.26f }, { 0.66f, 0.38f }, 0.040f);

    Path stalk;
    stalk.startNewSubPath (rotAbout (c, { 0.50f, 0.22f }, about, tilt));
    stalk.quadraticTo (rotAbout (c, { 0.55f, 0.13f }, about, tilt), rotAbout (c, { 0.59f, 0.06f }, about, tilt));
    g.setColour (leaf);
    g.strokePath (stalk, c.stroke (0.050f));
    g.setColour (lift (leaf, 0.30f).withAlpha (0.45f));
    g.strokePath (stalk, c.stroke (0.020f));

    const auto knot = toUnit (c, rotAbout (c, { 0.50f, 0.23f }, about, tilt));
    g.setColour (sink (leaf, 0.45f));
    g.fillEllipse (c.e (knot.x - 0.05f, knot.y - 0.035f, 0.10f, 0.07f));
}

void limeMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    contactShadow (g, c, 0.14f, 0.74f, 0.72f, 0.16f, 0.30f);

    // a lime cut across: the rind, the pale pith, then the flesh in segments
    solidEllipse (g, c, 0.13f, 0.18f, 0.72f, 0.68f, sink (ink, 0.40f), 1.0f);
    solidEllipse (g, c, 0.17f, 0.22f, 0.64f, 0.60f, lift (ink, 0.80f), 0.25f);
    solidEllipse (g, c, 0.21f, 0.26f, 0.56f, 0.52f, lift (ink, 0.22f), 0.35f);

    const auto centre = Point<float> { 0.49f, 0.52f };
    g.setColour (lift (ink, 0.72f).withAlpha (0.85f));
    for (int i = 0; i < 8; ++i)
    {
        const auto a = juce::MathConstants<float>::twoPi * (float) i / 8.0f + 0.18f;
        g.drawLine (Line<float> (c.p (centre.x + 0.045f * std::cos (a), centre.y + 0.045f * std::sin (a)),
                                 c.p (centre.x + 0.245f * std::cos (a), centre.y + 0.235f * std::sin (a))),
                    c.stroke (0.014f).getStrokeThickness());
    }
    solidEllipse (g, c, centre.x - 0.040f, centre.y - 0.036f, 0.080f, 0.072f, lift (ink, 0.62f), 0.3f);

    gloss (g, c, 0.20f, 0.24f, 0.22f, 0.14f, 0.30f);
    paintLeaf (g, c, { 0.62f, 0.22f }, { 0.90f, 0.06f }, 0.055f, leaf);
    stem (g, c, { 0.62f, 0.22f }, { 0.58f, 0.16f }, { 0.52f, 0.14f }, leaf, 0.036f);
}

void watermelonMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    contactShadow (g, c, 0.14f, 0.86f, 0.72f, 0.10f, 0.26f);

    // A cut face seen head-on, so the fruit sits in the middle of the badge instead of hanging off its centre: the
    // rind, the pale line under it, the flesh.
    const auto x = 0.07f, y = 0.08f, size = 0.86f;
    solidEllipse (g, c, x, y, size, size, sink (leaf, 0.18f), 1.05f);
    solidEllipse (g, c, x + 0.035f, y + 0.035f, size - 0.07f, size - 0.07f, lift (leaf, 0.80f), 0.25f);
    solidEllipse (g, c, x + 0.065f, y + 0.065f, size - 0.13f, size - 0.13f, ink, 0.75f);

    // the seeds: one ring near the rind, a few inside it, each turned to face the middle
    g.setColour (sink (ink, 0.72f));
    const auto centre = Point<float> { x + size * 0.5f, y + size * 0.5f };
    const auto seed = [&] (float angle, float dist, float w, float h)
    {
        const auto sx = centre.x + dist * std::cos (angle);
        const auto sy = centre.y + dist * std::sin (angle);
        const auto at = c.p (sx, sy);
        Path p;
        p.addEllipse (c.e (sx - w * 0.5f, sy - h * 0.5f, w, h));
        p.applyTransform (juce::AffineTransform::rotation (angle + juce::MathConstants<float>::halfPi, at.x, at.y));
        g.fillPath (p);
    };

    for (int i = 0; i < 8; ++i)
        seed (juce::MathConstants<float>::twoPi * (float) i / 8.0f + 0.35f, 0.285f, 0.040f, 0.058f);
    for (int i = 0; i < 5; ++i)
        seed (juce::MathConstants<float>::twoPi * (float) i / 5.0f + 0.95f, 0.150f, 0.032f, 0.048f);

    gloss (g, c, 0.21f, 0.22f, 0.34f, 0.18f, 0.24f);
}

// The art from tools/reference/theme-fruits.png, embedded by tools/extract_fruit_marks.py and decoded once per id.
// It is the design: the fruit keeps its own shading and colours, so the badge only supplies the disc and the ring. A
// kit without art (Cherry, Watermelon, Lime, Avocado, Lychee) still draws the vector mark below.
const juce::Image* artFor (const std::string& id)
{
    static std::map<std::string, juce::Image> decoded;
    auto it = decoded.find (id);
    if (it == decoded.end())
    {
        juce::Image image;
        for (const auto& mark : foodmarks::marks)
            if (id == mark.id)
            {
                image = juce::ImageFileFormat::loadFrom (mark.png, (size_t) mark.size);
                break;
            }
        it = decoded.emplace (id, image).first;
    }
    return it->second.isNull() ? nullptr : &it->second;
}

void cherryMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    contactShadow (g, c, 0.10f, 0.78f, 0.76f, 0.14f, 0.30f);

    // the two stems, joined at the top
    stem (g, c, { 0.50f, 0.12f }, { 0.34f, 0.28f }, { 0.30f, 0.56f }, leaf, 0.034f);
    stem (g, c, { 0.50f, 0.12f }, { 0.62f, 0.32f }, { 0.68f, 0.60f }, leaf, 0.034f);

    const auto cherry = [&] (float x, float y, float d, Colour colour) {
        contactShadow (g, c, x + d * 0.04f, y + d * 0.68f, d * 0.92f, d * 0.28f, 0.28f);
        solidEllipse (g, c, x, y, d, d, colour, 1.0f);
        gloss (g, c, x + d * 0.18f, y + d * 0.12f, d * 0.30f, d * 0.20f, 0.60f);
    };
    cherry (0.46f, 0.56f, 0.34f, sink (ink, 0.30f));
    cherry (0.10f, 0.52f, 0.38f, ink);

    // the little knot where the stems meet
    g.setColour (sink (leaf, 0.30f));
    g.fillEllipse (c.e (0.465f, 0.10f, 0.07f, 0.05f));
    paintLeaf (g, c, { 0.52f, 0.13f }, { 0.82f, 0.05f }, 0.055f, leaf);
}

void avocadoMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    contactShadow (g, c, 0.18f, 0.82f, 0.62f, 0.14f, 0.30f);

    // the half: dark skin, pale flesh, one stone
    Path skin;
    skin.startNewSubPath (c.p (0.50f, 0.09f));
    skin.cubicTo (c.p (0.67f, 0.09f), c.p (0.74f, 0.28f), c.p (0.76f, 0.46f));
    skin.cubicTo (c.p (0.79f, 0.68f), c.p (0.69f, 0.90f), c.p (0.50f, 0.92f));
    skin.cubicTo (c.p (0.31f, 0.90f), c.p (0.21f, 0.68f), c.p (0.24f, 0.46f));
    skin.cubicTo (c.p (0.26f, 0.28f), c.p (0.33f, 0.09f), c.p (0.50f, 0.09f));
    skin.closeSubPath();
    solid (g, skin, sink (leaf, 0.32f), 1.0f);

    Path flesh;
    flesh.startNewSubPath (c.p (0.50f, 0.16f));
    flesh.cubicTo (c.p (0.63f, 0.16f), c.p (0.68f, 0.31f), c.p (0.70f, 0.46f));
    flesh.cubicTo (c.p (0.72f, 0.64f), c.p (0.64f, 0.84f), c.p (0.50f, 0.86f));
    flesh.cubicTo (c.p (0.36f, 0.84f), c.p (0.28f, 0.64f), c.p (0.30f, 0.46f));
    flesh.cubicTo (c.p (0.32f, 0.31f), c.p (0.37f, 0.16f), c.p (0.50f, 0.16f));
    flesh.closeSubPath();
    solid (g, flesh, lift (ink, 0.28f), 0.55f);

    // the stone, sitting in the lower half
    solidEllipse (g, c, 0.38f, 0.53f, 0.25f, 0.27f, mix (ink, Colour (0xff8a5a2b), 0.78f), 0.9f);
    gloss (g, c, 0.42f, 0.57f, 0.09f, 0.07f, 0.45f);

    paintLeaf (g, c, { 0.66f, 0.22f }, { 0.92f, 0.05f }, 0.050f, leaf);
}

void dragonfruitMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    contactShadow (g, c, 0.16f, 0.80f, 0.66f, 0.12f, 0.14f);

    solidEllipse (g, c, 0.13f, 0.23f, 0.74f, 0.64f, ink, 1.0f);

    // the green scales curling off the skin
    const auto centre = Point<float> { 0.50f, 0.55f };
    for (int i = 0; i < 7; ++i)
    {
        const auto a = juce::MathConstants<float>::twoPi * (float) i / 7.0f - 1.9f;
        const auto base = Point<float> { centre.x + 0.22f * std::cos (a), centre.y + 0.19f * std::sin (a) };
        const auto tip = Point<float> { centre.x + 0.365f * std::cos (a), centre.y + 0.335f * std::sin (a) };
        paintLeaf (g, c, base, tip, 0.034f, leaf);
    }

    // the crown of leaves at the top
    paintLeaf (g, c, { 0.43f, 0.26f }, { 0.33f, 0.03f }, 0.042f, leaf);
    paintLeaf (g, c, { 0.51f, 0.25f }, { 0.58f, 0.02f }, 0.040f, leaf);
    paintLeaf (g, c, { 0.47f, 0.25f }, { 0.45f, 0.01f }, 0.036f, leaf);

    // the cut face at the front: white flesh, its speckle of seeds, the pink rim round it
    solidEllipse (g, c, 0.28f, 0.36f, 0.44f, 0.44f, lift (ink, 0.55f), 0.55f);
    solidEllipse (g, c, 0.31f, 0.39f, 0.38f, 0.38f, lift (ink, 0.90f), 0.15f);
    g.setColour (sink (ink, 0.55f));
    for (int i = 0; i < 21; ++i)
    {
        const auto a = 2.39996f * (float) i;
        const auto d = 0.012f + 0.140f * std::sqrt ((float) i / 21.0f);
        g.fillEllipse (c.e (0.50f + d * std::cos (a) - 0.007f, 0.58f + d * std::sin (a) - 0.007f, 0.014f, 0.014f));
    }
}

void lycheeMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    contactShadow (g, c, 0.12f, 0.78f, 0.72f, 0.16f, 0.30f);

    solidEllipse (g, c, 0.12f, 0.22f, 0.72f, 0.70f, ink, 1.0f);

    // the bumps: the fruit's skin is all of them, so they carry the shape
    for (int i = 0; i < 22; ++i)
    {
        const auto a = 2.39996f * (float) i;
        const auto d = 0.30f * std::sqrt (((float) i + 0.4f) / 22.0f);
        const auto x = 0.48f + d * std::cos (a), y = 0.57f + d * std::sin (a) * 0.92f;
        g.setColour (sink (ink, 0.26f));
        g.fillEllipse (c.e (x - 0.038f, y - 0.032f, 0.076f, 0.064f));
        g.setColour (lift (ink, 0.30f).withAlpha (0.55f));
        g.fillEllipse (c.e (x - 0.022f, y - 0.024f, 0.030f, 0.022f));
    }
    gloss (g, c, 0.20f, 0.28f, 0.24f, 0.16f, 0.40f);

    stem (g, c, { 0.54f, 0.26f }, { 0.54f, 0.18f }, { 0.48f, 0.13f }, leaf, 0.034f);
    paintLeaf (g, c, { 0.56f, 0.27f }, { 0.86f, 0.08f }, 0.050f, leaf);
}

void figMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    contactShadow (g, c, 0.16f, 0.84f, 0.68f, 0.12f, 0.14f);

    // a squat fig: as wide as it is tall, with a flat crown and a round base
    Path skin;
    skin.startNewSubPath (c.p (0.50f, 0.20f));
    skin.cubicTo (c.p (0.72f, 0.20f), c.p (0.82f, 0.40f), c.p (0.78f, 0.62f));
    skin.cubicTo (c.p (0.74f, 0.84f), c.p (0.62f, 0.92f), c.p (0.50f, 0.92f));
    skin.cubicTo (c.p (0.38f, 0.92f), c.p (0.26f, 0.84f), c.p (0.22f, 0.62f));
    skin.cubicTo (c.p (0.18f, 0.40f), c.p (0.28f, 0.20f), c.p (0.50f, 0.20f));
    skin.closeSubPath();
    solid (g, skin, ink, 1.05f);
    gloss (g, c, 0.23f, 0.33f, 0.14f, 0.20f, 0.30f);

    // cut open: the pale rim under the skin, the red interior and its seeds
    Path rim;
    rim.addEllipse (c.e (0.24f, 0.28f, 0.54f, 0.56f));
    solid (g, rim, lift (ink, 0.60f), 0.4f);

    const auto flesh = mix (ink, Colour (0xffc8305a), 0.72f);
    Path inside;
    inside.addEllipse (c.e (0.27f, 0.31f, 0.48f, 0.50f));
    solid (g, inside, flesh, 0.7f);

    g.setColour (lift (flesh, 0.55f).withAlpha (0.9f));
    for (int i = 0; i < 18; ++i)
    {
        const auto a = 2.39996f * (float) i;
        const auto d = 0.020f + 0.150f * std::sqrt (((float) i + 0.5f) / 18.0f);
        g.fillEllipse (c.e (0.51f + d * std::cos (a) - 0.008f, 0.56f + d * std::sin (a) - 0.007f, 0.016f, 0.014f));
    }

    stem (g, c, { 0.50f, 0.21f }, { 0.46f, 0.13f }, { 0.52f, 0.06f }, leaf, 0.046f);
    paintLeaf (g, c, { 0.52f, 0.14f }, { 0.82f, 0.05f }, 0.050f, leaf);
}

void coconutMark (Graphics& g, const Canvas& c, Colour ink, Colour leaf)
{
    const auto husk = mix (ink, Colour (0xff6b4a2a), 0.62f);
    contactShadow (g, c, 0.08f, 0.76f, 0.86f, 0.16f, 0.30f);

    // the whole nut: husk, fibres and the three eyes
    solidEllipse (g, c, 0.05f, 0.16f, 0.62f, 0.64f, husk, 1.0f);
    g.setColour (sink (husk, 0.30f).withAlpha (0.35f));
    for (int i = 0; i < 9; ++i)
    {
        const auto a = juce::MathConstants<float>::twoPi * (float) i / 9.0f;
        g.drawLine (Line<float> (c.p (0.36f + 0.10f * std::cos (a), 0.46f + 0.12f * std::sin (a)),
                                 c.p (0.36f + 0.26f * std::cos (a), 0.46f + 0.30f * std::sin (a))),
                    c.stroke (0.012f).getStrokeThickness());
    }
    g.setColour (sink (husk, 0.62f));
    for (int i = 0; i < 3; ++i)
        g.fillEllipse (c.e (0.26f + 0.075f * (float) i, 0.30f, 0.058f, 0.058f));
    gloss (g, c, 0.10f, 0.21f, 0.22f, 0.14f, 0.32f);

    // the half beside it, split open on its white meat
    solidEllipse (g, c, 0.46f, 0.44f, 0.50f, 0.50f, husk, 1.0f);
    solidEllipse (g, c, 0.505f, 0.485f, 0.41f, 0.41f, lift (ink, 0.62f), 0.35f);
    solidEllipse (g, c, 0.535f, 0.515f, 0.35f, 0.35f, lift (ink, 0.14f), 0.45f);
    gloss (g, c, 0.56f, 0.53f, 0.16f, 0.12f, 0.35f);

    paintLeaf (g, c, { 0.64f, 0.24f }, { 0.92f, 0.08f }, 0.055f, leaf);
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
} // namespace

void paintMark (Graphics& g, Rectangle<float> area, const themes::Kit& kit, Colour ink, Colour leaf)
{
    const auto square = squareIn (area);
    if (square.getWidth() <= 0.0f)
        return;

    if (const auto* art = artFor (kit.id))
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (*art, square, juce::RectanglePlacement::centred, false);
        return;
    }

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

    // the fruit nearly fills the disc, as on the reference sheet: the art carries its own margins
    paintMark (g, disc.reduced (radius * 0.06f), kit, accent, leaf);
}

void paintLogo (Graphics& g, Rectangle<float> area, const themes::Kit& kit)
{
    const auto square = squareIn (area);
    if (square.getWidth() <= 0.0f)
        return;

    const auto accent = opaque (kit.accent);
    const auto background = opaque (kit.background);
    const auto leaf = opaque (kit.leaf);
    const auto corner = square.getWidth() * 0.22f;

    Path body;
    body.addRoundedRectangle (square, corner);
    // The square is the kit's own dark surface, not its accent: the fruit keeps its colours on it (a banana in ink on a
    // yellow square was a black crescent), and the accent comes in as the glowing edge, as on the badge.
    g.setGradientFill (juce::ColourGradient (background.brighter (0.16f), square.getX(), square.getY(), background.darker (0.55f), square.getX(),
                                            square.getBottom(), false));
    g.fillPath (body);

    // a soft light from the top left, then the accent's edge and the halo outside it
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), square.getX(), square.getY(), juce::Colours::transparentWhite,
                                            square.getRight(), square.getBottom(), false));
    g.fillPath (body);
    g.setColour (accent.withAlpha (0.20f));
    g.drawRoundedRectangle (square.expanded (square.getWidth() * 0.035f), corner, juce::jmax (1.0f, square.getWidth() * 0.012f));
    g.setColour (accent.withAlpha (0.85f));
    g.strokePath (body, juce::PathStrokeType (juce::jmax (1.0f, square.getWidth() * 0.016f)));

    paintMark (g, square.reduced (square.getWidth() * 0.10f), kit, accent, leaf);
}
} // namespace lynx::food
