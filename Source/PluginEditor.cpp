#include "PluginEditor.h"
#include <cmath>

namespace logo
{
    // Logo "poussière" : le mot se désintègre en particules vers la droite
    inline void drawDustLogo (juce::Graphics& g, juce::Rectangle<float> area, float fontSize)
    {
        using namespace juce;
        const Colour blue (0xff2f8dff), sky (0xff9fd2ff), lilac (0xffa58cff);
        const float k0 = fontSize / 64.f;

        GlyphArrangement ga;
        ga.addFittedText (Font (FontOptions (fontSize, Font::bold)).withExtraKerningFactor (0.06f), "DELUSION",
                          area.getX(), area.getY(), area.getWidth() * 0.78f, area.getHeight(), Justification::centredLeft, 1, 0.6f);
        Path t;
        ga.createPath (t);
        const auto b = t.getBounds();
        const float cut = b.getX() + b.getWidth() * 0.55f;

        DropShadow (blue.withAlpha (0.45f), (int) (18.f * k0) + 2, {}).drawForPath (g, t);
        {
            Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (Rectangle<float> (b.getX() - 5.f, b.getY() - 5.f, cut - b.getX() + 5.f, b.getHeight() + 10.f).toNearestInt());
            g.setGradientFill (ColourGradient (Colours::white, b.getX(), 0.f, sky, cut, 0.f, false));
            g.fillPath (t);
        }

        Random r (11);
        const float step = juce::jmax (0.8f, 1.6f * k0);
        const float fade = 40.f * k0;
        for (float y = b.getY(); y < b.getBottom(); y += step)
            for (float x = cut - fade; x < b.getRight(); x += step)
            {
                if (! t.contains (x, y)) continue;
                const float k = jlimit (0.f, 1.f, (x - cut + fade) / (b.getRight() - cut + fade));
                if (r.nextFloat() < k * 0.75f) continue;
                const float dx = k * k * (30.f + r.nextFloat() * 70.f) * k0;
                const float dy = -k * k * (r.nextFloat() * 40.f - 8.f) * k0;
                g.setColour ((r.nextFloat() < 0.15f ? lilac : sky.interpolatedWith (Colours::white, 1.f - k)).withAlpha (1.f - k * 0.7f));
                const float sz = (1.2f + (1.f - k) * 0.6f) * jmax (0.7f, k0);
                g.fillRect (x + dx, y + dy, sz, sz);
            }
    }
}

namespace
{
    const float kTwoPi = juce::MathConstants<float>::twoPi;

    float waveAt (int wave, float ph)
    {
        switch (wave)
        {
            case 0:  return std::sin (kTwoPi * ph);
            case 1:  return 2.f * ph - 1.f;
            case 2:  return ph < 0.5f ? 1.f : -1.f;
            default: return 4.f * std::abs (ph - 0.5f) - 1.f;
        }
    }

    float warpAt (int wave, int warp, float amt, float ph)
    {
        if (warp == 0 || amt < 0.001f) return waveAt (wave, ph);
        switch (warp)
        {
            case 1:  return std::sin (waveAt (wave, ph) * (1.f + amt * 5.f) * juce::MathConstants<float>::halfPi);
            case 2:  { float p = ph * (1.f + amt * 7.f); return waveAt (wave, p - std::floor (p)); }
            case 3:  return waveAt (wave, std::pow (ph, 1.f / (1.f + amt * 4.f)));
            default: { const float d = 0.5f - 0.45f * amt;
                       return waveAt (wave, ph < d ? 0.5f * ph / d : 0.5f + 0.5f * (ph - d) / (1.f - d)); }
        }
    }

    // trace néon holographique
    void neon (juce::Graphics& g, const juce::Path& line, const juce::Path* fill, juce::Rectangle<float> r)
    {
        if (fill != nullptr)
        {
            g.setGradientFill (juce::ColourGradient (ui::violet().withAlpha (0.28f), 0.f, r.getY(),
                                                     ui::violet().withAlpha (0.f), 0.f, r.getBottom(), false));
            g.fillPath (*fill);
        }
        g.setGradientFill (ui::holo (r.getX(), r.getRight(), 0.f, 0.22f));
        g.strokePath (line, juce::PathStrokeType (6.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setGradientFill (ui::holo (r.getX(), r.getRight()));
        g.strokePath (line, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Font uiFont (float h, bool bold = false, float kern = 0.06f)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor (kern);
    }

    // panneau a coin coupé (éclat de verre)
    juce::Path shardPanel (juce::Rectangle<float> r, float cut)
    {
        juce::Path p;
        p.startNewSubPath (r.getX() + cut, r.getY());
        p.lineTo (r.getRight(), r.getY());
        p.lineTo (r.getRight(), r.getBottom() - cut * 0.6f);
        p.lineTo (r.getRight() - cut * 0.6f, r.getBottom());
        p.lineTo (r.getX(), r.getBottom());
        p.lineTo (r.getX(), r.getY() + cut);
        p.closeSubPath();
        return p;
    }
}

// ------------------------------------------------------------------ style
DarkLnF::DarkLnF()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff050d1f));
    setColour (juce::ComboBox::outlineColourId, ui::panelEdge());
    setColour (juce::ComboBox::textColourId, ui::ink());
    setColour (juce::ComboBox::arrowColourId, ui::accent());

    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff071229));
    setColour (juce::PopupMenu::textColourId, ui::ink());
    setColour (juce::PopupMenu::headerTextColourId, ui::violet());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::accent());
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0d1a35));
    setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff163a7a));
    setColour (juce::TextButton::textColourOffId, juce::Colour (0xffa9bddf));
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);

    setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffd2e2ff));
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff030a18));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff17305a));
    setColour (juce::Slider::textBoxHighlightColourId, ui::violet().withAlpha (0.5f));
    setColour (juce::Label::textColourId, ui::ink());
}

void DarkLnF::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                float pos, float startAngle, float endAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float arcR = radius - 4.f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // graduations
    g.setColour (juce::Colour (0xff27416e));
    for (int i = 0; i <= 12; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / 12.f;
        const juce::Point<float> d (std::sin (a), -std::cos (a));
        g.drawLine (juce::Line<float> (centre + d * (arcR + 2.f), centre + d * (arcR + (i % 3 == 0 ? 5.f : 3.5f))), 1.f);
    }

    juce::Path rail;
    rail.addCentredArc (centre.x, centre.y, arcR - 1.f, arcR - 1.f, 0.f, startAngle, endAngle, true);
    g.setColour (ui::track());
    g.strokePath (rail, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR - 1.f, arcR - 1.f, 0.f, startAngle, angle, true);
    juce::ColourGradient grad (ui::deep(), bounds.getX(), bounds.getBottom(), juce::Colours::white, bounds.getRight(), bounds.getY(), false);
    grad.addColour (0.5, ui::accent());
    auto glow = grad;
    glow.multiplyOpacity (0.28f);
    g.setGradientFill (glow);
    g.strokePath (value, juce::PathStrokeType (7.f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    g.setGradientFill (grad);
    g.strokePath (value, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    // corps : métal bleuté poli
    const float bodyR = arcR - 7.f;
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (centre.x - bodyR - 1.f, centre.y - bodyR + 2.f, bodyR * 2.f + 2.f, bodyR * 2.f + 2.f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a5788), centre.x, centre.y - bodyR,
                                             juce::Colour (0xff0a1630), centre.x, centre.y + bodyR, false));
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.f, bodyR * 2.f);
    const float capR = bodyR * 0.78f;
    juce::ColourGradient cap (juce::Colour (0xff1e3561), centre.x - capR * 0.4f, centre.y - capR * 0.6f,
                              juce::Colour (0xff0b162d), centre.x + capR * 0.5f, centre.y + capR, true);
    g.setGradientFill (cap);
    g.fillEllipse (centre.x - capR, centre.y - capR, capR * 2.f, capR * 2.f);
    // reflet
    juce::Path shine;
    shine.addCentredArc (centre.x, centre.y, capR - 1.5f, capR - 1.5f, 0.f, -2.2f, -0.6f, true);
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.strokePath (shine, juce::PathStrokeType (1.2f));

    const juce::Point<float> dir (std::sin (angle), -std::cos (angle));
    g.setColour (juce::Colours::white);
    g.drawLine (juce::Line<float> (centre + dir * (capR * 0.25f), centre + dir * (capR * 0.92f)), 2.f);
    // LED au bout de l'arc
    const auto tip = centre + dir * (arcR - 1.f);
    g.setColour (ui::violet().withAlpha (0.35f));
    g.fillEllipse (tip.x - 4.f, tip.y - 4.f, 8.f, 8.f);
    g.setColour (juce::Colours::white);
    g.fillEllipse (tip.x - 1.6f, tip.y - 1.6f, 3.2f, 3.2f);
}

