#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

using APF = juce::AudioParameterFloat;
using APC = juce::AudioParameterChoice;
using PID = juce::ParameterID;

static const char* kCats[] = { "Bass", "Lead", "Pad", "Pluck", "Keys" };

juce::String InfiniteProcessor::catName (int cat) { return kCats[juce::jlimit (1, 4, cat)]; }

// ------------------------------------------------------------------ oscillateurs
static float polyBlep (float t, float dt)
{
    if (t < dt)         { t /= dt;           return t + t - t * t - 1.f; }
    if (t > 1.f - dt)   { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

static float oscWave (int wave, float ph, float dt)
{
    switch (wave)
    {
        case 0:  return std::sin (juce::MathConstants<float>::twoPi * ph);
        case 1:  { float v = 2.f * ph - 1.f; v -= polyBlep (ph, dt); return v; }
        case 2:  { float v = ph < 0.5f ? 1.f : -1.f;
                   v += polyBlep (ph, dt);
                   float p2 = ph + 0.5f; if (p2 >= 1.f) p2 -= 1.f;
                   v -= polyBlep (p2, dt); return v; }
        default: return 4.f * std::abs (ph - 0.5f) - 1.f;
    }
}

// ------------------------------------------------------------------ construction
InfiniteProcessor::InfiniteProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    loadFavoritesFromDisk();
    loadSeed (1, 1);   // Lead bank 01 des l'ouverture
}

juce::AudioProcessorValueTreeState::ParameterLayout InfiniteProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    juce::StringArray waves { "Sine", "Saw", "Square", "Tri" };

    p.push_back (std::make_unique<APC> (PID { "osc1Wave", 1 }, "Osc1 forme", waves, 1));
    p.push_back (std::make_unique<APC> (PID { "osc2Wave", 1 }, "Osc2 forme", waves, 1));

    for (auto& f : floatParams())
    {
        juce::NormalisableRange<float> range (f.lo, f.hi, f.step);
        if (f.centre > 0.f) range.setSkewForCentre (f.centre);
        p.push_back (std::make_unique<APF> (PID { f.id, 1 }, f.name, range, f.def));
    }
    return { p.begin(), p.end() };
}

// ------------------------------------------------------------------ générateur
juce::String InfiniteProcessor::makeName (int cat, int seed)
{
    static const char* adj[]  = { "Velvet","Neon","Hollow","Glass","Rusty","Lunar","Frozen","Electric",
                                  "Silent","Burning","Golden","Dusty","Cosmic","Wild","Sleepy","Toxic" };
    static const char* noun[] = { "Comet","Ritual","Tide","Engine","Garden","Signal","Ghost","Temple",
                                  "Storm","Echo","Mirror","Harbor","Oracle","Circuit","Bloom","Shadow" };
    // chaque seed de 0 a 255 donne un nom unique dans sa banque
    const int s = juce::jmax (0, seed);
    const int a = (s + cat * 3) % 16;
    const int n = ((s / 16) + 5 * (s % 16) + cat * 7) % 16;
    return juce::String (adj[a]) + " " + noun[n];
}

