#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <map>
#include <vector>
#include "Effects.h"

struct ParamInfo { const char* id; const char* name; float lo, hi, def, step, centre; };

inline const std::vector<ParamInfo>& floatParams()
{
    static const std::vector<ParamInfo> p = {
        { "osc2Semi",  "Osc2 demi-tons",   -24.f,  24.f,   0.f,   1.f,    0.f },
        { "osc2Fine",  "Osc2 fin (cents)", -50.f,  50.f,   7.f,   0.1f,   0.f },
        { "osc2Mix",   "Osc2 niveau",        0.f,   1.f,   0.5f,  0.01f,  0.f },
        { "sub",       "Sub",                0.f,   1.f,   0.f,   0.01f,  0.f },
        { "noise",     "Bruit",              0.f,   1.f,   0.f,   0.01f,  0.f },
        { "fmAmt",     "FM",                 0.f,   4.f,   0.f,   0.01f,  0.f },
        { "fmRatio",   "FM ratio",           0.5f,  8.f,   1.f,   0.5f,   0.f },
        { "cutoff",    "Cutoff",            40.f, 16000.f, 2000.f, 1.f, 1000.f },
        { "reso",      "Resonance",          0.5f,  8.f,   0.7f,  0.01f,  0.f },
        { "envAmt",    "Env filtre",        -4.f,   4.f,   1.5f,  0.01f,  0.f },
        { "fdecay",    "Decay filtre",       0.05f, 2.f,   0.4f,  0.01f,  0.4f },
        { "lfoRate",   "LFO vitesse",        0.05f, 12.f,  1.f,   0.01f,  1.f },
        { "lfoDepth",  "LFO profondeur",     0.f,   3.f,   0.f,   0.01f,  0.f },
        { "attack",    "Attack",             0.001f, 2.f,  0.005f, 0.001f, 0.1f },
        { "decay",     "Decay",              0.01f, 3.f,   0.5f,  0.01f,  0.5f },
        { "sustain",   "Sustain",            0.f,   1.f,   0.7f,  0.01f,  0.f },
        { "release",   "Release",            0.01f, 4.f,   0.3f,  0.01f,  0.4f },
        { "drive",     "Saturation",         0.f,   1.f,   0.2f,  0.01f,  0.f },
        { "volume",    "Volume (dB)",      -24.f,   6.f,  -6.f,   0.1f,   0.f },
        { "glide",     "Glide",              0.f,   1.f,   0.f,   0.001f, 0.12f },

        // ---- effets ----
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
        { "dlyTime",   "Delay time (ms)",   20.f, 1500.f, 375.f,  1.f,  300.f },
        { "dlyFb",     "Delay feedback",     0.f,   0.95f, 0.4f,  0.01f,  0.f },
        { "dlyMix",    "Delay mix",          0.f,   1.f,   0.3f,  0.01f,  0.f },
        { "revSize",   "Reverb size",        0.f,   1.f,   0.6f,  0.01f,  0.f },
        { "revDamp",   "Reverb damping",     0.f,   1.f,   0.5f,  0.01f,  0.f },
        { "revWidth",  "Reverb width",       0.f,   1.f,   1.f,   0.01f,  0.f },
        { "revMix",    "Reverb mix",         0.f,   1.f,   0.3f,  0.01f,  0.f }
    };
    return p;
}

// Interrupteurs : mode mono + un par effet
inline const std::vector<std::pair<const char*, const char*>>& boolParams()
{
    static const std::vector<std::pair<const char*, const char*>> b = {
        { "mono", "Mono" },
        { "distOn", "Distortion" }, { "crushOn", "Bitcrush" }, { "eqOn", "EQ" }, { "compOn", "Compressor" },
        { "chorOn", "Chorus" }, { "flanOn", "Flanger" }, { "phasOn", "Phaser" }, { "tremOn", "Tremolo" },
        { "dlyOn", "Delay" }, { "revOn", "Reverb" }
    };
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
    static constexpr int kBankSize = 64;            // sons fixes par banque (seeds 1 a 64)
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
    struct Voice
    {
        int note = -1, samples = 0, relCount = 0;
        bool gate = false;
        float fade = 1.f;
        float curNote = 60.f, targetNote = 60.f;   // glide
        float vel = 0.f;
        unsigned int age = 0;
        float ph1 = 0.f, ph2 = 0.f, phSub = 0.f, phMod = 0.f;
        juce::ADSR amp, fenv;
        juce::dsp::StateVariableTPTFilter<float> filter;
    };

    struct Params
    {
        int w1 = 1, w2 = 1;
        float semi = 0, fine = 0, mix = 0.5f, sub = 0, noise = 0, fm = 0, fmRatio = 1;
        float cutoff = 2000, reso = 0.7f, envAmt = 0, fdecay = 0.4f, lfoRate = 1, lfoDepth = 0;
        float attack = 0.005f, decay = 0.5f, sustain = 0.7f, release = 0.3f, drive = 0, volume = -6;
        bool mono = false;
        float glide = 0.f;
    } P;

    FxSettings fxs;
    FxChain fx;
    std::array<int, 128> held {};   // notes tenues en mode mono
    int heldCount = 0;
    float bend = 0.f;               // pitch bend en demi-tons

    void readParams();
    void render (juce::AudioBuffer<float>& buffer, int start, int num);
    void noteOn (int note, float vel);
    void noteOff (int note);

    std::array<Voice, 8> voices;
    std::vector<Fav> favs;
    juce::Random rng;
    double sr = 44100.0;
    float lfoPhase = 0.f;
    unsigned int ageCounter = 0;
    juce::ADSR::Parameters lastAmp { -1.f, -1.f, -1.f, -1.f };
    float lastFDecay = -1.f, lastReso = -1.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InfiniteProcessor)
};