void DarkLnF::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                float sliderPos, float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    // barre bipolaire (matrice de modulation)
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (0.f, 3.f);
    g.setColour (juce::Colour (0xff030a18));
    g.fillRect (r);
    g.setColour (ui::panelEdge());
    g.drawRect (r, 1.f);

    const float mid = r.getCentreX();
    const float v = (float) s.getValue();
    auto bar = juce::Rectangle<float>::leftTopRightBottom (juce::jmin (mid, sliderPos), r.getY() + 3.f,
                                                           juce::jmax (mid, sliderPos), r.getBottom() - 3.f);
    g.setGradientFill (juce::ColourGradient ((v >= 0.f ? ui::accent() : ui::lilac()).withAlpha (0.5f), mid, 0.f,
                                             (v >= 0.f ? ui::accent() : ui::lilac()), sliderPos, 0.f, false));
    g.fillRect (bar);
    g.setColour (ui::mute());
    g.drawVerticalLine ((int) mid, r.getY(), r.getBottom());

    const juce::String txt = (v > 0.f ? "+" : "") + juce::String (v * 100.f, 1) + "%";
    auto tr = (v >= 0.f ? r.withTrimmedRight (r.getWidth() * 0.5f) : r.withTrimmedLeft (r.getWidth() * 0.5f)).reduced (6.f, 0.f).toNearestInt();
    g.setFont (uiFont (11.f, true));
    g.setColour (ui::ink());
    g.drawText (txt, tr, v >= 0.f ? juce::Justification::centredLeft : juce::Justification::centredRight);
}

void DarkLnF::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                    bool highlighted, bool down)
{
    auto b = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();
    auto c = backgroundColour;
    if (down)             c = c.brighter (0.12f);
    else if (highlighted) c = c.brighter (0.06f);

    const float cut = juce::jmin (7.f, b.getHeight() * 0.3f);
    auto shape = shardPanel (b, cut);
    g.setGradientFill (juce::ColourGradient (c.brighter (0.05f), 0.f, b.getY(), c.darker (0.3f), 0.f, b.getBottom(), false));
    g.fillPath (shape);

    // reflet du haut
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), 0.f, b.getY(),
                                             juce::Colours::transparentWhite, 0.f, b.getCentreY(), false));
    g.fillPath (shape);

    if (on)
    {
        g.setGradientFill (juce::ColourGradient (ui::accent().withAlpha (0.55f), 0.f, b.getY(),
                                                 ui::deep().withAlpha (0.35f), 0.f, b.getBottom(), false));
        g.fillPath (shape);
        g.setColour (ui::violet());
        g.strokePath (shape, juce::PathStrokeType (1.2f));
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.fillRect (b.getX() + cut + 4.f, b.getBottom() - 2.f, b.getWidth() - cut * 2.f - 8.f, 1.f);
        if (button.getClickingTogglesState() && b.getWidth() > 60.f)
        {
            g.setColour (ui::ice().withAlpha (0.35f));
            g.fillEllipse (b.getX() + 6.f, b.getCentreY() - 5.f, 10.f, 10.f);
            g.setColour (juce::Colours::white);
            g.fillEllipse (b.getX() + 8.f, b.getCentreY() - 3.f, 6.f, 6.f);
        }
    }
    else
    {
        g.setColour (ui::panelEdge());
        g.strokePath (shape, juce::PathStrokeType (1.f));
    }
}

void DarkLnF::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    const bool on = button.getToggleState();
    g.setColour (button.findColour (on ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId));
    g.setFont (uiFont (juce::jmin (12.f, (float) button.getHeight() * 0.55f), true, 0.1f));
    auto area = button.getLocalBounds().reduced (6, 1);
    if (on && button.getClickingTogglesState() && area.getWidth() > 60) area.removeFromLeft (10);
    g.drawFittedText (button.getButtonText().toUpperCase(), area, juce::Justification::centred, 1);
}

void DarkLnF::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    auto b = juce::Rectangle<float> (0.f, 0.f, (float) width, (float) height).reduced (0.5f);
    auto shape = shardPanel (b, juce::jmin (6.f, b.getHeight() * 0.3f));
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillPath (shape);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.strokePath (shape, juce::PathStrokeType (1.f));

    const float cx = (float) width - 12.f, cy = (float) height * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (cx - 4.f, cy - 2.f, cx + 4.f, cy - 2.f, cx, cy + 3.f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

juce::Font DarkLnF::getComboBoxFont (juce::ComboBox& b) { return uiFont (juce::jmin (13.f, (float) b.getHeight() * 0.55f), true); }
juce::Font DarkLnF::getPopupMenuFont()                  { return uiFont (14.f); }
juce::Font DarkLnF::getLabelFont (juce::Label&)         { return uiFont (11.f); }

void DarkLnF::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

// ------------------------------------------------------------------ éditeur
size_t InfiniteEditor::addKnob (const juce::String& id, const juce::String& name, std::vector<size_t>& group, int pg)
{
    Knob k;
    k.name = name;
    k.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
    k.slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff17305a));
    k.slider->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff030a18));
    k.slider->setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffd2e2ff));
    k.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 15);

    juce::Slider::RotaryParameters rp;
    rp.startAngleRadians = juce::MathConstants<float>::pi * 1.2f;
    rp.endAngleRadians   = juce::MathConstants<float>::pi * 2.8f;
    rp.stopAtEnd = true;
    k.slider->setRotaryParameters (rp);

    k.att = std::make_unique<SliderAtt> (proc.apvts, id, *k.slider);
    addAndMakeVisible (*k.slider);
    pageComps[(size_t) pg].push_back (k.slider.get());
    group.push_back (knobs.size());
    knobs.push_back (std::move (k));
    return knobs.size() - 1;
}

juce::ComboBox& InfiniteEditor::addCombo (Combo& c, const juce::String& id, int pg)
{
    c.box = std::make_unique<juce::ComboBox>();
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c.box->addItemList (p->choices, 1);
    c.att = std::make_unique<ComboAtt> (proc.apvts, id, *c.box);
    addAndMakeVisible (*c.box);
    pageComps[(size_t) pg].push_back (c.box.get());
    return *c.box;
}

juce::TextButton& InfiniteEditor::addToggle (Toggle& t, const juce::String& id, const juce::String& text, int pg)
{
    t.btn = std::make_unique<juce::TextButton> (text);
    t.btn->setClickingTogglesState (true);
    t.att = std::make_unique<ButtonAtt> (proc.apvts, id, *t.btn);
    addAndMakeVisible (*t.btn);
    pageComps[(size_t) pg].push_back (t.btn.get());
    return *t.btn;
}

