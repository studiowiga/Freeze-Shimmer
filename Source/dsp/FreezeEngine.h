// Freeze Shimmer - moteur "Sound Retainer" inspiré du Deep Freeze
//
// Principe : freeze spectral à conservation de fréquence (vocodeur de phase).
//  - à la capture on analyse les N derniers échantillons (+ une trame décalée d'un hop)
//    => magnitudes + avance de phase par bin (donc la hauteur du son figé est conservée)
//  - la resynthèse tourne en boucle par recouvrement-addition (75 %), sustain infini
//  - chaque capture est une "voix" ; plusieurs voix se superposent (LAYER)
//  - GLISS = durée du fondu entre deux freezes (morphing d'un son figé au suivant)
//  - SPEED = durée d'attaque / de relâche du freeze
//  Modes : LATCH (chaque appui capture, jusqu'au relâchement), MOMENT (tant que l'on appuie),
//          AUTO (capture automatique sur chaque nouvelle attaque détectée)
#pragma once
#include <array>
#include <complex>
#include "Util.h"
#include "FFT.h"

namespace fs
{
enum class FreezeMode { latch = 0, moment = 1, autoMode = 2 };

struct FreezeParams
{
    float speed = 0.5f, layer = 0.3f, gliss = 0.35f;
    FreezeMode mode = FreezeMode::latch;
};

class FreezeEngine
{
public:
    static constexpr int kVoices = 8;
    static constexpr int kMaxLayers = 4;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        order = sr > 60000.0 ? 13 : 12;
        N = 1 << order;
        hop = N / 4;
        fft.setOrder (order);
        win.resize ((size_t) N);
        for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * (float) i / (float) N);
        ringSize = N * 2;
        for (int c = 0; c < 2; ++c)
        {
            inRing[c].assign ((size_t) ringSize, 0.0f);
            outRing[c].assign ((size_t) N, 0.0f);
        }
        work.assign ((size_t) N, std::complex<float> (0, 0));
        tmpA.assign ((size_t) N, std::complex<float> (0, 0));
        tmpB.assign ((size_t) N, std::complex<float> (0, 0));
        bins = N / 2 + 1;
        for (auto& v : voices)
            for (int c = 0; c < 2; ++c)
            {
                v.mag[c].assign ((size_t) bins, 0.0f);
                v.adv[c].assign ((size_t) bins, 0.0f);
                v.phase[c].assign ((size_t) bins, 0.0f);
            }
        reset();
        // gain de resynthèse calibré (voir tests/dsp_test.cpp)
        synthGain = 0.67f;
        rng[0] = Rng (0xa341316cu); rng[1] = Rng (0xc8013ea4u);
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            std::fill (inRing[c].begin(), inRing[c].end(), 0.0f);
            std::fill (outRing[c].begin(), outRing[c].end(), 0.0f);
        }
        for (auto& v : voices) { v.gain = 0; v.target = 0; v.used = false; v.age = 0; }
        writePos = 0; outPos = 0; hopCount = 0; captureCounter = 0; ageCounter = 0;
        frozenFlag = false; autoCountdown = -1; autoEnv = 0; autoSlow = 0; sinceAuto = 1 << 30;
        momentHeld = false; holding = false; holdVoice = -1; lastCaptured = -1; refreshCountdown = 0;
    }

    void setParams (const FreezeParams& np)
    {
        p = np;
    }
    void setAutoArmed (bool a) { autoArmed = a; }

    // --- commandes (thread audio) ---
    void capture()
    {
        doCapture (std::min (0.12f, speedTime()));
    }
    // LATCH : footswitch MAINTENU = le moteur fige ce qu'il entend (capture rafraîchie en continu) ;
    //         au RELÂCHEMENT la dernière capture est conservée, on joue par-dessus.
    void holdPress()
    {
        holding = true;
        holdVoice = -1;
        refreshCountdown = 0;
        holdCapture (std::min (0.12f, speedTime()));
    }
    void holdRelease() { holding = false; holdVoice = -1; }
    void momentPress()  { momentHeld = true;  holdPress(); }
    void momentRelease(){ momentHeld = false; releaseAll(); }
    void releaseAll()
    {
        holding = false; holdVoice = -1;
        for (auto& v : voices)
            if (v.used) { v.target = 0.0f; v.fadeTime = std::max (0.02f, std::min (speedTime(), 0.6f)); }
        autoCountdown = -1;
    }

    bool isFrozen() const
    {
        for (auto& v : voices) if (v.used && v.target > 0.5f) return true;
        return false;
    }
    int activeVoices() const
    {
        int n = 0;
        for (auto& v : voices) if (v.used && v.gain > 0.001f) ++n;
        return n;
    }

    // traite un bloc : alimente l'historique avec in*, produit la sortie figée dans out*
    void process (const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            inRing[0][(size_t) writePos] = inL[i];
            inRing[1][(size_t) writePos] = inR[i];
            writePos = (writePos + 1) % ringSize;

            if (holding && --refreshCountdown <= 0)
            {
                refreshCountdown = (int) (sr * 0.16);
                holdCapture (0.14f);
            }
            if (p.mode == FreezeMode::autoMode && autoArmed) autoDetect (std::max (std::abs (inL[i]), std::abs (inR[i])));
            else autoCountdown = -1;
            if (autoCountdown >= 0 && --autoCountdown < 0) { doCapture(); sinceAuto = 0; }
            if (sinceAuto < (1 << 30)) ++sinceAuto;

            if (++hopCount >= hop) { hopCount = 0; synthHop(); }

            outL[i] = outRing[0][(size_t) outPos];
            outR[i] = outRing[1][(size_t) outPos];
            outRing[0][(size_t) outPos] = 0.0f;
            outRing[1][(size_t) outPos] = 0.0f;
            outPos = (outPos + 1) % N;
        }
    }

    int fftSize() const { return N; }

