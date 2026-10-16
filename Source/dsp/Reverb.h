// Freeze Shimmer - réverbération FDN 8 lignes (matrice de Householder)
// diffusion d'entrée, amortissement par ligne, modulation, mode "infini".
#pragma once
#include "Util.h"

namespace fs
{
class Reverb
{
public:
    static constexpr int N = 8;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const double k = sr / 44100.0;
        static const int baseLen[N] = { 1487, 1759, 2083, 2381, 2689, 3001, 3331, 3643 };
        static const int apLen[4]   = { 142, 107, 379, 277 };

        for (int i = 0; i < N; ++i)
        {
            len[i] = (float) (baseLen[i] * k);
            lines[i].prepare ((int) (len[i] * 1.0f) + 256);
            lines[i].clear();
            damp[i] = 0; dc[i] = 0; dcIn[i] = 0;
            gain[i] = gainTarget[i] = 0.7f;
            lfoPhase[i] = (float) i / (float) N;
        }
        for (int c = 0; c < 2; ++c)
            for (int a = 0; a < 4; ++a)
            {
                apLenS[c][a] = (int) (apLen[a] * k * (c == 0 ? 1.0 : 1.13));
                ap[c][a].assign ((size_t) apLenS[c][a], 0.0f);
                apPos[c][a] = 0;
            }
        smooth = onePoleCoef (sr, 0.05f);
        dcCoef = 1.0f - std::exp (-kTwoPi * 18.0f / (float) sr);
        setParams (2.0f, false, 0.4f, 0.3f, 0.3f);
        for (int i = 0; i < N; ++i) gain[i] = gainTarget[i];
    }

    void reset()
    {
        for (int i = 0; i < N; ++i) { lines[i].clear(); damp[i] = 0; dc[i] = 0; dcIn[i] = 0; }
        for (int c = 0; c < 2; ++c)
            for (int a = 0; a < 4; ++a) std::fill (ap[c][a].begin(), ap[c][a].end(), 0.0f);
    }

    // rt60 en secondes, infinite = tank figé (gain 1), dampAmt 0..1 (1 = très sombre),
    // modDepth 0..1, modRate 0..1 (0.1 Hz .. 1.5 Hz)
    void setParams (float rt60, bool infinite, float dampAmt, float modDepth, float modRate)
    {
        inf = infinite;
        for (int i = 0; i < N; ++i)
        {
            const float seconds = len[i] / (float) sr;
            gainTarget[i] = infinite ? 0.99995f : std::pow (10.0f, -3.0f * seconds / std::max (0.05f, rt60));
        }
        const float fc = lerpf (14000.0f, 1800.0f, clampf (dampAmt, 0.0f, 1.0f));
        dampCoef = 1.0f - std::exp (-kTwoPi * fc / (float) sr);
        depthSamples = modDepth * (float) (sr * 0.0025);   // jusqu'à ~2.5 ms
        lfoInc = lerpf (0.1f, 3.0f, clampf (modRate, 0.0f, 1.0f)) / (float) sr;
    }

    inline void process (float inL, float inR, float& outL, float& outR)
    {
        // entrée : réduit en mode infini pour éviter l'accumulation
        const float inGain = inf ? 0.25f : 1.0f;
        float l = inL * inGain, r = inR * inGain;
        for (int a = 0; a < 4; ++a)
        {
            l = allpass (0, a, l);
            r = allpass (1, a, r);
        }

        float v[N];
        float sum = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            lfoPhase[i] += lfoInc * (1.0f + 0.07f * (float) i);
            if (lfoPhase[i] >= 1.0f) lfoPhase[i] -= 1.0f;
            const float mod = depthSamples * std::sin (kTwoPi * lfoPhase[i]);
            float y = lines[i].read (std::max (2.0f, len[i] + mod));
            gain[i] += smooth * (gainTarget[i] - gain[i]);
            // amortissement HF
            damp[i] += dampCoef * (y - damp[i]);
            y = damp[i] * gain[i];
            // bloqueur de continu
            dc[i] += dcCoef * (y - dc[i]);
            y -= dc[i];
            v[i] = y;
            sum += y;
        }
        const float h = sum * (2.0f / (float) N);

        float oL = 0.0f, oR = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            float fb = v[i] - h;
            if (inf) fb = softClip (fb * 0.9f) * (1.0f / 0.9f);
            const float in = (i & 1) ? r : l;
            lines[i].write (flush (fb + in));
            if (i & 1) oR += (i & 2) ? -v[i] : v[i];
            else       oL += (i & 2) ? -v[i] : v[i];
        }
        outL = oL * 0.5f;
        outR = oR * 0.5f;
    }

private:
    inline float allpass (int c, int a, float x)
    {
        auto& b = ap[c][a];
        int& p = apPos[c][a];
        const float d = b[(size_t) p];
        const float g = 0.6f;
        const float y = d - g * x;
        b[(size_t) p] = x + g * d;
        if (++p >= apLenS[c][a]) p = 0;
        return y;
    }

    double sr = 48000.0;
    DelayLine lines[N];
    float len[N] {}, gain[N] {}, gainTarget[N] {}, damp[N] {}, dc[N] {}, dcIn[N] {}, lfoPhase[N] {};
    std::vector<float> ap[2][4];
    int apLenS[2][4] {}, apPos[2][4] {};
    float dampCoef = 0.3f, dcCoef = 0.002f, smooth = 0.001f, depthSamples = 0.0f, lfoInc = 0.0f;
    bool inf = false;
};
} // namespace fs