InfiniteEditor::InfiniteEditor (InfiniteProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    // ---- catégories ----
    static const char* catNames[] = { "Lead", "Pad", "Pluck", "Keys", "Piano", "E-Piano", "Brass", "Guitar", "Strings", "Winds", "All" };
    for (size_t i = 0; i < catButtons.size(); ++i)
    {
        auto& b = catButtons[i];
        b.setButtonText (catNames[i]);
        b.setClickingTogglesState (true);
        b.setRadioGroupId (101);
        b.onClick = [this, i] { if (catButtons[i].getToggleState()) proc.loadSeed ((int) i + 1, 1); };
        addAndMakeVisible (b);
    }
    catButtons[(size_t) (juce::jlimit (1, 11, currentCat()) - 1)].setToggleState (true, juce::dontSendNotification);

    // ---- pages ----
    static const char* tabNames[] = { "Synth", "Mod", "FX" };
    for (size_t i = 0; i < pageTabs.size(); ++i)
    {
        auto& t = pageTabs[i];
        t.setButtonText (tabNames[i]);
        t.setClickingTogglesState (true);
        t.setRadioGroupId (102);
        t.onClick = [this, i] { if (pageTabs[i].getToggleState()) showPage ((int) i); };
        addAndMakeVisible (t);
    }

    // ---- banque ----
    prevBtn.onClick = [this] { proc.loadSeed (currentCat(), juce::jmax (1, currentSeed() - 1)); };
    nextBtn.onClick = [this] { proc.loadSeed (currentCat(), currentSeed() + 1); };
    genBtn.onClick  = [this] { proc.loadSeed (currentCat(), InfiniteProcessor::kBankSize + 1
                                                            + juce::Random::getSystemRandom().nextInt (1000000)); };
    saveBtn.onClick = [this] { proc.saveFavorite(); refreshFavorites(); };
    genBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1b4fb5));
    genBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    for (auto* b : { &prevBtn, &nextBtn, &genBtn, &saveBtn }) addAndMakeVisible (b);

    favorites.setTextWhenNothingSelected ("Favorites");
    favorites.onChange = [this]
    {
        const int id = favorites.getSelectedId();
        if (id > 0)
        {
            proc.loadFavorite (id - 1);
            favorites.setSelectedId (0, juce::dontSendNotification);
        }
    };
    addAndMakeVisible (favorites);
    refreshFavorites();

    // ---- écrans ----
    scope.painter   = [this] (juce::Graphics& g, juce::Rectangle<float> r) { paintScope (g, r); };
    filterD.painter = [this] (juce::Graphics& g, juce::Rectangle<float> r) { paintFilter (g, r); };
    ampD.painter    = [this] (juce::Graphics& g, juce::Rectangle<float> r) { paintAdsr (g, r, "attack", "decay", "sustain", "release"); };
    env2D.painter   = [this] (juce::Graphics& g, juce::Rectangle<float> r) { paintAdsr (g, r, "e2a", "e2d", "e2s", "e2r"); };
    env3D.painter   = [this] (juce::Graphics& g, juce::Rectangle<float> r) { paintAdsr (g, r, "e3a", "e3d", "e3s", "e3r"); };
    uniD.painter    = [this] (juce::Graphics& g, juce::Rectangle<float> r) { paintUnison (g, r); };
    lfoD.painter    = [this] (juce::Graphics& g, juce::Rectangle<float> r) { paintLfo (g, r); };
    driveD.painter  = [this] (juce::Graphics& g, juce::Rectangle<float> r) { paintDrive (g, r); };
    for (auto* d : { &scope, &filterD, &ampD, &uniD, &driveD }) { addAndMakeVisible (d); pageComps[0].push_back (d); }
    for (auto* d : { &env2D, &env3D, &lfoD })                   { addAndMakeVisible (d); pageComps[1].push_back (d); }

    // ---- page SYNTH ----
    addCombo (w1, "osc1Wave", 0);
    addCombo (warp1, "warp1Mode", 0);
    addCombo (w2, "osc2Wave", 0);
    addCombo (warp2, "warp2Mode", 0);
    addCombo (filtType, "filtType", 0);
    addToggle (mono, "mono", "Mono / Slide", 0);

    addKnob ("warp1Amt", "Warp 1", gOsc, 0);
    addKnob ("osc2Semi", "Semi", gOsc, 0);
    addKnob ("osc2Fine", "Fine", gOsc, 0);
    addKnob ("osc2Mix",  "Osc2 Lvl", gOsc, 0);
    addKnob ("warp2Amt", "Warp 2", gOsc, 0);
    addKnob ("sub",      "Sub", gOsc, 0);
    addKnob ("noise",    "Noise", gOsc, 0);
    addKnob ("fmAmt",    "FM", gOsc, 0);
    addKnob ("fmRatio",  "FM Ratio", gOsc, 0);

    addCombo (engine, "engine", 0);
    for (int i = 1; i <= 4; ++i) addKnob ("inst" + juce::String (i), "Macro", gInst, 0);

    addKnob ("cutoff",   "Cutoff", gFilter, 0);
    addKnob ("reso",     "Reso", gFilter, 0);
    addKnob ("envAmt",   "Env 2 Amt", gFilter, 0);
    addKnob ("keyTrack", "Key Track", gFilter, 0);

    addKnob ("attack",   "Attack", gAmp, 0);
    addKnob ("decay",    "Decay", gAmp, 0);
    addKnob ("sustain",  "Sustain", gAmp, 0);
    addKnob ("release",  "Release", gAmp, 0);

    addKnob ("uniVoices", "Voices", gUni, 0);
    addKnob ("uniDetune", "Detune", gUni, 0);
    addKnob ("uniSpread", "Spread", gUni, 0);

    addKnob ("glide",    "Glide", gOut, 0);
    addKnob ("drive",    "Drive", gOut, 0);
    addKnob ("volume",   "Volume", gOut, 0);

    // ---- page MOD ----
    addKnob ("e2a", "Attack", gEnv2, 1);
    addKnob ("e2d", "Decay", gEnv2, 1);
    addKnob ("e2s", "Sustain", gEnv2, 1);
    addKnob ("e2r", "Release", gEnv2, 1);
    addKnob ("e3a", "Attack", gEnv3, 1);
    addKnob ("e3d", "Decay", gEnv3, 1);
    addKnob ("e3s", "Sustain", gEnv3, 1);
    addKnob ("e3r", "Release", gEnv3, 1);

    for (size_t i = 0; i < lfoTabs.size(); ++i)
    {
        auto& t = lfoTabs[i];
        t.setButtonText (juce::String ((int) i + 1));
        t.setClickingTogglesState (true);
        t.setRadioGroupId (103);
        t.onClick = [this, i] { if (lfoTabs[i].getToggleState()) bindLfo ((int) i); };
        addAndMakeVisible (t);
        pageComps[1].push_back (&t);
    }
    lfoShape.addItemList (lists::lfoShapes(), 1);
    lfoDiv.addItemList (lists::divs(), 1);
    lfoSync.setClickingTogglesState (true);
    lfoRetrig.setClickingTogglesState (true);
    lfoRate.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    lfoRate.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff17305a));
    lfoRate.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff030a18));
    lfoRate.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffd2e2ff));
    lfoRate.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 15);
    {
        juce::Slider::RotaryParameters rp;
        rp.startAngleRadians = juce::MathConstants<float>::pi * 1.2f;
        rp.endAngleRadians   = juce::MathConstants<float>::pi * 2.8f;
        rp.stopAtEnd = true;
        lfoRate.setRotaryParameters (rp);
    }
    for (auto* c : std::initializer_list<juce::Component*> { &lfoShape, &lfoDiv, &lfoSync, &lfoRetrig, &lfoRate })
    {
        addAndMakeVisible (c);
        pageComps[1].push_back (c);
    }
    lfoTabs[0].setToggleState (true, juce::dontSendNotification);
    bindLfo (0);

    for (int i = 0; i < lists::numSlots; ++i)
    {
        auto& row = modRows[(size_t) i];
        const juce::String n (i + 1);
        addCombo (row.src, "mod" + n + "Src", 1);
        addCombo (row.dst, "mod" + n + "Dst", 1);
        row.amt = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        row.amtAtt = std::make_unique<SliderAtt> (proc.apvts, "mod" + n + "Amt", *row.amt);
        row.amt->setDoubleClickReturnValue (true, 0.0);
        addAndMakeVisible (*row.amt);
        pageComps[1].push_back (row.amt.get());
    }

    // ---- page FX ----
    struct FxDef { const char* on; const char* title; std::vector<std::pair<const char*, const char*>> knobs; };
    const std::vector<FxDef> fxDefs = {
        { "distOn",  "Distortion", { { "distDrive", "Drive" }, { "distTone", "Tone" }, { "distMix", "Mix" } } },
        { "crushOn", "Bitcrush",   { { "crushBits", "Bits" }, { "crushRate", "Downsample" }, { "crushMix", "Mix" } } },
        { "eqOn",    "EQ",         { { "eqLow", "Low" }, { "eqMid", "Mid" }, { "eqHigh", "High" } } },
        { "compOn",  "Compressor", { { "compThresh", "Threshold" }, { "compRatio", "Ratio" }, { "compGain", "Gain" } } },
        { "chorOn",  "Chorus",     { { "chorRate", "Rate" }, { "chorDepth", "Depth" }, { "chorMix", "Mix" } } },
        { "flanOn",  "Flanger",    { { "flanRate", "Rate" }, { "flanDepth", "Depth" }, { "flanFb", "Feedback" }, { "flanMix", "Mix" } } },
        { "phasOn",  "Phaser",     { { "phasRate", "Rate" }, { "phasDepth", "Depth" }, { "phasFb", "Feedback" }, { "phasMix", "Mix" } } },
        { "tremOn",  "Tremolo",    { { "tremRate", "Rate" }, { "tremDepth", "Depth" }, { "tremPan", "Auto-pan" } } },
        { "dlyOn",   "Delay",      { { "dlyTime", "Time" }, { "dlyFb", "Feedback" }, { "dlyMix", "Mix" } } },
        { "revOn",   "Reverb",     { { "revSize", "Size" }, { "revDamp", "Damping" }, { "revWidth", "Width" }, { "revMix", "Mix" } } }
    };
    for (size_t i = 0; i < fxDefs.size() && i < fxToggles.size(); ++i)
    {
        addToggle (fxToggles[i], fxDefs[i].on, fxDefs[i].title, 2);
        for (auto& k : fxDefs[i].knobs) addKnob (k.first, k.second, gFx[i], 2);
    }

    pageTabs[0].setToggleState (true, juce::dontSendNotification);
    showPage (0);

    startTimerHz (20);
    timerCallback();
    setSize (1000, 660);   // en dernier : tous les composants existent déjà
}

