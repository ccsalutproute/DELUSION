#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

using APF = juce::AudioParameterFloat;
using APC = juce::AudioParameterChoice;
using APB = juce::AudioParameterBool;
using PID = juce::ParameterID;

static const char* kCats[] = { "Bass", "Lead", "Pad", "Pluck", "Keys", "Piano", "E-Piano", "Brass", "Plucked", "Strings", "Winds" };

juce::String InfiniteProcessor::catName (int cat) { return kCats[juce::jlimit (1, kNumCats, cat)]; }

// index des sources / cibles de modulation
enum Src { sOff, sEnv2, sEnv3, sLfo1, sLfo2, sLfo3, sLfo4, sVel, sWheel, sNote, sRand, sCount };
enum Dst { dOff, dCutoff, dReso, dPitch, dPitch1, dPitch2, dLevel2, dSub, dNoise, dFm, dWarp1, dWarp2,
           dDrive, dAmp, dPan, dDetune, dCount };

// ------------------------------------------------------------------ oscillateurs
static float polyBlep (float t, float dt)
{
    if (dt <= 0.f) return 0.f;
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

// forme d'onde avec "warp" (Fold, Sync, Bend, PWM)
static float warpedOsc (int wave, int warp, float amt, float ph, float dt)
{
    if (warp == 0 || amt < 0.001f) return oscWave (wave, ph, dt);
    switch (warp)
    {
        case 1:  return std::sin (oscWave (wave, ph, dt) * (1.f + amt * 5.f) * juce::MathConstants<float>::halfPi);
        case 2:  { const float r = 1.f + amt * 7.f; float p = ph * r; p -= std::floor (p);
                   return oscWave (wave, p, juce::jmin (0.5f, dt * r)); }
        case 3:  return oscWave (wave, std::pow (ph, 1.f / (1.f + amt * 4.f)), dt);
        default: { const float d = 0.5f - 0.45f * amt;
                   const float p = ph < d ? 0.5f * ph / d : 0.5f + 0.5f * (ph - d) / (1.f - d);
                   return oscWave (wave, p, dt); }
    }
}

// ------------------------------------------------------------------ construction
InfiniteProcessor::InfiniteProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    loadFavoritesFromDisk();
    loadSeed (1, 1);
}

