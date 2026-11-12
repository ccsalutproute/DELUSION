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

    // taps > 1 : plusieurs voix de chorus décalées (son plus large et plus riche)
    void process (float* L, float* R, int n, float rate, float centreMs, float depthMs, float fb, float mix, int taps = 1)
    {
        taps = juce::jlimit (1, 4, taps);
        auto* bl = buf.getWritePointer (0);
        auto* br = buf.getWritePointer (1);
        const float twoPi = juce::MathConstants<float>::twoPi;

        for (int i = 0; i < n; ++i)
        {
            ph += rate / fs;
            if (ph >= 1.f) ph -= 1.f;

            for (int ch = 0; ch < 2; ++ch)
            {
                float* b = ch == 0 ? bl : br;
                float* io = ch == 0 ? L : R;

                float y = 0.f;
                for (int t = 0; t < taps; ++t)
                {
                    const float lfo = std::sin (twoPi * (ph + (float) t / (float) taps + (ch == 0 ? 0.f : 0.25f)));
                    float d = (centreMs * (1.f + 0.18f * (float) t) + depthMs * lfo) * 0.001f * fs;
                    d = juce::jlimit (1.f, (float) (size - 3), d);
                    float rp = (float) w - d;
                    if (rp < 0.f) rp += (float) size;
                    int i0 = (int) rp;
                    const float frac = rp - (float) i0;
                    if (i0 >= size) i0 -= size;
                    int i1 = i0 + 1;
                    if (i1 >= size) i1 = 0;
                    y += b[i0] + frac * (b[i1] - b[i0]);
                }
                y /= (float) taps;

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

// Reverb à réseau de retards (FDN 8 lignes) : dense, douce, modulée, stéréo
class FdnReverb
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        const int maxLen = (int) (0.16f * fs) + 8;
        for (auto& l : lines) { l.assign ((size_t) maxLen, 0.f); }
        for (size_t i = 0; i < ap.size(); ++i) ap[i].assign ((size_t) ((float) apMs[i] * 0.001f * fs) + 4, 0.f);
        pre.assign ((size_t) (0.06f * fs) + 4, 0.f);
        reset();
    }

    void reset()
    {
        for (auto& l : lines) std::fill (l.begin(), l.end(), 0.f);
        for (auto& a : ap) std::fill (a.begin(), a.end(), 0.f);
        std::fill (pre.begin(), pre.end(), 0.f);
        w = 0; preW = 0; apW.fill (0); lp.fill (0.f); hp = 0.f; modPh = 0.f;
    }

    void process (float* L, float* R, int n, float size, float damp, float width, float mix)
    {
        const float scale = 0.45f + size * 1.25f;
        const float rt = 0.35f + size * size * 7.5f;
        const float c = 0.08f + 0.85f * (1.f - damp);
        const int maxLen = (int) lines[0].size();
        std::array<float, 8> len {}, g {};
        for (int i = 0; i < 8; ++i)
        {
            len[(size_t) i] = juce::jlimit (8.f, (float) maxLen - 4.f, baseMs[i] * 0.001f * fs * scale);
            g[(size_t) i] = std::pow (10.f, -3.f * len[(size_t) i] / (rt * fs));
        }
        const int preLen = juce::jlimit (1, (int) pre.size() - 1, (int) ((0.005f + 0.03f * size) * fs));
        const float twoPi = juce::MathConstants<float>::twoPi;

        for (int s = 0; s < n; ++s)
        {
            // entrée mono, coupe-bas, pré-délai
            float in = 0.5f * (L[s] + R[s]);
            hp += 0.01f * (in - hp);
            in -= hp;
            pre[(size_t) preW] = in;
            int pr = preW - preLen; if (pr < 0) pr += (int) pre.size();
            in = pre[(size_t) pr];
            if (++preW >= (int) pre.size()) preW = 0;

            // diffusion : 4 passe-tout en série
            for (size_t a = 0; a < ap.size(); ++a)
            {
                auto& buf = ap[a];
                const int sz = (int) buf.size() - 1;
                int rp = apW[a] - sz + 1; if (rp < 0) rp += (int) buf.size();
                const float d = buf[(size_t) rp];
                const float v = in + 0.6f * d;
                buf[(size_t) apW[a]] = v;
                in = d - 0.6f * v;
                if (++apW[a] >= (int) buf.size()) apW[a] = 0;
            }

            // lecture des 8 lignes (deux sont modulées)
            modPh += 0.6f / fs; if (modPh >= 1.f) modPh -= 1.f;
            std::array<float, 8> y {};
            for (int i = 0; i < 8; ++i)
            {
                float d = len[(size_t) i];
                if (i == 1) d += 12.f * std::sin (twoPi * modPh);
                if (i == 5) d += 12.f * std::sin (twoPi * (modPh + 0.33f));
                float rp = (float) w - d;
                while (rp < 0.f) rp += (float) maxLen;
                int i0 = (int) rp;
                const float fr = rp - (float) i0;
                if (i0 >= maxLen) i0 -= maxLen;
                const int i1 = i0 + 1 >= maxLen ? 0 : i0 + 1;
                const auto& l = lines[(size_t) i];
                float v = l[(size_t) i0] + fr * (l[(size_t) i1] - l[(size_t) i0]);
                lp[(size_t) i] += c * (v - lp[(size_t) i]);
                y[(size_t) i] = lp[(size_t) i] * g[(size_t) i];
            }

            // matrice de Householder (mélange sans perte)
            float sum = 0.f;
            for (auto v : y) sum += v;
            sum *= 0.25f;
            for (int i = 0; i < 8; ++i)
                lines[(size_t) i][(size_t) w] = (y[(size_t) i] - sum) + in * ((i & 1) ? -0.35f : 0.35f);
            if (++w >= maxLen) w = 0;

            const float outL = (y[0] + y[2] + y[4] + y[6]) * 0.35f;
            const float outR = (y[1] + y[3] + y[5] + y[7]) * 0.35f;
            const float mid = 0.5f * (outL + outR), side = 0.5f * (outL - outR) * width;
            L[s] = L[s] * (1.f - mix * 0.5f) + (mid + side) * mix;
            R[s] = R[s] * (1.f - mix * 0.5f) + (mid - side) * mix;
        }
    }

private:
    static constexpr float baseMs[8] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f };
    static constexpr int apMs[4] = { 5, 7, 11, 13 };
    float fs = 44100.f;
    std::array<std::vector<float>, 8> lines;
    std::array<std::vector<float>, 4> ap;
    std::vector<float> pre;
    std::array<int, 4> apW {};
    std::array<float, 8> lp {};
    int w = 0, preW = 0;
    float hp = 0.f, modPh = 0.f;
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

        reverb.prepare (sampleRate);
        oversampler.initProcessing ((size_t) kChunk);
        oversampler.reset();

        for (auto& f : eq) { f.reset(); }
        lastLow = lastMid = lastHigh = 1000.f;   // force le calcul des filtres

        distLpL = distLpR = dlyLpL = dlyLpR = dlyHpL = dlyHpR = 0.f;
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

        // 1. Distortion (suréchantillonnée x4 : pas de grésillement numérique)
        if (s.distOn)
        {
            const float k = 1.f + s.distDrive * 24.f;
            const float norm = 1.f / std::tanh (k);
            const float a = 1.f - std::exp (-twoPi * s.distTone / (fs * 4.f));
            float* chans[] = { L, R };
            juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
            auto up = oversampler.processSamplesUp (block);
            for (size_t ch = 0; ch < 2; ++ch)
            {
                auto* d = up.getChannelPointer (ch);
                float& st = ch == 0 ? distLpL : distLpR;
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                {
                    const float x = d[i];
                    const float wv = std::tanh (x * k) * norm;
                    st += a * (wv - st);
                    d[i] = x + s.distMix * (st - x);
                }
            }
            oversampler.processSamplesDown (block);
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
            chorus.process (L, R, n, s.chorRate, 12.f, 1.f + s.chorDepth * 8.f, 0.05f, s.chorMix, 3);

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
                // coupe-bas dans la boucle : les répétitions restent claires
                dlyHpL += 0.015f * (dlyLpL - dlyHpL);
                dlyHpR += 0.015f * (dlyLpR - dlyHpR);

                const float in = 0.5f * (L[i] + R[i]);
                bl[dlyW] = in + (dlyLpR - dlyHpR) * s.dlyFb;   // gauche -> droite -> gauche
                br[dlyW] = (dlyLpL - dlyHpL) * s.dlyFb;

                L[i] += s.dlyMix * yl;
                R[i] += s.dlyMix * yr;
                if (++dlyW >= dlySize) dlyW = 0;
            }
        }

        // 10. Reverb
        if (s.revOn)
        {
            reverb.process (L, R, n, s.revSize, s.revDamp, s.revWidth, s.revMix);
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
    FdnReverb reverb;
    juce::dsp::Oversampling<float> oversampler { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true };
    std::array<juce::dsp::IIR::Filter<float>, 6> eq;
    float lastLow = 1000.f, lastMid = 1000.f, lastHigh = 1000.f;

    juce::AudioBuffer<float> dly;
    int dlySize = 8, dlyW = 0;
    float dlySmooth = 1000.f, dlyLpL = 0.f, dlyLpR = 0.f, dlyHpL = 0.f, dlyHpR = 0.f;

    float distLpL = 0.f, distLpR = 0.f;
    float crushHoldL = 0.f, crushHoldR = 0.f;
    int crushCount = 0;
    float tremPh = 0.f;
};
