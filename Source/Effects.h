#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>

// Réglages des 10 effets, lus une fois par bloc
struct FxSettings
{
    bool  distOn = false;  float distDrive = 0.5f, distTone = 6000.f, distMix = 1.f;
    bool  crushOn = false; float crushBits = 8.f, crushRate = 4.f, crushMix = 1.f;
    bool  eqOn = false;    float eqLow = 0.f, eqMid = 0.f, eqHigh = 0.f;
    bool  compOn = false;  float compThresh = -18.f, compRatio = 4.f, compGain = 4.f;
    bool  chorOn = false;  float chorRate = 0.8f, chorDepth = 0.4f, chorMix = 0.5f;
    bool  flanOn = false;  float flanRate = 0.3f, flanDepth = 0.5f, flanFb = 0.6f, flanMix = 0.5f;
    bool  phasOn = false;  float phasRate = 0.4f, phasDepth = 0.6f, phasFb = 0.4f, phasMix = 0.5f;
    bool  tremOn = false;  float tremRate = 4.f, tremDepth = 0.5f, tremPan = 0.f;
    bool  dlyOn = false;   float dlyTime = 375.f, dlyFb = 0.4f, dlyMix = 0.3f;
    bool  revOn = false;   float revSize = 0.6f, revDamp = 0.5f, revWidth = 1.f, revMix = 0.3f;
};

// Ligne de retard modulée en stéréo (chorus et flanger)
class ModDelay
{
public:
    void prepare (double sampleRate, float maxMs)
    {
        fs = (float) sampleRate;
        size = juce::jmax (8, (int) (maxMs * 0.001f * fs) + 8);
        buf.setSize (2, size);
        buf.clear();
        w = 0;
        ph = 0.f;
    }

    void process (float* L, float* R, int n, float rate, float centreMs, float depthMs, float fb, float mix)
    {
        auto* bl = buf.getWritePointer (0);
        auto* br = buf.getWritePointer (1);
        const float twoPi = juce::MathConstants<float>::twoPi;

        for (int i = 0; i < n; ++i)
        {
            const float lfoL = std::sin (twoPi * ph);
            const float lfoR = std::sin (twoPi * (ph + 0.25f));
            ph += rate / fs;
            if (ph >= 1.f) ph -= 1.f;

            for (int ch = 0; ch < 2; ++ch)
            {
                float* b = ch == 0 ? bl : br;
                float* io = ch == 0 ? L : R;
                const float lfo = ch == 0 ? lfoL : lfoR;

                float d = (centreMs + depthMs * lfo) * 0.001f * fs;
                d = juce::jlimit (1.f, (float) (size - 3), d);
                float rp = (float) w - d;
                if (rp < 0.f) rp += (float) size;
                int i0 = (int) rp;
                const float frac = rp - (float) i0;
                if (i0 >= size) i0 -= size;
                int i1 = i0 + 1;
                if (i1 >= size) i1 = 0;
                const float y = b[i0] + frac * (b[i1] - b[i0]);

                const float x = io[i];
                b[w] = x + y * fb;
                io[i] = x * (1.f - mix) + y * mix * (1.f - 0.6f * fb);   // compense la résonance
            }

            if (++w >= size) w = 0;
        }
    }

private:
    juce::AudioBuffer<float> buf;
    float fs = 44100.f, ph = 0.f;
    int size = 8, w = 0;
};

class FxChain
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) kChunk, 2 };

        comp.prepare (spec);
        comp.setAttack (10.f);
        comp.setRelease (120.f);
        phaser.prepare (spec);
        phaser.setCentreFrequency (900.f);

        chorus.prepare (sampleRate, 40.f);
        flanger.prepare (sampleRate, 20.f);

        dlySize = juce::jmax (8, (int) (2.1f * fs));
        dly.setSize (2, dlySize);
        dly.clear();
        dlyW = 0;
        dlySmooth = 375.f * 0.001f * fs;

        reverb.setSampleRate (sampleRate);
        reverb.reset();

        for (auto& f : eq) { f.reset(); }
        lastLow = lastMid = lastHigh = 1000.f;   // force le calcul des filtres

        distLpL = distLpR = dlyLpL = dlyLpR = 0.f;
        crushHoldL = crushHoldR = 0.f;
        crushCount = 0;
        tremPh = 0.f;
    }

    void process (juce::AudioBuffer<float>& buffer, const FxSettings& s)
    {
        if (buffer.getNumChannels() < 2) return;
        const int total = buffer.getNumSamples();

        for (int start = 0; start < total; start += kChunk)
        {
            const int n = juce::jmin (kChunk, total - start);
            float* L = buffer.getWritePointer (0, start);
            float* R = buffer.getWritePointer (1, start);
            processChunk (L, R, n, s);
        }
    }

