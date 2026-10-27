#pragma once
// Punto Tune Pro - compresor feed-forward con rodilla suave, enlace estéreo y mezcla paralela.

#include <algorithm>
#include <atomic>
#include <cmath>

namespace punto
{

struct CompParams
{
    bool  on          = false;
    float thresholdDb = -18.0f;
    float ratio       = 4.0f;
    float attackMs    = 10.0f;
    float releaseMs   = 120.0f;
    float makeupDb    = 0.0f;
    float mix         = 1.0f;     // 0..1 (compresión paralela)
};

class Compressor
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        grDb = 0.0f;
        grMeter.store (0.0f);
    }

    void reset() { grDb = 0.0f; }

    float getGainReductionDb() const noexcept { return grMeter.load (std::memory_order_relaxed); }

    void process (float* const* data, int numCh, int numSamples, const CompParams& p) noexcept
    {
        if (! p.on)
        {
            grDb = 0.0f;
            grMeter.store (0.0f, std::memory_order_relaxed);
            return;
        }

        const float atk = std::exp (-1.0f / (0.001f * std::max (0.05f, p.attackMs)  * (float) sr));
        const float rel = std::exp (-1.0f / (0.001f * std::max (1.0f,  p.releaseMs) * (float) sr));
        const float makeup = std::pow (10.0f, p.makeupDb * 0.05f);
        const float knee = 6.0f;
        const float invR = 1.0f / std::max (1.0f, p.ratio);

        float worst = 0.0f;

        for (int n = 0; n < numSamples; ++n)
        {
            float peak = 0.0f;
            for (int c = 0; c < numCh; ++c)
                peak = std::max (peak, std::abs (data[c][n]));

            const float levelDb = 8.685889638f * std::log (std::max (peak, 1.0e-6f));   // 20*log10
            const float over = levelDb - p.thresholdDb;

            float targetGr;                                   // <= 0 dB
            if (2.0f * over < -knee)       targetGr = 0.0f;
            else if (2.0f * std::abs (over) <= knee)
            {
                const float x = over + knee * 0.5f;
                targetGr = (invR - 1.0f) * x * x / (2.0f * knee);
            }
            else                            targetGr = over * (invR - 1.0f);

            const float coeff = targetGr < grDb ? atk : rel;
            grDb = targetGr + coeff * (grDb - targetGr);

            const float g = std::pow (10.0f, grDb * 0.05f) * makeup;
            worst = std::min (worst, grDb);

            for (int c = 0; c < numCh; ++c)
            {
                const float dry = data[c][n];
                data[c][n] = dry * (1.0f - p.mix) + dry * g * p.mix;
            }
        }
        grMeter.store (worst, std::memory_order_relaxed);
    }

private:
    double sr = 44100.0;
    float grDb = 0.0f;
    std::atomic<float> grMeter { 0.0f };
};

} // namespace punto
