#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include <vector>
#include "SampleLibrary.h"

// ======================================================================
//  Instruments modélisés (sans échantillons) :
//  1 Piano  2 E-Piano  3 Brass  4 Cordes pincées  5 Cordes  6 Flûte  7 Chœur
//  Chaque instrument a 4 "macros" (0..1) : inst1..inst4.
// ======================================================================

namespace inst
{
    enum Engine { Synth = 0, Piano, EPiano, Brass, Plucked, Strings, Flute, Choir, NumEngines };

    inline juce::StringArray engineNames()
    {
        return { "Synth", "Piano", "E-Piano", "Brass", "Plucked", "Strings", "Winds", "Choir" };
    }

    // ---- instruments joués avec de vrais enregistrements ----
    inline bool isSampled (int engine)
    {
        return engine == Piano || engine == Brass || engine == Plucked || engine == Strings || engine == Flute;
    }

    struct Variant { const char* name; const char* ids[3]; };

    inline const std::vector<Variant>& variants (int engine)
    {
        static const std::vector<Variant> none;
        static const std::vector<Variant> piano   { { "Grand Piano", { "piano_grand" } }, { "Upright Piano", { "piano_upright" } } };
        static const std::vector<Variant> brass   { { "Trumpet", { "trumpet" } }, { "Trombone", { "trombone" } },
                                                    { "French Horn", { "horn" } }, { "Brass Section", { "trumpet", "trombone", "horn" } } };
        static const std::vector<Variant> plucked { { "Nylon Guitar", { "guitar_nylon" } }, { "Steel Guitar", { "guitar_steel" } },
                                                    { "Harp", { "harp" } }, { "Kalimba", { "kalimba" } } };
        static const std::vector<Variant> strings { { "String Ensemble", { "strings_ens" } }, { "Tremolo Strings", { "strings_trem" } },
                                                    { "Pizzicato", { "strings_pizz" } }, { "Solo Violin", { "violin_solo" } } };
        static const std::vector<Variant> winds   { { "Flute", { "flute" } }, { "Oboe", { "oboe" } },
                                                    { "Clarinet", { "clarinet" } }, { "Tenor Sax", { "sax" } } };
        switch (engine)
        {
            case Piano: return piano;  case Brass: return brass;  case Plucked: return plucked;
            case Strings: return strings;  case Flute: return winds;  default: return none;
        }
    }

    inline int variantIndex (int engine, float macro2)
    {
        const int n = (int) variants (engine).size();
        return n == 0 ? 0 : juce::jlimit (0, n - 1, (int) (macro2 * (float) n));
    }

    // noms des 4 macros pour chaque instrument
    inline juce::StringArray macroNames (int engine, bool sampled = false)
    {
        if (sampled && isSampled (engine)) return { "Tone", "Model", "Dynamics", "Width" };
        switch (engine)
        {
            case Piano:   return { "Tone", "Detune", "Sustain", "Hammer" };
            case EPiano:  return { "Bell", "Bark", "Sustain", "Rhodes>Wurli" };
            case Brass:   return { "Bright", "Growl", "Swell", "Section" };
            case Plucked: return { "Bright", "Body", "Sustain", "Pick Pos" };
            case Strings: return { "Bright", "Ensemble", "Bow Noise", "Vibrato" };
            case Flute:   return { "Breath", "Overblow", "Vibrato", "Chiff" };
            case Choir:   return { "Vowel", "Bright", "Ensemble", "Breath" };
            default:      return { "", "", "", "" };
        }
    }