juce::AudioProcessorValueTreeState::ParameterLayout InfiniteProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    for (auto& c : choiceParams())
        p.push_back (std::make_unique<APC> (PID { c.id, 1 }, c.name, c.items, c.def));

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
    for (auto& f : floatParams())  m[f.id] = f.def;
    for (auto& c : choiceParams()) m[c.id] = (float) c.def;
    for (auto& b : boolParams())   m[b.first] = 0.f;
    m["volume"] = -6.f;
    if (cat >= 5) { m["filtType"] = 0.f; m["reso"] = 0.7f; m["envAmt"] = 0.f; m["keyTrack"] = 0.f; m["drive"] = 0.05f; m["osc2Mix"] = 0.f; }

    int slot = 0;
    auto mod = [&] (int src, int dst, float amt)
    {
        if (slot >= lists::numSlots) return;
        ++slot;
        const juce::String n (slot);
        m["mod" + n + "Src"] = (float) src;
        m["mod" + n + "Dst"] = (float) dst;
        m["mod" + n + "Amt"] = juce::jlimit (-1.f, 1.f, amt);
    };
    auto lfo = [&] (int n, int shape, float hz)
    {
        const juce::String s (n);
        m["lfo" + s + "Shape"] = (float) shape;
        m["lfo" + s + "Rate"] = hz;
    };
    auto syncLfo = [&] (int n, int div)
    {
        const juce::String s (n);
        m["lfo" + s + "Sync"] = 1.f;
        m["lfo" + s + "Div"] = (float) div;
    };
    auto warp = [&] (int osc, float p, float lo, float hi)
    {
        if (! chance (p)) return;
        const juce::String s (osc);
        m["warp" + s + "Mode"] = (float) (1 + r.nextInt (4));
        m["warp" + s + "Amt"] = rf (lo, hi);
    };
    auto unison = [&] (float p, int lo, int hi, float dLo, float dHi)
    {
        if (! chance (p)) return;
        m["uniVoices"] = (float) (lo + r.nextInt (hi - lo + 1));
        m["uniDetune"] = rf (dLo, dHi);
        m["uniSpread"] = rf (0.5f, 1.f);
    };

    switch (cat)
    {
        case 1: // Lead
            m["osc1Wave"] = pick ({ 1, 2 });         m["osc2Wave"] = pick ({ 1, 2, 3 });
            m["osc2Semi"] = pick ({ 0, 0, 12, 7 });  m["osc2Fine"] = rf (-20, 20);
            m["osc2Mix"]  = rf (0.3f, 0.8f);         m["sub"] = 0.f;
            m["noise"]    = chance (0.3f) ? rf (0.f, 0.05f) : 0.f;
            m["fmAmt"]    = chance (0.5f) ? rf (0.1f, 1.5f) : 0.f;  m["fmRatio"] = pick ({ 1, 2, 3 });
            m["filtType"] = pick ({ 0, 0, 1 });
            m["cutoff"]   = rl (1500, 9000);         m["reso"] = rf (0.7f, 4.f);
            m["envAmt"]   = rf (0.f, 2.f);           m["keyTrack"] = rf (0.2f, 0.6f);
            m["e2a"] = rf (0.001f, 0.01f); m["e2d"] = rf (0.2f, 1.f); m["e2s"] = rf (0.f, 0.3f); m["e2r"] = rf (0.2f, 0.5f);
            m["attack"]   = rf (0.002f, 0.05f);      m["decay"] = rf (0.2f, 1.f);
            m["sustain"]  = rf (0.5f, 1.f);          m["release"] = rf (0.15f, 0.6f);
            m["drive"]    = rf (0.f, 0.4f);
            if (chance (0.8f)) { m["mono"] = 1.f; m["glide"] = rf (0.04f, 0.18f); }
            unison (0.4f, 3, 5, 0.08f, 0.3f);
            warp (1, 0.35f, 0.1f, 0.6f);
            lfo (1, 0, rf (4.5f, 6.5f));
            if (chance (0.6f)) mod (sLfo1, dPitch, rf (0.003f, 0.012f));      // vibrato
            if (chance (0.5f)) mod (sVel, dCutoff, rf (0.08f, 0.2f));
            if (chance (0.3f)) { m["e3d"] = rf (0.1f, 0.5f); mod (sEnv3, dWarp1, rf (0.2f, 0.6f)); }
            if (chance (0.25f)) { lfo (2, pick ({ 0, 1, 6 }), rl (0.2f, 3.f)); mod (sLfo2, dCutoff, rf (0.05f, 0.2f)); }
            mod (sWheel, dCutoff, 0.3f);
            break;

        case 2: // Pad
            m["osc1Wave"] = pick ({ 1, 3, 0 });      m["osc2Wave"] = pick ({ 1, 3 });
            m["osc2Semi"] = pick ({ 0, 7, 12, -12 }); m["osc2Fine"] = rf (-30, 30);
            m["osc2Mix"]  = rf (0.5f, 1.f);          m["sub"] = rf (0.f, 0.3f);
            m["noise"]    = rf (0.f, 0.1f);
            m["fmAmt"]    = chance (0.4f) ? rf (0.1f, 0.6f) : 0.f;  m["fmRatio"] = pick ({ 0.5f, 1, 2 });
            m["filtType"] = chance (0.15f) ? 4.f : pick ({ 0, 1 });
            m["cutoff"]   = rl (400, 4000);          m["reso"] = rf (0.7f, 2.5f);
            m["envAmt"]   = rf (-1.f, 2.f);          m["keyTrack"] = rf (0.f, 0.4f);
            m["e2a"] = rf (0.3f, 1.5f); m["e2d"] = rf (0.8f, 2.f); m["e2s"] = rf (0.3f, 0.8f); m["e2r"] = rf (1.f, 3.f);
            m["attack"]   = rf (0.3f, 1.5f);         m["decay"] = rf (0.5f, 2.f);
            m["sustain"]  = rf (0.6f, 1.f);          m["release"] = rf (1.f, 3.5f);
            m["drive"]    = rf (0.f, 0.2f);
            unison (0.7f, 4, 7, 0.2f, 0.5f);
            warp (1, 0.25f, 0.1f, 0.5f);
            warp (2, 0.2f, 0.1f, 0.5f);
            lfo (1, pick ({ 0, 1 }), rl (0.1f, 1.5f));
            mod (sLfo1, dCutoff, rf (0.05f, 0.25f));
            lfo (2, 6, rl (0.1f, 0.5f));
            if (chance (0.6f)) mod (sLfo2, dPan, rf (0.2f, 0.5f));
            if (chance (0.4f)) { lfo (3, pick ({ 1, 6 }), rl (0.05f, 0.4f)); mod (sLfo3, dWarp1, rf (0.1f, 0.4f)); }
            if (chance (0.3f)) { syncLfo (4, pick ({ 4, 6, 7 })); m["lfo4Shape"] = pick ({ 1, 4 }); mod (sLfo4, dAmp, rf (-0.4f, -0.15f)); }
            mod (sWheel, dCutoff, 0.35f);
            break;

        case 3: // Pluck
            m["osc1Wave"] = pick ({ 1, 2, 3 });      m["osc2Wave"] = pick ({ 1, 2, 3 });
            m["osc2Semi"] = pick ({ 0, 12, 7, -12 }); m["osc2Fine"] = rf (-15, 15);
            m["osc2Mix"]  = rf (0.2f, 0.7f);         m["sub"] = rf (0.f, 0.3f);
            m["fmAmt"]    = rf (0.f, 2.f);           m["fmRatio"] = 0.5f * (float) (1 + r.nextInt (16));
            m["filtType"] = pick ({ 0, 0, 1, 2 });
            m["cutoff"]   = rl (500, 5000);          m["reso"] = rf (0.7f, 3.f);
            m["envAmt"]   = rf (1.5f, 3.5f);         m["keyTrack"] = rf (0.3f, 0.7f);
            m["e2a"] = 0.001f; m["e2d"] = rf (0.08f, 0.4f); m["e2s"] = 0.f; m["e2r"] = rf (0.1f, 0.4f);
            m["attack"]   = 0.001f;                  m["decay"] = rf (0.15f, 0.6f);
            m["sustain"]  = rf (0.f, 0.1f);          m["release"] = rf (0.1f, 0.5f);
            m["drive"]    = rf (0.f, 0.4f);
            if (chance (0.3f)) { m["mono"] = 1.f; m["glide"] = rf (0.03f, 0.1f); }
            unison (0.3f, 2, 4, 0.08f, 0.25f);
            warp (1, 0.3f, 0.1f, 0.7f);
            m["e3a"] = 0.001f; m["e3d"] = rf (0.03f, 0.15f);
            if (chance (0.35f)) mod (sEnv3, dPitch2, rf (0.05f, 0.25f));
            if (chance (0.35f)) mod (sEnv3, dFm, rf (0.2f, 0.5f));
            if (chance (0.4f)) mod (sVel, dCutoff, rf (0.1f, 0.25f));
            if (chance (0.25f)) { syncLfo (1, pick ({ 2, 4 })); m["lfo1Shape"] = 5.f; mod (sLfo1, dCutoff, rf (0.1f, 0.3f)); }
            mod (sWheel, dWarp1, 0.5f);
            break;

        case 4: // Keys / cloches
            m["osc1Wave"] = pick ({ 0, 3 });         m["osc2Wave"] = 0.f;
            m["osc2Semi"] = pick ({ 0, 12, 19, 24 }); m["osc2Fine"] = rf (-8, 8);
            m["osc2Mix"]  = rf (0.2f, 0.6f);
            m["fmAmt"]    = rf (0.3f, 2.5f);         m["fmRatio"] = pick ({ 1, 2, 3, 3.5f, 4, 5, 7 });
            m["filtType"] = 0.f;
            m["cutoff"]   = rl (2000, 12000);        m["reso"] = 0.7f;
            m["envAmt"]   = rf (0.f, 1.5f);          m["keyTrack"] = rf (0.3f, 0.8f);
            m["e2a"] = 0.001f; m["e2d"] = rf (0.3f, 1.5f); m["e2s"] = 0.f; m["e2r"] = rf (0.3f, 1.f);
            m["attack"]   = rf (0.001f, 0.01f);      m["decay"] = rf (0.5f, 2.5f);
            m["sustain"]  = rf (0.f, 0.3f);          m["release"] = rf (0.4f, 2.f);
            m["drive"]    = rf (0.f, 0.15f);
            unison (0.2f, 2, 3, 0.05f, 0.15f);
            m["e3a"] = 0.001f; m["e3d"] = rf (0.2f, 1.2f); m["e3s"] = 0.f;
            mod (sEnv3, dFm, rf (0.15f, 0.5f));                                 // FM qui decroit = cloche
            if (chance (0.3f)) { lfo (1, 1, rf (3.f, 6.f)); mod (sLfo1, dAmp, rf (-0.25f, -0.1f)); }
            if (chance (0.3f)) { lfo (2, 0, rf (4.f, 6.f)); mod (sLfo2, dPitch, rf (0.002f, 0.006f)); }
            mod (sVel, dFm, rf (0.1f, 0.3f));
            mod (sWheel, dAmp, -0.4f);
            break;

        case 5: // Piano (vrais enregistrements : Grand / Upright)
        {
            m["engine"] = (float) inst::Piano;
            const bool felt = chance (0.3f);                     // piano doux et sombre "lo-fi"
            m["inst2"] = chance (0.7f) ? 0.2f : 0.7f;            // Grand ou Upright
            m["inst1"] = felt ? rf (0.35f, 0.55f) : rf (0.8f, 1.f);
            m["inst3"] = rf (0.4f, 0.7f);
            m["inst4"] = rf (0.45f, 0.75f);
            m["cutoff"] = felt ? rl (3000, 7000) : 16000.f;
            m["attack"] = 0.001f; m["decay"] = 3.f; m["sustain"] = 1.f; m["release"] = rf (0.3f, 0.7f);
            m["revOn"] = 1.f; m["revSize"] = felt ? rf (0.6f, 0.9f) : rf (0.45f, 0.75f); m["revMix"] = rf (0.12f, 0.28f); m["revDamp"] = rf (0.4f, 0.7f);
            if (felt && chance (0.5f)) { m["crushOn"] = 1.f; m["crushBits"] = (float) (10 + r.nextInt (4)); m["crushRate"] = 1.f; m["crushMix"] = rf (0.15f, 0.35f); }
            if (chance (0.2f)) { m["dlyOn"] = 1.f; m["dlyTime"] = pick ({ 375, 500 }); m["dlyFb"] = rf (0.2f, 0.4f); m["dlyMix"] = rf (0.06f, 0.16f); }
            break;
        }

        case 6: // E-Piano
            m["engine"] = (float) inst::EPiano;
            m["inst1"] = rf (0.1f, 0.7f);  m["inst2"] = rf (0.1f, 0.6f);  m["inst3"] = rf (0.3f, 0.8f);
            m["inst4"] = chance (0.7f) ? rf (0.f, 0.25f) : rf (0.6f, 1.f);
            m["cutoff"] = rl (5000, 14000);
            m["attack"] = 0.001f; m["decay"] = 3.f; m["sustain"] = 1.f; m["release"] = rf (0.25f, 0.6f);
            mod (sVel, dCutoff, rf (0.08f, 0.2f));
            if (chance (0.5f)) { m["tremOn"] = 1.f; m["tremRate"] = rf (3.5f, 6.f); m["tremDepth"] = rf (0.25f, 0.5f); m["tremPan"] = 1.f; }
            if (chance (0.4f)) { m["chorOn"] = 1.f; m["chorDepth"] = rf (0.2f, 0.5f); m["chorMix"] = rf (0.2f, 0.4f); }
            if (chance (0.15f)) { m["phasOn"] = 1.f; m["phasRate"] = rl (0.1f, 0.6f); m["phasMix"] = rf (0.2f, 0.45f); }
            m["revOn"] = chance (0.75f) ? 1.f : 0.f; m["revSize"] = rf (0.4f, 0.75f); m["revMix"] = rf (0.12f, 0.3f);
            break;

        case 7: // Brass (trompette, trombone, cor, section)
        {
            m["engine"] = (float) inst::Brass;
            m["inst2"] = pick ({ 0.1f, 0.1f, 0.35f, 0.6f, 0.85f, 0.85f });
            m["inst1"] = rf (0.75f, 1.f);  m["inst3"] = rf (0.4f, 0.7f);  m["inst4"] = rf (0.5f, 0.8f);
            m["cutoff"] = 16000.f;
            m["attack"] = rf (0.003f, 0.02f); m["decay"] = 1.f; m["sustain"] = 1.f; m["release"] = rf (0.2f, 0.4f);
            if (m["inst2"] < 0.3f && chance (0.4f)) { m["mono"] = 1.f; m["glide"] = rf (0.02f, 0.05f); }
            mod (sWheel, dPitch, 0.01f);
            m["revOn"] = 1.f; m["revSize"] = rf (0.5f, 0.85f); m["revMix"] = rf (0.15f, 0.3f);
            if (chance (0.25f)) { m["dlyOn"] = 1.f; m["dlyTime"] = pick ({ 250, 375 }); m["dlyFb"] = rf (0.15f, 0.35f); m["dlyMix"] = rf (0.06f, 0.15f); }
            break;
        }

        case 8: // Cordes pincées : guitares, harpe, kalimba
            m["engine"] = (float) inst::Plucked;
            m["inst2"] = pick ({ 0.1f, 0.1f, 0.35f, 0.35f, 0.6f, 0.85f });
            m["inst1"] = rf (0.7f, 1.f);  m["inst3"] = rf (0.4f, 0.7f);  m["inst4"] = rf (0.5f, 0.85f);
            m["cutoff"] = 16000.f;
            m["attack"] = 0.001f; m["decay"] = 3.f; m["sustain"] = 1.f; m["release"] = rf (0.3f, 0.8f);
            if (chance (0.45f)) { m["dlyOn"] = 1.f; m["dlyTime"] = pick ({ 250, 375, 500 }); m["dlyFb"] = rf (0.2f, 0.45f); m["dlyMix"] = rf (0.08f, 0.22f); }
            m["revOn"] = chance (0.75f) ? 1.f : 0.f; m["revSize"] = rf (0.4f, 0.8f); m["revMix"] = rf (0.12f, 0.28f);
            if (chance (0.2f)) { m["chorOn"] = 1.f; m["chorDepth"] = rf (0.15f, 0.35f); m["chorMix"] = rf (0.12f, 0.25f); }
            break;

        case 9: // Cordes : ensemble, trémolo, pizzicato, violon solo
        {
            m["engine"] = (float) inst::Strings;
            m["inst2"] = pick ({ 0.1f, 0.1f, 0.1f, 0.35f, 0.6f, 0.85f });
            const bool pizz = m["inst2"] > 0.5f && m["inst2"] < 0.75f;
            m["inst1"] = rf (0.7f, 1.f);  m["inst3"] = rf (0.35f, 0.65f);  m["inst4"] = rf (0.6f, 0.9f);
            m["cutoff"] = 16000.f;
            m["attack"] = pizz ? 0.001f : rf (0.02f, 0.35f);
            m["decay"] = 1.f;  m["sustain"] = 1.f;
            m["release"] = pizz ? rf (0.2f, 0.4f) : rf (0.4f, 1.f);
            mod (sWheel, dCutoff, 0.25f);
            m["revOn"] = 1.f; m["revSize"] = rf (0.7f, 0.92f); m["revMix"] = rf (0.2f, 0.38f);
            break;
        }

        default: // Vents : flûte, hautbois, clarinette, saxophone
            m["engine"] = (float) inst::Flute;
            m["inst2"] = pick ({ 0.1f, 0.1f, 0.35f, 0.6f, 0.85f, 0.85f });
            m["inst1"] = rf (0.75f, 1.f);  m["inst3"] = rf (0.4f, 0.7f);  m["inst4"] = rf (0.4f, 0.7f);
            m["cutoff"] = 16000.f;
            m["attack"] = rf (0.005f, 0.04f); m["decay"] = 1.f; m["sustain"] = 1.f; m["release"] = rf (0.15f, 0.35f);
            if (chance (0.35f)) { m["mono"] = 1.f; m["glide"] = rf (0.02f, 0.06f); }
            mod (sWheel, dPitch, 0.006f);
            m["revOn"] = 1.f; m["revSize"] = rf (0.6f, 0.9f); m["revMix"] = rf (0.18f, 0.35f);
            if (chance (0.3f)) { m["dlyOn"] = 1.f; m["dlyTime"] = pick ({ 375, 500 }); m["dlyFb"] = rf (0.2f, 0.4f); m["dlyMix"] = rf (0.06f, 0.16f); }
            break;
    }

    // ---- effets selon la catégorie ----
    auto on = [&] (const char* id, float p) { m[id] = chance (p) ? 1.f : 0.f; return m[id] > 0.5f; };

    if (cat == 1)
    {
        if (on ("distOn", 0.3f)) { m["distDrive"] = rf (0.2f, 0.7f); m["distMix"] = rf (0.4f, 1.f); m["distTone"] = rl (3000, 12000); }
        if (on ("chorOn", 0.3f)) { m["chorDepth"] = rf (0.2f, 0.6f); m["chorMix"] = rf (0.2f, 0.5f); }
        if (on ("dlyOn", 0.6f))  { m["dlyTime"] = pick ({ 250, 300, 375, 500 }); m["dlyFb"] = rf (0.2f, 0.5f); m["dlyMix"] = rf (0.15f, 0.35f); }
        if (on ("revOn", 0.6f))  { m["revSize"] = rf (0.4f, 0.8f); m["revMix"] = rf (0.15f, 0.35f); }
        if (on ("phasOn", 0.15f)) { m["phasRate"] = rl (0.1f, 1.f); m["phasMix"] = rf (0.3f, 0.6f); }
    }
    else if (cat == 2)
    {
        if (on ("chorOn", 0.5f)) { m["chorDepth"] = rf (0.3f, 0.8f); m["chorMix"] = rf (0.3f, 0.6f); m["chorRate"] = rl (0.1f, 1.f); }
        if (on ("phasOn", 0.3f)) { m["phasRate"] = rl (0.05f, 0.5f); m["phasMix"] = rf (0.3f, 0.6f); }
        if (on ("flanOn", 0.15f)) { m["flanRate"] = rl (0.05f, 0.3f); m["flanMix"] = rf (0.2f, 0.5f); }
        if (on ("dlyOn", 0.3f))  { m["dlyTime"] = pick ({ 375, 500, 750 }); m["dlyFb"] = rf (0.3f, 0.6f); m["dlyMix"] = rf (0.1f, 0.3f); }
        m["revOn"] = chance (0.9f) ? 1.f : 0.f;
        m["revSize"] = rf (0.7f, 0.95f); m["revMix"] = rf (0.3f, 0.55f);
    }
    else if (cat == 3)
    {
        if (on ("dlyOn", 0.7f))  { m["dlyTime"] = pick ({ 250, 300, 375, 500 }); m["dlyFb"] = rf (0.25f, 0.55f); m["dlyMix"] = rf (0.15f, 0.35f); }
        if (on ("revOn", 0.7f))  { m["revSize"] = rf (0.4f, 0.85f); m["revMix"] = rf (0.15f, 0.4f); }
        if (on ("crushOn", 0.15f)) { m["crushBits"] = (float) (6 + r.nextInt (6)); m["crushRate"] = (float) (1 + r.nextInt (4)); m["crushMix"] = rf (0.3f, 0.7f); }
        if (on ("chorOn", 0.25f)) { m["chorDepth"] = rf (0.2f, 0.5f); m["chorMix"] = rf (0.2f, 0.4f); }
    }
    else if (cat == 4)
    {
        if (on ("chorOn", 0.4f)) { m["chorDepth"] = rf (0.2f, 0.5f); m["chorMix"] = rf (0.2f, 0.4f); }
        if (on ("tremOn", 0.2f)) { m["tremRate"] = rf (3.f, 6.f); m["tremDepth"] = rf (0.2f, 0.4f); m["tremPan"] = rf (0.f, 1.f); }
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
    catSel = juce::jlimit (1, kAllCat, catSel);   // plus de banque Bass
    int cat = catSel;
    if (cat == kAllCat)
    {
        juce::Random r ((juce::int64) seed * 977 + 3);
        r.nextInt();
        cat = 1 + r.nextInt (kNumCats);
    }
    applyValues (makePatch (cat, seed));
    apvts.state.setProperty ("seed", seed, nullptr);
    apvts.state.setProperty ("catSel", catSel, nullptr);
    apvts.state.setProperty ("presetName", makeName (cat, seed), nullptr);
    const juce::String info = (catSel != kAllCat && seed <= kBankSize)
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
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), 2 };
    for (auto& v : voices)
    {
        for (auto* f : { &v.fA, &v.fB })
        {
            f->prepare (spec);
            f->setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        }
        for (auto* e : { &v.amp, &v.env2, &v.env3 })
        {
            e->setSampleRate (sampleRate);
            e->reset();
        }
        v.gate = false;
        v.sustained = false;
        v.iv.prepare (sampleRate);
    }
    pedal = false;
    cutS = 2000.f;  gainS = 0.5f;  driveS = 0.f;
    lastAmp.fill (-1.f);  lastE2.fill (-1.f);  lastE3.fill (-1.f);
    lastReso = -1.f;
    lastFilt = -1;
    fx.prepare (sampleRate);
    heldCount = 0;
    bend = 0.f;
}

