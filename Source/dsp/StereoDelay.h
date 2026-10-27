#pragma once
// Punto Tune Pro - delay estéreo con feedback filtrado, ping-pong y cambio de tiempo suavizado.

#include <algorithm>
#include <cmath>
#include <vector>

namespace punto
{

struct DelayParams
{
    bool  on       = false;
    float timeMs   = 350.0f;
    float feedback = 0.35f;     // 0..0.95
    float toneHz   = 6000.0f;   // paso bajo del feedback
    bool  pingPong = false;
    float mix      = 0.25f;     // nivel del eco (el dry se mantiene)
};

class StereoDelay
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        size = 1;
        while (size < (int) (sr * 2.6)) size <<= 1;
        mask = size - 1;
        for (auto& b : buf) b.assign ((size_t) size, 0.0f);
        reset();
    }

    void reset()
    {
        for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
        pos = 0;
        lp[0] = lp[1] = hp[0] = hp[1] = hpIn[0] = hpIn[1] = 0.0f;
        smoothedDelay = -1.0f;
    }

    void process (float* const* data, int numCh, int numSamples, const DelayParams& p) noexcept
    {
        if (! p.on) return;

        const float targetDelay = std::clamp (p.timeMs * 0.001f * (float) sr, 16.0f, (float) (size - 8));
        if (smoothedDelay < 0.0f) smoothedDelay = targetDelay;

        const float lpCoeff = 1.0f - std::exp (-6.2831853f * std::clamp (p.toneHz, 200.0f, 20000.0f) / (float) sr);
        const float hpCoeff = std::exp (-6.2831853f * 30.0f / (float) sr);
        const float fb = std::clamp (p.feedback, 0.0f, 0.95f);
        const int nch = std::min (numCh, 2);

        for (int n = 0; n < numSamples; ++n)
        {
            smoothedDelay += 0.0006f * (targetDelay - smoothedDelay);

            float tap[2] = { 0.0f, 0.0f };
            for (int c = 0; c < nch; ++c)
                tap[c] = read (c, smoothedDelay);

            float in[2] = { data[0][n], nch > 1 ? data[1][n] : data[0][n] };

            // filtro del feedback (paso bajo + corte de DC)
            float fbSig[2];
            for (int c = 0; c < 2; ++c)
            {
                const float t = (c < nch) ? tap[c] : 0.0f;
                lp[c] += lpCoeff * (t - lp[c]);
                const float y = lp[c] - hpIn[c] + hpCoeff * hp[c];
                hpIn[c] = lp[c];
                hp[c] = y;
                fbSig[c] = std::tanh (y * fb);
            }

            if (p.pingPong && nch > 1)
            {
                const float mono = 0.5f * (in[0] + in[1]);
                buf[0][(size_t) pos] = mono + fbSig[1];
                buf[1][(size_t) pos] = fbSig[0];
            }
            else
            {
                for (int c = 0; c < nch; ++c)
                    buf[c][(size_t) pos] = in[c] + fbSig[c];
            }

            for (int c = 0; c < nch; ++c)
                data[c][n] += tap[c] * p.mix;

            pos = (pos + 1) & mask;
        }
    }

private:
    inline float read (int c, float delay) const noexcept
    {
        const float rp = (float) pos - delay;
        const int i0 = (int) std::floor (rp);
        const float f = rp - (float) i0;
        const float a = buf[c][(size_t) (i0 & mask)];
        const float b = buf[c][(size_t) ((i0 + 1) & mask)];
        return a + f * (b - a);
    }

    double sr = 44100.0;
    int size = 131072, mask = 131071, pos = 0;
    std::vector<float> buf[2];
    float lp[2] { 0, 0 }, hp[2] { 0, 0 }, hpIn[2] { 0, 0 };
    float smoothedDelay = -1.0f;
};

} // namespace punto
