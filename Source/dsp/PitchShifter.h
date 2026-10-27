#pragma once
// Punto Tune Pro - cambiador de tono en tiempo real.
// Línea de retardo con dos "taps" cruzados con ventana Hann. Si la longitud de
// ventana es un múltiplo de 2 periodos de la voz, los dos taps suman en fase y
// no hay peine ni modulación audible.

#include <algorithm>
#include <cmath>
#include <vector>

namespace punto
{

class PitchShifter
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        sr = sampleRate;
        channels = std::max (1, numChannels);
        size = 1;
        while (size < (int) (sr * 0.25)) size <<= 1;
        mask = size - 1;
        buffers.assign ((size_t) channels, std::vector<float> ((size_t) size, 0.0f));
        reset();
    }

    void reset()
    {
        for (auto& b : buffers) std::fill (b.begin(), b.end(), 0.0f);
        writePos = 0;
        phase = 0.0f;
    }

    // Un frame (un sample por canal). ratio = factor de frecuencia de salida.
    // windowSamples = longitud de ventana (idealmente 2*k periodos).
    inline void processFrame (const float* in, float* out, float ratio, float windowSamples) noexcept
    {
        const float W = std::clamp (windowSamples, 64.0f, (float) (size - 16));

        for (int c = 0; c < channels; ++c)
            buffers[(size_t) c][(size_t) writePos] = in[c];

        const float p2 = phase + 0.5f >= 1.0f ? phase - 0.5f : phase + 0.5f;
        const float d1 = phase * W + 2.0f;
        const float d2 = p2 * W + 2.0f;

        const float s1 = std::sin (3.14159265f * phase);
        const float g1 = s1 * s1;
        const float g2 = 1.0f - g1;

        for (int c = 0; c < channels; ++c)
        {
            const auto& b = buffers[(size_t) c];
            out[c] = g1 * readCubic (b, d1) + g2 * readCubic (b, d2);
        }

        writePos = (writePos + 1) & mask;

        phase += (1.0f - ratio) / W;
        if (phase >= 1.0f) phase -= 1.0f;
        if (phase < 0.0f)  phase += 1.0f;
    }

private:
    inline float readCubic (const std::vector<float>& b, float delay) const noexcept
    {
        // posición de lectura = writePos - delay (writePos ya contiene la muestra actual)
        const float pos = (float) writePos - delay;
        const int   i0  = (int) std::floor (pos);
        const float f   = pos - (float) i0;

        const float y0 = b[(size_t) ((i0 - 1) & mask)];
        const float y1 = b[(size_t) ((i0    ) & mask)];
        const float y2 = b[(size_t) ((i0 + 1) & mask)];
        const float y3 = b[(size_t) ((i0 + 2) & mask)];

        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }

    double sr = 44100.0;
    int channels = 2, size = 16384, mask = 16383, writePos = 0;
    float phase = 0.0f;
    std::vector<std::vector<float>> buffers;
};

} // namespace punto