InfiniteEditor::~InfiniteEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

int InfiniteEditor::currentSeed() const { return (int) proc.apvts.state.getProperty ("seed", 1); }
int InfiniteEditor::currentCat()  const { return (int) proc.apvts.state.getProperty ("catSel", 1); }
float InfiniteEditor::pv (const juce::String& id) const
{
    if (auto* v = proc.apvts.getRawParameterValue (id)) return v->load();
    return 0.f;
}

void InfiniteEditor::bindLfo (int index)
{
    curLfo = juce::jlimit (0, lists::numLfos - 1, index);
    const juce::String n (curLfo + 1);
    lfoShapeAtt.reset();  lfoDivAtt.reset();  lfoSyncAtt.reset();  lfoRetrigAtt.reset();  lfoRateAtt.reset();
    lfoShapeAtt  = std::make_unique<ComboAtt>  (proc.apvts, "lfo" + n + "Shape", lfoShape);
    lfoDivAtt    = std::make_unique<ComboAtt>  (proc.apvts, "lfo" + n + "Div", lfoDiv);
    lfoSyncAtt   = std::make_unique<ButtonAtt> (proc.apvts, "lfo" + n + "Sync", lfoSync);
    lfoRetrigAtt = std::make_unique<ButtonAtt> (proc.apvts, "lfo" + n + "Retrig", lfoRetrig);
    lfoRateAtt   = std::make_unique<SliderAtt> (proc.apvts, "lfo" + n + "Rate", lfoRate);
    repaint();
}

void InfiniteEditor::showPage (int pg)
{
    page = juce::jlimit (0, 2, pg);
    for (int i = 0; i < 3; ++i)
        for (auto* c : pageComps[(size_t) i])
            c->setVisible (i == page);
    shownEngine = -1;
    if (! gInst.empty()) updateEngineView();
    if (! pageTabs[(size_t) page].getToggleState())
        pageTabs[(size_t) page].setToggleState (true, juce::dontSendNotification);
    repaint();
}

void InfiniteEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && displayRect.contains (e.getPosition()))
        showBankMenu();
}

void InfiniteEditor::showBankMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("BANKS");

    const int curCat = currentCat(), curSeed = currentSeed();
    for (int cat = 1; cat <= InfiniteProcessor::kNumCats; ++cat)
    {
        juce::PopupMenu sub;
        for (int s = 1; s <= InfiniteProcessor::kBankSize; ++s)
            sub.addItem (cat * 1000 + s,
                         juce::String (s).paddedLeft ('0', 2) + "   " + InfiniteProcessor::presetName (cat, s),
                         true, cat == curCat && s == curSeed);
        menu.addSubMenu (InfiniteProcessor::catName (cat), sub, true, nullptr, cat == curCat);
    }

    auto favNames = proc.getFavoriteNames();
    juce::PopupMenu favMenu;
    for (int i = 0; i < favNames.size(); ++i) favMenu.addItem (100000 + i, favNames[i]);
    menu.addSeparator();
    menu.addSubMenu ("Favorites", favMenu, favNames.size() > 0);

    juce::Component::SafePointer<InfiniteEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withParentComponent (this)
                            .withTargetScreenArea (localAreaToGlobal (displayRect))
                            .withMinimumWidth (260),
                        [safe] (int id)
                        {
                            if (safe == nullptr || id <= 0) return;
                            if (id >= 100000) safe->proc.loadFavorite (id - 100000);
                            else              safe->proc.loadSeed (id / 1000, id % 1000);
                        });
}

void InfiniteEditor::refreshFavorites()
{
    favorites.clear (juce::dontSendNotification);
    auto names = proc.getFavoriteNames();
    for (int i = 0; i < names.size(); ++i) favorites.addItem (names[i], i + 1);
}

void InfiniteEditor::timerCallback()
{
    auto t = proc.getTitle(), i = proc.getInfo();
    if (t != lastTitle || i != lastInfo)
    {
        lastTitle = t;
        lastInfo = i;
        repaint (displayRect);
    }

    const size_t c = (size_t) (juce::jlimit (1, 11, currentCat()) - 1);
    if (! catButtons[c].getToggleState())
        catButtons[c].setToggleState (true, juce::dontSendNotification);

    if ((int) pv ("engine") != shownEngine) updateEngineView();

    for (auto* d : { &scope, &filterD, &ampD, &uniD, &driveD, &env2D, &env3D, &lfoD })
        if (d->isVisible()) d->repaint();
}

void InfiniteEditor::updateEngineView()
{
    shownEngine = (int) pv ("engine");
    const bool synth = shownEngine == inst::Synth;
    const bool p0 = page == 0;
    for (auto* c : std::initializer_list<juce::Component*> { w1.box.get(), w2.box.get(), warp1.box.get(), warp2.box.get() })
        c->setVisible (p0 && synth);
    for (auto i : gOsc)  knobs[i].slider->setVisible (p0 && synth);
    auto names = inst::macroNames (shownEngine);
    for (size_t j = 0; j < gInst.size(); ++j)
    {
        knobs[gInst[j]].slider->setVisible (p0 && ! synth);
        knobs[gInst[j]].name = names[(int) j];
    }
    repaint();
}

void InfiniteEditor::paintOverChildren (juce::Graphics& g)
{
    // l'unison ne s'applique qu'au moteur Synth : on l'assombrit pour les instruments
    if (page == 0 && shownEngine != inst::Synth && ! pUni.isEmpty())
    {
        g.setColour (ui::bg2().withAlpha (0.72f));
        g.fillRect (pUni.reduced (1));
        g.setColour (ui::mute());
        g.setFont (juce::Font (juce::FontOptions (11.f, juce::Font::bold)).withExtraKerningFactor (0.25f));
        g.drawText ("SYNTH ENGINE ONLY", pUni, juce::Justification::centred);
    }
}