    // ---- filtre d'état variable (TPT), léger ----
    struct Svf
    {
        float a1 = 1.f, a2 = 0.f, a3 = 0.f, k = 1.f, ic1 = 0.f, ic2 = 0.f;
        void set (float fc, float q, float fs)
        {
            fc = juce::jlimit (10.f, fs * 0.45f, fc);
            const float g = std::tan (juce::MathConstants<float>::pi * fc / fs);
            k = 1.f / juce::jmax (0.05f, q);
            a1 = 1.f / (1.f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }
        void reset() { ic1 = ic2 = 0.f; }
        inline void run (float v0, float& lp, float& bp)
        {
            const float v3 = v0 - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.f * v1 - ic1;
            ic2 = 2.f * v2 - ic2;
            lp = v2;  bp = v1;
        }
        inline float lp (float x) { float l, b; run (x, l, b); return l; }
        inline float bp (float x) { float l, b; run (x, l, b); return b; }
    };

    inline float blep (float t, float dt)
    {
        if (dt <= 0.f) return 0.f;
        if (t < dt)       { t /= dt;            return t + t - t * t - 1.f; }
        if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
        return 0.f;
    }
    inline float saw (float ph, float dt) { return 2.f * ph - 1.f - blep (ph, dt); }
    inline void wrap (float& p) { if (p >= 1.f) p -= std::floor (p); }

    struct Voice
    {
        static constexpr int NP = 16;     // partiels par corde (piano)

        int engine = 0;
        float note = 60.f, vel = 1.f, t = 0.f, baseFreq = 440.f;
        juce::Random rng;

        // ---- partiels (piano) : oscillateurs récursifs ----
        int np = 0;
        float freqRef = 0.f;
        std::array<float, NP * 2> re {}, im {}, cr {}, ci {}, aF {}, aS {}, dF {}, dS {}, ratio {}, gl {}, gr {};

        // ---- bruit d'attaque ----
        float burst = 0.f, burstDec = 0.f;
        Svf burstF;

        // ---- FM (e-piano) ----
        float pc = 0.f, pm = 0.f, pt = 0.f, ampE = 1.f, ampDec = 1.f, idx0 = 0.f, idxDec = 1.f, idx = 0.f, tineA = 0.f, tineDec = 1.f;

        // ---- oscillateurs d'ensemble (brass, cordes, chœur) ----
        std::array<float, 5> ph {}, det {}, drift {}, driftT {};
        int nOsc = 1;
        float lip = 0.f, blip = 0.f;
        Svf lpA, lpB, f1, f2, f3, nf;
        int ctl = 0;

        // ---- Karplus-Strong (cordes pincées) ----
        std::vector<float> ks;
        int ksW = 0;
        float ksLp = 0.f, ksGain = 0.99f, ksC = 0.5f, ksMod = 0.f, ksModA = 0.f, ksModD = 1.f;
        Svf body1, body2;

        // ---- flûte ----
        float vibPh = 0.f;

        // ---- lecteur d'enregistrements ----
        struct Player { const SampleZone* z = nullptr; double pos = 0.0; float gain = 1.f; bool done = true; };
        bool sampled = false;
        std::array<Player, 3> pl {};
        float sLpL = 0.f, sLpR = 0.f, velGain = 1.f;
        bool finished() const
        {
            if (! sampled) return false;
            for (auto& p : pl) if (! p.done) return false;
            return true;
        }

        void prepare (double fs)
        {
            ks.assign ((size_t) (fs / 15.0) + 16, 0.f);
        }
    };

    // ------------------------------------------------------------------ note jouée avec des enregistrements
    // renvoie false si aucun enregistrement ne couvre la note
    inline bool startSampled (Voice& v, int engine, SampleInstrument* const* layers, int nLayers,
                              float note, float vel, const std::array<float, 4>& m)
    {
        int used = 0;
        const int vel127 = juce::jlimit (1, 127, (int) std::lround (vel * 127.f));
        for (auto& p : v.pl) { p.z = nullptr; p.done = true; p.pos = 0.0; }
        for (int i = 0; i < nLayers && used < 3; ++i)
            if (layers[i] != nullptr)
                if (auto* z = layers[i]->pick ((int) std::lround (note), vel127))
                {
                    auto& p = v.pl[(size_t) used++];
                    p.z = z;  p.pos = 0.0;  p.done = false;
                }
        if (used == 0) { v.sampled = false; return false; }
        for (int i = 0; i < used; ++i) v.pl[(size_t) i].gain = 1.f / std::sqrt ((float) used);
        v.engine = engine;
        v.sampled = true;
        v.note = note;  v.vel = vel;  v.t = 0.f;
        v.sLpL = v.sLpR = 0.f;
        v.velGain = std::pow (juce::jmax (0.02f, vel), 0.15f + 1.35f * m[2]);
        return true;
    }

