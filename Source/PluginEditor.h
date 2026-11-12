#pragma once
#include "PluginProcessor.h"
#include <array>
#include <functional>
#include <vector>

// Palette : noir violacé, reflets holographiques (cyan -> violet -> magenta)
namespace ui
{
    inline juce::Colour bg1()       { return juce::Colour (0xff081530); }
    inline juce::Colour bg2()       { return juce::Colour (0xff020713); }
    inline juce::Colour header()    { return juce::Colour (0xff040b1b); }
    inline juce::Colour panel()     { return juce::Colour (0xff0b1a36); }
    inline juce::Colour panelLow()  { return juce::Colour (0xff060f22); }
    inline juce::Colour panelHead() { return juce::Colour (0xff10244a); }
    inline juce::Colour panelEdge() { return juce::Colour (0xff1d3a6e); }
    inline juce::Colour screen()    { return juce::Colour (0xff01050f); }
    inline juce::Colour grid()      { return juce::Colour (0xff0a1a36); }
    inline juce::Colour ink()       { return juce::Colour (0xfff3f8ff); }
    inline juce::Colour mute()      { return juce::Colour (0xff7d97c4); }
    inline juce::Colour accent()    { return juce::Colour (0xff2f8dff); }   // bleu electrique
    inline juce::Colour violet()    { return juce::Colour (0xff5fb8ff); }   // bleu ciel
    inline juce::Colour ice()       { return juce::Colour (0xffd6efff); }   // blanc glacé
    inline juce::Colour deep()      { return juce::Colour (0xff1446b8); }   // bleu profond
    inline juce::Colour track()     { return juce::Colour (0xff12264a); }
    inline juce::Colour dark()      { return juce::Colour (0xff030817); }
    inline juce::Colour lilac()     { return juce::Colour (0xffa58cff); }   // 3e couleur, utilisée avec parcimonie

    // dégradé blanc -> bleu ciel -> bleu électrique
    inline juce::ColourGradient holo (float x1, float x2, float y = 0.f, float alpha = 1.f)
    {
        juce::ColourGradient g (juce::Colours::white.withAlpha (alpha), x1, y, accent().withAlpha (alpha), x2, y, false);
        g.addColour (0.45, violet().withAlpha (alpha));
        return g;
    }

    // petits crochets de visée aux coins (style HUD)
    inline void brackets (juce::Graphics& g, juce::Rectangle<float> r, float len, juce::Colour c, float th = 1.2f)
    {
        g.setColour (c);
        g.fillRect (r.getX(), r.getY(), len, th);                 g.fillRect (r.getX(), r.getY(), th, len);
        g.fillRect (r.getRight() - len, r.getY(), len, th);       g.fillRect (r.getRight() - th, r.getY(), th, len);
        g.fillRect (r.getX(), r.getBottom() - th, len, th);       g.fillRect (r.getX(), r.getBottom() - len, th, len);
        g.fillRect (r.getRight() - len, r.getBottom() - th, len, th); g.fillRect (r.getRight() - th, r.getBottom() - len, th, len);
    }
}