// ------------------------------------------------------------------ dessin des écrans
void InfiniteEditor::paintScope (juce::Graphics& g, juce::Rectangle<float> r)
{
    const int eng = (int) pv ("engine");
    if (eng != inst::Synth)
    {
        // spectre approximatif de l'instrument : barres d'harmoniques
        const float tone = pv ("inst1"), ch = pv ("inst2");
        const int n = 24;
        const float bw = r.getWidth() / (float) n;
        for (int i = 1; i <= n; ++i)
        {
            float a;
            switch (eng)
            {
                case inst::Piano:   a = std::pow ((float) i, -(2.3f - 1.4f * tone)) * (0.4f + 0.6f * std::abs (std::sin (3.1416f * (float) i / 8.3f))); break;
                case inst::EPiano:  a = (i == 1 ? 1.f : 0.35f * ch / (float) i) + (i == 14 ? tone * 0.6f : 0.f); break;
                case inst::Brass:   a = std::pow ((float) i, -(1.6f - 1.1f * tone)); break;
                case inst::Plucked: a = std::pow ((float) i, -(1.8f - 1.2f * tone)) * (0.3f + 0.7f * std::abs (std::sin (3.1416f * (float) i * (0.06f + 0.4f * pv ("inst4"))))); break;
                case inst::Strings: a = std::pow ((float) i, -(1.4f - 0.8f * tone)); break;
                case inst::Flute:   a = i == 1 ? 1.f : (i == 2 ? 0.1f + 0.35f * ch : (i == 3 ? 0.12f * ch : 0.02f * tone)); break;
                default:            a = std::pow ((float) i, -1.f) * (0.4f + 0.6f * std::abs (std::sin ((float) i * 0.9f + tone * 3.f))); break;
            }
            a = juce::jlimit (0.f, 1.f, a);
            const float h = std::pow (a, 0.5f) * r.getHeight() * 0.92f;
            const float x = r.getX() + (float) (i - 1) * bw;
            g.setGradientFill (juce::ColourGradient (juce::Colours::white, 0.f, r.getBottom() - h, ui::accent().withAlpha (0.4f), 0.f, r.getBottom(), false));
            g.fillRect (x + bw * 0.2f, r.getBottom() - h, bw * 0.6f, h);
        }
        return;
    }

    const int w1v = (int) pv ("osc1Wave"), w2v = (int) pv ("osc2Wave");
    const int m1 = (int) pv ("warp1Mode"), m2 = (int) pv ("warp2Mode");
    const float a1 = pv ("warp1Amt"), a2 = pv ("warp2Amt");
    const float mix = pv ("osc2Mix"), sub = pv ("sub"), fm = pv ("fmAmt"), ratio = pv ("fmRatio");
    const float detune = std::exp2 ((pv ("osc2Semi") + pv ("osc2Fine") / 100.f) / 12.f);
    const float k = 1.f + pv ("drive") * 5.f;

    const int n = 260;
    std::vector<float> ys ((size_t) n);
    float peak = 0.001f;
    for (int i = 0; i < n; ++i)
    {
        const float x = 2.f * (float) i / (float) (n - 1);
        float p1 = x + 0.5f * fm * std::sin (kTwoPi * ratio * x);
        p1 -= std::floor (p1);
        float p2 = x * detune;
        p2 -= std::floor (p2);
        float s = warpAt (w1v, m1, a1, p1) + mix * warpAt (w2v, m2, a2, p2) + sub * std::sin (kTwoPi * x * 0.5f);
        s = std::tanh (s * k);
        ys[(size_t) i] = s;
        peak = std::max (peak, std::abs (s));
    }

    const float cy = r.getCentreY(), amp = r.getHeight() * 0.46f;
    juce::Path line, fill;
    for (int i = 0; i < n; ++i)
    {
        const float px = r.getX() + r.getWidth() * (float) i / (float) (n - 1);
        const float py = cy - (ys[(size_t) i] / peak) * amp;
        if (i == 0) { line.startNewSubPath (px, py); fill.startNewSubPath (px, cy); fill.lineTo (px, py); }
        else        { line.lineTo (px, py); fill.lineTo (px, py); }
    }
    fill.lineTo (r.getRight(), cy);
    fill.closeSubPath();
    neon (g, line, &fill, r);
}

void InfiniteEditor::paintFilter (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float fc = std::max (pv ("cutoff"), 20.f), q = std::max (pv ("reso"), 0.3f);
    const int type = (int) pv ("filtType");
    const int n = 200;

    juce::Path line, fill;
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) (n - 1);
        const float f = 20.f * std::pow (1000.f, t);
        const float rr = f / fc;
        const float den = std::max (0.0001f, std::sqrt ((1.f - rr * rr) * (1.f - rr * rr) + (rr / q) * (rr / q)));
        const float d2  = std::max (0.0001f, std::sqrt ((1.f - rr * rr) * (1.f - rr * rr) + (rr / 0.6f) * (rr / 0.6f)));
        float mag;
        switch (type)
        {
            case 1:  mag = 1.f / (den * d2); break;
            case 2:  mag = rr * rr / den; break;
            case 3:  mag = (rr * rr / den) * (rr * rr / d2); break;
            case 4:  mag = (rr / q) / den; break;
            default: mag = 1.f / den; break;
        }
        const float db = 20.f * std::log10 (std::max (mag, 0.00001f));
        const float yn = juce::jlimit (0.f, 1.f, (db + 36.f) / 60.f);
        const float px = r.getX() + r.getWidth() * t;
        const float py = r.getBottom() - yn * r.getHeight();
        if (i == 0) { line.startNewSubPath (px, py); fill.startNewSubPath (px, r.getBottom()); fill.lineTo (px, py); }
        else        { line.lineTo (px, py); fill.lineTo (px, py); }
    }
    fill.lineTo (r.getRight(), r.getBottom());
    fill.closeSubPath();
    neon (g, line, &fill, r);

    const float xc = r.getX() + r.getWidth() * (std::log (fc / 20.f) / std::log (1000.f));
    g.setColour (ui::ice().withAlpha (0.6f));
    g.drawVerticalLine ((int) juce::jlimit (r.getX(), r.getRight(), xc), r.getY(), r.getBottom());
}

void InfiniteEditor::paintAdsr (juce::Graphics& g, juce::Rectangle<float> r, const char* ia, const char* id, const char* is, const char* ir)
{
    const float a = std::max (pv (ia), 0.01f), d = std::max (pv (id), 0.02f);
    const float s = juce::jlimit (0.f, 1.f, pv (is)), rel = std::max (pv (ir), 0.02f);
    const float hold = 0.3f * (a + d + rel);
    const float total = a + d + hold + rel;

    const float top = r.getY() + 3.f, bot = r.getBottom() - 1.f;
    const float xa = r.getX() + r.getWidth() * (a / total);
    const float xd = xa + r.getWidth() * (d / total);
    const float xh = xd + r.getWidth() * (hold / total);
    const float sy = bot - s * (bot - top);

    juce::Path line;
    line.startNewSubPath (r.getX(), bot);
    line.quadraticTo (r.getX() + (xa - r.getX()) * 0.3f, top + (bot - top) * 0.2f, xa, top);
    line.quadraticTo (xa + (xd - xa) * 0.2f, sy, xd, sy);
    line.lineTo (xh, sy);
    line.quadraticTo (xh + (r.getRight() - xh) * 0.2f, bot, r.getRight(), bot);

    juce::Path fill (line);
    fill.lineTo (r.getX(), bot);
    fill.closeSubPath();
    neon (g, line, &fill, r);

    g.setColour (juce::Colours::white);
    for (auto p : { juce::Point<float> (xa, top), juce::Point<float> (xd, sy), juce::Point<float> (xh, sy) })
    {
        juce::Path dmd;
        dmd.addQuadrilateral (p.x, p.y - 4.f, p.x + 4.f, p.y, p.x, p.y + 4.f, p.x - 4.f, p.y);
        g.fillPath (dmd);
    }
}

void InfiniteEditor::paintUnison (juce::Graphics& g, juce::Rectangle<float> r)
{
    const int n = juce::jlimit (1, 7, (int) pv ("uniVoices"));
    const float det = pv ("uniDetune"), spread = pv ("uniSpread");
    const float cx = r.getCentreX(), cy = r.getCentreY();

    for (int k = 0; k < n; ++k)
    {
        const float pos = n > 1 ? 2.f * (float) k / (float) (n - 1) - 1.f : 0.f;
        const float x = cx + pos * spread * r.getWidth() * 0.45f;
        const float h = r.getHeight() * (0.9f - std::abs (pos) * det * 0.5f);
        juce::Path bar;
        bar.startNewSubPath (x, cy - h * 0.5f);
        bar.lineTo (x + pos * det * 10.f, cy + h * 0.5f);
        neon (g, bar, nullptr, r);
    }
}

void InfiniteEditor::paintLfo (juce::Graphics& g, juce::Rectangle<float> r)
{
    const int shape = (int) pv ("lfo" + juce::String (curLfo + 1) + "Shape");
    const float cy = r.getCentreY(), amp = r.getHeight() * 0.44f;
    juce::Random rnd (1234);
    const int cycles = 2, n = 240;
    std::array<float, 4> vals {};
    for (auto& v : vals) v = rnd.nextFloat() * 2.f - 1.f;

    juce::Path line;
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) (n - 1) * (float) cycles;
        const int c = juce::jlimit (0, 2, (int) t);
        const float ph = t - (float) c;
        float v;
        switch (shape)
        {
            case 0:  v = std::sin (kTwoPi * ph); break;
            case 1:  v = 1.f - 4.f * std::abs (ph - 0.5f); break;
            case 2:  v = 2.f * ph - 1.f; break;
            case 3:  v = 1.f - 2.f * ph; break;
            case 4:  v = ph < 0.5f ? 1.f : -1.f; break;
            case 5:  v = vals[(size_t) c]; break;
            default: v = vals[(size_t) c] + (vals[(size_t) c + 1] - vals[(size_t) c]) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * ph)); break;
        }
        const float px = r.getX() + r.getWidth() * (float) i / (float) (n - 1);
        const float py = cy - v * amp;
        if (i == 0) line.startNewSubPath (px, py); else line.lineTo (px, py);
    }
    neon (g, line, nullptr, r);
}

