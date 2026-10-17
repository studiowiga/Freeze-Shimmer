// Freeze Shimmer - moteur "cosmic reverb" inspiré du Pico Shimmer
//   3 scènes : INTERGALACTIC, OFF-WORLD, ETHERDUST
//   BLEND (dry/wet) · TIME (decay, infini au maximum) · VOICE (plate → shimmer → strings) · TONE
//   secondaires : RVB MIX · DELAY (0-950 ms) · DEPTH · RATE
#pragma once
#include "Util.h"
#include "Reverb.h"
#include "PitchShifter.h"

namespace fs
{
struct ShimmerParams
{
    float blend = 0.5f, rvbMix = 0.7f, time = 0.55f, delay = 0.0f;
    float voice = 0.5f, depth = 0.35f, tone = 0.55f, rate = 0.3f;
    int   scene = 0; // 0 intergalactic, 1 off-world, 2 etherdust
};

class ShimmerEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        rev.prepare (sr);
        const int maxDelay = (int) (sr * 1.05) + 8;
        for (int c = 0; c < 2; ++c)
        {
            dl[c].prepare (maxDelay);
            gl[c][0].prepare (maxDelay);
            for (int k = 0; k < 3; ++k)
            {
                shifter[c][k].prepare (sr, 70.0f);
                shifter[c][k].setPhaseOffset (0.31f * (float) k + 0.5f * (float) c);
            }
            glitchShift[c].prepare (sr, 40.0f);
            glitchShift[c].setRatio (2.0f);
            fbLp[c].reset(); toneLp[c].reset(); toneHp[c].reset(); delLp[c].reset();
            fbShimmer[c] = 0.0f;
            tremPhase[c] = 0.25f * (float) c;
        }
        shifter[0][0].setRatio (2.0f); shifter[1][0].setRatio (2.0f);
        shifter[0][1].setRatio (2.0f * 1.0035f); shifter[1][1].setRatio (2.0f * 0.9965f);
        shifter[0][2].setRatio (1.0f * 1.0045f); shifter[1][2].setRatio (1.0f * 0.9955f);
        cT = onePoleCoef (sr, 0.04f);
        a1 = onePoleCoef (sr, 0.1f);
        rng = Rng (0x9e3779b9u);
        glitchSamples = 0; glitchActive = 0;
        curDelay = 0.0f;
        lfoD = 0.0f;
        setParams (ShimmerParams {});
        sBlend = p.blend; sRvb = p.rvbMix;
    }

    void reset()
    {
        rev.reset();
        for (int c = 0; c < 2; ++c)
        {
            dl[c].clear(); gl[c][0].clear();
            for (int k = 0; k < 3; ++k) shifter[c][k].reset();
            glitchShift[c].reset();
            fbLp[c].reset(); toneLp[c].reset(); toneHp[c].reset(); delLp[c].reset();
            fbShimmer[c] = 0.0f;
        }
    }

    void setParams (const ShimmerParams& np)
    {
        p = np;
        const bool inf = p.time >= 0.985f;
        // TIME : 0.4 s .. 18 s (courbe exponentielle), infini au maximum
        const float rt = 0.4f * std::pow (45.0f, clampf (p.time / 0.985f, 0.0f, 1.0f));
        float dampAmt = 0.25f + 0.35f * (1.0f - p.tone);
        float modMul = 1.0f;
        if (p.scene == 1) { dampAmt += 0.18f; modMul = 1.8f; }       // off-world : plus doux, plus de modulation
        if (p.scene == 2) { dampAmt -= 0.05f; }
        rev.setParams (rt, inf, dampAmt, clampf (p.depth * modMul, 0.0f, 1.0f), p.rate);
        // TONE : luminosité de la partie wet
        const float fc = 500.0f * std::pow (36.0f, p.tone);  // 0.5 k .. 18 k
        toneLp[0].setLowpass (sr, fc); toneLp[1].setLowpass (sr, fc);
        toneHp[0].setHighpass (sr, 70.0f); toneHp[1].setHighpass (sr, 70.0f);
        fbLpCoef = 1.0f - std::exp (-kTwoPi * (p.scene == 1 ? 2800.0f : 6000.0f) / (float) sr);
        delLpCoef = 1.0f - std::exp (-kTwoPi * 4500.0f / (float) sr);
        // retard : 0 = désactivé, sinon jusqu'à 950 ms
        delayTarget = p.delay <= 0.001f ? 0.0f : (float) (sr * 0.95 * p.delay);
        lfoInc = lerpf (0.15f, 6.0f, p.rate) / (float) sr;
        modDepthSamp = p.depth * (float) (sr * (p.scene == 1 ? 0.020 : 0.012));
        // voice / shimmer
        const float v = p.voice;
        if (p.scene == 2)
        {
            shimAmt = 0.0f; strAmt = 0.0f;
        }
        else
        {
            shimAmt = v < 0.5f ? (v / 0.5f) * 0.55f : lerpf (0.55f, 0.35f, (v - 0.5f) * 2.0f);
            strAmt  = clampf ((v - 0.5f) * 2.0f, 0.0f, 1.0f);
        }
        if (p.scene == 1 && v > 0.5f) strAmt = clampf ((v - 0.5f) * 2.0f, 0.0f, 1.0f) * 0.8f;
        // le gain de boucle diminue quand le decay s'allonge (sinon auto-oscillation)
        loopGain = inf ? 0.07f : clampf (0.36f / std::sqrt (std::max (0.4f, rt) * 0.5f), 0.08f, 0.36f);
    }

    // traite un bloc stéréo : écrit UNIQUEMENT la partie wet (déjà dosée par RVB MIX / TONE)
    void process (const float* inL, const float* inR, float* wetL, float* wetR, int n)
    {
        const bool etherdust = p.scene == 2;
        for (int i = 0; i < n; ++i)
        {
            sRvb += a1 * (p.rvbMix - sRvb);
            curDelay += cT * (delayTarget - curDelay);
            lfoD += lfoInc; if (lfoD >= 1.0f) lfoD -= 1.0f;

            const float x[2] = { inL[i], inR[i] };
            float d[2] = { 0.0f, 0.0f };

            // ---------------- DELAY (modulé) / GLITCH (etherdust) ----------------
            if (etherdust)
            {
                glitchStep();
                for (int c = 0; c < 2; ++c)
                {
                    const float tA = glitchTime[glitchActive] * (c ? 1.07f : 1.0f);
                    const float tB = glitchTime[1 - glitchActive] * (c ? 1.07f : 1.0f);
                    const float ya = gl[c][0].read (std::max (2.0f, tA));
                    const float yb = gl[c][0].read (std::max (2.0f, tB));
                    const float y  = ya * glitchFade + yb * (1.0f - glitchFade);
                    delLp[c].z += delLpCoef * (y - delLp[c].z);
                    gl[c][0].write (flush (x[c] + delLp[c].z * 0.42f));
                    d[c] = glitchShift[c].process (y * 0.9f) * 0.6f + y * 0.5f;
                }
            }
            else if (delayTarget > 0.5f || curDelay > 1.0f)
            {
                for (int c = 0; c < 2; ++c)
                {
                    const float m = modDepthSamp * std::sin (kTwoPi * (lfoD + 0.25f * (float) c));
                    const float y = dl[c].read (std::max (2.0f, curDelay + m));
                    delLp[c].z += delLpCoef * (y - delLp[c].z);
                    // ping-pong léger : une partie du feedback passe sur l'autre canal
                    const float other = (c == 0) ? lastD[1] : lastD[0];
                    dl[c].write (flush (x[c] + (delLp[c].z * 0.62f + other * 0.2f) * 0.5f * 1.45f));
                    lastD[c] = y;
                    d[c] = y;
                }
            }
            else
            {
                for (int c = 0; c < 2; ++c) { dl[c].write (0.0f); lastD[c] = 0.0f; }
            }

            // ---------------- boucle shimmer ----------------
            float fb[2];
            float octOut[2] = { 0.0f, 0.0f }, strOut[2] = { 0.0f, 0.0f };
            for (int c = 0; c < 2; ++c)
            {
                float s = 0.0f;
                if (!etherdust)
                {
                    const float revOut = fbShimmer[c];
                    float oct = shifter[c][0].process (revOut);
                    float stringsAcc = 0.0f;
                    if (strAmt > 0.001f)
                    {
                        tremPhase[c] += 0.31f / (float) sr; if (tremPhase[c] >= 1.0f) tremPhase[c] -= 1.0f;
                        const float trem = 0.75f + 0.25f * std::sin (kTwoPi * tremPhase[c]);
                        const float s1 = shifter[c][1].process (revOut);
                        const float s2 = shifter[c][2].process (revOut);
                        stringsAcc = (s1 * 0.8f + s2 * 0.9f) * trem;
                    }
                    else
                    {   // garde les shifters "chauds" pour éviter les clics à l'activation
                        shifter[c][1].process (revOut);
                        shifter[c][2].process (revOut);
                    }
                    octOut[c] = oct * shimAmt * (1.0f - 0.35f * strAmt);
                    strOut[c] = stringsAcc * strAmt;
                    s = octOut[c] + strOut[c] * 0.6f;
                    fbLp[c].z += fbLpCoef * (s - fbLp[c].z);
                    s = fbLp[c].z;
                }
                fb[c] = s;
            }

            const float inGain = 0.5f;
            const float rIn[2] = { (x[0] * 0.9f + d[0] * 0.7f) * inGain + softClip (fb[0] * loopGain * 3.0f) * (1.0f / 3.0f),
                                   (x[1] * 0.9f + d[1] * 0.7f) * inGain + softClip (fb[1] * loopGain * 3.0f) * (1.0f / 3.0f) };
            float rl, rr;
            rev.process (rIn[0], rIn[1], rl, rr);
            fbShimmer[0] = rl; fbShimmer[1] = rr;

            // ---------------- mix wet interne ----------------
            const float rv = sRvb;
            float w[2];
            const float rvOut[2] = { rl, rr };
            for (int c = 0; c < 2; ++c)
            {
                // etherdust : retard de glitch plus présent
                const float dGain = etherdust ? 0.9f : 0.85f;
                // shimmer en sortie directe (octave + strings) : VOICE est nettement audible
                float y = rvOut[c] * (rv * 1.3f) + (octOut[c] * 1.1f + strOut[c] * 0.9f) * rv
                        + d[c] * dGain * (0.35f + 0.65f * (1.0f - rv));
                y = toneHp[c].process (toneLp[c].process (y));
                w[c] = softClip (y);
            }
            wetL[i] = w[0];
            wetR[i] = w[1];
        }
    }