std::map<juce::String, float> InfiniteProcessor::makePatch (int cat, int seed)
{
    juce::Random r ((juce::int64) seed * 7919 + cat * 104729 + 17);
    for (int i = 0; i < 4; ++i) r.nextInt();

    auto rf     = [&] (float a, float b) { return a + (b - a) * r.nextFloat(); };
    auto rl     = [&] (float a, float b) { return a * std::pow (b / a, r.nextFloat()); };
    auto pick   = [&] (std::initializer_list<float> l) { return *(l.begin() + r.nextInt ((int) l.size())); };
    auto chance = [&] (float p) { return r.nextFloat() < p; };

    std::map<juce::String, float> m;
    for (auto& f : floatParams()) m[f.id] = f.def;
    m["osc1Wave"] = 1.f; m["osc2Wave"] = 1.f;
    m["volume"] = -6.f;

    switch (cat)
    {
        case 0: // Bass
            m["osc1Wave"] = pick ({ 0, 1, 2 });      m["osc2Wave"] = pick ({ 1, 2 });
            m["osc2Semi"] = pick ({ -12, 0, 0 });    m["osc2Fine"] = rf (-12, 12);
            m["osc2Mix"]  = rf (0.2f, 0.6f);         m["sub"] = rf (0.3f, 0.9f);
            m["noise"]    = 0.f;
            m["fmAmt"]    = chance (0.4f) ? rf (0.1f, 1.2f) : 0.f;  m["fmRatio"] = pick ({ 1, 2 });
            m["cutoff"]   = rl (120, 900);           m["reso"] = rf (0.7f, 3.f);
            m["envAmt"]   = rf (1.f, 3.f);           m["fdecay"] = rf (0.1f, 0.5f);
            m["lfoDepth"] = 0.f;
            m["attack"]   = rf (0.002f, 0.01f);      m["decay"] = rf (0.2f, 1.f);
            m["sustain"]  = rf (0.4f, 0.9f);         m["release"] = rf (0.05f, 0.25f);
            m["drive"]    = rf (0.1f, 0.6f);
            break;
        case 1: // Lead
            m["osc1Wave"] = pick ({ 1, 2 });         m["osc2Wave"] = pick ({ 1, 2, 3 });
            m["osc2Semi"] = pick ({ 0, 0, 12, 7 });  m["osc2Fine"] = rf (-20, 20);
            m["osc2Mix"]  = rf (0.3f, 0.8f);         m["sub"] = 0.f;
            m["noise"]    = chance (0.3f) ? rf (0.f, 0.05f) : 0.f;
            m["fmAmt"]    = chance (0.5f) ? rf (0.1f, 1.5f) : 0.f;  m["fmRatio"] = pick ({ 1, 2, 3 });
            m["cutoff"]   = rl (1500, 9000);         m["reso"] = rf (0.7f, 4.f);
            m["envAmt"]   = rf (0.f, 2.f);           m["fdecay"] = rf (0.2f, 1.f);
            m["lfoRate"]  = rf (4.f, 7.f);           m["lfoDepth"] = rf (0.f, 0.3f);
            m["attack"]   = rf (0.002f, 0.05f);      m["decay"] = rf (0.2f, 1.f);
            m["sustain"]  = rf (0.5f, 1.f);          m["release"] = rf (0.15f, 0.6f);
            m["drive"]    = rf (0.f, 0.4f);
            break;
        case 2: // Pad
            m["osc1Wave"] = pick ({ 1, 3, 0 });      m["osc2Wave"] = pick ({ 1, 3 });
            m["osc2Semi"] = pick ({ 0, 7, 12, -12 }); m["osc2Fine"] = rf (-30, 30);
            m["osc2Mix"]  = rf (0.5f, 1.f);          m["sub"] = rf (0.f, 0.3f);
            m["noise"]    = rf (0.f, 0.1f);
            m["fmAmt"]    = chance (0.4f) ? rf (0.1f, 0.6f) : 0.f;  m["fmRatio"] = pick ({ 0.5f, 1, 2 });
            m["cutoff"]   = rl (400, 4000);          m["reso"] = rf (0.7f, 2.5f);
            m["envAmt"]   = rf (-1.f, 2.f);          m["fdecay"] = rf (0.8f, 2.f);
            m["lfoRate"]  = rl (0.1f, 1.5f);         m["lfoDepth"] = rf (0.2f, 1.5f);
            m["attack"]   = rf (0.3f, 1.5f);         m["decay"] = rf (0.5f, 2.f);
            m["sustain"]  = rf (0.6f, 1.f);          m["release"] = rf (1.f, 3.5f);
            m["drive"]    = rf (0.f, 0.2f);
            break;
        case 3: // Pluck
            m["osc1Wave"] = pick ({ 1, 2, 3 });      m["osc2Wave"] = pick ({ 1, 2, 3 });
            m["osc2Semi"] = pick ({ 0, 12, 7, -12 }); m["osc2Fine"] = rf (-15, 15);
            m["osc2Mix"]  = rf (0.2f, 0.7f);         m["sub"] = rf (0.f, 0.3f);
            m["fmAmt"]    = rf (0.f, 2.f);           m["fmRatio"] = 0.5f * (float) (1 + r.nextInt (16));
            m["cutoff"]   = rl (500, 5000);          m["reso"] = rf (0.7f, 3.f);
            m["envAmt"]   = rf (1.5f, 3.5f);         m["fdecay"] = rf (0.08f, 0.4f);
            m["attack"]   = 0.001f;                  m["decay"] = rf (0.15f, 0.6f);
            m["sustain"]  = rf (0.f, 0.1f);          m["release"] = rf (0.1f, 0.5f);
            m["drive"]    = rf (0.f, 0.4f);
            break;
        default: // Keys / cloches
            m["osc1Wave"] = pick ({ 0, 3 });         m["osc2Wave"] = 0.f;
            m["osc2Semi"] = pick ({ 0, 12, 19, 24 }); m["osc2Fine"] = rf (-8, 8);
            m["osc2Mix"]  = rf (0.2f, 0.6f);
            m["fmAmt"]    = rf (0.5f, 3.5f);         m["fmRatio"] = pick ({ 1, 2, 3, 3.5f, 4, 5, 7 });
            m["cutoff"]   = rl (2000, 12000);        m["reso"] = 0.7f;
            m["envAmt"]   = rf (0.f, 1.5f);          m["fdecay"] = rf (0.3f, 1.5f);
            m["lfoDepth"] = 0.f;
            m["attack"]   = rf (0.001f, 0.01f);      m["decay"] = rf (0.5f, 2.5f);
            m["sustain"]  = rf (0.f, 0.3f);          m["release"] = rf (0.4f, 2.f);
            m["drive"]    = rf (0.f, 0.15f);
            break;
    }
    return m;
}