void InfiniteEditor::paintDrive (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float k = 1.f + pv ("drive") * 5.f;
    const float cy = r.getCentreY();
    g.setColour (ui::track());
    g.drawLine (r.getX(), r.getBottom(), r.getRight(), r.getY(), 1.f);

    const int n = 120;
    juce::Path line;
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) (n - 1);
        const float px = r.getX() + r.getWidth() * t;
        const float py = cy - std::tanh (k * (t * 2.f - 1.f)) * r.getHeight() * 0.5f;
        if (i == 0) line.startNewSubPath (px, py); else line.lineTo (px, py);
    }
    neon (g, line, nullptr, r);
}

// ------------------------------------------------------------------ décor
void InfiniteEditor::buildShards()
{
    // verre brisé : fissures qui partent d'un point d'impact, très discrètes
    shards = juce::Image (juce::Image::ARGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()), true);
    juce::Graphics g (shards);
    juce::Random rnd (7);

    const juce::Point<float> impact (860.f, 560.f);
    for (int i = 0; i < 26; ++i)
    {
        const float a = (float) i / 26.f * kTwoPi + rnd.nextFloat() * 0.2f;
        juce::Path crack;
        juce::Point<float> p = impact;
        crack.startNewSubPath (p);
        const float len = 200.f + rnd.nextFloat() * 700.f;
        const int segs = 6 + rnd.nextInt (6);
        for (int s = 1; s <= segs; ++s)
        {
            const float aa = a + (rnd.nextFloat() - 0.5f) * 0.35f;
            p += juce::Point<float> (std::cos (aa), std::sin (aa)) * (len / (float) segs);
            crack.lineTo (p);
        }
        g.setColour (juce::Colours::white.withAlpha (0.035f + rnd.nextFloat() * 0.03f));
        g.strokePath (crack, juce::PathStrokeType (0.8f));
    }
    for (int i = 1; i < 6; ++i)
    {
        const float rr = (float) i * 38.f + rnd.nextFloat() * 10.f;
        juce::Path ring;
        for (int s = 0; s <= 26; ++s)
        {
            const float a = (float) s / 26.f * kTwoPi;
            const float jitter = rr * (0.92f + rnd.nextFloat() * 0.16f);
            const juce::Point<float> q (impact.x + std::cos (a) * jitter, impact.y + std::sin (a) * jitter);
            if (s == 0) ring.startNewSubPath (q); else ring.lineTo (q);
        }
        g.setColour (juce::Colours::white.withAlpha (0.025f));
        g.strokePath (ring, juce::PathStrokeType (0.7f));
    }
    for (int i = 0; i < 9; ++i)
    {
        juce::Path tri;
        const juce::Point<float> c (rnd.nextFloat() * (float) getWidth(), 140.f + rnd.nextFloat() * (float) (getHeight() - 140));
        const float s = 40.f + rnd.nextFloat() * 120.f;
        tri.addTriangle (c, c + juce::Point<float> (s, rnd.nextFloat() * s * 0.4f), c + juce::Point<float> (rnd.nextFloat() * s * 0.5f, s));
        g.setGradientFill (juce::ColourGradient ((i % 2 ? juce::Colours::white : ui::accent()).withAlpha (0.04f), c.x, c.y,
                                                 ui::accent().withAlpha (0.f), c.x + s, c.y + s, false));
        g.fillPath (tri);
    }
}

void InfiniteEditor::paintEmblem (juce::Graphics& g, juce::Rectangle<float> r)
{
    // emblème : un prisme-miroir brisé qui contient un oeil
    const auto c = r.getCentre();
    const float w = r.getWidth() * 0.5f, h = r.getHeight() * 0.5f;

    // halo
    g.setGradientFill (juce::ColourGradient (ui::accent().withAlpha (0.35f), c.x, c.y,
                                             ui::accent().withAlpha (0.f), c.x + w * 1.4f, c.y, true));
    g.fillEllipse (r.expanded (w * 0.4f));

    const juce::Point<float> top (c.x, c.y - h), right (c.x + w, c.y), bottom (c.x, c.y + h), left (c.x - w, c.y);
    const juce::Point<float> p0 = left + (top - left) * 0.42f;           // départ de la fissure
    const juce::Point<float> p1 (c.x + w * 0.08f, c.y - h * 0.06f);      // coude au centre
    const juce::Point<float> p2 = right + (bottom - right) * 0.38f;      // arrivée

    juce::Path upper, lower;
    upper.startNewSubPath (p0); upper.lineTo (top); upper.lineTo (right); upper.lineTo (p2); upper.lineTo (p1); upper.closeSubPath();
    lower.startNewSubPath (p2); lower.lineTo (bottom); lower.lineTo (left); lower.lineTo (p0); lower.lineTo (p1); lower.closeSubPath();

    auto drawPiece = [&] (const juce::Path& piece, juce::Point<float> off, float shade)
    {
        juce::Graphics::ScopedSaveState s (g);
        g.addTransform (juce::AffineTransform::translation (off));

        // verre
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c3f86).withMultipliedBrightness (shade), c.x, c.y - h,
                                                 juce::Colour (0xff050d22), c.x, c.y + h, false));
        g.fillPath (piece);

        {
            juce::Graphics::ScopedSaveState s2 (g);
            g.reduceClipRegion (piece);

            // oeil
            const float ew = w * 0.78f, eh = h * 0.42f;
            juce::Path lid;
            lid.startNewSubPath (c.x - ew, c.y);
            lid.quadraticTo (c.x, c.y - eh * 2.f, c.x + ew, c.y);
            lid.quadraticTo (c.x, c.y + eh * 2.f, c.x - ew, c.y);
            lid.closeSubPath();
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.strokePath (lid, juce::PathStrokeType (1.3f));

            // iris en facettes
            const float ir = eh * 0.95f;
            for (int i = 0; i < 8; ++i)
            {
                const float a0 = (float) i / 8.f * kTwoPi, a1 = (float) (i + 1) / 8.f * kTwoPi;
                juce::Path facet;
                facet.addTriangle (c.x, c.y, c.x + std::cos (a0) * ir, c.y + std::sin (a0) * ir,
                                   c.x + std::cos (a1) * ir, c.y + std::sin (a1) * ir);
                g.setColour ((i % 2 == 0 ? ui::accent() : ui::violet()).withMultipliedBrightness (0.7f + 0.06f * (float) i));
                g.fillPath (facet);
            }
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawEllipse (c.x - ir, c.y - ir, ir * 2.f, ir * 2.f, 1.f);
            // pupille en losange
            juce::Path pupil;
            pupil.addQuadrilateral (c.x, c.y - ir * 0.55f, c.x + ir * 0.22f, c.y, c.x, c.y + ir * 0.55f, c.x - ir * 0.22f, c.y);
            g.setColour (ui::screen());
            g.fillPath (pupil);
            g.setColour (juce::Colours::white);
            g.fillEllipse (c.x + ir * 0.3f, c.y - ir * 0.45f, 2.6f, 2.6f);

            // reflet diagonal sur le verre
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.18f), c.x - w, c.y - h,
                                                     juce::Colours::transparentWhite, c.x, c.y, false));
            g.fillRect (r);
        }

        g.setGradientFill (ui::holo (c.x - w, c.x + w));
        g.strokePath (piece, juce::PathStrokeType (1.2f, juce::PathStrokeType::mitered));
    };

    drawPiece (lower, { 0.f, 0.f }, 1.f);
    drawPiece (upper, { 2.5f, -2.5f }, 1.3f);    // éclat décalé : le miroir est cassé

    // petits éclats qui s'envolent
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    juce::Path chip1, chip2;
    chip1.addTriangle (right.x + 1.f, top.y + 6.f, right.x + 5.f, top.y + 3.f, right.x + 4.f, top.y + 9.f);
    chip2.addTriangle (top.x + 9.f, top.y + 1.f, top.x + 12.f, top.y - 1.f, top.x + 11.f, top.y + 4.f);
    g.fillPath (chip1);
    g.setColour (ui::violet());
    g.fillPath (chip2);
}

