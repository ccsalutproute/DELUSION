#include "PluginEditor.h"
#include <cmath>

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

    void fillAndStroke (juce::Graphics& g, const juce::Path& line, const juce::Path& fill,
                        juce::Rectangle<float> r, juce::Colour c)
    {
        g.setGradientFill (juce::ColourGradient (c.withAlpha (0.35f), 0.f, r.getY(),
                                                 c.withAlpha (0.02f), 0.f, r.getBottom(), false));
        g.fillPath (fill);
        g.setColour (c);
        g.strokePath (line, juce::PathStrokeType (2.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

// ------------------------------------------------------------------ style
DarkLnF::DarkLnF()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff25252d));
    setColour (juce::ComboBox::outlineColourId, ui::panelEdge());
    setColour (juce::ComboBox::textColourId, ui::ink());
    setColour (juce::ComboBox::arrowColourId, ui::mute());

    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1e1e25));
    setColour (juce::PopupMenu::textColourId, ui::ink());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::accent());
    setColour (juce::PopupMenu::highlightedTextColourId, ui::dark());

    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2a33));
    setColour (juce::TextButton::buttonOnColourId, ui::accent());
    setColour (juce::TextButton::textColourOffId, ui::ink());
    setColour (juce::TextButton::textColourOnId, ui::dark());

    setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffc9c5d0));
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, ui::accent().withAlpha (0.4f));
    setColour (juce::Label::textColourId, ui::ink());
}

void DarkLnF::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                float pos, float startAngle, float endAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (5.f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float arcR = radius - 2.f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // corps du potard
    const float bodyR = arcR - 7.f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2c36), centre.x, centre.y - bodyR,
                                             juce::Colour (0xff17171c), centre.x, centre.y + bodyR, false));
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.f, bodyR * 2.f);

    // rail et valeur
    juce::Path rail;
    rail.addCentredArc (centre.x, centre.y, arcR, arcR, 0.f, startAngle, endAngle, true);
    g.setColour (ui::track());
    g.strokePath (rail, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.f, startAngle, angle, true);
    g.setColour (ui::accent());
    g.strokePath (value, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // aiguille
    const juce::Point<float> dir (std::sin (angle), -std::cos (angle));
    g.setColour (ui::ink());
    g.drawLine (juce::Line<float> (centre + dir * (bodyR * 0.25f), centre + dir * (bodyR * 0.9f)), 2.2f);
}

void DarkLnF::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                    bool highlighted, bool down)
{
    auto b = button.getLocalBounds().toFloat().reduced (0.5f);
    auto c = backgroundColour;
    if (down)             c = c.brighter (0.15f);
    else if (highlighted) c = c.brighter (0.08f);

    g.setColour (c);
    g.fillRoundedRectangle (b, 7.f);
    g.setColour (ui::panelEdge());
    g.drawRoundedRectangle (b, 7.f, 1.f);
}

