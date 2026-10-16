// Test hors-JUCE du moteur DSP : génère des WAV et vérifie niveaux / stabilité.
//   g++ -O2 -std=c++17 -I../Source/dsp dsp_test.cpp -o dsp_test && ./dsp_test [dossier_sortie_wav]
#include <cstdio>
#include <cstring>
#include <vector>
#include <cmath>
#if defined(__SSE__)
 #include <xmmintrin.h>
 #include <pmmintrin.h>
#endif
#include <string>
#include "ShimmerEngine.h"
#include "FreezeEngine.h"

static void writeWav (const char* path, const std::vector<float>& l, const std::vector<float>& r, int sr)
{
    FILE* f = fopen (path, "wb"); if (!f) return;
    const int n = (int) l.size(), ch = 2, bits = 16, dataSize = n * ch * 2;
    auto w32 = [&](int v){ fwrite (&v, 4, 1, f); }; auto w16 = [&](short v){ fwrite (&v, 2, 1, f); };
    fwrite ("RIFF", 1, 4, f); w32 (36 + dataSize); fwrite ("WAVEfmt ", 1, 8, f); w32 (16); w16 (1); w16 (ch);
    w32 (sr); w32 (sr * ch * 2); w16 (ch * 2); w16 (bits); fwrite ("data", 1, 4, f); w32 (dataSize);
    for (int i = 0; i < n; ++i)
    {
        short a = (short) (fs::clampf (l[(size_t) i], -1, 1) * 32767), b = (short) (fs::clampf (r[(size_t) i], -1, 1) * 32767);
        w16 (a); w16 (b);
    }
    fclose (f);
}

static float rms (const std::vector<float>& v, size_t a, size_t b)
{
    double s = 0; for (size_t i = a; i < b && i < v.size(); ++i) s += (double) v[i] * v[i];
    return (float) std::sqrt (s / std::max<size_t> (1, b - a));
}
static float peak (const std::vector<float>& v) { float m = 0; for (float x : v) m = std::max (m, std::abs (x)); return m; }

// estimation de fréquence dominante par FFT
static float domFreq (const std::vector<float>& v, size_t start, int sr)
{
    fs::FFT fft (14);
    std::vector<std::complex<float>> x (16384);
    for (int i = 0; i < 16384; ++i)
        x[(size_t) i] = { v[start + (size_t) i] * (0.5f - 0.5f * std::cos (fs::kTwoPi * i / 16384.0f)), 0 };
    fft.forward (x.data());
    int best = 1; float bm = 0;
    for (int k = 1; k < 8192; ++k) { float m = std::abs (x[(size_t) k]); if (m > bm) { bm = m; best = k; } }
    return (float) best * sr / 16384.0f;
}