private:
    static constexpr int kChunk = 256;

    void processChunk (float* L, float* R, int n, const FxSettings& s)
    {
        const float twoPi = juce::MathConstants<float>::twoPi;

        // 1. Distortion
        if (s.distOn)
        {
            const float k = 1.f + s.distDrive * 24.f;
            const float norm = 1.f / std::tanh (k);
            const float a = 1.f - std::exp (-twoPi * s.distTone / fs);
            for (int i = 0; i < n; ++i)
            {
                const float wl = std::tanh (L[i] * k) * norm;
                const float wr = std::tanh (R[i] * k) * norm;
                distLpL += a * (wl - distLpL);
                distLpR += a * (wr - distLpR);
                L[i] += s.distMix * (distLpL - L[i]);
                R[i] += s.distMix * (distLpR - R[i]);
            }
        }

        // 2. Bitcrusher
        if (s.crushOn)
        {
            const float levels = std::exp2 (s.crushBits - 1.f);
            const int hold = juce::jmax (1, (int) s.crushRate);
            for (int i = 0; i < n; ++i)
            {
                if (++crushCount >= hold)
                {
                    crushCount = 0;
                    crushHoldL = std::round (L[i] * levels) / levels;
                    crushHoldR = std::round (R[i] * levels) / levels;
                }
                L[i] += s.crushMix * (crushHoldL - L[i]);
                R[i] += s.crushMix * (crushHoldR - R[i]);
            }
        }

        // 3. EQ 3 bandes
        if (s.eqOn)
        {
            updateEq (s);
            for (int i = 0; i < n; ++i)
            {
                float l = L[i], r = R[i];
                for (int b = 0; b < 3; ++b)
                {
                    l = eq[(size_t) (b * 2)].processSample (l);
                    r = eq[(size_t) (b * 2 + 1)].processSample (r);
                }
                L[i] = l;
                R[i] = r;
            }
        }

        // 4. Compresseur
        if (s.compOn)
        {
            comp.setThreshold (s.compThresh);
            comp.setRatio (juce::jmax (1.f, s.compRatio));
            float* chans[] = { L, R };
            juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
            comp.process (juce::dsp::ProcessContextReplacing<float> (block));
            const float g = juce::Decibels::decibelsToGain (s.compGain);
            for (int i = 0; i < n; ++i) { L[i] *= g; R[i] *= g; }
        }

        // 5. Chorus
        if (s.chorOn)
            chorus.process (L, R, n, s.chorRate, 14.f, 1.f + s.chorDepth * 9.f, 0.1f, s.chorMix);

        // 6. Flanger
        if (s.flanOn)
            flanger.process (L, R, n, s.flanRate, 3.f, 0.2f + s.flanDepth * 2.6f, s.flanFb, s.flanMix);

        // 7. Phaser
        if (s.phasOn)
        {
            phaser.setRate (s.phasRate);
            phaser.setDepth (s.phasDepth);
            phaser.setFeedback (s.phasFb);
            phaser.setMix (s.phasMix);
            float* chans[] = { L, R };
            juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
            phaser.process (juce::dsp::ProcessContextReplacing<float> (block));
        }

        // 8. Tremolo / auto-pan
        if (s.tremOn)
        {
            for (int i = 0; i < n; ++i)
            {
                const float lfo = std::sin (twoPi * tremPh);
                tremPh += s.tremRate / fs;
                if (tremPh >= 1.f) tremPh -= 1.f;

                const float trem = 1.f - s.tremDepth * (0.5f - 0.5f * lfo);
                const float panL = 1.f - s.tremDepth * (0.5f + 0.5f * lfo);
                const float panR = 1.f - s.tremDepth * (0.5f - 0.5f * lfo);
                L[i] *= (1.f - s.tremPan) * trem + s.tremPan * panL;
                R[i] *= (1.f - s.tremPan) * trem + s.tremPan * panR;
            }
        }

        // 9. Delay ping-pong
        if (s.dlyOn)
        {
            auto* bl = dly.getWritePointer (0);
            auto* br = dly.getWritePointer (1);
            const float target = juce::jlimit (1.f, (float) (dlySize - 3), s.dlyTime * 0.001f * fs);
            const float a = 1.f - std::exp (-twoPi * 5000.f / fs);

            for (int i = 0; i < n; ++i)
            {
                dlySmooth += 0.0005f * (target - dlySmooth);
                float rp = (float) dlyW - dlySmooth;
                if (rp < 0.f) rp += (float) dlySize;
                int i0 = (int) rp;
                const float frac = rp - (float) i0;
                if (i0 >= dlySize) i0 -= dlySize;
                int i1 = i0 + 1;
                if (i1 >= dlySize) i1 = 0;

                const float yl = bl[i0] + frac * (bl[i1] - bl[i0]);
                const float yr = br[i0] + frac * (br[i1] - br[i0]);
                dlyLpL += a * (yl - dlyLpL);
                dlyLpR += a * (yr - dlyLpR);

                const float in = 0.5f * (L[i] + R[i]);
                bl[dlyW] = in + dlyLpR * s.dlyFb;   // gauche -> droite -> gauche
                br[dlyW] = dlyLpL * s.dlyFb;

                L[i] += s.dlyMix * yl;
                R[i] += s.dlyMix * yr;
                if (++dlyW >= dlySize) dlyW = 0;
            }
        }

        // 10. Reverb
        if (s.revOn)
        {
            juce::Reverb::Parameters p;
            p.roomSize = s.revSize;
            p.damping = s.revDamp;
            p.width = s.revWidth;
            p.wetLevel = s.revMix;
            p.dryLevel = 1.f - s.revMix * 0.5f;
            p.freezeMode = 0.f;
            reverb.setParameters (p);
            reverb.processStereo (L, R, n);
        }
    }

    void updateEq (const FxSettings& s)
    {
        using C = juce::dsp::IIR::Coefficients<float>;
        const double sr = (double) fs;
        if (s.eqLow != lastLow)
        {
            auto c = C::makeLowShelf (sr, 200.0, 0.707f, juce::Decibels::decibelsToGain (s.eqLow));
            eq[0].coefficients = c; eq[1].coefficients = c;
            lastLow = s.eqLow;
        }
        if (s.eqMid != lastMid)
        {
            auto c = C::makePeakFilter (sr, 1500.0, 0.8f, juce::Decibels::decibelsToGain (s.eqMid));
            eq[2].coefficients = c; eq[3].coefficients = c;
            lastMid = s.eqMid;
        }
        if (s.eqHigh != lastHigh)
        {
            auto c = C::makeHighShelf (sr, 6000.0, 0.707f, juce::Decibels::decibelsToGain (s.eqHigh));
            eq[4].coefficients = c; eq[5].coefficients = c;
            lastHigh = s.eqHigh;
        }
    }

    float fs = 44100.f;
    juce::dsp::Compressor<float> comp;
    juce::dsp::Phaser<float> phaser;
    ModDelay chorus, flanger;
    juce::Reverb reverb;
    std::array<juce::dsp::IIR::Filter<float>, 6> eq;
    float lastLow = 1000.f, lastMid = 1000.f, lastHigh = 1000.f;

    juce::AudioBuffer<float> dly;
    int dlySize = 8, dlyW = 0;
    float dlySmooth = 1000.f, dlyLpL = 0.f, dlyLpR = 0.f;

    float distLpL = 0.f, distLpR = 0.f;
    float crushHoldL = 0.f, crushHoldR = 0.f;
    int crushCount = 0;
    float tremPh = 0.f;
};