bool InfiniteProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void InfiniteProcessor::readParams()
{
    auto g  = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };
    auto gi = [&] (const juce::String& id) { return (int) std::lround (g (id)); };
    auto gb = [&] (const juce::String& id) { return g (id) > 0.5f; };

    P.w1 = gi ("osc1Wave");       P.w2 = gi ("osc2Wave");
    P.warp1 = gi ("warp1Mode");   P.warp2 = gi ("warp2Mode");
    P.warp1Amt = g ("warp1Amt");  P.warp2Amt = g ("warp2Amt");
    P.semi = g ("osc2Semi");      P.fine = g ("osc2Fine");   P.mix = g ("osc2Mix");
    P.sub = g ("sub");            P.noise = g ("noise");
    P.fm = g ("fmAmt");           P.fmRatio = g ("fmRatio");
    P.uni = juce::jlimit (1, kMaxUni, gi ("uniVoices"));
    P.uniDetune = g ("uniDetune"); P.uniSpread = g ("uniSpread");
    P.filt = gi ("filtType");
    P.cutoff = g ("cutoff");      P.reso = g ("reso");       P.envAmt = g ("envAmt");  P.keyTrack = g ("keyTrack");
    P.attack = g ("attack");      P.decay = g ("decay");     P.sustain = g ("sustain");
    P.release = g ("release");    P.drive = g ("drive");     P.volume = g ("volume");
    P.mono = gb ("mono");         P.glide = g ("glide");
    P.engine = juce::jlimit (0, (int) inst::NumEngines - 1, gi ("engine"));
    P.macro = { g ("inst1"), g ("inst2"), g ("inst3"), g ("inst4") };

    // on prépare à l'avance les enregistrements de l'instrument choisi
    if (inst::isSampled (P.engine))
    {
        const int key = P.engine * 16 + inst::variantIndex (P.engine, P.macro[1]);
        if (key != lastVariantKey)
        {
            lastVariantKey = key;
            for (auto* id : inst::variants (P.engine)[(size_t) inst::variantIndex (P.engine, P.macro[1])].ids)
                if (id != nullptr) samples->request (id);
        }
    }

    for (int i = 0; i < lists::numLfos; ++i)
    {
        const juce::String n (i + 1);
        P.lfoShape[(size_t) i] = gi ("lfo" + n + "Shape");
        P.lfoRetrig[(size_t) i] = gb ("lfo" + n + "Retrig");
        float hz = g ("lfo" + n + "Rate");
        if (gb ("lfo" + n + "Sync"))
        {
            const int d = juce::jlimit (0, 10, gi ("lfo" + n + "Div"));
            hz = (float) (bpm / 60.0) / lists::divBeats()[d];
        }
        P.lfoHz[(size_t) i] = hz;
    }
    for (int i = 0; i < lists::numSlots; ++i)
    {
        const juce::String n (i + 1);
        auto& s = P.slots[(size_t) i];
        s.src = juce::jlimit (0, sCount - 1, gi ("mod" + n + "Src"));
        s.dst = juce::jlimit (0, dCount - 1, gi ("mod" + n + "Dst"));
        s.amt = g ("mod" + n + "Amt");
    }

    // enveloppes : mises a jour seulement si un reglage change
    auto updEnv = [this] (std::array<float, 12>& last, float a, float d, float s, float r, juce::ADSR Voice::* env)
    {
        if (a == last[0] && d == last[1] && s == last[2] && r == last[3]) return;
        last[0] = a; last[1] = d; last[2] = s; last[3] = r;
        for (auto& v : voices) (v.*env).setParameters ({ a, d, s, r });
    };
    updEnv (lastAmp, P.attack, P.decay, P.sustain, P.release, &Voice::amp);
    updEnv (lastE2, g ("e2a"), g ("e2d"), g ("e2s"), g ("e2r"), &Voice::env2);
    updEnv (lastE3, g ("e3a"), g ("e3d"), g ("e3s"), g ("e3r"), &Voice::env3);

    if (P.filt != lastFilt)
    {
        using T = juce::dsp::StateVariableTPTFilterType;
        const auto type = (P.filt == 2 || P.filt == 3) ? T::highpass : (P.filt == 4 ? T::bandpass : T::lowpass);
        for (auto& v : voices) { v.fA.setType (type); v.fB.setType (type); }
        lastFilt = P.filt;
    }

    auto& s = fxs;
    s.distOn = gb ("distOn");  s.distDrive = g ("distDrive"); s.distTone = g ("distTone"); s.distMix = g ("distMix");
    s.crushOn = gb ("crushOn"); s.crushBits = g ("crushBits"); s.crushRate = g ("crushRate"); s.crushMix = g ("crushMix");
    s.eqOn = gb ("eqOn");      s.eqLow = g ("eqLow"); s.eqMid = g ("eqMid"); s.eqHigh = g ("eqHigh");
    s.compOn = gb ("compOn");  s.compThresh = g ("compThresh"); s.compRatio = g ("compRatio"); s.compGain = g ("compGain");
    s.chorOn = gb ("chorOn");  s.chorRate = g ("chorRate"); s.chorDepth = g ("chorDepth"); s.chorMix = g ("chorMix");
    s.flanOn = gb ("flanOn");  s.flanRate = g ("flanRate"); s.flanDepth = g ("flanDepth"); s.flanFb = g ("flanFb"); s.flanMix = g ("flanMix");
    s.phasOn = gb ("phasOn");  s.phasRate = g ("phasRate"); s.phasDepth = g ("phasDepth"); s.phasFb = g ("phasFb"); s.phasMix = g ("phasMix");
    s.tremOn = gb ("tremOn");  s.tremRate = g ("tremRate"); s.tremDepth = g ("tremDepth"); s.tremPan = g ("tremPan");
    s.dlyOn = gb ("dlyOn");    s.dlyTime = g ("dlyTime"); s.dlyFb = g ("dlyFb"); s.dlyMix = g ("dlyMix");
    s.revOn = gb ("revOn");    s.revSize = g ("revSize"); s.revDamp = g ("revDamp"); s.revWidth = g ("revWidth"); s.revMix = g ("revMix");
}