void InfiniteProcessor::applyValues (const std::map<juce::String, float>& values)
{
    for (auto& kv : values)
        if (auto* p = apvts.getParameter (kv.first))
            p->setValueNotifyingHost (p->convertTo0to1 (kv.second));
}

std::map<juce::String, float> InfiniteProcessor::currentValues() const
{
    std::map<juce::String, float> m;
    for (const char* id : { "osc1Wave", "osc2Wave" })
        if (auto* p = apvts.getParameter (id)) m[id] = p->convertFrom0to1 (p->getValue());
    for (auto& f : floatParams())
        if (auto* p = apvts.getParameter (f.id)) m[f.id] = p->convertFrom0to1 (p->getValue());
    return m;
}

void InfiniteProcessor::loadSeed (int catSel, int seed)
{
    seed = juce::jmax (1, seed);
    catSel = juce::jlimit (1, 5, catSel);   // plus de banque Bass
    int cat = catSel;
    if (cat == 5)
    {
        juce::Random r ((juce::int64) seed * 977 + 3);
        r.nextInt();
        cat = 1 + r.nextInt (4);
    }
    applyValues (makePatch (cat, seed));
    apvts.state.setProperty ("seed", seed, nullptr);
    apvts.state.setProperty ("catSel", catSel, nullptr);
    apvts.state.setProperty ("presetName", makeName (cat, seed), nullptr);
    const juce::String info = (catSel != 5 && seed <= kBankSize)
        ? juce::String (kCats[cat]).toUpperCase() + " BANK   " + juce::String (seed).paddedLeft ('0', 2) + " / " + juce::String (kBankSize)
        : juce::String (kCats[cat]).toUpperCase() + "   generated #" + juce::String (seed);
    apvts.state.setProperty ("presetInfo", info, nullptr);
}

juce::String InfiniteProcessor::getTitle() const { return apvts.state.getProperty ("presetName", "Init").toString(); }
juce::String InfiniteProcessor::getInfo()  const { return apvts.state.getProperty ("presetInfo", "").toString(); }

// ------------------------------------------------------------------ favoris
juce::File InfiniteProcessor::favoritesFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("MonStudio").getChildFile ("Delusion").getChildFile ("favorites.xml");
}

void InfiniteProcessor::loadFavoritesFromDisk()
{
    favs.clear();
    auto f = favoritesFile();
    if (! f.existsAsFile()) return;
    if (auto xml = juce::XmlDocument::parse (f))
        for (auto* e : xml->getChildIterator())
        {
            Fav fav;
            fav.name = e->getStringAttribute ("name");
            for (const char* id : { "osc1Wave", "osc2Wave" })
                fav.values[id] = (float) e->getDoubleAttribute (id, 1.0);
            for (auto& pi : floatParams())
                fav.values[pi.id] = (float) e->getDoubleAttribute (pi.id, pi.def);
            favs.push_back (std::move (fav));
        }
}