    inline float zoneAt (const SampleZone& z, int i, int ch)
    {
        if (z.loopEnd > 0 && i >= z.loopEnd) i -= (z.loopEnd - z.loopStart);
        i = juce::jlimit (0, juce::jmax (0, z.len - 1), i);
        return (float) z.data[(size_t) i * 2 + (size_t) ch] * (1.f / 32767.f);
    }

    inline void renderSampled (Voice& v, float freq, const std::array<float, 4>& m, float fs, float& L, float& R)
    {
        L = R = 0.f;
        for (auto& p : v.pl)
        {
            if (p.done || p.z == nullptr) continue;
            const auto& z = *p.z;
            const int i = (int) p.pos;
            const float f = (float) (p.pos - (double) i);
            for (int ch = 0; ch < 2; ++ch)
            {
                // interpolation d'Hermite (4 points) : son propre même en transposant
                const float y0 = zoneAt (z, i - 1, ch), y1 = zoneAt (z, i, ch), y2 = zoneAt (z, i + 1, ch), y3 = zoneAt (z, i + 2, ch);
                const float c1 = 0.5f * (y2 - y0);
                const float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
                const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
                const float s = ((c3 * f + c2) * f + c1) * f + y1;
                (ch == 0 ? L : R) += s * p.gain;
            }
            p.pos += (double) (freq / z.keyFreq) * (z.sr / (double) fs);
            if (z.loopEnd > 0 && p.pos >= (double) z.loopEnd) p.pos -= (double) (z.loopEnd - z.loopStart);
            else if (p.pos >= (double) (z.len - 2)) p.done = true;
        }
        // tonalité (passe-bas doux), puis largeur stéréo
        if (m[0] < 0.97f)
        {
            const float fc = 900.f * std::exp2 (m[0] * 4.4f);
            const float c = 1.f - std::exp (-juce::MathConstants<float>::twoPi * fc / fs);
            v.sLpL += c * (L - v.sLpL);
            v.sLpR += c * (R - v.sLpR);
            L = v.sLpL;  R = v.sLpR;
        }
        const float mid = 0.5f * (L + R), side = 0.5f * (L - R) * (m[3] * 2.f);
        L = (mid + side) * v.velGain * 2.6f;
        R = (mid - side) * v.velGain * 2.6f;
    }