void DarkLnF::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    auto b = juce::Rectangle<float> (0.f, 0.f, (float) width, (float) height).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (b, 7.f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (b, 7.f, 1.f);

    const float cx = (float) width - 16.f, cy = (float) height * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (cx - 4.f, cy - 2.f, cx + 4.f, cy - 2.f, cx, cy + 3.f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

// ------------------------------------------------------------------ éditeur
InfiniteEditor::InfiniteEditor (InfiniteProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    // catégories (catSel 1 a 5)
    static const char* catNames[] = { "Lead", "Pad", "Pluck", "Keys", "All" };
    for (size_t i = 0; i < catButtons.size(); ++i)
    {
        auto& b = catButtons[i];
        b.setButtonText (catNames[i]);
        b.setClickingTogglesState (true);
        b.setRadioGroupId (101);
        b.onClick = [this, i]
        {
            if (catButtons[i].getToggleState())
                proc.loadSeed ((int) i + 1, 1);   // premier son de la banque
        };
        addAndMakeVisible (b);
    }
    catButtons[(size_t) (juce::jlimit (1, 5, currentCat()) - 1)].setToggleState (true, juce::dontSendNotification);

    // navigation dans la banque
    prevBtn.onClick = [this] { proc.loadSeed (currentCat(), juce::jmax (1, currentSeed() - 1)); };
    nextBtn.onClick = [this] { proc.loadSeed (currentCat(), currentSeed() + 1); };
    genBtn.onClick  = [this] { proc.loadSeed (currentCat(), InfiniteProcessor::kBankSize + 1
                                                            + juce::Random::getSystemRandom().nextInt (1000000)); };
    saveBtn.onClick = [this] { proc.saveFavorite(); refreshFavorites(); };
    genBtn.setColour (juce::TextButton::buttonColourId, ui::accent());
    genBtn.setColour (juce::TextButton::textColourOffId, ui::dark());
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

    // formes d'onde
    juce::StringArray waves { "Sine", "Saw", "Square", "Tri" };
    w1.addItemList (waves, 1);
    w2.addItemList (waves, 1);
    w1a = std::make_unique<ComboAtt> (proc.apvts, "osc1Wave", w1);
    w2a = std::make_unique<ComboAtt> (proc.apvts, "osc2Wave", w2);
    addAndMakeVisible (w1);
    addAndMakeVisible (w2);

    // écrans
    scope.painter   = [this] (juce::Graphics& g, juce::Rectangle<float> r) { paintScope (g, r); };
    filterD.painter = [this] (juce::Graphics& g, juce::Rectangle<float> r) { paintFilter (g, r); };
    envD.painter    = [this] (juce::Graphics& g, juce::Rectangle<float> r) { paintEnv (g, r); };
    lfoD.painter    = [this] (juce::Graphics& g, juce::Rectangle<float> r) { paintLfo (g, r); };
    driveD.painter  = [this] (juce::Graphics& g, juce::Rectangle<float> r) { paintDrive (g, r); };
    for (auto* d : { &scope, &filterD, &envD, &lfoD, &driveD }) addAndMakeVisible (d);

    // potards
    auto addKnob = [this] (const char* id, const char* name, std::vector<size_t>& group)
    {
        Knob k;
        k.name = name;
        k.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        k.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);

        juce::Slider::RotaryParameters rp;
        rp.startAngleRadians = juce::MathConstants<float>::pi * 1.2f;
        rp.endAngleRadians   = juce::MathConstants<float>::pi * 2.8f;
        rp.stopAtEnd = true;
        k.slider->setRotaryParameters (rp);

        k.att = std::make_unique<SliderAtt> (proc.apvts, id, *k.slider);
        addAndMakeVisible (*k.slider);
        group.push_back (knobs.size());
        knobs.push_back (std::move (k));
    };

    addKnob ("osc2Semi", "Osc2 Semi", gOsc);
    addKnob ("osc2Fine", "Osc2 Fine", gOsc);
    addKnob ("osc2Mix",  "Osc2 Level", gOsc);
    addKnob ("sub",      "Sub",       gOsc);
    addKnob ("noise",    "Noise",     gOsc);
    addKnob ("fmAmt",    "FM Amount", gOsc);
    addKnob ("fmRatio",  "FM Ratio",  gOsc);

    addKnob ("cutoff",   "Cutoff",    gFilter);
    addKnob ("reso",     "Resonance", gFilter);
    addKnob ("envAmt",   "Env Amount", gFilter);
    addKnob ("fdecay",   "Env Decay", gFilter);

    addKnob ("attack",   "Attack",    gAmp);
    addKnob ("decay",    "Decay",     gAmp);
    addKnob ("sustain",  "Sustain",   gAmp);
    addKnob ("release",  "Release",   gAmp);

    addKnob ("lfoRate",  "Rate",      gLfo);
    addKnob ("lfoDepth", "Depth",     gLfo);

    addKnob ("drive",    "Drive",     gOut);
    addKnob ("volume",   "Volume",    gOut);

    startTimerHz (15);
    timerCallback();
    setSize (1000, 660);   // en dernier : tous les composants existent déjà
}

InfiniteEditor::~InfiniteEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

int InfiniteEditor::currentSeed() const { return (int) proc.apvts.state.getProperty ("seed", 1); }
int InfiniteEditor::currentCat()  const { return (int) proc.apvts.state.getProperty ("catSel", 5); }
float InfiniteEditor::pv (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

void InfiniteEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && displayRect.contains (e.getPosition()))
        showBankMenu();
}

void InfiniteEditor::showBankMenu()
{
    // ids : cat * 1000 + seed pour les banques, 100000 + i pour les favoris
    juce::PopupMenu menu;
    menu.addSectionHeader ("DELUSION BANKS");

    const int curCat = currentCat(), curSeed = currentSeed();
    for (int cat = 1; cat <= 4; ++cat)
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

    const size_t c = (size_t) (juce::jlimit (1, 5, currentCat()) - 1);
    if (! catButtons[c].getToggleState())
        catButtons[c].setToggleState (true, juce::dontSendNotification);

    scope.repaint();
    filterD.repaint();
    envD.repaint();
    lfoD.repaint();
    driveD.repaint();
}

