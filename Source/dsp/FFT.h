// Freeze Shimmer - FFT radix-2 complexe (en place), sans dépendance
#pragma once
#include <complex>
#include <vector>
#include "Util.h"

namespace fs
{
class FFT
{
public:
    using Cx = std::complex<float>;

    explicit FFT (int order = 12) { setOrder (order); }

    void setOrder (int order)
    {
        n = 1 << order;
        cosT.resize ((size_t) n / 2);
        sinT.resize ((size_t) n / 2);
        rev.resize ((size_t) n);
        for (int i = 0; i < n / 2; ++i)
        {
            const double a = -2.0 * 3.14159265358979323846 * i / n;
            cosT[(size_t) i] = (float) std::cos (a);
            sinT[(size_t) i] = (float) std::sin (a);
        }
        for (int i = 0; i < n; ++i)
        {
            int r = 0, x = i;
            for (int b = 0; b < order; ++b) { r = (r << 1) | (x & 1); x >>= 1; }
            rev[(size_t) i] = r;
        }
    }

    int size() const { return n; }

    void forward (Cx* x) const { run (x, false); }
    // transformée inverse, normalisée par 1/N
    void inverse (Cx* x) const
    {
        run (x, true);
        const float s = 1.0f / (float) n;
        for (int i = 0; i < n; ++i) x[i] *= s;
    }

private:
    void run (Cx* x, bool inv) const
    {
        for (int i = 0; i < n; ++i)
        {
            const int j = rev[(size_t) i];
            if (j > i) std::swap (x[i], x[j]);
        }
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1, step = n / len;
            for (int i = 0; i < n; i += len)
            {
                for (int k = 0; k < half; ++k)
                {
                    const float wr = cosT[(size_t) (k * step)];
                    const float wi = inv ? -sinT[(size_t) (k * step)] : sinT[(size_t) (k * step)];
                    const Cx a = x[i + k];
                    const Cx b = x[i + k + half];
                    const Cx t (b.real() * wr - b.imag() * wi, b.real() * wi + b.imag() * wr);
                    x[i + k]        = a + t;
                    x[i + k + half] = a - t;
                }
            }
        }
    }

    int n = 0;
    std::vector<float> cosT, sinT;
    std::vector<int> rev;
};
} // namespace fs
