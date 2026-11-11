#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

using APF = juce::AudioParameterFloat;
using APC = juce::AudioParameterChoice;
using APB = juce::AudioParameterBool;
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
    for (auto& b : boolParams())
        p.push_back (std::make_unique<APB> (PID { b.first, 1 }, b.second, false));
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
    for (auto& b : boolParams()) m[b.first] = 0.f;
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
    // ---- glide et effets selon la catégorie ----
    auto on = [&] (const char* id, float p) { m[id] = chance (p) ? 1.f : 0.f; return m[id] > 0.5f; };

    if (cat == 1)        // Lead : mono + slide la plupart du temps
    {
        if (on ("mono", 0.8f)) m["glide"] = rf (0.04f, 0.18f);
        if (on ("distOn", 0.3f)) { m["distDrive"] = rf (0.2f, 0.7f); m["distMix"] = rf (0.4f, 1.f); m["distTone"] = rl (3000, 12000); }
        if (on ("chorOn", 0.3f)) { m["chorDepth"] = rf (0.2f, 0.6f); m["chorMix"] = rf (0.2f, 0.5f); }
        if (on ("dlyOn", 0.6f))  { m["dlyTime"] = pick ({ 250, 300, 375, 500 }); m["dlyFb"] = rf (0.2f, 0.5f); m["dlyMix"] = rf (0.15f, 0.35f); }
        if (on ("revOn", 0.6f))  { m["revSize"] = rf (0.4f, 0.8f); m["revMix"] = rf (0.15f, 0.35f); }
        if (on ("phasOn", 0.15f)) { m["phasRate"] = rl (0.1f, 1.f); m["phasMix"] = rf (0.3f, 0.6f); }
    }
    else if (cat == 2)   // Pad : large et spatial
    {
        if (on ("chorOn", 0.6f)) { m["chorDepth"] = rf (0.3f, 0.8f); m["chorMix"] = rf (0.3f, 0.6f); m["chorRate"] = rl (0.1f, 1.f); }
        if (on ("phasOn", 0.3f)) { m["phasRate"] = rl (0.05f, 0.5f); m["phasMix"] = rf (0.3f, 0.6f); }
        if (on ("flanOn", 0.15f)) { m["flanRate"] = rl (0.05f, 0.3f); m["flanMix"] = rf (0.2f, 0.5f); }
        if (on ("tremOn", 0.2f)) { m["tremRate"] = rl (0.2f, 2.f); m["tremDepth"] = rf (0.2f, 0.6f); m["tremPan"] = 1.f; }
        if (on ("dlyOn", 0.3f))  { m["dlyTime"] = pick ({ 375, 500, 750 }); m["dlyFb"] = rf (0.3f, 0.6f); m["dlyMix"] = rf (0.1f, 0.3f); }
        m["revOn"] = chance (0.9f) ? 1.f : 0.f;
        m["revSize"] = rf (0.7f, 0.95f); m["revMix"] = rf (0.3f, 0.55f);
    }
    else if (cat == 3)   // Pluck : un peu de slide, beaucoup d'echo
    {
        if (on ("mono", 0.3f)) m["glide"] = rf (0.03f, 0.1f);
        if (on ("dlyOn", 0.7f))  { m["dlyTime"] = pick ({ 250, 300, 375, 500 }); m["dlyFb"] = rf (0.25f, 0.55f); m["dlyMix"] = rf (0.15f, 0.35f); }
        if (on ("revOn", 0.7f))  { m["revSize"] = rf (0.4f, 0.85f); m["revMix"] = rf (0.15f, 0.4f); }
        if (on ("crushOn", 0.15f)) { m["crushBits"] = (float) (6 + r.nextInt (6)); m["crushRate"] = (float) (1 + r.nextInt (4)); m["crushMix"] = rf (0.3f, 0.7f); }
        if (on ("chorOn", 0.25f)) { m["chorDepth"] = rf (0.2f, 0.5f); m["chorMix"] = rf (0.2f, 0.4f); }
    }
    else if (cat == 4)   // Keys
    {
        if (on ("chorOn", 0.4f)) { m["chorDepth"] = rf (0.2f, 0.5f); m["chorMix"] = rf (0.2f, 0.4f); }
        if (on ("tremOn", 0.25f)) { m["tremRate"] = rf (3.f, 6.f); m["tremDepth"] = rf (0.2f, 0.4f); m["tremPan"] = rf (0.f, 1.f); }
        if (on ("dlyOn", 0.3f))  { m["dlyTime"] = pick ({ 375, 500 }); m["dlyFb"] = rf (0.2f, 0.45f); m["dlyMix"] = rf (0.1f, 0.25f); }
        if (on ("revOn", 0.75f)) { m["revSize"] = rf (0.4f, 0.8f); m["revMix"] = rf (0.15f, 0.35f); }
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
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            m[rp->paramID] = rp->convertFrom0to1 (rp->getValue());
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
            for (auto* p : getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                    fav.values[rp->paramID] = (float) e->getDoubleAttribute (rp->paramID,
                                                  rp->convertFrom0to1 (rp->getDefaultValue()));
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
    fx.prepare (sampleRate);
    heldCount = 0;
    bend = 0.f;
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
    P.mono = g ("mono") > 0.5f;   P.glide = g ("glide");

    auto& s = fxs;
    s.distOn = g ("distOn") > 0.5f;  s.distDrive = g ("distDrive"); s.distTone = g ("distTone"); s.distMix = g ("distMix");
    s.crushOn = g ("crushOn") > 0.5f; s.crushBits = g ("crushBits"); s.crushRate = g ("crushRate"); s.crushMix = g ("crushMix");
    s.eqOn = g ("eqOn") > 0.5f;      s.eqLow = g ("eqLow"); s.eqMid = g ("eqMid"); s.eqHigh = g ("eqHigh");
    s.compOn = g ("compOn") > 0.5f;  s.compThresh = g ("compThresh"); s.compRatio = g ("compRatio"); s.compGain = g ("compGain");
    s.chorOn = g ("chorOn") > 0.5f;  s.chorRate = g ("chorRate"); s.chorDepth = g ("chorDepth"); s.chorMix = g ("chorMix");
    s.flanOn = g ("flanOn") > 0.5f;  s.flanRate = g ("flanRate"); s.flanDepth = g ("flanDepth"); s.flanFb = g ("flanFb"); s.flanMix = g ("flanMix");
    s.phasOn = g ("phasOn") > 0.5f;  s.phasRate = g ("phasRate"); s.phasDepth = g ("phasDepth"); s.phasFb = g ("phasFb"); s.phasMix = g ("phasMix");
    s.tremOn = g ("tremOn") > 0.5f;  s.tremRate = g ("tremRate"); s.tremDepth = g ("tremDepth"); s.tremPan = g ("tremPan");
    s.dlyOn = g ("dlyOn") > 0.5f;    s.dlyTime = g ("dlyTime"); s.dlyFb = g ("dlyFb"); s.dlyMix = g ("dlyMix");
    s.revOn = g ("revOn") > 0.5f;    s.revSize = g ("revSize"); s.revDamp = g ("revDamp"); s.revWidth = g ("revWidth"); s.revMix = g ("revMix");

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
    if (P.mono)
    {
        // pile des notes tenues (la plus récente gagne)
        for (int i = 0; i < heldCount; ++i)
            if (held[(size_t) i] == note)
            {
                for (int j = i; j < heldCount - 1; ++j) held[(size_t) j] = held[(size_t) (j + 1)];
                --heldCount;
                break;
            }
        if (heldCount < (int) held.size()) held[(size_t) heldCount++] = note;

        auto& v = voices[0];
        if (v.gate && v.amp.isActive())
        {
            // legato : on glisse vers la nouvelle note sans redémarrer l'enveloppe
            v.note = note;
            v.targetNote = (float) note;
            if (P.glide < 0.001f) v.curNote = (float) note;
            return;
        }
        for (size_t i = 1; i < voices.size(); ++i)
            if (voices[i].gate) { voices[i].gate = false; voices[i].relCount = 0; voices[i].amp.noteOff(); voices[i].fenv.noteOff(); }

        v.note = note;   v.vel = vel;   v.gate = true;
        v.age = ++ageCounter;   v.samples = 0;
        v.relCount = 0;   v.fade = 1.f;
        v.curNote = v.targetNote = (float) note;
        v.ph1 = v.ph2 = v.phSub = v.phMod = 0.f;
        v.filter.reset();
        v.amp.noteOn();
        v.fenv.noteOn();
        return;
    }

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
    target->curNote = target->targetNote = (float) note;
    target->ph1 = target->ph2 = target->phSub = target->phMod = 0.f;
    target->filter.reset();
    target->amp.noteOn();
    target->fenv.noteOn();
}

void InfiniteProcessor::noteOff (int note)
{
    if (P.mono)
    {
        for (int i = 0; i < heldCount; ++i)
            if (held[(size_t) i] == note)
            {
                for (int j = i; j < heldCount - 1; ++j) held[(size_t) j] = held[(size_t) (j + 1)];
                --heldCount;
                break;
            }

        auto& v = voices[0];
        if (v.gate && v.note == note && heldCount > 0)
        {
            // retour en glissant vers la note encore tenue
            v.note = held[(size_t) (heldCount - 1)];
            v.targetNote = (float) v.note;
            if (P.glide < 0.001f) v.curNote = v.targetNote;
            return;
        }
    }

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
    const float glideCoef = P.glide > 0.001f ? 1.f - std::exp (-4.f / (P.glide * fs)) : 1.f;

    for (int i = 0; i < num; ++i)
    {
        const float lfo = std::sin (twoPi * lfoPhase);
        lfoPhase += P.lfoRate / fs;
        if (lfoPhase >= 1.f) lfoPhase -= 1.f;

        float mix = 0.f;

        for (auto& v : voices)
        {
            if (! v.amp.isActive()) continue;

            v.curNote += (v.targetNote - v.curNote) * glideCoef;
            const float f1 = 440.f * std::exp2 ((v.curNote + bend - 69.f) / 12.f);
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
        else if (m.isPitchWheel()) bend = (float) (m.getPitchWheelValue() - 8192) / 8192.f * 2.f;
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            heldCount = 0;
            for (auto& v : voices) if (v.gate) { v.gate = false; v.relCount = 0; v.amp.noteOff(); v.fenv.noteOff(); }
        }
    }
    render (buffer, pos, total - pos);
    fx.process (buffer, fxs);

    // protection des oreilles : jamais au-dessus de 0 dB
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < total; ++i)
            d[i] = std::isfinite (d[i]) ? juce::jlimit (-1.f, 1.f, d[i]) : 0.f;
    }
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