// ------------------------------------------------------------------ dessin des écrans
void InfiniteEditor::paintScope (juce::Graphics& g, juce::Rectangle<float> r)
{
    const int w1v = (int) pv ("osc1Wave"), w2v = (int) pv ("osc2Wave");
    const float mix = pv ("osc2Mix"), sub = pv ("sub"), fm = pv ("fmAmt"), ratio = pv ("fmRatio");
    const float detune = std::exp2 ((pv ("osc2Semi") + pv ("osc2Fine") / 100.f) / 12.f);
    const float k = 1.f + pv ("drive") * 5.f;

    const int n = 240;
    std::vector<float> ys ((size_t) n);
    float peak = 0.001f;

    for (int i = 0; i < n; ++i)
    {
        const float x = 2.f * (float) i / (float) (n - 1);   // deux cycles
        float p1 = x + 0.5f * fm * std::sin (kTwoPi * ratio * x);
        p1 -= std::floor (p1);
        float p2 = x * detune;
        p2 -= std::floor (p2);

        float s = waveAt (w1v, p1) + mix * waveAt (w2v, p2) + sub * std::sin (kTwoPi * x * 0.5f);
        s = std::tanh (s * k);
        ys[(size_t) i] = s;
        peak = std::max (peak, std::abs (s));
    }

    const float cy = r.getCentreY(), amp = r.getHeight() * 0.46f;
    g.setColour (ui::track());
    g.drawHorizontalLine ((int) cy, r.getX(), r.getRight());
    g.drawVerticalLine ((int) r.getCentreX(), r.getY(), r.getBottom());

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
    fillAndStroke (g, line, fill, r, ui::cyan());
}

void InfiniteEditor::paintFilter (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float fc = std::max (pv ("cutoff"), 20.f), q = std::max (pv ("reso"), 0.3f);
    const int n = 200;

    g.setColour (ui::track());
    for (int i = 1; i < 4; ++i)
        g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) i * 0.25f), r.getY(), r.getBottom());
    g.drawHorizontalLine ((int) (r.getBottom() - r.getHeight() * (30.f / 54.f)), r.getX(), r.getRight());

    juce::Path line, fill;
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) (n - 1);
        const float f = 20.f * std::pow (1000.f, t);          // 20 Hz à 20 kHz
        const float rr = f / fc;
        const float den = std::sqrt ((1.f - rr * rr) * (1.f - rr * rr) + (rr / q) * (rr / q));
        const float db = 20.f * std::log10 (1.f / std::max (den, 0.0001f));
        const float yn = juce::jlimit (0.f, 1.f, (db + 30.f) / 54.f);
        const float px = r.getX() + r.getWidth() * t;
        const float py = r.getBottom() - yn * r.getHeight();
        if (i == 0) { line.startNewSubPath (px, py); fill.startNewSubPath (px, r.getBottom()); fill.lineTo (px, py); }
        else        { line.lineTo (px, py); fill.lineTo (px, py); }
    }
    fill.lineTo (r.getRight(), r.getBottom());
    fill.closeSubPath();
    fillAndStroke (g, line, fill, r, ui::cyan());

    const float xc = r.getX() + r.getWidth() * (std::log (fc / 20.f) / std::log (1000.f));
    g.setColour (ui::accent().withAlpha (0.8f));
    g.drawVerticalLine ((int) juce::jlimit (r.getX(), r.getRight(), xc), r.getY(), r.getBottom());
}

void InfiniteEditor::paintEnv (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float a = std::max (pv ("attack"), 0.01f), d = std::max (pv ("decay"), 0.02f);
    const float s = juce::jlimit (0.f, 1.f, pv ("sustain")), rel = std::max (pv ("release"), 0.02f);
    const float hold = 0.3f * (a + d + rel);
    const float total = a + d + hold + rel;

    const float top = r.getY() + 3.f, bot = r.getBottom() - 1.f;
    const float xa = r.getX() + r.getWidth() * (a / total);
    const float xd = xa + r.getWidth() * (d / total);
    const float xh = xd + r.getWidth() * (hold / total);
    const float xr = r.getRight();
    const float sy = bot - s * (bot - top);

    juce::Path line, fill;
    line.startNewSubPath (r.getX(), bot);
    line.lineTo (xa, top);
    line.lineTo (xd, sy);
    line.lineTo (xh, sy);
    line.lineTo (xr, bot);

    fill = line;
    fill.lineTo (r.getX(), bot);
    fill.closeSubPath();
    fillAndStroke (g, line, fill, r, ui::cyan());

    g.setColour (ui::accent());
    for (auto p : { juce::Point<float> (xa, top), juce::Point<float> (xd, sy), juce::Point<float> (xh, sy) })
        g.fillEllipse (p.x - 3.f, p.y - 3.f, 6.f, 6.f);
}