int main (int argc, char** argv)
{
#if defined(__SSE__)
    _MM_SET_FLUSH_ZERO_MODE (_MM_FLUSH_ZERO_ON); _MM_SET_DENORMALS_ZERO_MODE (_MM_DENORMALS_ZERO_ON);
#endif
    const std::string outDir = argc > 1 ? argv[1] : ".";   // dossier de sortie des WAV d'écoute
    auto wav = [&] (const std::string& n) { return outDir + "/" + n; };
    const int sr = 48000;
    int fails = 0;
    auto check = [&](bool ok, const char* msg){ printf ("[%s] %s\n", ok ? " OK " : "FAIL", msg); if (!ok) ++fails; };

    // ---------- 1. FREEZE : accord figé ----------
    {
        fs::FreezeEngine fz; fz.prepare (sr);
        fs::FreezeParams p; p.mode = fs::FreezeMode::latch; p.speed = 0.2f; p.layer = 0.0f; p.gliss = 0.1f;
        fz.setParams (p);
        const int total = sr * 6;
        std::vector<float> in ((size_t) total), outL ((size_t) total), outR ((size_t) total), tmpR ((size_t) total);
        // 440 Hz + 554.37 Hz + 659.25 Hz pendant 1,5 s puis silence
        for (int i = 0; i < total; ++i)
        {
            float t = (float) i / sr, s = 0;
            if (t < 1.5f) s = 0.15f * (std::sin (fs::kTwoPi * 440.0f * t) + std::sin (fs::kTwoPi * 554.37f * t) + std::sin (fs::kTwoPi * 659.25f * t));
            in[(size_t) i] = s;
        }
        int captured = 0;
        for (int i = 0; i < total; i += 64)
        {
            if (!captured && i >= (int) (1.2 * sr)) { fz.capture(); captured = 1; }
            fz.process (&in[(size_t) i], &in[(size_t) i], &outL[(size_t) i], &outR[(size_t) i], 64);
        }
        const float rIn = rms (in, (size_t) (0.5 * sr), (size_t) (1.2 * sr));
        const float rOut = rms (outL, (size_t) (3 * sr), (size_t) (5 * sr));
        printf ("  freeze: RMS entrée %.4f / sortie figée %.4f (ratio %.3f)\n", rIn, rOut, rOut / rIn);
        check (rOut > 0.2f * rIn, "le son figé est soutenu bien après la fin de l'entrée");
        const float rLate = rms (outL, (size_t) (5.0 * sr), (size_t) (5.9 * sr));
        check (std::abs (rLate - rOut) < 0.2f * rOut, "sustain stable (pas de décroissance)");
        const float f0 = domFreq (outL, (size_t) (3 * sr), sr);
        printf ("  fréquence dominante après freeze : %.1f Hz\n", f0);
        check (std::abs (f0 - 440.0f) < 8.0f || std::abs (f0 - 554.37f) < 8.0f || std::abs (f0 - 659.25f) < 8.0f, "la hauteur est conservée");
        check (peak (outL) < 1.0f, "pas de saturation");
        std::vector<float> outLn = outL;
        writeWav (wav ("freeze_chord.wav").c_str(), outL, outR, sr);
        // release
        fz.releaseAll();
        std::vector<float> o1 ((size_t) sr * 2), o2 ((size_t) sr * 2), z ((size_t) sr * 2);
        for (int i = 0; i < sr * 2; i += 64) fz.process (&z[(size_t) i], &z[(size_t) i], &o1[(size_t) i], &o2[(size_t) i], 64);
        check (rms (o1, (size_t) (1.5 * sr), (size_t) (2 * sr)) < 0.001f, "relâchement : silence");
    }

    // ---------- 2. SHIMMER : octave supérieure ----------
    {
        fs::ShimmerEngine sh; sh.prepare (sr);
        fs::ShimmerParams p; p.rvbMix = 1.0f; p.time = 0.6f; p.voice = 0.5f; p.tone = 0.8f; p.scene = 0;
        sh.setParams (p);
        const int total = sr * 8;
        std::vector<float> in ((size_t) total), wl ((size_t) total), wr ((size_t) total);
        for (int i = 0; i < total; ++i) { float t = (float) i / sr; in[(size_t) i] = t < 0.3f ? 0.3f * std::sin (fs::kTwoPi * 220.0f * t) : 0.0f; }
        for (int i = 0; i < total; i += 128) sh.process (&in[(size_t) i], &in[(size_t) i], &wl[(size_t) i], &wr[(size_t) i], 128);
        printf ("  shimmer: pic %.3f, RMS queue (6-7 s) %.4f\n", peak (wl), rms (wl, (size_t) (6 * sr), (size_t) (7 * sr)));
        check (peak (wl) < 1.0f && std::isfinite (peak (wl)), "shimmer stable, borné");
        check (rms (wl, (size_t) (1 * sr), (size_t) (2 * sr)) > 0.003f, "queue de réverb présente");
        const float f1 = domFreq (wl, (size_t) (2 * sr), sr);
        printf ("  fréquence dominante de la queue : %.1f Hz (attendu 440 = octave de 220)\n", f1);
        writeWav (wav ("shimmer_intergalactic.wav").c_str(), wl, wr, sr);

        // infini
        p.time = 1.0f; sh.setParams (p); sh.reset();
        for (int i = 0; i < total; i += 128) sh.process (&in[(size_t) i], &in[(size_t) i], &wl[(size_t) i], &wr[(size_t) i], 128);
        const float rA = rms (wl, (size_t) (3 * sr), (size_t) (4 * sr)), rB = rms (wl, (size_t) (7 * sr), (size_t) (8 * sr));
        printf ("  infini: RMS 3-4 s %.4f, 7-8 s %.4f\n", rA, rB);
        check (std::isfinite (rB) && peak (wl) < 1.0f && rB > 0.2f * rA, "mode infini stable (ni explosion, ni extinction rapide)");

        for (int scene = 0; scene < 3; ++scene)
        {
            p.scene = scene; p.time = 0.7f; p.delay = 0.35f; p.rvbMix = 0.4f; p.voice = 0.8f; p.depth = 0.6f; p.rate = 0.5f;
            sh.setParams (p); sh.reset();
            for (int i = 0; i < total; i += 128) sh.process (&in[(size_t) i], &in[(size_t) i], &wl[(size_t) i], &wr[(size_t) i], 128);
            char m[96]; snprintf (m, sizeof m, "scène %d : sortie finie et bornée (pic %.2f)", scene, peak (wl));
            check (std::isfinite (peak (wl)) && peak (wl) < 1.0f && peak (wl) > 0.01f, m);
            writeWav (wav ("shimmer_scene" + std::to_string (scene) + ".wav").c_str(), wl, wr, sr);
        }
    }

    // ---------- 3. FREEZE : maintien = fige, relâche = maintenu, release = stop ----------
    {
        fs::FreezeEngine fz; fz.prepare (sr);
        fs::FreezeParams p; p.mode = fs::FreezeMode::latch; p.speed = 0.3f; p.layer = 0.0f; p.gliss = 0.2f;
        fz.setParams (p);
        const int total = sr * 10;
        std::vector<float> in ((size_t) total), oL ((size_t) total), oR ((size_t) total);
        // accord joué de 0,8 s à 2,0 s
        for (int i = 0; i < total; ++i)
        {
            float t = (float) i / sr, s = 0;
            if (t > 0.8f && t < 2.0f) s = 0.15f * (std::sin (fs::kTwoPi * 220.0f * t) + std::sin (fs::kTwoPi * 277.18f * t) + std::sin (fs::kTwoPi * 329.63f * t));
            in[(size_t) i] = s;
        }
        for (int i = 0; i < total; i += 64)
        {
            if (i == (int) (1.2 * sr)) fz.holdPress();      // appui maintenu pendant que l'accord sonne
            if (i == (int) (1.8 * sr)) fz.holdRelease();    // relâche : le son reste
            if (i == (int) (7.0 * sr)) fz.releaseAll();     // double-clic : arrêt
            fz.process (&in[(size_t) i], &in[(size_t) i], &oL[(size_t) i], &oR[(size_t) i], 64);
        }
        const float rIn  = rms (in, (size_t) (1.0 * sr), (size_t) (1.9 * sr));
        const float rHold = rms (oL, (size_t) (3.0 * sr), (size_t) (6.5 * sr));
        const float rLate = rms (oL, (size_t) (5.5 * sr), (size_t) (6.9 * sr));
        const float rStop = rms (oL, (size_t) (9.0 * sr), (size_t) (10.0 * sr));
        printf ("  hold: RMS jeu %.3f | figé après relâche %.3f | fin %.3f | après stop %.5f\n", rIn, rHold, rLate, rStop);
        check (rHold > 0.5f * rIn, "relâche du footswitch : le freeze est maintenu (entrée silencieuse)");
        check (std::abs (rLate - rHold) < 0.25f * rHold, "le freeze maintenu reste stable dans la durée");
        check (rStop < 0.002f, "double-clic (release) : le freeze s'arrête");
        const float f0 = domFreq (oL, (size_t) (4 * sr), sr);
        check (std::abs (f0 - 220.0f) < 6.0f || std::abs (f0 - 277.18f) < 6.0f || std::abs (f0 - 329.63f) < 6.0f, "accord figé à la bonne hauteur");
        // jouer par-dessus : l'entrée continue de passer indépendamment (le moteur ne la modifie pas)
        writeWav (wav ("freeze_hold_release.wav").c_str(), oL, oR, sr);
    }

    // ---------- 4. SHIMMER : sensibilité de chaque potard ----------
    {
        auto render = [&] (const fs::ShimmerParams& sp, std::vector<float>& out)
        {
            fs::ShimmerEngine sh; sh.prepare (sr); sh.setParams (sp);
            const int total = sr * 6;
            std::vector<float> in ((size_t) total), l ((size_t) total), r ((size_t) total);
            fs::Rng rg (7);
            for (int i = 0; i < total; ++i)
            {
                float t = (float) i / sr, s = 0;
                if (t < 0.45f) { const float e = std::exp (-4.0f * t); s = e * 0.35f * (std::sin (fs::kTwoPi * 220.0f * t) + 0.5f * std::sin (fs::kTwoPi * 440.0f * t) + 0.25f * std::sin (fs::kTwoPi * 660.0f * t)) + 0.01f * rg.bi(); }
                in[(size_t) i] = s;
            }
            for (int i = 0; i < total; i += 128) sh.process (&in[(size_t) i], &in[(size_t) i], &l[(size_t) i], &r[(size_t) i], 128);
            out = l;
        };
        auto diff = [&] (const std::vector<float>& a, const std::vector<float>& b)
        {
            double num = 0, den = 0;
            for (size_t i = 0; i < a.size(); ++i) { const double d = a[i] - b[i]; num += d * d; den += 0.5 * (a[i] * a[i] + b[i] * b[i]); }
            return (float) std::sqrt (num / std::max (1e-12, den));
        };
        fs::ShimmerParams base; base.rvbMix = 0.8f; base.blend = 0.6f; base.time = 0.55f; base.voice = 0.5f; base.tone = 0.55f; base.depth = 0.35f; base.rate = 0.3f; base.delay = 0.3f;
        struct Case { const char* name; void (*setLo) (fs::ShimmerParams&); void (*setHi) (fs::ShimmerParams&); };
        const Case cases[] = {
            { "RVB MIX", [] (fs::ShimmerParams& q) { q.rvbMix = 0.1f; }, [] (fs::ShimmerParams& q) { q.rvbMix = 0.95f; } },
            { "TIME",    [] (fs::ShimmerParams& q) { q.time = 0.15f; },  [] (fs::ShimmerParams& q) { q.time = 0.9f; } },
            { "DELAY",   [] (fs::ShimmerParams& q) { q.delay = 0.0f; },  [] (fs::ShimmerParams& q) { q.delay = 0.6f; } },
            { "VOICE",   [] (fs::ShimmerParams& q) { q.voice = 0.0f; },  [] (fs::ShimmerParams& q) { q.voice = 0.55f; } },
            { "VOICE strings", [] (fs::ShimmerParams& q) { q.voice = 0.55f; }, [] (fs::ShimmerParams& q) { q.voice = 1.0f; } },
            { "TONE",    [] (fs::ShimmerParams& q) { q.tone = 0.05f; },  [] (fs::ShimmerParams& q) { q.tone = 0.95f; } },
            { "DEPTH",   [] (fs::ShimmerParams& q) { q.depth = 0.0f; },  [] (fs::ShimmerParams& q) { q.depth = 1.0f; } },
            { "RATE",    [] (fs::ShimmerParams& q) { q.depth = 0.9f; q.rate = 0.05f; }, [] (fs::ShimmerParams& q) { q.depth = 0.9f; q.rate = 0.95f; } },
        };
        for (const auto& cs : cases)
        {
            fs::ShimmerParams a = base, b = base; cs.setLo (a); cs.setHi (b);
            std::vector<float> ra, rb; render (a, ra); render (b, rb);
            const float d = diff (ra, rb);
            char m[128]; snprintf (m, sizeof m, "%-14s écart relatif %.2f (seuil 0.30)", cs.name, d);
            check (d > 0.30f, m);
        }
        // octave : l'énergie à 440 Hz (octave de 220) doit augmenter avec VOICE plate -> shimmer
        auto bandEnergy = [&] (const std::vector<float>& v, float f0)
        {
            fs::FFT fft (14); std::vector<std::complex<float>> x (16384);
            for (int i = 0; i < 16384; ++i) x[(size_t) i] = { v[(size_t) (sr + i)] * (0.5f - 0.5f * std::cos (fs::kTwoPi * i / 16384.0f)), 0 };
            fft.forward (x.data());
            const int k0 = (int) (f0 * 16384.0f / sr); double e = 0;
            for (int k = k0 - 6; k <= k0 + 6; ++k) e += std::norm (x[(size_t) k]);
            return (float) e;
        };
        fs::ShimmerParams pl = base, sm = base; pl.voice = 0.0f; sm.voice = 0.5f; pl.delay = sm.delay = 0.0f; pl.rvbMix = sm.rvbMix = 1.0f;
        std::vector<float> ra, rb; render (pl, ra); render (sm, rb);
        const float gain = 10.0f * std::log10 (bandEnergy (rb, 440.0f) / std::max (1e-12f, bandEnergy (ra, 440.0f)));
        printf ("  octave +12 (440 Hz) : shimmer vs plate = %+.1f dB\n", gain);
        check (gain > 4.0f, "VOICE : l'octave supérieure apparaît nettement (shimmer vs plate)");
    }
    printf (fails ? "\n%d ECHEC(S)\n" : "\nTOUT EST OK\n", fails);
    return fails ? 1 : 0;
}
