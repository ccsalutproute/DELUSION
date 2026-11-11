#pragma once
#include "PluginProcessor.h"
#include <array>
#include <functional>
#include <vector>

// Palette de l'interface : fond sombre, accent orange, affichages cyan
namespace ui
{
    inline juce::Colour bg1()       { return juce::Colour (0xff1b1b21); }
    inline juce::Colour bg2()       { return juce::Colour (0xff111115); }
    inline juce::Colour header()    { return juce::Colour (0xff15151a); }
    inline juce::Colour panel()     { return juce::Colour (0xff1e1e25); }
    inline juce::Colour panelEdge() { return juce::Colour (0xff2e2e38); }
    inline juce::Colour screen()    { return juce::Colour (0xff0c0c10); }
    inline juce::Colour ink()       { return juce::Colour (0xfff1eee9); }
    inline juce::Colour mute()      { return juce::Colour (0xff8b8896); }
    inline juce::Colour accent()    { return juce::Colour (0xffff8a3d); }
    inline juce::Colour cyan()      { return juce::Colour (0xff55d3ff); }
    inline juce::Colour track()     { return juce::Colour (0xff33333e); }
    inline juce::Colour dark()      { return juce::Colour (0xff16161a); }
}

// Style des potards, boutons et menus
class DarkLnF : public juce::LookAndFeel_V4
{
public:
    DarkLnF();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
};

// Petit écran qui dessine une courbe en direct
class LiveDisplay : public juce::Component
{
public:
    std::function<void (juce::Graphics&, juce::Rectangle<float>)> painter;

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (ui::screen());
        g.fillRoundedRectangle (b, 7.f);
        if (painter) painter (g, b.reduced (8.f));
        g.setColour (ui::panelEdge());
        g.drawRoundedRectangle (b.reduced (0.5f), 7.f, 1.f);
    }
};

class InfiniteEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit InfiniteEditor (InfiniteProcessor&);
    ~InfiniteEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<SliderAtt> att;
        juce::String name;
        juce::Rectangle<int> labelRect;
    };

    void timerCallback() override;
    void refreshFavorites();
    void showBankMenu();
    void showPage (bool fx);
    int  currentSeed() const;
    int  currentCat() const;
    float pv (const char* id) const;

    void paintScope  (juce::Graphics&, juce::Rectangle<float>);
    void paintFilter (juce::Graphics&, juce::Rectangle<float>);
    void paintEnv    (juce::Graphics&, juce::Rectangle<float>);
    void paintLfo    (juce::Graphics&, juce::Rectangle<float>);
    void paintDrive  (juce::Graphics&, juce::Rectangle<float>);

    InfiniteProcessor& proc;
    DarkLnF lnf;   // doit rester le premier objet d'interface : il est détruit en dernier

    LiveDisplay scope, filterD, envD, lfoD, driveD;
    juce::ComboBox favorites, w1, w2;
    std::array<juce::TextButton, 5> catButtons;   // Lead, Pad, Pluck, Keys, All
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, genBtn { "Generate" }, saveBtn { "Save favorite" };

    std::vector<Knob> knobs;
    std::vector<size_t> gOsc, gFilter, gAmp, gLfo, gOut;
    std::unique_ptr<ComboAtt> w1a, w2a;

    // mono / glide
    juce::TextButton monoBtn { "MONO / SLIDE" };
    std::unique_ptr<ButtonAtt> monoAtt;

    // page des effets
    juce::TextButton synthTab { "SYNTH" }, fxTab { "FX" };
    bool fxPage = false;
    std::array<juce::TextButton, 10> fxBtns;
    std::array<std::unique_ptr<ButtonAtt>, 10> fxAtts;
    std::array<std::vector<size_t>, 10> gFx;
    std::array<juce::Rectangle<int>, 10> pFx;

    juce::Rectangle<int> displayRect, pOsc, pFilter, pAmp, pLfo, pOut, osc1Label, osc2Label;
    juce::String lastTitle, lastInfo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InfiniteEditor)
};