float InfiniteProcessor::lfoValue (LfoState& s, int shape, float inc)
{
    const float ph = s.phase;
    float v;
    switch (shape)
    {
        case 0:  v = std::sin (juce::MathConstants<float>::twoPi * ph); break;
        case 1:  v = 1.f - 4.f * std::abs (ph - 0.5f); break;
        case 2:  v = 2.f * ph - 1.f; break;
        case 3:  v = 1.f - 2.f * ph; break;
        case 4:  v = ph < 0.5f ? 1.f : -1.f; break;
        case 5:  v = s.held; break;
        default: v = s.prev + (s.next - s.prev) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * ph)); break;
    }
    s.phase += inc;
    if (s.phase >= 1.f)
    {
        s.phase -= std::floor (s.phase);
        s.prev = s.next;
        s.next = rng.nextFloat() * 2.f - 1.f;
        s.held = rng.nextFloat() * 2.f - 1.f;
    }
    return v;
}

void InfiniteProcessor::startVoice (Voice& v, int note, float vel)
{
    v.note = note;   v.vel = vel;   v.gate = true;   v.sustained = false;
    v.age = ++ageCounter;   v.samples = 0;
    if (P.engine != inst::Synth)
    {
        bool ok = false;
        if (inst::isSampled (P.engine) && samples->available())
        {
            const auto& vars = inst::variants (P.engine);
            const auto& var = vars[(size_t) inst::variantIndex (P.engine, P.macro[1])];
            SampleInstrument* layers[3] = { nullptr, nullptr, nullptr };
            int n = 0;
            for (auto* id : var.ids)
                if (id != nullptr) layers[n++] = samples->get (id);
            ok = inst::startSampled (v.iv, P.engine, layers, n, (float) note, vel, P.macro);
        }
        if (! ok) inst::start (v.iv, P.engine, (float) note, vel, P.macro, (float) sr);   // modèle de secours
    }
    else
    {
        v.iv.engine = inst::Synth;
        v.iv.sampled = false;
    }
    v.relCount = 0;   v.fade = 1.f;
    v.curNote = v.targetNote = (float) note;
    v.rnd = rng.nextFloat() * 2.f - 1.f;
    for (int k = 0; k < kMaxUni; ++k)
    {
        // phases aleatoires pour l'unison, phase 0 pour une seule voix (attaque nette)
        v.ph1[(size_t) k] = P.uni > 1 ? rng.nextFloat() : 0.f;
        v.ph2[(size_t) k] = P.uni > 1 ? rng.nextFloat() : 0.f;
    }
    v.phSub = v.phMod = 0.f;
    for (int i = 0; i < lists::numLfos; ++i)
    {
        auto& l = v.lfo[(size_t) i];
        l = freeLfo[(size_t) i];
        if (P.lfoRetrig[(size_t) i]) { l.phase = 0.f; l.held = rng.nextFloat() * 2.f - 1.f; }
    }
    v.fA.reset();  v.fB.reset();
    v.amp.noteOn();  v.env2.noteOn();  v.env3.noteOn();
}

