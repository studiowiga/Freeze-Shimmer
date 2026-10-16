// Freeze Shimmer - pitch shifter granulaire (deux têtes de lecture en quinconce)
// Utilisé pour l'octave supérieure du "shimmer" et pour les couches "strings".
#pragma once
#include "Util.h"

namespace fs
{
class PitchShifter
{
public:
    void prepare (double sampleRate, float grainMs)
    {
        sr    = sampleRate;
        grain = std::max (64.0f, (float) (sampleRate * grainMs * 0.001));
        line.prepare ((int) grain + 16);
        line.clear();
        phase = 0.0f;
    }
    void reset() { line.clear(); phase = 0.0f; }
    void setRatio (float r) { ratio = r; }
    void setPhaseOffset (float p) { phase = p - std::floor (p); }

    inline float process (float x)
    {
        line.write (x);
        const float p2 = phase + 0.5f - std::floor (phase + 0.5f);
        const float d1 = 2.0f + phase * grain;
        const float d2 = 2.0f + p2 * grain;
        // fenêtres complémentaires (somme = 1)
        const float c  = std::cos (kTwoPi * phase);
        const float g1 = 0.5f - 0.5f * c;
        const float g2 = 0.5f + 0.5f * c;
        const float y  = line.read (d1) * g1 + line.read (d2) * g2;

        phase += (1.0f - ratio) / grain;
        phase -= std::floor (phase);
        return y;
    }

private:
    DelayLine line;
    double sr = 48000.0;
    float grain = 4096.0f, ratio = 2.0f, phase = 0.0f;
};
} // namespace fs
