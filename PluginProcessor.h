#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <map>
#include <vector>

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
        { "volume",    "Volume (dB)",      -24.f,   6.f,  -6.f,   0.1f,   0.f }
    };
    return p;
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
    // catSel : 0 Bass, 1 Lead, 2 Pad, 3 Pluck, 4 Keys, 5 Tout
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
        int note = -1, samples = 0;
        bool gate = false;
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
    } P;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InfiniteProcessor)
};