void InfiniteEditor::paintLogo (juce::Graphics& g, juce::Rectangle<float> r)
{
    logo::drawDustLogo (g, r, 36.f);
}

// ------------------------------------------------------------------ dessin général
void InfiniteEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (ui::bg1(), 0.f, 0.f, ui::bg2(), 0.f, (float) getHeight(), false));
    g.fillAll();
    {
        juce::ColourGradient glow (ui::accent().withAlpha (0.12f), 500.f, 360.f, ui::accent().withAlpha (0.f), 1050.f, 660.f, true);
        g.setGradientFill (glow);
        g.fillAll();
    }
    if (shards.isValid()) g.drawImageAt (shards, 0, 0);

    // bandeau du haut
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b1d40), 0.f, 0.f, ui::header(), 0.f, 132.f, false));
    g.fillRect (0, 0, getWidth(), 132);
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.drawHorizontalLine (1, 0.f, (float) getWidth());
    g.setColour (ui::panelEdge().withAlpha (0.6f));
    g.drawHorizontalLine (90, 16.f, (float) getWidth() - 16.f);
    // filet lumineux sous l'en-tête
    g.setGradientFill (juce::ColourGradient (ui::accent().withAlpha (0.25f), (float) getWidth() * 0.5f, 132.f,
                                             ui::accent().withAlpha (0.f), (float) getWidth() * 0.5f, 150.f, false));
    g.fillRect (0, 132, getWidth(), 18);
    g.setGradientFill (juce::ColourGradient (ui::accent().withAlpha (0.f), 0.f, 0.f, ui::accent().withAlpha (0.f), (float) getWidth(), 0.f, false));
    {
        juce::ColourGradient line (ui::accent().withAlpha (0.f), 0.f, 0.f, ui::accent().withAlpha (0.f), (float) getWidth(), 0.f, false);
        line.addColour (0.5, juce::Colours::white);
        line.addColour (0.25, ui::accent());
        line.addColour (0.75, ui::accent());
        g.setGradientFill (line);
        g.fillRect (0, 131, getWidth(), 1);
    }

    paintLogo (g, juce::Rectangle<float> (14.f, 10.f, 196.f, 76.f));

    // grand écran du preset
    if (! displayRect.isEmpty())
    {
        auto d = displayRect.toFloat();
        auto shape = shardPanel (d, 12.f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0a1c40), d.getCentreX(), d.getCentreY(),
                                                 ui::screen(), d.getX(), d.getY(), true));
        g.fillPath (shape);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), 0.f, d.getY(),
                                                 juce::Colours::transparentWhite, 0.f, d.getCentreY(), false));
        g.fillPath (shape);
        g.setColour (ui::panelEdge());
        g.strokePath (shape, juce::PathStrokeType (1.f));
        ui::brackets (g, d.reduced (3.f), 9.f, ui::violet());

        auto inner = displayRect.reduced (14, 6);
        g.setColour (ui::ink());
        g.setFont (juce::Font (juce::FontOptions (23.f, juce::Font::bold)).withExtraKerningFactor (0.1f));
        g.drawFittedText (lastTitle.toUpperCase(), inner.removeFromTop (30), juce::Justification::centred, 1);
        g.setColour (ui::violet());
        g.setFont (uiFont (11.f, true, 0.2f));
        g.drawFittedText (lastInfo, inner.removeFromTop (16), juce::Justification::centred, 1);
        g.setColour (ui::mute());
        g.setFont (uiFont (9.f, false, 0.25f));
        g.drawFittedText ("RIGHT-CLICK : BANKS", inner, juce::Justification::centred, 1);
    }

    // panneaux : éclats de verre
    auto drawPanel = [&g] (juce::Rectangle<int> r, const juce::String& title)
    {
        if (r.isEmpty()) return;
        auto f = r.toFloat();
        auto shape = shardPanel (f, 14.f);

        // ombre bleutée
        juce::DropShadow (ui::accent().withAlpha (0.18f), 14, { 0, 2 }).drawForPath (g, shape);

        g.setGradientFill (juce::ColourGradient (ui::panel(), 0.f, f.getY(), ui::panelLow(), 0.f, f.getBottom(), false));
        g.fillPath (shape);
        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (shape);
            // fines diagonales en texture
            g.setColour (juce::Colours::white.withAlpha (0.012f));
            for (float x = f.getX() - f.getHeight(); x < f.getRight(); x += 7.f)
                g.drawLine (x, f.getBottom(), x + f.getHeight(), f.getY(), 1.f);

            if (title.isNotEmpty())
            {
                g.setGradientFill (juce::ColourGradient (ui::panelHead(), 0.f, f.getY(), ui::panel(), 0.f, f.getY() + 22.f, false));
                g.fillRect (f.withHeight (22.f));
                g.setColour (juce::Colours::white.withAlpha (0.06f));
                g.fillRect (f.getX(), f.getY(), f.getWidth(), 1.f);
                g.setGradientFill (ui::holo (f.getX() + 14.f, f.getX() + 160.f));
                g.fillRect (f.getX() + 14.f, f.getY() + 21.f, 130.f, 1.f);
                // marqueur losange
                juce::Path dmd;
                const float mx = f.getX() + 22.f, my = f.getY() + 11.f;
                dmd.addQuadrilateral (mx, my - 4.f, mx + 4.f, my, mx, my + 4.f, mx - 4.f, my);
                g.setColour (ui::violet());
                g.fillPath (dmd);
                g.setColour (ui::ink());
                g.setFont (juce::Font (juce::FontOptions (11.f, juce::Font::bold)).withExtraKerningFactor (0.26f));
                g.drawText (title, (int) f.getX() + 32, (int) f.getY(), 300, 22, juce::Justification::centredLeft);
            }
        }
        g.setColour (ui::panelEdge());
        g.strokePath (shape, juce::PathStrokeType (1.f));
        g.setColour (ui::violet().withAlpha (0.55f));
        g.fillRect (f.getRight() - 40.f, f.getY(), 40.f, 1.5f);
        g.fillRect (f.getRight() - 1.5f, f.getY(), 1.5f, 16.f);
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawLine (f.getRight() - 30.f, f.getBottom(), f.getRight() - 18.f, f.getBottom() - 14.f, 0.7f);
        g.drawLine (f.getRight() - 18.f, f.getBottom() - 14.f, f.getRight() - 6.f, f.getBottom() - 18.f, 0.7f);
    };

    if (page == 0)
    {
        drawPanel (pOsc, shownEngine > 0 ? "INSTRUMENT" : "OSCILLATORS");
        drawPanel (pFilter, "FILTER");
        drawPanel (pAmp, "ENV 1  /  AMP");
        drawPanel (pUni, "UNISON");
        drawPanel (pOut, "VOICE / OUTPUT");
    }
    else if (page == 1)
    {
        drawPanel (pEnv2, "ENV 2");
        drawPanel (pEnv3, "ENV 3");
        drawPanel (pLfo, "LFO");
        drawPanel (pMatrix, "MOD MATRIX");
    }
    else
    {
        for (auto& r : pFx) drawPanel (r, {});
    }

    g.setFont (juce::Font (juce::FontOptions (10.f, juce::Font::bold)).withExtraKerningFactor (0.12f));
    g.setColour (ui::mute());
    for (auto& k : knobs)
        if (k.slider->isVisible())
            g.drawText (k.name.toUpperCase(), k.labelRect, juce::Justification::centred);

    if (page == 0)
    {
        if (shownEngine <= 0)
        {
            g.drawText ("OSC 1", osc1Label, juce::Justification::centredLeft);
            g.drawText ("OSC 2", osc2Label, juce::Justification::centredLeft);
        }
        else
        {
            // nom de l'instrument en grand à droite de l'écran
            auto area = juce::Rectangle<int> (pOsc.getX() + 322, pOsc.getY() + 36, 268, 96);
            g.setColour (ui::ink());
            g.setFont (juce::Font (juce::FontOptions (24.f, juce::Font::bold)).withExtraKerningFactor (0.12f));
            g.drawFittedText (inst::engineNames()[shownEngine].toUpperCase(), area.removeFromTop (40), juce::Justification::centred, 1);
            g.setColour (ui::violet());
            g.setFont (juce::Font (juce::FontOptions (10.f, juce::Font::bold)).withExtraKerningFactor (0.3f));
            g.drawFittedText ("PHYSICAL MODEL", area.removeFromTop (18), juce::Justification::centred, 1);
            g.setColour (ui::mute());
            g.drawFittedText ("4 MACROS BELOW  -  FILTER / ENV / FX STILL ACTIVE", area.removeFromTop (30), juce::Justification::centred, 2);
            g.setColour (ui::mute());
        }
    }
    else if (page == 1)
    {
        g.drawText ("RATE", lfoRateLabel, juce::Justification::centred);
        for (int i = 0; i < lists::numSlots; ++i)
        {
            auto& row = modRows[(size_t) i];
            g.setColour (ui::lilac());
            g.drawText (juce::String (i + 1).paddedLeft ('0', 2), row.numRect, juce::Justification::centredLeft);
            g.setColour (ui::mute());
            g.drawText (">", row.numRect.translated (132, 0).withWidth (14), juce::Justification::centred);
        }
    }
}

