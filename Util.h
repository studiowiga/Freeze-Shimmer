// Freeze Shimmer - petits utilitaires DSP (sans dépendance JUCE)
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace fs
{
constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf (float a, float b, float t) { return a + (b - a) * t; }

// anti-denormal : on ajoute un offset minuscule (inaudible)
inline float flush (float x) { return x + 1.0e-20f - 1.0e-20f; }

inline float softClip (float x)
{
    // saturation douce, quasi linéaire sous 0.6
    if (x > 3.0f)  return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// coefficient de lissage one-pole pour une constante de temps (secondes)
inline float onePoleCoef (double sr, float seconds)
{
    return 1.0f - std::exp (-1.0f / (float) (sr * std::max (1.0e-4f, seconds)));
}

struct OnePole
{
    float z = 0.0f;
    inline float lp (float x, float a) { z += a * (x - z); return z; }
    void reset() { z = 0.0f; }
};

// générateur pseudo-aléatoire rapide (xorshift32), déterministe
struct Rng
{
    uint32_t s = 0x1234abcdu;
    explicit Rng (uint32_t seed = 0x1234abcdu) : s (seed ? seed : 1u) {}
    inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }         // [0,1)
    inline float bi()  { return uni() * 2.0f - 1.0f; }                                    // [-1,1)
};

// ligne à retard à lecture interpolée (Hermite 4 points)
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        int n = 16;
        while (n < maxSamples + 8) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }

    inline void write (float x) { w = (w + 1) & mask; buf[(size_t) w] = x; }

    // retard en échantillons (>= 1)
    inline float read (float d) const
    {
        const float pos = (float) w - d;
        const int   i   = (int) std::floor (pos);
        const float f   = pos - (float) i;
        const float y0 = buf[(size_t) ((i - 1) & mask)];
        const float y1 = buf[(size_t) (i & mask)];
        const float y2 = buf[(size_t) ((i + 1) & mask)];
        const float y3 = buf[(size_t) ((i + 2) & mask)];
        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }
    inline float readLinear (float d) const
    {
        const float pos = (float) w - d;
        const int   i   = (int) std::floor (pos);
        const float f   = pos - (float) i;
        return lerpf (buf[(size_t) (i & mask)], buf[(size_t) ((i + 1) & mask)], f);
    }

private:
    std::vector<float> buf;
    int mask = 15, w = 0;
};

// filtre biquad (RBJ) mono, forme transposée directe II
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }
    void setLowpass (double sr, float f, float q = 0.7071f)
    {
        f = clampf (f, 20.0f, (float) (sr * 0.45));
        const float w = kTwoPi * f / (float) sr, c = std::cos (w), s = std::sin (w), al = s / (2 * q);
        const float a0 = 1 + al;
        b0 = (1 - c) * 0.5f / a0; b1 = (1 - c) / a0; b2 = b0;
        a1 = -2 * c / a0; a2 = (1 - al) / a0;
    }
    void setHighpass (double sr, float f, float q = 0.7071f)
    {
        f = clampf (f, 10.0f, (float) (sr * 0.45));
        const float w = kTwoPi * f / (float) sr, c = std::cos (w), s = std::sin (w), al = s / (2 * q);
        const float a0 = 1 + al;
        b0 = (1 + c) * 0.5f / a0; b1 = -(1 + c) / a0; b2 = b0;
        a1 = -2 * c / a0; a2 = (1 - al) / a0;
    }
};
} // namespace fs