// Style des potards, boutons, menus et barres
class DarkLnF : public juce::LookAndFeel_V4
{
public:
    DarkLnF();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

// Petit écran qui dessine une courbe en direct
class LiveDisplay : public juce::Component
{
public:
    std::function<void (juce::Graphics&, juce::Rectangle<float>)> painter;

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff04102a), b.getCentreX(), b.getCentreY(),
                                                 ui::screen(), b.getX(), b.getY(), true));
        g.fillRect (b);

        // grille fine + grille majeure
        g.setColour (ui::grid().withAlpha (0.7f));
        for (int i = 1; i < 16; ++i)
            g.drawVerticalLine ((int) (b.getWidth() * (float) i / 16.f), 0.f, b.getHeight());
        for (int i = 1; i < 8; ++i)
            g.drawHorizontalLine ((int) (b.getHeight() * (float) i / 8.f), 0.f, b.getWidth());
        g.setColour (juce::Colour (0xff12305e));
        for (int i = 1; i < 4; ++i)
            g.drawVerticalLine ((int) (b.getWidth() * (float) i / 4.f), 0.f, b.getHeight());
        g.drawHorizontalLine ((int) (b.getHeight() * 0.5f), 0.f, b.getWidth());

        if (painter) painter (g, b.reduced (6.f));

        // reflet de vitre
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f), 0.f, 0.f,
                                                 juce::Colours::transparentWhite, 0.f, b.getHeight() * 0.45f, false));
        g.fillRect (b.withHeight (b.getHeight() * 0.45f));

        g.setColour (ui::panelEdge());
        g.drawRect (b, 1.f);
        ui::brackets (g, b.reduced (2.f), 7.f, ui::violet().withAlpha (0.8f));
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

    void debugShowPage (int page) { showPage (page); }

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

    struct Combo
    {
        std::unique_ptr<juce::ComboBox> box;
        std::unique_ptr<ComboAtt> att;
    };

    struct Toggle
    {
        std::unique_ptr<juce::TextButton> btn;
        std::unique_ptr<ButtonAtt> att;
    };

    struct ModRow
    {
        Combo src, dst;
        std::unique_ptr<juce::Slider> amt;
        std::unique_ptr<SliderAtt> amtAtt;
        juce::Rectangle<int> numRect;
    };

    void timerCallback() override;
    void refreshFavorites();
    void showBankMenu();
    void showPage (int page);
    void updateEngineView();
    void paintOverChildren (juce::Graphics&) override;
    void bindLfo (int index);
    int  currentSeed() const;
    int  currentCat() const;
    float pv (const juce::String& id) const;

    size_t addKnob (const juce::String& id, const juce::String& name, std::vector<size_t>& group, int page);
    juce::ComboBox& addCombo (Combo& c, const juce::String& id, int page);
    juce::TextButton& addToggle (Toggle& t, const juce::String& id, const juce::String& text, int page);

    void paintLogo (juce::Graphics&, juce::Rectangle<float>);
    void paintEmblem (juce::Graphics&, juce::Rectangle<float>);
    void buildShards();
    void paintScope  (juce::Graphics&, juce::Rectangle<float>);
    void paintFilter (juce::Graphics&, juce::Rectangle<float>);
    void paintAdsr   (juce::Graphics&, juce::Rectangle<float>, const char* a, const char* d, const char* s, const char* r);
    void paintUnison (juce::Graphics&, juce::Rectangle<float>);
    void paintLfo    (juce::Graphics&, juce::Rectangle<float>);
    void paintDrive  (juce::Graphics&, juce::Rectangle<float>);

    InfiniteProcessor& proc;
    DarkLnF lnf;   // doit rester le premier objet d'interface : il est détruit en dernier

    std::array<std::vector<juce::Component*>, 3> pageComps;
    int page = 0;

    // en-tête
    std::array<juce::TextButton, 11> catButtons;     // 10 banques + All
    std::array<juce::TextButton, 3> pageTabs;        // SYNTH, MOD, FX
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, genBtn { "Generate" }, saveBtn { "Save" };
    juce::ComboBox favorites;

    // écrans
    LiveDisplay scope, filterD, ampD, uniD, driveD, env2D, env3D, lfoD;

    // potards
    std::vector<Knob> knobs;
    std::vector<size_t> gOsc, gFilter, gAmp, gUni, gOut, gEnv2, gEnv3, gInst;
    Combo engine;
    int shownEngine = -1;

    // listes et interrupteurs fixes
    Combo w1, w2, warp1, warp2, filtType;
    Toggle mono;

    // LFO (un seul panneau, 4 LFO)
    std::array<juce::TextButton, lists::numLfos> lfoTabs;
    int curLfo = 0;
    juce::ComboBox lfoShape, lfoDiv;
    juce::TextButton lfoSync { "Sync" }, lfoRetrig { "Retrig" };
    juce::Slider lfoRate;
    std::unique_ptr<ComboAtt> lfoShapeAtt, lfoDivAtt;
    std::unique_ptr<ButtonAtt> lfoSyncAtt, lfoRetrigAtt;
    std::unique_ptr<SliderAtt> lfoRateAtt;
    juce::Rectangle<int> lfoRateLabel;

    // matrice
    std::array<ModRow, lists::numSlots> modRows;

    // effets
    std::array<Toggle, 10> fxToggles;
    std::array<std::vector<size_t>, 10> gFx;
    std::array<juce::Rectangle<int>, 10> pFx;

    // zones
    juce::Rectangle<int> displayRect, pOsc, pFilter, pAmp, pUni, pOut, pEnv2, pEnv3, pLfo, pMatrix;
    juce::Rectangle<int> osc1Label, osc2Label;
    juce::String lastTitle, lastInfo;
    juce::Image shards;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InfiniteEditor)
};