private:
    struct Voice
    {
        std::vector<float> mag[2], adv[2], phase[2];
        float gain = 0.0f, target = 0.0f;
        bool used = false;
        uint32_t age = 0;
        float fadeTime = 0.1f;
    };

    int maxLayers() const { return 1 + (int) std::floor (clampf (p.layer, 0.0f, 0.999f) * (float) kMaxLayers); }
    // SPEED : 20 ms .. 3 s
    float speedTime() const { return 0.02f * std::pow (150.0f, clampf (p.speed, 0.0f, 1.0f)); }
    // GLISS : 20 ms .. 5 s (fondu entre deux freezes)
    float glissTime() const { return 0.02f + 5.0f * std::pow (clampf (p.gliss, 0.0f, 1.0f), 2.2f); }

    void autoDetect (float a)
    {
        autoEnv  = std::max (a, autoEnv * 0.9995f);
        autoSlow += 0.0004f * (autoEnv - autoSlow);
        if (autoCountdown < 0 && sinceAuto > (int) (sr * 0.45)
            && autoEnv > 0.012f && autoEnv > autoSlow * 2.4f && a > 0.01f)
            autoCountdown = (int) (sr * 0.14);   // laisse le corps de la note se développer
    }

    void analyse (int c, int endOffsetFromNow, std::vector<std::complex<float>>& dst)
    {
        // N échantillons se terminant à (writePos - endOffsetFromNow)
        int start = writePos - endOffsetFromNow - N;
        start = ((start % ringSize) + ringSize) % ringSize;
        for (int i = 0; i < N; ++i)
            dst[(size_t) i] = std::complex<float> (inRing[c][(size_t) ((start + i) % ringSize)] * win[(size_t) i], 0.0f);
        fft.forward (dst.data());
    }

    void doCapture (float attackOverride = -1.0f)
    {
        // choisit une voix : libre, sinon la plus ancienne
        int idx = -1;
        for (int i = 0; i < kVoices; ++i) if (!voices[(size_t) i].used) { idx = i; break; }
        if (idx < 0)
        {
            uint32_t oldest = 0xffffffffu;
            for (int i = 0; i < kVoices; ++i)
                if (voices[(size_t) i].age < oldest) { oldest = voices[(size_t) i].age; idx = i; }
        }
        Voice& v = voices[(size_t) idx];

        for (int c = 0; c < 2; ++c)
        {
            analyse (c, 0, tmpB);        // trame la plus récente
            analyse (c, hop, tmpA);      // trame précédente (un hop plus tôt)
            for (int k = 0; k < bins; ++k)
            {
                const auto a = tmpA[(size_t) k], b = tmpB[(size_t) k];
                v.mag[c][(size_t) k] = 0.5f * (std::abs (a) + std::abs (b));
                const auto prod = b * std::conj (a);
                v.adv[c][(size_t) k] = std::arg (prod);
                v.phase[c][(size_t) k] = std::arg (b);
            }
        }
        // fondu d'entrée : SPEED s'il n'y a rien d'autre de figé, sinon GLISS (morphing)
        bool others = false;
        for (int i = 0; i < kVoices; ++i)
            if (i != idx && voices[(size_t) i].used && voices[(size_t) i].target > 0.5f) others = true;

        const bool wasUsed = v.used;
        v.used = true;
        v.age = ++ageCounter;
        v.target = 1.0f;
        if (!wasUsed) v.gain = 0.0f;
        v.fadeTime = std::max (0.02f, attackOverride > 0.0f ? attackOverride : (others ? glissTime() : speedTime()));
        lastCaptured = idx;

        // limite de couches : les plus anciennes disparaissent en fondu (durée GLISS)
        const int lim = (p.mode == FreezeMode::latch) ? maxLayers() : 1;
        for (;;)
        {
            int active = 0; uint32_t oldest = 0xffffffffu; int oi = -1;
            for (int i = 0; i < kVoices; ++i)
            {
                auto& o = voices[(size_t) i];
                if (o.used && o.target > 0.5f)
                {
                    ++active;
                    if (o.age < oldest) { oldest = o.age; oi = i; }
                }
            }
            if (active <= lim || oi < 0) break;
            voices[(size_t) oi].target = 0.0f;
            voices[(size_t) oi].fadeTime = std::max (0.02f, glissTime());
        }
    }

    void holdCapture (float fade)
    {
        const int prev = holdVoice;
        // la capture précédente du même maintien s'efface ; elle ne compte pas comme une couche
        if (prev >= 0 && voices[(size_t) prev].used) { voices[(size_t) prev].target = 0.0f; voices[(size_t) prev].fadeTime = std::max (0.02f, fade); }
        doCapture (fade);
        holdVoice = lastCaptured;
    }

    void synthHop()
    {
        const float hopSec = (float) hop / (float) sr;
        int active = 0;
        for (auto& v : voices)
        {
            if (!v.used) continue;
            // rampe linéaire du gain vers la cible
            const float step = hopSec / std::max (0.02f, v.fadeTime);
            if (v.gain < v.target) v.gain = std::min (v.target, v.gain + step);
            else if (v.gain > v.target) v.gain = std::max (v.target, v.gain - step);
            if (v.target <= 0.0f && v.gain <= 0.0f) { v.used = false; continue; }
            ++active;
        }
        if (active == 0) return;
        const float norm = synthGain / std::sqrt ((float) std::max (1, active));

        for (int c = 0; c < 2; ++c)
        {
            bool any = false;
            std::fill (work.begin(), work.end(), std::complex<float> (0, 0));
            for (auto& v : voices)
            {
                if (!v.used || v.gain <= 0.0f) continue;
                any = true;
                // vérifie qu'on accumule dans le domaine fréquentiel : un seul IFFT pour toutes les voix
                const float g = v.gain * norm;
                for (int k = 0; k < bins; ++k)
                {
                    float ph = v.phase[c][(size_t) k] + v.adv[c][(size_t) k] + 0.02f * rng[c].bi();
                    if (ph > kPi) ph -= kTwoPi; else if (ph < -kPi) ph += kTwoPi;
                    v.phase[c][(size_t) k] = ph;
                    const float m = v.mag[c][(size_t) k] * g;
                    work[(size_t) k] += std::complex<float> (m * std::cos (ph), m * std::sin (ph));
                }
            }
            if (!any) continue;
            // symétrie hermitienne
            work[0] = std::complex<float> (work[0].real(), 0.0f);
            work[(size_t) N / 2] = std::complex<float> (work[(size_t) N / 2].real(), 0.0f);
            for (int k = 1; k < N / 2; ++k) work[(size_t) (N - k)] = std::conj (work[(size_t) k]);
            fft.inverse (work.data());
            // recouvrement-addition (fenêtre de Hann, 75 %)
            for (int i = 0; i < N; ++i)
            {
                const int o = (outPos + i) % N;
                outRing[c][(size_t) o] += work[(size_t) i].real() * win[(size_t) i];
            }
        }
    }

    double sr = 48000.0;
    int order = 12, N = 4096, hop = 1024, bins = 2049, ringSize = 8192;
    FFT fft;
    std::vector<float> win, inRing[2], outRing[2];
    std::vector<std::complex<float>> work, tmpA, tmpB;
    std::array<Voice, kVoices> voices;
    FreezeParams p;
    int writePos = 0, outPos = 0, hopCount = 0;
    uint32_t captureCounter = 0, ageCounter = 0;
    bool frozenFlag = false, momentHeld = false, autoArmed = true, holding = false;
    int holdVoice = -1, lastCaptured = -1, refreshCountdown = 0;
    int autoCountdown = -1, sinceAuto = 1 << 30;
    float autoEnv = 0, autoSlow = 0, synthGain = 1.0f;
    Rng rng[2];
};
} // namespace fs