void InfiniteEditor::paintLfo (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float cycles = 1.f + pv ("lfoRate") * 0.4f;
    const float amp = 0.12f + juce::jlimit (0.f, 3.f, pv ("lfoDepth")) / 3.f * 0.78f;
    const float cy = r.getCentreY();

    g.setColour (ui::track());
    g.drawHorizontalLine ((int) cy, r.getX(), r.getRight());

    const int n = 160;
    juce::Path line, fill;
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) (n - 1);
        const float px = r.getX() + r.getWidth() * t;
        const float py = cy - std::sin (kTwoPi * cycles * t) * amp * r.getHeight() * 0.5f;
        if (i == 0) { line.startNewSubPath (px, py); fill.startNewSubPath (px, cy); fill.lineTo (px, py); }
        else        { line.lineTo (px, py); fill.lineTo (px, py); }
    }
    fill.lineTo (r.getRight(), cy);
    fill.closeSubPath();
    fillAndStroke (g, line, fill, r, ui::cyan());
}

void InfiniteEditor::paintDrive (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float k = 1.f + pv ("drive") * 5.f;
    const float cy = r.getCentreY();

    g.setColour (ui::track());
    g.drawLine (r.getX(), r.getBottom(), r.getRight(), r.getY(), 1.f);   // sans saturation
    g.drawHorizontalLine ((int) cy, r.getX(), r.getRight());

    const int n = 120;
    juce::Path line;
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) (n - 1);
        const float x = t * 2.f - 1.f;
        const float px = r.getX() + r.getWidth() * t;
        const float py = cy - std::tanh (k * x) * r.getHeight() * 0.5f;
        if (i == 0) line.startNewSubPath (px, py);
        else        line.lineTo (px, py);
    }
    g.setColour (ui::accent());
    g.strokePath (line, juce::PathStrokeType (2.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ------------------------------------------------------------------ dessin général
void InfiniteEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (ui::bg1(), 0.f, 0.f, ui::bg2(), 0.f, (float) getHeight(), false));
    g.fillAll();

    // bandeau du haut
    g.setColour (ui::header());
    g.fillRect (0, 0, getWidth(), 132);
    g.setGradientFill (juce::ColourGradient (ui::accent(), 0.f, 0.f, ui::cyan(), (float) getWidth(), 0.f, false));
    g.fillRect (0, 131, getWidth(), 1);

    g.setColour (ui::ink());
    g.setFont (juce::Font (juce::FontOptions (26.f, juce::Font::bold)));
    g.drawText ("DELUSION", 16, 8, 180, 34, juce::Justification::centredLeft);
    g.setColour (ui::accent());
    g.setFont (juce::Font (juce::FontOptions (13.f)));
    g.drawText ("generative synth", 16, 42, 180, 18, juce::Justification::centredLeft);

    // grand écran du son
    if (! displayRect.isEmpty())
    {
        g.setColour (ui::screen());
        g.fillRoundedRectangle (displayRect.toFloat(), 10.f);
        g.setColour (ui::panelEdge());
        g.drawRoundedRectangle (displayRect.toFloat().reduced (0.5f), 10.f, 1.f);

        auto inner = displayRect.reduced (12, 6);
        g.setColour (ui::ink());
        g.setFont (juce::Font (juce::FontOptions (26.f, juce::Font::bold)));
        g.drawFittedText (lastTitle, inner.removeFromTop (30), juce::Justification::centred, 1);
        g.setColour (ui::accent());
        g.setFont (juce::Font (juce::FontOptions (14.f)));
        g.drawFittedText (lastInfo, inner.removeFromTop (16), juce::Justification::centred, 1);
        g.setColour (ui::mute());
        g.setFont (juce::Font (juce::FontOptions (10.f)));
        g.drawFittedText ("right-click for bank", inner, juce::Justification::centred, 1);
    }

    // panneaux
    auto drawPanel = [&g] (juce::Rectangle<int> r, const char* title)
    {
        if (r.isEmpty()) return;
        g.setColour (ui::panel());
        g.fillRoundedRectangle (r.toFloat(), 10.f);
        g.setColour (ui::panelEdge());
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 10.f, 1.f);
        g.setColour (ui::accent());
        g.fillRoundedRectangle ((float) r.getX() + 12.f, (float) r.getY() + 10.f, 3.f, 12.f, 1.5f);
        g.setColour (ui::ink());
        g.setFont (juce::Font (juce::FontOptions (13.f, juce::Font::bold)));
        g.drawText (title, r.getX() + 22, r.getY() + 6, 220, 20, juce::Justification::centredLeft);
    };
    drawPanel (pOsc, "OSCILLATORS");
    drawPanel (pFilter, "FILTER");
    drawPanel (pAmp, "AMP ENVELOPE");
    drawPanel (pLfo, "LFO");
    drawPanel (pOut, "OUTPUT");

    // noms des potards et des oscillateurs
    g.setFont (juce::Font (juce::FontOptions (12.f)));
    g.setColour (ui::mute());
    for (auto& k : knobs)
        g.drawText (k.name, k.labelRect, juce::Justification::centred);

    g.drawText ("Osc 1 shape", osc1Label, juce::Justification::centredLeft);
    g.drawText ("Osc 2 shape", osc2Label, juce::Justification::centredLeft);
}

