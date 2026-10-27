#pragma once
// Punto Tune Pro - detector de pitch monofónico (YIN con salida temprana).
// C++ puro (sin JUCE) para poder probarlo fuera del plugin.

#include <algorithm>
#include <cmath>
#include <vector>

namespace punto
{

struct PitchResult
{
    bool   voiced     = false;
    float  frequency  = 0.0f;   // Hz
    float  confidence = 0.0f;   // 0..1
    float  periodSamples = 0.0f;
};

class PitchDetector
{
public:
    void prepare (double sampleRate, float minHz = 70.0f, float maxHz = 1000.0f)
    {
        sr      = sampleRate;
        minTau  = std::max (2, (int) std::floor (sr / (double) maxHz));
        maxTau  = (int) std::ceil (sr / (double) minHz);
        window  = std::max (512, (int) std::lround (sr * 0.024));   // ~24 ms de integración
        ringSize = 1;
        while (ringSize < window + maxTau + 8) ringSize <<= 1;
        ring.assign ((size_t) ringSize, 0.0f);
        linear.assign ((size_t) (window + maxTau + 8), 0.0f);
        diff.assign ((size_t) (maxTau + 2), 0.0f);
        cmnd.assign ((size_t) (maxTau + 2), 0.0f);
        writePos = 0;
    }

    inline void push (float s) noexcept
    {
        ring[(size_t) writePos] = s;
        writePos = (writePos + 1) & (ringSize - 1);
    }

    // Analiza los últimos (window + maxTau) samples.
    PitchResult detect (float threshold = 0.15f) noexcept
    {
        PitchResult out;
        const int total = window + maxTau;

        // copia lineal (el más antiguo primero)
        int idx = (writePos - total + ringSize * 2) & (ringSize - 1);
        double energy = 0.0;
        for (int i = 0; i < total; ++i)
        {
            const float v = ring[(size_t) idx];
            linear[(size_t) i] = v;
            if (i >= total - window) energy += (double) v * v;
            idx = (idx + 1) & (ringSize - 1);
        }

        const float rms = (float) std::sqrt (energy / (double) window);
        if (rms < gateLevel)
            return out;                       // silencio => sin pitch

        // YIN: función de diferencia + normalización acumulada con parada temprana
        const float* x = linear.data() + (total - window - maxTau);   // base de la ventana
        const int W = window;

        cmnd[0] = 1.0f;
        double runningSum = 0.0;
        int found = -1;

        for (int tau = 1; tau <= maxTau; ++tau)
        {
            double d = 0.0;
            const float* a = x + maxTau;           // x[j]  (parte más reciente)
            const float* b = x + maxTau - tau;     // x[j+tau] desplazado hacia atrás
            for (int j = 0; j < W; ++j)
            {
                const float t = a[j] - b[j];
                d += (double) t * t;
            }
            diff[(size_t) tau] = (float) d;
            runningSum += d;
            cmnd[(size_t) tau] = runningSum > 1e-12 ? (float) (d * tau / runningSum) : 1.0f;

            if (tau >= minTau && cmnd[(size_t) tau] < threshold)
            {
                // seguir bajando hasta el mínimo local
                int t2 = tau;
                while (t2 + 1 <= maxTau)
                {
                    // calcular el siguiente
                    const int tn = t2 + 1;
                    double dn = 0.0;
                    const float* bn = x + maxTau - tn;
                    for (int j = 0; j < W; ++j)
                    {
                        const float t = a[j] - bn[j];
                        dn += (double) t * t;
                    }
                    diff[(size_t) tn] = (float) dn;
                    runningSum += dn;
                    cmnd[(size_t) tn] = runningSum > 1e-12 ? (float) (dn * tn / runningSum) : 1.0f;
                    if (cmnd[(size_t) tn] < cmnd[(size_t) t2]) t2 = tn;
                    else break;
                }
                found = t2;
                break;
            }
        }

        if (found < 0)
            return out;                       // no hay periodicidad clara

        // interpolación parabólica
        float tauF = (float) found;
        if (found > 1 && found < maxTau)
        {
            const float s0 = cmnd[(size_t) found - 1];
            const float s1 = cmnd[(size_t) found];
            const float s2 = cmnd[(size_t) found + 1];
            const float den = s0 + s2 - 2.0f * s1;
            if (std::abs (den) > 1e-9f)
                tauF += 0.5f * (s0 - s2) / den;
        }

        out.voiced        = true;
        out.periodSamples = tauF;
        out.frequency     = (float) (sr / (double) tauF);
        out.confidence    = std::clamp (1.0f - cmnd[(size_t) found], 0.0f, 1.0f);
        return out;
    }

    void setGate (float rmsLevel) noexcept { gateLevel = rmsLevel; }
    int  getWindow() const noexcept { return window; }

private:
    double sr = 44100.0;
    int minTau = 40, maxTau = 630, window = 1058;
    int ringSize = 4096, writePos = 0;
    float gateLevel = 0.004f;                 // ~ -48 dBFS
    std::vector<float> ring, linear, diff, cmnd;
};

} // namespace punto