void InfiniteEditor::resized()
{
    if (knobs.empty()) return;   // l'interface n'est pas encore construite

    buildShards();

    // ---- en-tête ----
    prevBtn.setBounds (216, 12, 28, 72);
    displayRect = juce::Rectangle<int> (250, 12, 412, 72);
    nextBtn.setBounds (668, 12, 28, 72);
    genBtn.setBounds (708, 12, 136, 34);
    saveBtn.setBounds (852, 12, 136, 34);
    favorites.setBounds (708, 50, 280, 34);
    for (size_t i = 0; i < catButtons.size(); ++i)
        catButtons[i].setBounds (14 + (int) i * 61, 96, 58, 28);
    for (size_t i = 0; i < pageTabs.size(); ++i)
        pageTabs[i].setBounds (696 + (int) i * 98, 96, 94, 28);

    auto layoutKnobs = [this] (const std::vector<size_t>& idx, juce::Rectangle<int> area)
    {
        if (idx.empty()) return;
        const int cw = area.getWidth() / (int) idx.size();
        for (size_t j = 0; j < idx.size(); ++j)
        {
            auto cell = juce::Rectangle<int> (area.getX() + (int) j * cw, area.getY(), cw, area.getHeight());
            auto& k = knobs[idx[j]];
            k.labelRect = cell.removeFromTop (16);
            k.slider->setBounds (cell.reduced (1, 0));
        }
    };

    // ---- page SYNTH ----
    pOsc    = { 12, 140, 600, 250 };
    pFilter = { 624, 140, 364, 250 };
    pAmp    = { 12, 402, 440, 244 };
    pUni    = { 464, 402, 240, 244 };
    pOut    = { 716, 402, 272, 244 };

    scope.setBounds (pOsc.getX() + 10, pOsc.getY() + 32, 300, 98);
    {
        const int x = pOsc.getX() + 322;
        osc1Label = { x, pOsc.getY() + 40, 50, 26 };
        w1.box->setBounds (x + 50, pOsc.getY() + 40, 104, 26);
        warp1.box->setBounds (x + 160, pOsc.getY() + 40, 108, 26);
        osc2Label = { x, pOsc.getY() + 84, 50, 26 };
        w2.box->setBounds (x + 50, pOsc.getY() + 84, 104, 26);
        warp2.box->setBounds (x + 160, pOsc.getY() + 84, 108, 26);
    }
    layoutKnobs (gOsc, { pOsc.getX() + 8, pOsc.getY() + 140, pOsc.getWidth() - 16, 102 });
    layoutKnobs (gInst, { pOsc.getX() + 40, pOsc.getY() + 140, pOsc.getWidth() - 80, 102 });
    engine.box->setBounds (pOsc.getRight() - 140, pOsc.getY() + 1, 134, 20);

    filterD.setBounds (pFilter.getX() + 10, pFilter.getY() + 32, 344, 98);
    filtType.box->setBounds (pFilter.getRight() - 104, pFilter.getY() + 1, 98, 20);
    layoutKnobs (gFilter, { pFilter.getX() + 10, pFilter.getY() + 140, 344, 102 });

    ampD.setBounds (pAmp.getX() + 10, pAmp.getY() + 32, 420, 94);
    layoutKnobs (gAmp, { pAmp.getX() + 10, pAmp.getY() + 134, 420, 102 });

    uniD.setBounds (pUni.getX() + 10, pUni.getY() + 32, 220, 94);
    layoutKnobs (gUni, { pUni.getX() + 10, pUni.getY() + 134, 220, 102 });

    driveD.setBounds (pOut.getX() + 10, pOut.getY() + 32, 252, 56);
    mono.btn->setBounds (pOut.getX() + 10, pOut.getY() + 96, 252, 28);
    layoutKnobs (gOut, { pOut.getX() + 10, pOut.getY() + 134, 252, 102 });

    // ---- page MOD ----
    pEnv2   = { 12, 140, 316, 250 };
    pEnv3   = { 340, 140, 316, 250 };
    pLfo    = { 668, 140, 320, 250 };
    pMatrix = { 12, 402, 976, 244 };

    env2D.setBounds (pEnv2.getX() + 10, pEnv2.getY() + 32, 296, 98);
    layoutKnobs (gEnv2, { pEnv2.getX() + 10, pEnv2.getY() + 140, 296, 102 });
    env3D.setBounds (pEnv3.getX() + 10, pEnv3.getY() + 32, 296, 98);
    layoutKnobs (gEnv3, { pEnv3.getX() + 10, pEnv3.getY() + 140, 296, 102 });

    for (size_t i = 0; i < lfoTabs.size(); ++i)
        lfoTabs[i].setBounds (pLfo.getRight() - 150 + (int) i * 36, pLfo.getY() + 1, 34, 20);
    lfoD.setBounds (pLfo.getX() + 10, pLfo.getY() + 32, 300, 84);
    lfoShape.setBounds (pLfo.getX() + 10, pLfo.getY() + 124, 140, 26);
    lfoSync.setBounds (pLfo.getX() + 158, pLfo.getY() + 124, 72, 26);
    lfoRetrig.setBounds (pLfo.getX() + 236, pLfo.getY() + 124, 74, 26);
    lfoRateLabel = { pLfo.getX() + 10, pLfo.getY() + 158, 110, 14 };
    lfoRate.setBounds (pLfo.getX() + 10, pLfo.getY() + 172, 110, 74);
    lfoDiv.setBounds (pLfo.getX() + 158, pLfo.getY() + 190, 152, 26);

    for (int i = 0; i < lists::numSlots; ++i)
    {
        auto& row = modRows[(size_t) i];
        const int col = i / 4, r = i % 4;
        const int x = pMatrix.getX() + 14 + col * 486;
        const int y = pMatrix.getY() + 36 + r * 50;
        row.numRect = { x, y, 24, 28 };
        row.src.box->setBounds (x + 26, y, 128, 28);
        row.dst.box->setBounds (x + 172, y, 140, 28);
        row.amt->setBounds (x + 320, y, 140, 28);
    }

    // ---- page FX : 5 colonnes x 2 rangées ----
    const int fw = 186, fh = 247, gap = 11;
    for (size_t i = 0; i < pFx.size(); ++i)
    {
        const int col = (int) (i % 5), row = (int) (i / 5);
        pFx[i] = { 13 + col * (fw + gap), 140 + row * (fh + 12), fw, fh };
        fxToggles[i].btn->setBounds (pFx[i].getX() + 10, pFx[i].getY() + 12, fw - 20, 28);

        auto area = juce::Rectangle<int> (pFx[i].getX() + 8, pFx[i].getY() + 48, fw - 16, fh - 56);
        const int cw = area.getWidth() / 2, ch = area.getHeight() / 2;
        for (size_t j = 0; j < gFx[i].size(); ++j)
        {
            auto cell = juce::Rectangle<int> (area.getX() + (int) (j % 2) * cw, area.getY() + (int) (j / 2) * ch, cw, ch);
            auto& k = knobs[gFx[i][j]];
            if (j >= 2) cell.removeFromTop (6);
            k.labelRect = cell.removeFromTop (16);
            k.slider->setBounds (cell.reduced (2, 0));
        }
    }
}