static void removeHeld (std::array<int, 128>& held, int& count, int note)
{
    for (int i = 0; i < count; ++i)
        if (held[(size_t) i] == note)
        {
            for (int j = i; j < count - 1; ++j) held[(size_t) j] = held[(size_t) (j + 1)];
            --count;
            return;
        }
}

void InfiniteProcessor::noteOn (int note, float vel)
{
    if (P.mono)
    {
        removeHeld (held, heldCount, note);
        if (heldCount < (int) held.size()) held[(size_t) heldCount++] = note;

        auto& v = voices[0];
        if (v.gate && v.amp.isActive())
        {
            v.note = note;                       // legato : glisse sans redemarrer
            v.targetNote = (float) note;
            if (P.glide < 0.001f) v.curNote = (float) note;
            return;
        }
        for (size_t i = 1; i < voices.size(); ++i)
            if (voices[i].gate)
            {
                auto& o = voices[i];
                o.gate = false; o.relCount = 0;
                o.amp.noteOff(); o.env2.noteOff(); o.env3.noteOff();
            }
        startVoice (v, note, vel);
        return;
    }

    Voice* target = nullptr;
    for (auto& v : voices) if (! v.amp.isActive()) { target = &v; break; }
    if (target == nullptr)
    {
        target = &voices[0];
        for (auto& v : voices) if (v.age < target->age) target = &v;
    }
    startVoice (*target, note, vel);
}