    // ------------------------------------------------------------------ démarrage d'une note
    inline void start (Voice& v, int engine, float note, float vel, const std::array<float, 4>& m, float fs)
    {
        v.sampled = false;
        v.engine = engine;
        v.note = note;
        v.vel = vel;
        v.t = 0.f;
        v.ctl = 0;
        v.vibPh = 0.f;
        const float f0 = 440.f * std::exp2 ((note - 69.f) / 12.f);
        v.baseFreq = f0;
        v.freqRef = f0;
        auto rnd = [&v] { return v.rng.nextFloat() * 2.f - 1.f; };

        switch (engine)
        {
            case Piano:
            {
                const float B = juce::jlimit (0.00002f, 0.004f, 0.00012f * std::exp2 ((note - 60.f) / 18.f));
                const float bright = juce::jlimit (0.f, 1.f, 0.25f + 0.55f * m[0] + 0.45f * vel);
                const float tilt = 2.3f - 1.7f * bright;
                const float T = juce::jlimit (0.3f, 25.f, (2.f + 14.f * m[2]) * std::exp2 (-(note - 40.f) / 20.f));
                const float detCents = (0.3f + m[1] * 3.5f);
                const float keyPan = juce::jlimit (-0.4f, 0.4f, (note - 64.f) / 60.f);

                v.np = 0;
                for (int s = 0; s < 2; ++s)
                {
                    const float sDet = std::exp2 ((s == 0 ? -0.5f : 0.5f) * detCents / 1200.f);
                    for (int n = 1; n <= Voice::NP; ++n)
                    {
                        const float r = (float) n * std::sqrt (1.f + B * (float) (n * n)) * sDet;
                        if (f0 * r > fs * 0.45f) break;
                        const int i = v.np++;
                        const float hammerPos = 0.35f + 0.65f * std::abs (std::sin (juce::MathConstants<float>::pi * (float) n / 8.3f));
                        const float a = std::pow ((float) n, -tilt) * hammerPos * 0.5f;
                        const float tauS = T / (1.f + 0.18f * std::pow ((float) n, 1.3f));
                        const float tauF = tauS * 0.16f;
                        v.ratio[(size_t) i] = r;
                        v.aF[(size_t) i] = a * 0.62f;
                        v.aS[(size_t) i] = a * 0.38f;
                        v.dF[(size_t) i] = std::exp (-1.f / (tauF * fs));
                        v.dS[(size_t) i] = std::exp (-1.f / (tauS * fs));
                        const float w = juce::MathConstants<float>::twoPi * f0 * r / fs;
                        v.cr[(size_t) i] = std::cos (w);
                        v.ci[(size_t) i] = std::sin (w);
                        v.re[(size_t) i] = 1.f;
                        v.im[(size_t) i] = 0.f;
                        const float pan = keyPan + (s == 0 ? -0.25f : 0.25f);
                        v.gl[(size_t) i] = 1.f - juce::jmax (0.f, pan);
                        v.gr[(size_t) i] = 1.f + juce::jmin (0.f, pan);
                    }
                }
                v.burst = vel * (0.08f + 0.5f * m[3]);
                v.burstDec = std::exp (-1.f / (0.006f * fs));
                v.burstF.reset();
                v.burstF.set (900.f + 4500.f * vel * (0.4f + 0.6f * m[0]), 0.9f, fs);
                break;
            }

            case EPiano:
            {
                const float T = juce::jlimit (0.4f, 14.f, (1.5f + 7.f * m[2]) * std::exp2 (-(note - 48.f) / 24.f));
                v.ampE = 1.f;
                v.ampDec = std::exp (-1.f / (T * fs));
                v.idx0 = (0.4f + 2.6f * m[1]) * std::pow (vel, 1.4f);
                v.idx = v.idx0;
                v.idxDec = std::exp (-1.f / ((0.25f + 0.35f * (1.f - vel)) * fs));
                v.tineA = m[0] * vel * 0.6f;
                v.tineDec = std::exp (-1.f / ((0.04f + 0.08f * m[0]) * fs));
                v.pc = v.pm = v.pt = 0.f;
                break;
            }

            case Brass:
            {
                v.nOsc = 1 + (int) std::lround (m[3] * 2.f);
                for (int k = 0; k < v.nOsc; ++k)
                {
                    v.ph[(size_t) k] = v.rng.nextFloat();
                    v.det[(size_t) k] = v.nOsc > 1 ? ((float) k / (float) (v.nOsc - 1) * 2.f - 1.f) * 9.f : 0.f;
                    v.drift[(size_t) k] = 0.f;
                }
                v.lip = 0.f;
                v.blip = -0.35f * (1.f - 0.5f * m[2]);
                v.lpA.reset();  v.lpB.reset();  v.f1.reset();  v.nf.reset();
                v.f1.set (1250.f, 1.4f, fs);
                v.nf.set (1600.f, 0.8f, fs);
                break;
            }

            case Plucked:
            {
                const float L = juce::jlimit (2.f, (float) v.ks.size() - 4.f, fs / f0);
                const int Li = (int) L;
                const float bright = juce::jlimit (0.05f, 1.f, 0.2f + 0.8f * m[0] * (0.5f + 0.5f * vel));
                std::fill (v.ks.begin(), v.ks.end(), 0.f);
                float y = 0.f;
                for (int i = 0; i <= Li; ++i)
                {
                    y += bright * (rnd() - y);
                    v.ks[(size_t) i] = y * vel;
                }
                // position du médiator : filtre en peigne (calculé à l'envers, sans mémoire en plus)
                const int P = juce::jlimit (1, juce::jmax (1, Li - 1), (int) ((float) Li * (0.06f + 0.4f * m[3])));
                for (int i = Li; i >= P; --i)
                    v.ks[(size_t) i] -= v.ks[(size_t) (i - P)];
                v.ksW = Li + 1;
                v.ksLp = 0.f;
                const float harp = juce::jlimit (0.f, 1.f, 1.f - std::abs (m[1] - 0.5f) * 2.f);
                const float T60 = juce::jlimit (0.3f, 20.f, (0.8f + 9.f * m[2]) * (1.f + harp * 0.8f) * std::exp2 (-(note - 48.f) / 30.f));
                v.ksGain = std::pow (10.f, -3.f / (T60 * f0));
                v.ksC = 0.35f + 0.6f * m[0];
                const float kal = juce::jlimit (0.f, 1.f, (m[1] - 0.6f) * 2.5f);
                v.ksModA = kal * 0.35f * vel;
                v.ksModD = std::exp (-1.f / ((0.12f + 0.2f * kal) * fs));
                v.ksMod = 0.f;
                v.body1.reset();  v.body2.reset();
                v.body1.set (110.f, 2.5f, fs);
                v.body2.set (240.f, 3.f, fs);
                v.blip = kal * 0.25f;   // le kalimba "tombe" légèrement à l'attaque
                break;
            }

            case Strings:
            case Choir:
            {
                v.nOsc = engine == Strings ? 2 + (int) std::lround (m[1] * 3.f) : 3;
                const float spread = engine == Strings ? (4.f + m[1] * 10.f) : (3.f + m[2] * 12.f);
                for (int k = 0; k < v.nOsc; ++k)
                {
                    v.ph[(size_t) k] = v.rng.nextFloat();
                    v.det[(size_t) k] = v.nOsc > 1 ? ((float) k / (float) (v.nOsc - 1) * 2.f - 1.f) * spread : 0.f;
                    v.drift[(size_t) k] = rnd() * 3.f;
                    v.driftT[(size_t) k] = rnd() * 3.f;
                }
                v.lpA.reset();  v.lpB.reset();  v.f1.reset();  v.f2.reset();  v.f3.reset();  v.nf.reset();
                if (engine == Strings)
                {
                    v.f1.set (280.f, 2.f, fs);
                    v.nf.set (2200.f, 2.f, fs);
                }
                break;
            }

            case Flute:
            {
                v.pc = 0.f;
                v.burst = 0.2f + 0.5f * m[3];
                v.burstDec = std::exp (-1.f / (0.035f * fs));
                v.burstF.reset();
                v.burstF.set (f0 * 3.f, 1.5f, fs);
                v.nf.reset();
                v.nf.set (f0 * 2.f, 3.f, fs);
                v.lpA.reset();
                break;
            }
            default: break;
        }
    }

