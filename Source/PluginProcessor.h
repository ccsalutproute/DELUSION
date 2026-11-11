#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <map>
#include <vector>
#include "Effects.h"

struct ParamInfo { juce::String id; juce::String name; float lo, hi, def, step, centre; };

// ------------------------------------------------------------------ listes de choix
namespace lists
{
    inline juce::StringArray waves()    { return { "Sine", "Saw", "Square", "Triangle" }; }
    inline juce::StringArray warps()    { return { "Off", "Fold", "Sync", "Bend", "PWM" }; }
    inline juce::StringArray filters()  { return { "LP 12", "LP 24", "HP 12", "HP 24", "Band" }; }
    inline juce::StringArray lfoShapes(){ return { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H", "Drift" }; }
    inline juce::StringArray divs()     { return { "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1 bar", "2 bars", "4 bars" }; }
    inline juce::StringArray modSrcs()  { return { "Off", "Env 2", "Env 3", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "Velocity", "Mod Wheel", "Note", "Random" }; }
    inline juce::StringArray modDsts()  { return { "Off", "Cutoff", "Resonance", "Pitch", "Osc1 Pitch", "Osc2 Pitch", "Osc2 Level",
                                                   "Sub", "Noise", "FM Amount", "Warp 1", "Warp 2", "Drive", "Amp", "Pan", "Uni Detune" }; }
    inline const float* divBeats()
    {
        static const float b[] = { 0.125f, 1.f / 6.f, 0.25f, 1.f / 3.f, 0.5f, 2.f / 3.f, 1.f, 2.f, 4.f, 8.f, 16.f };
        return b;
    }
    constexpr int numLfos = 4, numSlots = 8;
}

inline const std::vector<ParamInfo>& floatParams()
{
    static const std::vector<ParamInfo> p = [] {
        std::vector<ParamInfo> v = {
            { "osc2Semi",  "Osc2 Semi",        -24.f,  24.f,   0.f,   1.f,    0.f },
            { "osc2Fine",  "Osc2 Fine",        -50.f,  50.f,   7.f,   0.1f,   0.f },
            { "osc2Mix",   "Osc2 Level",         0.f,   1.f,   0.5f,  0.01f,  0.f },
            { "sub",       "Sub",                0.f,   1.f,   0.f,   0.01f,  0.f },
            { "noise",     "Noise",              0.f,   1.f,   0.f,   0.01f,  0.f },
            { "fmAmt",     "FM",                 0.f,   4.f,   0.f,   0.01f,  0.f },
            { "fmRatio",   "FM Ratio",           0.5f,  8.f,   1.f,   0.5f,   0.f },
            { "warp1Amt",  "Warp 1",             0.f,   1.f,   0.f,   0.01f,  0.f },
            { "warp2Amt",  "Warp 2",             0.f,   1.f,   0.f,   0.01f,  0.f },
            { "uniVoices", "Unison Voices",      1.f,   7.f,   1.f,   1.f,    0.f },
            { "uniDetune", "Unison Detune",      0.f,   1.f,   0.25f, 0.01f,  0.f },
            { "uniSpread", "Unison Spread",      0.f,   1.f,   0.7f,  0.01f,  0.f },
            { "cutoff",    "Cutoff",            40.f, 16000.f, 2000.f, 1.f, 1000.f },
            { "reso",      "Resonance",          0.5f,  8.f,   0.7f,  0.01f,  0.f },
            { "envAmt",    "Filter Env",        -4.f,   4.f,   1.5f,  0.01f,  0.f },
            { "keyTrack",  "Key Track",          0.f,   1.f,   0.3f,  0.01f,  0.f },
            { "attack",    "Attack",             0.001f, 2.f,  0.005f, 0.001f, 0.1f },
            { "decay",     "Decay",              0.01f, 3.f,   0.5f,  0.01f,  0.5f },
            { "sustain",   "Sustain",            0.f,   1.f,   0.7f,  0.01f,  0.f },
            { "release",   "Release",            0.01f, 4.f,   0.3f,  0.01f,  0.4f },
            { "e2a",       "Env2 Attack",        0.001f, 2.f,  0.001f, 0.001f, 0.1f },
            { "e2d",       "Env2 Decay",         0.01f, 3.f,   0.4f,  0.01f,  0.5f },
            { "e2s",       "Env2 Sustain",       0.f,   1.f,   0.f,   0.01f,  0.f },
            { "e2r",       "Env2 Release",       0.01f, 4.f,   0.3f,  0.01f,  0.4f },
            { "e3a",       "Env3 Attack",        0.001f, 2.f,  0.001f, 0.001f, 0.1f },
            { "e3d",       "Env3 Decay",         0.01f, 3.f,   0.3f,  0.01f,  0.5f },
            { "e3s",       "Env3 Sustain",       0.f,   1.f,   0.f,   0.01f,  0.f },
            { "e3r",       "Env3 Release",       0.01f, 4.f,   0.3f,  0.01f,  0.4f },
            { "drive",     "Drive",              0.f,   1.f,   0.2f,  0.01f,  0.f },
            { "volume",    "Volume",           -24.f,   6.f,  -6.f,   0.1f,   0.f },
            { "glide",     "Glide",              0.f,   1.f,   0.f,   0.001f, 0.12f },

            // effets
            { "distDrive", "Dist drive",         0.f,   1.f,   0.5f,  0.01f,  0.f },
            { "distTone",  "Dist tone",        500.f, 16000.f, 6000.f, 1.f, 3000.f },
            { "distMix",   "Dist mix",           0.f,   1.f,   1.f,   0.01f,  0.f },
            { "crushBits", "Crush bits",         2.f,  16.f,   8.f,   1.f,    0.f },
            { "crushRate", "Crush rate",         1.f,  32.f,   4.f,   1.f,    0.f },
            { "crushMix",  "Crush mix",          0.f,   1.f,   1.f,   0.01f,  0.f },
            { "eqLow",     "EQ low",           -12.f,  12.f,   0.f,   0.1f,   0.f },
            { "eqMid",     "EQ mid",           -12.f,  12.f,   0.f,   0.1f,   0.f },
            { "eqHigh",    "EQ high",          -12.f,  12.f,   0.f,   0.1f,   0.f },
            { "compThresh","Comp threshold",   -40.f,   0.f, -18.f,   0.1f,   0.f },
            { "compRatio", "Comp ratio",         1.f,  20.f,   4.f,   0.1f,   4.f },
            { "compGain",  "Comp gain",          0.f,  18.f,   4.f,   0.1f,   0.f },
            { "chorRate",  "Chorus rate",       0.05f,  5.f,   0.8f,  0.01f,  1.f },
            { "chorDepth", "Chorus depth",       0.f,   1.f,   0.4f,  0.01f,  0.f },
            { "chorMix",   "Chorus mix",         0.f,   1.f,   0.5f,  0.01f,  0.f },
            { "flanRate",  "Flanger rate",      0.05f,  5.f,   0.3f,  0.01f,  1.f },
            { "flanDepth", "Flanger depth",      0.f,   1.f,   0.5f,  0.01f,  0.f },
            { "flanFb",    "Flanger feedback",   0.f,   0.95f, 0.6f,  0.01f,  0.f },
            { "flanMix",   "Flanger mix",        0.f,   1.f,   0.5f,  0.01f,  0.f },
            { "phasRate",  "Phaser rate",       0.05f,  5.f,   0.4f,  0.01f,  1.f },
            { "phasDepth", "Phaser depth",       0.f,   1.f,   0.6f,  0.01f,  0.f },
            { "phasFb",    "Phaser feedback",   -0.9f,  0.9f,  0.4f,  0.01f,  0.f },
            { "phasMix",   "Phaser mix",         0.f,   1.f,   0.5f,  0.01f,  0.f },
            { "tremRate",  "Tremolo rate",      0.1f,  15.f,   4.f,   0.01f,  3.f },
            { "tremDepth", "Tremolo depth",      0.f,   1.f,   0.5f,  0.01f,  0.f },
            { "tremPan",   "Tremolo pan",        0.f,   1.f,   0.f,   0.01f,  0.f },
            { "dlyTime",   "Delay time",        20.f, 1500.f, 375.f,  1.f,  300.f },
            { "dlyFb",     "Delay feedback",     0.f,   0.95f, 0.4f,  0.01f,  0.f },
            { "dlyMix",    "Delay mix",          0.f,   1.f,   0.3f,  0.01f,  0.f },
            { "revSize",   "Reverb size",        0.f,   1.f,   0.6f,  0.01f,  0.f },
            { "revDamp",   "Reverb damping",     0.f,   1.f,   0.5f,  0.01f,  0.f },
            { "revWidth",  "Reverb width",       0.f,   1.f,   1.f,   0.01f,  0.f },
            { "revMix",    "Reverb mix",         0.f,   1.f,   0.3f,  0.01f,  0.f }
        };
        for (int i = 1; i <= lists::numLfos; ++i)
            v.push_back ({ "lfo" + juce::String (i) + "Rate", "LFO " + juce::String (i) + " Rate", 0.02f, 20.f, 1.f, 0.01f, 1.f });
        for (int i = 1; i <= lists::numSlots; ++i)
            v.push_back ({ "mod" + juce::String (i) + "Amt", "Mod " + juce::String (i) + " Amount", -1.f, 1.f, 0.f, 0.001f, 0.f });
        return v;
    }();
    return p;
}

struct ChoiceInfo { juce::String id; juce::String name; juce::StringArray items; int def; };

inline const std::vector<ChoiceInfo>& choiceParams()
{
    static const std::vector<ChoiceInfo> c = [] {
        std::vector<ChoiceInfo> v = {
            { "osc1Wave", "Osc1 Wave", lists::waves(), 1 },
            { "osc2Wave", "Osc2 Wave", lists::waves(), 1 },
            { "warp1Mode", "Warp 1 Mode", lists::warps(), 0 },
            { "warp2Mode", "Warp 2 Mode", lists::warps(), 0 },
            { "filtType", "Filter Type", lists::filters(), 0 }
        };
        for (int i = 1; i <= lists::numLfos; ++i)
        {
            v.push_back ({ "lfo" + juce::String (i) + "Shape", "LFO " + juce::String (i) + " Shape", lists::lfoShapes(), 0 });
            v.push_back ({ "lfo" + juce::String (i) + "Div", "LFO " + juce::String (i) + " Division", lists::divs(), 6 });
        }
        for (int i = 1; i <= lists::numSlots; ++i)
        {
            v.push_back ({ "mod" + juce::String (i) + "Src", "Mod " + juce::String (i) + " Source", lists::modSrcs(), 0 });
            v.push_back ({ "mod" + juce::String (i) + "Dst", "Mod " + juce::String (i) + " Target", lists::modDsts(), 0 });
        }
        return v;
    }();
    return c;
}

inline const std::vector<std::pair<juce::String, juce::String>>& boolParams()
{
    static const std::vector<std::pair<juce::String, juce::String>> b = [] {
        std::vector<std::pair<juce::String, juce::String>> v = {
            { "mono", "Mono" },
            { "distOn", "Distortion" }, { "crushOn", "Bitcrush" }, { "eqOn", "EQ" }, { "compOn", "Compressor" },
            { "chorOn", "Chorus" }, { "flanOn", "Flanger" }, { "phasOn", "Phaser" }, { "tremOn", "Tremolo" },
            { "dlyOn", "Delay" }, { "revOn", "Reverb" }
        };
        for (int i = 1; i <= lists::numLfos; ++i)
        {
            v.push_back ({ "lfo" + juce::String (i) + "Sync", "LFO " + juce::String (i) + " Sync" });
            v.push_back ({ "lfo" + juce::String (i) + "Retrig", "LFO " + juce::String (i) + " Retrig" });
        }
        return v;
    }();
    return b;
}

class InfiniteProcessor : public juce::AudioProcessor
{
public:
    InfiniteProcessor();
    ~InfiniteProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Delusion"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- Banque infinie + favoris (thread de l'interface uniquement) ----
    // catSel : 1 Lead, 2 Pad, 3 Pluck, 4 Keys, 5 Tout
    static constexpr int kBankSize = 64;
    static juce::String presetName (int cat, int seed) { return makeName (cat, seed); }
    static juce::String catName (int cat);
    void loadSeed (int catSel, int seed);
    juce::String getTitle() const;
    juce::String getInfo() const;
    void saveFavorite();
    void loadFavorite (int index);
    juce::StringArray getFavoriteNames() const;

    juce::AudioProcessorValueTreeState apvts;

private:
    struct Fav { juce::String name; std::map<juce::String, float> values; };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static std::map<juce::String, float> makePatch (int cat, int seed);
    static juce::String makeName (int cat, int seed);
    void applyValues (const std::map<juce::String, float>& values);
    std::map<juce::String, float> currentValues() const;
    juce::File favoritesFile() const;
    void loadFavoritesFromDisk();
    void writeFavoritesToDisk() const;

    // ---- Synthé ----
    static constexpr int kMaxUni = 7;

    struct LfoState { float phase = 0.f, held = 0.f, prev = 0.f, next = 0.f; };

    struct Voice
    {
        int note = -1, samples = 0, relCount = 0;
        bool gate = false;
        float vel = 0.f, rnd = 0.f, fade = 1.f;
        unsigned int age = 0;
        float curNote = 60.f, targetNote = 60.f;
        std::array<float, kMaxUni> ph1 {}, ph2 {};
        float phSub = 0.f, phMod = 0.f;
        std::array<LfoState, lists::numLfos> lfo {};
        juce::ADSR amp, env2, env3;
        juce::dsp::StateVariableTPTFilter<float> fA, fB;
    };

    struct Slot { int src = 0, dst = 0; float amt = 0.f; };

    struct Params
    {
        int w1 = 1, w2 = 1, warp1 = 0, warp2 = 0, filt = 0, uni = 1;
        float semi = 0, fine = 0, mix = 0.5f, sub = 0, noise = 0, fm = 0, fmRatio = 1;
        float warp1Amt = 0, warp2Amt = 0, uniDetune = 0.25f, uniSpread = 0.7f;
        float cutoff = 2000, reso = 0.7f, envAmt = 0, keyTrack = 0.3f;
        float attack = 0.005f, decay = 0.5f, sustain = 0.7f, release = 0.3f, drive = 0, volume = -6;
        bool mono = false;
        float glide = 0.f;
        std::array<int, lists::numLfos> lfoShape {};
        std::array<float, lists::numLfos> lfoHz {};
        std::array<bool, lists::numLfos> lfoRetrig {};
        std::array<Slot, lists::numSlots> slots {};
    } P;

    void readParams();
    void render (juce::AudioBuffer<float>& buffer, int start, int num);
    void startVoice (Voice& v, int note, float vel);
    void noteOn (int note, float vel);
    void noteOff (int note);
    float lfoValue (LfoState& s, int shape, float inc);

    std::array<Voice, 8> voices;
    std::vector<Fav> favs;
    juce::Random rng;
    double sr = 44100.0;
    unsigned int ageCounter = 0;

    std::array<float, 12> lastAmp {}, lastE2 {}, lastE3 {};
    float lastReso = -1.f;
    int lastFilt = -1;

    FxSettings fxs;
    FxChain fx;
    std::array<LfoState, lists::numLfos> freeLfo {};   // LFO libres (sans retrig)
    std::array<int, 128> held {};
    int heldCount = 0;
    float bend = 0.f, modWheel = 0.f;
    double bpm = 120.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InfiniteProcessor)
};