void InfiniteEditor::resized()
{
    if (knobs.empty()) return;   // l'interface n'est pas encore construite

    // bandeau du haut
    prevBtn.setBounds (192, 12, 30, 72);
    displayRect = juce::Rectangle<int> (228, 12, 424, 72);
    nextBtn.setBounds (658, 12, 30, 72);
    genBtn.setBounds (704, 12, 130, 34);
    saveBtn.setBounds (842, 12, 146, 34);
    favorites.setBounds (704, 50, 284, 34);
    for (size_t i = 0; i < catButtons.size(); ++i)
        catButtons[i].setBounds (16 + (int) i * 118, 94, 110, 30);

    // panneaux
    pOsc    = juce::Rectangle<int> (12, 140, 600, 250);
    pFilter = juce::Rectangle<int> (624, 140, 364, 250);
    pAmp    = juce::Rectangle<int> (12, 402, 440, 244);
    pLfo    = juce::Rectangle<int> (464, 402, 240, 244);
    pOut    = juce::Rectangle<int> (716, 402, 272, 244);

    auto layoutKnobs = [this] (const std::vector<size_t>& idx, juce::Rectangle<int> area)
    {
        if (idx.empty()) return;
        const int cw = area.getWidth() / (int) idx.size();
        for (size_t j = 0; j < idx.size(); ++j)
        {
            auto cell = juce::Rectangle<int> (area.getX() + (int) j * cw, area.getY(), cw, area.getHeight());
            auto& k = knobs[idx[j]];
            k.labelRect = cell.removeFromTop (16);
            k.slider->setBounds (cell.reduced (2, 0));
        }
    };

    // oscillateurs
    scope.setBounds (pOsc.getX() + 10, pOsc.getY() + 30, 380, 100);
    osc1Label = juce::Rectangle<int> (pOsc.getX() + 404, pOsc.getY() + 30, 186, 16);
    w1.setBounds (pOsc.getX() + 404, pOsc.getY() + 48, 186, 26);
    osc2Label = juce::Rectangle<int> (pOsc.getX() + 404, pOsc.getY() + 80, 186, 16);
    w2.setBounds (pOsc.getX() + 404, pOsc.getY() + 98, 186, 26);
    layoutKnobs (gOsc, juce::Rectangle<int> (pOsc.getX() + 10, pOsc.getY() + 140, pOsc.getWidth() - 20, 102));

    // filtre
    filterD.setBounds (pFilter.getX() + 10, pFilter.getY() + 30, 344, 100);
    layoutKnobs (gFilter, juce::Rectangle<int> (pFilter.getX() + 10, pFilter.getY() + 140, 344, 102));

    // enveloppe
    envD.setBounds (pAmp.getX() + 10, pAmp.getY() + 30, 420, 96);
    layoutKnobs (gAmp, juce::Rectangle<int> (pAmp.getX() + 10, pAmp.getY() + 134, 420, 102));

    // lfo
    lfoD.setBounds (pLfo.getX() + 10, pLfo.getY() + 30, 220, 96);
    layoutKnobs (gLfo, juce::Rectangle<int> (pLfo.getX() + 10, pLfo.getY() + 134, 220, 102));

    // sortie
    driveD.setBounds (pOut.getX() + 10, pOut.getY() + 30, 252, 96);
    layoutKnobs (gOut, juce::Rectangle<int> (pOut.getX() + 10, pOut.getY() + 134, 252, 102));
}