    // formants des voyelles A E I O U (Hz)
    inline void vowel (float pos, float& F1, float& F2, float& F3)
    {
        static const float tbl[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 1900, 2800 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
        pos = juce::jlimit (0.f, 0.999f, pos) * 4.f;
        const int i = (int) pos;
        const float f = pos - (float) i;
        const int j = juce::jmin (4, i + 1);
        F1 = tbl[i][0] + (tbl[j][0] - tbl[i][0]) * f;
        F2 = tbl[i][1] + (tbl[j][1] - tbl[i][1]) * f;
        F3 = tbl[i][2] + (tbl[j][2] - tbl[i][2]) * f;
    }

    // ------------------------------------------------------------------ un échantillon (stéréo)
    inline void render (Voice& v, float freq, const std::array<float, 4>& m, float fs, float& L, float& R)
    {
        if (v.sampled) { renderSampled (v, freq, m, fs, L, R); return; }
        const float twoPi = juce::MathConstants<float>::twoPi;
        const float dtT = 1.f / fs;
        v.t += dtT;
        L = R = 0.f;
        auto rnd = [&v] { return v.rng.nextFloat() * 2.f - 1.f; };

        switch (v.engine)
        {
            case Piano:
            {
                // si la hauteur change (bend, glide) on recalcule les rotations
                if (std::abs (freq - v.freqRef) > v.freqRef * 0.0003f)
                {
                    v.freqRef = freq;
                    for (int i = 0; i < v.np; ++i)
                    {
                        const float w = twoPi * freq * v.ratio[(size_t) i] / fs;
                        v.cr[(size_t) i] = std::cos (w);
                        v.ci[(size_t) i] = std::sin (w);
                    }
                }
                const bool renorm = (++v.ctl & 63) == 0;
                for (int i = 0; i < v.np; ++i)
                {
                    const size_t k = (size_t) i;
                    const float a = v.aF[k] + v.aS[k];
                    if (a < 0.00001f) continue;
                    const float nr = v.re[k] * v.cr[k] - v.im[k] * v.ci[k];
                    const float ni = v.re[k] * v.ci[k] + v.im[k] * v.cr[k];
                    v.re[k] = nr;  v.im[k] = ni;
                    if (renorm)
                    {
                        const float mag = std::sqrt (nr * nr + ni * ni);
                        if (mag > 0.f) { v.re[k] /= mag; v.im[k] /= mag; }
                    }
                    const float s = ni * a;
                    L += s * v.gl[k];
                    R += s * v.gr[k];
                    v.aF[k] *= v.dF[k];
                    v.aS[k] *= v.dS[k];
                }
                if (v.burst > 0.0001f)
                {
                    const float h = v.burstF.bp (rnd()) * v.burst;
                    v.burst *= v.burstDec;
                    L += h;  R += h;
                }
                break;
            }

            case EPiano:
            {
                const float mod = std::sin (twoPi * v.pm);
                float y = std::sin (twoPi * v.pc + v.idx * mod);
                // Wurli : plus carré et nasal
                const float wur = m[3];
                if (wur > 0.001f) y = (1.f - wur) * y + wur * std::tanh (y * (2.f + 3.f * v.vel)) * 0.8f;
                // "bark" : saturation asymétrique quand on joue fort
                y = std::tanh ((y + 0.3f * m[1] * y * y) * (1.f + m[1] * v.vel * 1.5f));
                const float tine = v.tineA * std::sin (twoPi * v.pt);
                const float s = (y * 0.55f + tine) * v.ampE * 1.35f;
                L = R = s;
                v.pc += freq / fs;          wrap (v.pc);
                v.pm += freq / fs;          wrap (v.pm);
                v.pt += freq * 14.03f / fs; wrap (v.pt);
                v.ampE *= v.ampDec;
                v.idx = v.idx0 * 0.2f + (v.idx - v.idx0 * 0.2f) * v.idxDec;
                v.tineA *= v.tineDec;
                break;
            }

            case Brass:
            {
                // lèvres : montée avec un petit dépassement ("blat") puis stabilisation
                const float swell = 0.012f + m[2] * 0.12f;
                const float target = v.t < swell * 1.4f ? 1.25f : 0.85f;
                v.lip += (target - v.lip) * (1.f - std::exp (-dtT / (swell * 0.5f)));
                v.blip *= std::exp (-dtT / 0.035f);
                const float vibAmt = juce::jlimit (0.f, 1.f, (v.t - 0.35f) * 2.f) * 0.12f;
                v.vibPh += 5.3f / fs;  wrap (v.vibPh);
                const float pitch = std::exp2 ((v.blip + vibAmt * std::sin (twoPi * v.vibPh)) / 12.f);

                float s = 0.f, sl = 0.f, sr = 0.f;
                for (int k = 0; k < v.nOsc; ++k)
                {
                    const float f = freq * pitch * std::exp2 (v.det[(size_t) k] / 1200.f);
                    const float dt = f / fs;
                    const float o = saw (v.ph[(size_t) k], dt);
                    v.ph[(size_t) k] += dt;  wrap (v.ph[(size_t) k]);
                    s += o;
                    if (k % 2 == 0) sl += o; else sr += o;
                }
                const float n = 1.f / std::sqrt ((float) v.nOsc);
                if ((v.ctl++ & 3) == 0)
                {
                    const float fc = freq * (1.2f + (2.f + 14.f * m[0]) * v.lip * (0.35f + 0.65f * v.vel));
                    v.lpA.set (fc, 0.7f, fs);
                    v.lpB.set (fc * 1.3f, 0.6f, fs);
                }
                auto shape = [&] (float x) { return v.lpB.lp (v.lpA.lp (x)); };
                float y = shape (s * n);
                y += 0.45f * v.f1.bp (y);                                   // résonance cuivrée
                const float growl = 1.f + m[1] * 0.3f * std::sin (twoPi * 27.f * v.t) * juce::jmin (1.f, v.lip);
                y = y * growl + m[1] * 0.06f * v.nf.bp (rnd()) * v.lip;    // souffle
                y *= 0.6f + 0.4f * juce::jmin (1.2f, v.lip);
                const float width = v.nOsc > 1 ? 0.25f : 0.f;
                const float bal = (sl - sr) * n * width * 0.3f;
                L = y + bal;  R = y - bal;
                break;
            }

            case Plucked:
            {
                v.blip *= std::exp (-dtT / 0.05f);
                const float fr = freq * std::exp2 (v.blip / 12.f);
                const float Ld = juce::jlimit (2.f, (float) v.ks.size() - 4.f, fs / fr);
                const int size = (int) v.ks.size();
                float rp = (float) v.ksW - Ld;
                while (rp < 0.f) rp += (float) size;
                int i0 = (int) rp;
                const float frac = rp - (float) i0;
                if (i0 >= size) i0 -= size;
                int i1 = i0 + 1;
                if (i1 >= size) i1 = 0;
                const float out = v.ks[(size_t) i0] + frac * (v.ks[(size_t) i1] - v.ks[(size_t) i0]);
                v.ksLp += v.ksC * (out - v.ksLp);
                v.ks[(size_t) v.ksW] = v.ksLp * v.ksGain;
                if (++v.ksW >= size) v.ksW = 0;

                // caisse de résonance (guitare) -> plus discrète pour harpe / kalimba
                const float guitar = juce::jlimit (0.f, 1.f, 1.f - m[1] * 2.f);
                float y = out + guitar * (0.35f * v.body1.bp (out) + 0.25f * v.body2.bp (out));
                // lame métallique du kalimba
                if (v.ksModA > 0.0001f)
                {
                    v.ksMod += freq * 5.8f / fs;  wrap (v.ksMod);
                    y += v.ksModA * std::sin (twoPi * v.ksMod);
                    v.ksModA *= v.ksModD;
                }
                L = R = y * 2.f;
                break;
            }

            case Strings:
            case Choir:
            {
                const bool choir = v.engine == Choir;
                // vibrato qui arrive doucement
                const float vibDepth = (choir ? 0.08f : m[3] * 0.25f) * juce::jlimit (0.f, 1.f, (v.t - 0.25f) * 2.f);
                v.vibPh += (choir ? 4.8f : 5.6f) / fs;  wrap (v.vibPh);
                const float vib = vibDepth * std::sin (twoPi * v.vibPh);

                float sl = 0.f, sr = 0.f;
                for (int k = 0; k < v.nOsc; ++k)
                {
                    const size_t kk = (size_t) k;
                    // dérive lente et aléatoire de chaque musicien
                    v.drift[kk] += (v.driftT[kk] - v.drift[kk]) * 0.00005f;
                    if (v.rng.nextFloat() < 0.0002f) v.driftT[kk] = rnd() * 3.f;
                    const float f = freq * std::exp2 ((v.det[kk] + v.drift[kk]) / 1200.f + vib / 12.f);
                    const float dt = f / fs;
                    const float o = saw (v.ph[kk], dt);
                    v.ph[kk] += dt;  wrap (v.ph[kk]);
                    if (k % 2 == 0) sl += o; else sr += o;
                }
                const float n = 1.f / std::sqrt ((float) v.nOsc);
                sl *= n;  sr *= n;
                const float mono = 0.5f * (sl + sr);

                if ((v.ctl++ & 15) == 0)
                {
                    if (choir)
                    {
                        float F1, F2, F3;
                        vowel (m[0], F1, F2, F3);
                        v.f1.set (F1, 6.f, fs);
                        v.f2.set (F2, 8.f, fs);
                        v.f3.set (F3, 9.f, fs);
                        v.lpA.set (1800.f + 3500.f * m[1], 0.6f, fs);
                    }
                    else
                    {
                        const float fc = juce::jmin (fs * 0.45f, freq * (2.f + 6.f * m[0]) + 1200.f * m[0]);
                        v.lpA.set (fc, 0.6f, fs);
                        v.lpB.set (fc, 0.6f, fs);
                        v.f2.set (fc * 1.25f, 0.6f, fs);   // 2e étage : cordes plus douces (24 dB/oct)
                        v.f3.set (fc * 1.25f, 0.6f, fs);
                    }
                }

                if (choir)
                {
                    v.burst += 0.2f * (rnd() - v.burst);                              // souffle adouci
                    const float br = m[3] * 0.15f * v.burst;
                    const float src = mono + br;
                    float y = 1.0f * v.f1.bp (src) + 0.55f * v.f2.bp (src) + (0.2f + 0.4f * m[1]) * v.f3.bp (src);
                    y = v.lpA.lp (y) * 1.6f;
                    const float side = (sl - sr) * 0.15f;
                    L = y + side;  R = y - side;
                }
                else
                {
                    const float body = 0.45f * v.f1.bp (mono);                       // chaleur de la caisse
                    const float yl = v.f2.lp (v.lpA.lp (sl)), yr = v.f3.lp (v.lpB.lp (sr));
                    const float bow = m[2] * 0.03f * v.nf.bp (rnd()) * (0.7f + 0.3f * std::sin (twoPi * 0.7f * v.t));
                    L = (yl + body + bow) * 1.3f;
                    R = (yr + body + bow) * 1.3f;
                }
                break;
            }

            case Flute:
            {
                const float vibDepth = m[2] * 0.15f * juce::jlimit (0.f, 1.f, (v.t - 0.2f) * 2.5f);
                v.vibPh += 5.f / fs;  wrap (v.vibPh);
                const float vs = std::sin (twoPi * v.vibPh);
                const float f = freq * std::exp2 (vibDepth * vs / 12.f);
                const float p = twoPi * v.pc;
                float y = std::sin (p) + (0.08f + 0.35f * m[1]) * std::sin (2.f * p) + 0.12f * m[1] * std::sin (3.f * p);
                v.pc += f / fs;  wrap (v.pc);
                y *= 1.f + m[2] * 0.1f * vs;
                const float breath = m[0] * (0.25f * v.nf.bp (rnd()) + 0.04f * rnd());
                float chiff = 0.f;
                if (v.burst > 0.0001f) { chiff = v.burstF.bp (rnd()) * v.burst; v.burst *= v.burstDec; }
                const float s = (y * 0.6f + breath + chiff);
                L = R = s;
                break;
            }
            default: break;
        }
    }
}