private:
    void glitchStep()
    {
        // étape aléatoire dont la durée dépend de RATE (35 ms .. 600 ms)
        if (glitchSamples <= 0)
        {
            static const float mult[6] = { 0.125f, 0.25f, 0.375f, 0.5f, 0.75f, 1.0f };
            const float maxMs = lerpf (60.0f, 950.0f, p.delay);
            const float ms = std::max (6.0f, maxMs * mult[rng.next() % 6]);
            glitchActive = 1 - glitchActive;
            glitchTime[glitchActive] = (float) (sr * ms * 0.001);
            glitchFade = 0.0f;
            glitchSamples = (int) (sr * lerpf (0.6f, 0.035f, p.rate) * (0.6f + 0.8f * rng.uni()));
            glitchFadeInc = 1.0f / (float) (sr * 0.006);
        }
        --glitchSamples;
        glitchFade = std::min (1.0f, glitchFade + glitchFadeInc);
    }

    double sr = 48000.0;
    ShimmerParams p;
    Reverb rev;
    DelayLine dl[2], gl[2][1];
    PitchShifter shifter[2][3], glitchShift[2];
    OnePole fbLp[2], delLp[2];
    Biquad toneLp[2], toneHp[2];
    float fbShimmer[2] {}, lastD[2] {}, tremPhase[2] {};
    float cT = 0.001f, a1 = 0.001f, sRvb = 0.7f, sBlend = 0.5f;
    float fbLpCoef = 0.5f, delLpCoef = 0.3f;
    float delayTarget = 0.0f, curDelay = 0.0f, lfoD = 0.0f, lfoInc = 0.0f, modDepthSamp = 0.0f;
    float shimAmt = 0.5f, strAmt = 0.0f, loopGain = 0.3f;
    // glitch
    Rng rng;
    int glitchSamples = 0, glitchActive = 0;
    float glitchTime[2] { 4800.0f, 4800.0f }, glitchFade = 1.0f, glitchFadeInc = 0.01f;
};
} // namespace fs