void InfiniteProcessor::writeFavoritesToDisk() const
{
    auto f = favoritesFile();
    f.getParentDirectory().createDirectory();
    juce::XmlElement root ("FAVS");
    for (auto& fav : favs)
    {
        auto* e = root.createNewChildElement ("P");
        e->setAttribute ("name", fav.name);
        for (auto& kv : fav.values) e->setAttribute (kv.first, (double) kv.second);
    }
    root.writeTo (f);
}

void InfiniteProcessor::saveFavorite()
{
    Fav fav;
    fav.name = getTitle() + "  (" + getInfo() + ")";
    fav.values = currentValues();
    favs.push_back (std::move (fav));
    writeFavoritesToDisk();
}

void InfiniteProcessor::loadFavorite (int index)
{
    if (index < 0 || index >= (int) favs.size()) return;
    applyValues (favs[(size_t) index].values);
    auto name = favs[(size_t) index].name;
    apvts.state.setProperty ("presetName", name.upToFirstOccurrenceOf ("  (", false, false), nullptr);
    apvts.state.setProperty ("presetInfo", "Favori", nullptr);
}

juce::StringArray InfiniteProcessor::getFavoriteNames() const
{
    juce::StringArray names;
    for (auto& f : favs) names.add (f.name);
    return names;
}

// ------------------------------------------------------------------ audio
void InfiniteProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    lastAmp = { -1.f, -1.f, -1.f, -1.f };   // force la mise a jour des enveloppes
    lastFDecay = lastReso = -1.f;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    for (auto& v : voices)
    {
        v.filter.prepare (spec);
        v.filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        v.amp.setSampleRate (sampleRate);
        v.fenv.setSampleRate (sampleRate);
        v.amp.reset();
        v.fenv.reset();
        v.gate = false;
    }
}

bool InfiniteProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void InfiniteProcessor::readParams()
{
    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    P.w1 = (int) g ("osc1Wave");  P.w2 = (int) g ("osc2Wave");
    P.semi = g ("osc2Semi");      P.fine = g ("osc2Fine");   P.mix = g ("osc2Mix");
    P.sub = g ("sub");            P.noise = g ("noise");
    P.fm = g ("fmAmt");           P.fmRatio = g ("fmRatio");
    P.cutoff = g ("cutoff");      P.reso = g ("reso");       P.envAmt = g ("envAmt");
    P.fdecay = g ("fdecay");      P.lfoRate = g ("lfoRate"); P.lfoDepth = g ("lfoDepth");
    P.attack = g ("attack");      P.decay = g ("decay");     P.sustain = g ("sustain");
    P.release = g ("release");    P.drive = g ("drive");     P.volume = g ("volume");

    // On ne touche aux enveloppes que si un reglage a change : JUCE recalcule
    // la vitesse de release a chaque appel, ce qui pouvait figer les notes.
    const bool ampChanged = P.attack != lastAmp.attack || P.decay != lastAmp.decay
                         || P.sustain != lastAmp.sustain || P.release != lastAmp.release;
    const bool fenvChanged = P.fdecay != lastFDecay;
    const bool resoChanged = P.reso != lastReso;

    for (auto& v : voices)
    {
        if (ampChanged)  v.amp.setParameters ({ P.attack, P.decay, P.sustain, P.release });
        if (fenvChanged) v.fenv.setParameters ({ 0.001f, P.fdecay, 0.f, 0.1f });
        if (resoChanged) v.filter.setResonance (P.reso);
    }
    lastAmp = { P.attack, P.decay, P.sustain, P.release };
    lastFDecay = P.fdecay;
    lastReso = P.reso;
}

void InfiniteProcessor::noteOn (int note, float vel)
{
    Voice* target = nullptr;
    for (auto& v : voices) if (! v.amp.isActive()) { target = &v; break; }
    if (target == nullptr)
    {
        target = &voices[0];
        for (auto& v : voices) if (v.age < target->age) target = &v;
    }
    target->note = note;   target->vel = vel;   target->gate = true;
    target->age = ++ageCounter;   target->samples = 0;
    target->relCount = 0;         target->fade = 1.f;
    target->ph1 = target->ph2 = target->phSub = target->phMod = 0.f;
    target->filter.reset();
    target->amp.noteOn();
    target->fenv.noteOn();
}