void InfiniteProcessor::noteOff (int note)
{
    if (P.mono)
    {
        removeHeld (held, heldCount, note);
        auto& v = voices[0];
        if (v.gate && v.note == note && heldCount > 0)
        {
            v.note = held[(size_t) (heldCount - 1)];   // retour en glissant
            v.targetNote = (float) v.note;
            if (P.glide < 0.001f) v.curNote = v.targetNote;
            return;
        }
    }

    for (auto& v : voices)
        if (v.gate && v.note == note)
        {
            if (pedal) { v.sustained = true; continue; }   // pédale enfoncée : la note continue
            v.gate = false;
            v.relCount = 0;
            v.amp.noteOff(); v.env2.noteOff(); v.env3.noteOff();
        }
}

void InfiniteProcessor::render (juce::AudioBuffer<float>& buffer, int start, int num)
{
    if (num <= 0) return;

    auto* left  = buffer.getWritePointer (0, start);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, start) : nullptr;

    const float fs = (float) sr;
    const float twoPi = juce::MathConstants<float>::twoPi;
    const float gainT = juce::Decibels::decibelsToGain (P.volume);
    const float smooth = 1.f - std::exp (-1.f / (0.012f * fs));   // lissage ~12 ms
    const float maxFc = std::min (18000.f, fs * 0.45f);
    const float glideCoef = P.glide > 0.001f ? 1.f - std::exp (-4.f / (P.glide * fs)) : 1.f;
    const int uni = P.uni;
    const float uniNorm = 1.f / std::sqrt ((float) uni);
    const bool ladder = (P.filt == 1 || P.filt == 3);

    // position de chaque voix d'unison entre -1 et 1
    std::array<float, kMaxUni> uniPos {};
    for (int k = 0; k < uni; ++k)
        uniPos[(size_t) k] = uni > 1 ? 2.f * (float) k / (float) (uni - 1) - 1.f : 0.f;

    bool anySlot = false;
    for (auto& s : P.slots) if (s.src != sOff && s.dst != dOff && s.amt != 0.f) anySlot = true;

    for (int i = 0; i < num; ++i)
    {
        // LFO libres (partagés)
        std::array<float, lists::numLfos> freeVal {};
        for (int l = 0; l < lists::numLfos; ++l)
            freeVal[(size_t) l] = lfoValue (freeLfo[(size_t) l], P.lfoShape[(size_t) l], P.lfoHz[(size_t) l] / fs);

        float outL = 0.f, outR = 0.f;
        cutS   += (P.cutoff - cutS) * smooth;
        gainS  += (gainT - gainS) * smooth;
        driveS += (P.drive - driveS) * smooth;

        for (auto& v : voices)
        {
            if (! v.amp.isActive()) continue;

            const float ae = v.amp.getNextSample();
            const float e2 = v.env2.getNextSample();
            const float e3 = v.env3.getNextSample();

            // ---- matrice de modulation ----
            std::array<float, dCount> d {};
            if (anySlot)
            {
                std::array<float, sCount> src {};
                src[sEnv2] = e2;  src[sEnv3] = e3;
                for (int l = 0; l < lists::numLfos; ++l)
                    src[(size_t) (sLfo1 + l)] = P.lfoRetrig[(size_t) l]
                        ? lfoValue (v.lfo[(size_t) l], P.lfoShape[(size_t) l], P.lfoHz[(size_t) l] / fs)
                        : freeVal[(size_t) l];
                src[sVel] = v.vel;  src[sWheel] = modWheel;
                src[sNote] = ((float) v.note - 60.f) / 24.f;
                src[sRand] = v.rnd;
                for (auto& s : P.slots)
                    if (s.src != sOff && s.dst != dOff)
                        d[(size_t) s.dst] += src[(size_t) s.src] * s.amt;
            }

            // ---- hauteur ----
            v.curNote += (v.targetNote - v.curNote) * glideCoef;
            const float base = v.curNote + bend + d[dPitch] * 24.f;
            const float f1 = 440.f * std::exp2 ((base + d[dPitch1] * 24.f - 69.f) / 12.f);
            const float f2 = 440.f * std::exp2 ((base + d[dPitch2] * 24.f + P.semi + P.fine / 100.f - 69.f) / 12.f);

            const float lvl2  = juce::jlimit (0.f, 1.f, P.mix + d[dLevel2]);
            const float sub   = juce::jlimit (0.f, 1.f, P.sub + d[dSub]);
            const float noise = juce::jlimit (0.f, 1.f, P.noise + d[dNoise]);
            const float fm    = juce::jmax (0.f, P.fm + d[dFm] * 4.f);
            const float wa1   = juce::jlimit (0.f, 1.f, P.warp1Amt + d[dWarp1]);
            const float wa2   = juce::jlimit (0.f, 1.f, P.warp2Amt + d[dWarp2]);
            const float drive = juce::jlimit (0.f, 1.f, driveS + d[dDrive]);
            const float ampM  = juce::jlimit (0.f, 2.f, 1.f + d[dAmp]);
            const float pan   = juce::jlimit (-1.f, 1.f, d[dPan]);
            const float det   = juce::jlimit (0.f, 1.f, P.uniDetune + d[dDetune]);

            float sl = 0.f, srr = 0.f;
            if (v.iv.engine != inst::Synth)
            {
                // ---- instrument modélisé ----
                inst::render (v.iv, f1, P.macro, fs, sl, srr);
                if (v.iv.finished()) v.fade = 0.f;   // l'enregistrement est terminé : la voix se libère
            }
            else
            {
                const float mod = std::sin (twoPi * v.phMod);
                v.phMod += f1 * P.fmRatio / fs;  if (v.phMod >= 1.f) v.phMod -= 1.f;
                const float fmOff = 0.5f * fm * mod;

                // ---- oscillateurs + unison ----
                for (int k = 0; k < uni; ++k)
                {
                    const float pos = uniPos[(size_t) k];
                    const float ratio = uni > 1 ? std::exp2 (pos * det * 0.5f / 12.f) : 1.f;
                    const float dt1 = f1 * ratio / fs, dt2 = f2 * ratio / fs;

                    float& p1 = v.ph1[(size_t) k];
                    float& p2 = v.ph2[(size_t) k];
                    float pm = p1 + fmOff;
                    pm -= std::floor (pm);

                    const float s = warpedOsc (P.w1, P.warp1, wa1, pm, dt1)
                                  + lvl2 * warpedOsc (P.w2, P.warp2, wa2, p2, dt2);

                    p1 += dt1;  if (p1 >= 1.f) p1 -= std::floor (p1);
                    p2 += dt2;  if (p2 >= 1.f) p2 -= std::floor (p2);

                    const float pp = pos * P.uniSpread;
                    sl  += s * (1.f - juce::jmax (0.f, pp));
                    srr += s * (1.f + juce::jmin (0.f, pp));
                }
                sl *= uniNorm;  srr *= uniNorm;

                const float center = sub * std::sin (twoPi * v.phSub) + noise * (rng.nextFloat() * 2.f - 1.f);
                v.phSub += f1 * 0.5f / fs;  if (v.phSub >= 1.f) v.phSub -= 1.f;
                sl += center;  srr += center;

            }

            // ---- filtre ----
            float fc = cutS * std::exp2 (P.envAmt * e2 + d[dCutoff] * 5.f + P.keyTrack * (v.curNote - 60.f) / 12.f);
            fc = juce::jlimit (30.f, maxFc, fc);
            const float q = juce::jlimit (0.5f, 10.f, P.reso + d[dReso] * 4.f);
            v.fA.setCutoffFrequency (fc);  v.fA.setResonance (q);
            sl  = v.fA.processSample (0, sl);
            srr = v.fA.processSample (1, srr);
            if (ladder)
            {
                v.fB.setCutoffFrequency (fc);  v.fB.setResonance (0.6f);
                sl  = v.fB.processSample (0, sl);
                srr = v.fB.processSample (1, srr);
            }

            if (P.filt == 4) { sl *= 2.5f; srr *= 2.5f; }   // le passe-bande est naturellement plus faible
            const float k = 1.f + drive * 5.f;
            sl  = std::tanh (sl * k);
            srr = std::tanh (srr * k);

            // securite : une note relachee s'arrete toujours
            if (! v.gate)
            {
                ++v.relCount;
                if (v.relCount > (int) ((P.release * 1.25f + 0.05f) * fs))
                    v.fade -= 1.f / (0.015f * fs);
            }

            const float a = ae * v.vel * 0.35f * juce::jmax (0.f, v.fade) * ampM;
            outL += sl  * a * juce::jmin (1.f, 1.f - pan);
            outR += srr * a * juce::jmin (1.f, 1.f + pan);

            ++v.samples;
            if (v.fade <= 0.f || (ae < 0.00005f && v.samples > (int) (P.attack * fs) + 512))
            {
                v.amp.reset();  v.env2.reset();  v.env3.reset();
                v.gate = false;
            }
        }

        left[i] = std::tanh (outL * gainS);
        if (right != nullptr) right[i] = std::tanh (outR * gainS);
    }
}

void InfiniteProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                if (*b > 20.0 && *b < 400.0) bpm = *b;

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
        else if (m.isController() && m.getControllerNumber() == 1) modWheel = (float) m.getControllerValue() / 127.f;
        else if (m.isController() && m.getControllerNumber() == 64)
        {
            pedal = m.getControllerValue() >= 64;
            if (! pedal)
                for (auto& v : voices)
                    if (v.gate && v.sustained)
                    {
                        v.gate = false; v.sustained = false; v.relCount = 0;
                        v.amp.noteOff(); v.env2.noteOff(); v.env3.noteOff();
                    }
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            heldCount = 0;
            pedal = false;
            for (auto& v : voices)
                if (v.gate) { v.gate = false; v.relCount = 0; v.amp.noteOff(); v.env2.noteOff(); v.env3.noteOff(); }
        }
    }
    render (buffer, pos, total - pos);
    fx.process (buffer, fxs);

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
