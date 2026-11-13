#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <map>
#include <memory>
#include <vector>

// ======================================================================
//  Banque de vrais enregistrements (piano, cuivres, cordes, vents, guitares)
//  - les fichiers sont dans Delusion.vst3/Contents/Resources/Samples
//  - chaque instrument est chargé en mémoire seulement quand on l'utilise
//  - partagée entre toutes les instances du plugin
// ======================================================================

struct SampleZone
{
    int lokey = 0, hikey = 127, key = 60, lovel = 0, hivel = 127;
    float tune = 0.f;
    int loopStart = -1, loopEnd = -1, len = 0;
    double sr = 44100.0;
    float keyFreq = 261.6f;            // fréquence de la note enregistrée
    std::vector<int16_t> data;         // stéréo entrelacé
    juce::String file;
};

struct SampleInstrument
{
    juce::String id;
    bool loop = false;
    std::vector<SampleZone> zones;
    std::atomic<bool> loaded { false }, requested { false };
    std::atomic<unsigned int> rr { 0 };

    // zones qui correspondent à la note et à la vélocité (plusieurs = round robin)
    const SampleZone* pick (int note, int vel127)
    {
        const SampleZone* found[8];
        int n = 0;
        for (auto& z : zones)
            if (note >= z.lokey && note <= z.hikey && vel127 >= z.lovel && vel127 <= z.hivel && n < 8)
                found[n++] = &z;
        if (n == 0)   // vélocité hors des couches : on prend la plus proche
            for (auto& z : zones)
                if (note >= z.lokey && note <= z.hikey && n < 8) found[n++] = &z;
        if (n == 0) return nullptr;
        return found[(rr++) % (unsigned int) n];
    }
};

class SampleLibrary : private juce::Thread
{
public:
    SampleLibrary() : juce::Thread ("Delusion samples")
    {
        formats.registerBasicFormats();
        root = findRoot();
        auto manifest = root.getChildFile ("manifest.json");
        if (manifest.existsAsFile())
        {
            auto json = juce::JSON::parse (manifest);
            if (auto* arr = json["instruments"].getArray())
                for (auto& iv : *arr)
                {
                    auto inst = std::make_unique<SampleInstrument>();
                    inst->id = iv["id"].toString();
                    inst->loop = (bool) iv["loop"];
                    if (auto* rs = iv["regions"].getArray())
                        for (auto& r : *rs)
                        {
                            SampleZone z;
                            z.file = r["file"].toString();
                            z.lokey = (int) r["lokey"];  z.hikey = (int) r["hikey"];  z.key = (int) r["key"];
                            z.lovel = (int) r["lovel"];  z.hivel = (int) r["hivel"];
                            z.tune = (float) (double) r["tune"];
                            z.loopStart = (int) r["loopStart"];  z.loopEnd = (int) r["loopEnd"];
                            z.keyFreq = 440.f * std::exp2 (((float) z.key + z.tune / 100.f - 69.f) / 12.f);
                            inst->zones.push_back (std::move (z));
                        }
                    byId[inst->id] = inst.get();
                    instruments.push_back (std::move (inst));
                }
        }
        startThread (juce::Thread::Priority::low);
    }

    ~SampleLibrary() override
    {
        signalThreadShouldExit();
        notify();
        stopThread (4000);
    }

    bool available() const { return ! instruments.empty(); }

    // appelé depuis n'importe quel thread : renvoie l'instrument s'il est prêt, sinon demande son chargement
    SampleInstrument* get (const juce::String& id)
    {
        auto it = byId.find (id);
        if (it == byId.end()) return nullptr;
        auto* inst = it->second;
        if (inst->loaded.load (std::memory_order_acquire)) return inst;
        if (! inst->requested.exchange (true)) notify();
        return nullptr;
    }

    void request (const juce::String& id) { get (id); }

private:
    static juce::File findRoot()
    {
        if (auto env = juce::SystemStats::getEnvironmentVariable ("DELUSION_SAMPLES", {}); env.isNotEmpty())
            return juce::File (env);
        // Delusion.vst3/Contents/MacOS/Delusion  ->  Delusion.vst3/Contents/Resources/Samples
        auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
        for (auto d = exe.getParentDirectory(); d.exists() && ! d.isRoot(); d = d.getParentDirectory())
        {
            auto s = d.getChildFile ("Resources/Samples");
            if (s.getChildFile ("manifest.json").existsAsFile()) return s;
            if (d.getFileName() == "Contents") break;
        }
        return exe.getParentDirectory().getParentDirectory().getChildFile ("Resources/Samples");
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            for (auto& inst : instruments)
            {
                if (threadShouldExit()) return;
                if (inst->requested.load() && ! inst->loaded.load())
                    load (*inst);
            }
            wait (-1);
        }
    }

    void load (SampleInstrument& inst)
    {
        for (auto& z : inst.zones)
        {
            if (threadShouldExit()) return;
            std::unique_ptr<juce::AudioFormatReader> r (formats.createReaderFor (root.getChildFile (z.file)));
            if (r == nullptr) continue;
            const int n = (int) r->lengthInSamples;
            juce::AudioBuffer<float> tmp (2, juce::jmax (1, n));
            r->read (&tmp, 0, n, 0, true, r->numChannels > 1);
            if (r->numChannels == 1) tmp.copyFrom (1, 0, tmp, 0, 0, n);
            z.sr = r->sampleRate;
            z.len = n;
            z.data.resize ((size_t) n * 2 + 8, 0);   // marge pour l'interpolation
            const float* L = tmp.getReadPointer (0);
            const float* R = tmp.getReadPointer (1);
            for (int i = 0; i < n; ++i)
            {
                z.data[(size_t) i * 2]     = (int16_t) juce::jlimit (-32767.f, 32767.f, L[i] * 32767.f);
                z.data[(size_t) i * 2 + 1] = (int16_t) juce::jlimit (-32767.f, 32767.f, R[i] * 32767.f);
            }
            if (z.loopEnd > n) z.loopEnd = n;
        }
        inst.loaded.store (true, std::memory_order_release);
    }

    juce::AudioFormatManager formats;
    juce::File root;
    std::vector<std::unique_ptr<SampleInstrument>> instruments;
    std::map<juce::String, SampleInstrument*> byId;
};