void InfiniteProcessor::noteOff (int note)
{
    for (auto& v : voices)
        if (v.gate && v.note == note)
        {
            v.gate = false;
            v.relCount = 0;
            v.amp.noteOff();
            v.fenv.noteOff();
        }
}

void InfiniteProcessor::render (juce::AudioBuffer<float>& buffer, int start, int num)
{
    if (num <= 0) return;

    auto* left  = buffer.getWritePointer (0, start);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, start) : nullptr;

    const float fs = (float) sr;
    const float twoPi = juce::MathConstants<float>::twoPi;
    const float gain = juce::Decibels::decibelsToGain (P.volume);
    const float driveK = 1.f + P.drive * 5.f;
    const float maxFc = std::min (18000.f, fs * 0.45f);
    const float detuneRatio = std::exp2 ((P.semi + P.fine / 100.f) / 12.f);

    for (int i = 0; i < num; ++i)
    {
        const float lfo = std::sin (twoPi * lfoPhase);
        lfoPhase += P.lfoRate / fs;
        if (lfoPhase >= 1.f) lfoPhase -= 1.f;

        float mix = 0.f;

        for (auto& v : voices)
        {
            if (! v.amp.isActive()) continue;

            const float f1 = 440.f * std::exp2 (((float) v.note - 69.f) / 12.f);
            const float f2 = f1 * detuneRatio;
            const float dt1 = f1 / fs, dt2 = f2 / fs;

            const float mod = std::sin (twoPi * v.phMod);
            float p1 = v.ph1 + 0.5f * P.fm * mod;
            p1 -= std::floor (p1);

            float s = oscWave (P.w1, p1, dt1)
                    + P.mix * oscWave (P.w2, v.ph2, dt2)
                    + P.sub * std::sin (twoPi * v.phSub)
                    + P.noise * (rng.nextFloat() * 2.f - 1.f);

            v.ph1   += dt1;               if (v.ph1   >= 1.f) v.ph1   -= 1.f;
            v.ph2   += dt2;               if (v.ph2   >= 1.f) v.ph2   -= 1.f;
            v.phSub += dt1 * 0.5f;        if (v.phSub >= 1.f) v.phSub -= 1.f;
            v.phMod += dt1 * P.fmRatio;   if (v.phMod >= 1.f) v.phMod -= 1.f;

            const float ae = v.amp.getNextSample();
            const float fe = v.fenv.getNextSample();

            float fc = P.cutoff * std::exp2 (P.envAmt * fe + P.lfoDepth * lfo);
            fc = juce::jlimit (30.f, maxFc, fc);
            v.filter.setCutoffFrequency (fc);
            s = v.filter.processSample (0, s);

            s = std::tanh (s * driveK);

            // Securite : une note relachee s'arrete toujours, au plus tard
            // un peu apres son temps de release (fondu de 15 ms).
            if (! v.gate)
            {
                ++v.relCount;
                if (v.relCount > (int) ((P.release * 1.25f + 0.05f) * fs))
                    v.fade -= 1.f / (0.015f * fs);
            }

            mix += s * ae * v.vel * 0.35f * juce::jmax (0.f, v.fade);

            ++v.samples;
            if (v.fade <= 0.f || (ae < 0.00005f && v.samples > (int) (P.attack * fs) + 512))
            {
                v.amp.reset();   // libère la voix
                v.fenv.reset();
                v.gate = false;
            }
        }

        const float out = std::tanh (mix * gain);
        left[i] = out;
        if (right != nullptr) right[i] = out;
    }
}

void InfiniteProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    readParams();

    int pos = 0;
    const int total = buffer.getNumSamples();

    for (const auto meta : midi)
    {
        const int t = juce::jlimit (0, total, meta.samplePosition);
        render (buffer, pos, t - pos);
        pos = t;

        const auto m = meta.getMessage();
        if (m.isNoteOn())        noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())  noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            for (auto& v : voices) if (v.gate) { v.gate = false; v.relCount = 0; v.amp.noteOff(); v.fenv.noteOff(); }
    }
    render (buffer, pos, total - pos);
}

juce::AudioProcessorEditor* InfiniteProcessor::createEditor()
{
    return new InfiniteEditor (*this);
}

void InfiniteProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void InfiniteProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new InfiniteProcessor();
}
