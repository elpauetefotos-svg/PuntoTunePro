#pragma once
// Punto Tune Pro - motor de afinación en tiempo real.
// Detecta el pitch (YIN), lo cuantiza a las notas permitidas, suaviza la
// corrección según "Speed" y aplica el cambio de tono con PitchShifter.

#include "PitchDetector.h"
#include "PitchShifter.h"
#include <atomic>
#include <array>
#include <cstdint>

namespace punto
{

struct TuneParams
{
    float    speedMs        = 20.0f;   // 0 = instantáneo (efecto robot), 400 = natural
    float    amount         = 1.0f;    // 0..1
    float    humanize       = 0.0f;    // 0..1  (notas largas se corrigen más despacio)
    float    transposeSemis = 0.0f;    // -12..+12
    float    fineCents      = 0.0f;    // -100..+100
    float    referenceA     = 440.0f;
    uint16_t noteMask       = 0x0FFF;  // bit 0 = C ... bit 11 = B
    bool     bypass         = false;
};

class AutotuneEngine
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        sr = sampleRate;
        channels = std::max (1, std::min (2, numChannels));
        detector.prepare (sr);
        shifter.prepare (sr, channels);
        hopSize = std::max (32, (int) std::lround (sr * 0.003));
        reset();
    }

    void reset()
    {
        shifter.reset();
        hopCounter = 0;
        correctionTarget = 0.0f;
        correctionSmoothed = 0.0f;
        currentTarget = -1;
        heldSamples = 0;
        voicedHold = 0;
        windowSmoothed = (float) (sr * 0.020);
        hist.fill (-1.0f);
        histIdx = 0;
        lastConfident = -1.0f;
        pendingJump = 0;
        detectedMidiAtomic.store (-1.0f);
        correctedMidiAtomic.store (-1.0f);
        targetMidiAtomic.store (-1.0f);
    }

    int getLatencySamples() const noexcept { return (int) (sr * 0.010); }

    // Telemetría para la interfaz (notas MIDI fraccionarias, -1 = sin voz)
    float getDetectedMidi()  const noexcept { return detectedMidiAtomic.load  (std::memory_order_relaxed); }
    float getCorrectedMidi() const noexcept { return correctedMidiAtomic.load (std::memory_order_relaxed); }
    float getTargetMidi()    const noexcept { return targetMidiAtomic.load    (std::memory_order_relaxed); }

    // Procesa en sitio. data[canal][muestra]
    void process (float* const* data, int numCh, int numSamples, const TuneParams& p) noexcept
    {
        const int nch = std::min (numCh, channels);
        if (nch <= 0) return;

        // coeficiente base de suavizado de la corrección
        float in[2] = { 0, 0 }, out[2] = { 0, 0 };

        for (int n = 0; n < numSamples; ++n)
        {
            float mono = 0.0f;
            for (int c = 0; c < nch; ++c) { in[c] = data[c][n]; mono += in[c]; }
            mono /= (float) nch;
            if (nch == 1) in[1] = in[0];

            detector.push (mono);

            if (++hopCounter >= hopSize)
            {
                hopCounter = 0;
                analyse (p);
            }

            if (p.bypass)
            {
                // seguimos alimentando el shifter para que no haya saltos al reactivar
                shifter.processFrame (in, out, 1.0f, windowSmoothed);
                continue;
            }

            // --- suavizado en semitonos ---
            float tauMs = p.speedMs;
            if (p.humanize > 0.0f)
            {
                const float held = std::min (1.0f, (float) heldSamples / (float) (sr * 0.35));
                tauMs *= 1.0f + p.humanize * held * 6.0f;
            }
            const float coeff = tauMs <= 0.5f ? 1.0f
                                              : 1.0f - std::exp (-1.0f / (0.001f * tauMs * (float) sr * 0.35f));
            correctionSmoothed += coeff * (correctionTarget - correctionSmoothed);

            // suavizado lento de la ventana para no provocar saltos de retardo
            windowSmoothed += 0.0008f * (windowTarget - windowSmoothed);

            const float totalSemis = correctionSmoothed * p.amount + p.transposeSemis + p.fineCents * 0.01f;
            const float ratio = std::exp2 (totalSemis * (1.0f / 12.0f));

            shifter.processFrame (in, out, ratio, windowSmoothed);

            for (int c = 0; c < nch; ++c)
                data[c][n] = out[c];

            if (n == numSamples - 1)
            {
                const float d = detectedMidiAtomic.load (std::memory_order_relaxed);
                correctedMidiAtomic.store (d < 0.0f ? -1.0f : d + totalSemis, std::memory_order_relaxed);
            }
        }
    }

private:
    static float freqToMidi (float f, float refA) noexcept
    {
        return 69.0f + 12.0f * std::log2 (f / refA);
    }

    int nearestAllowed (float midi, uint16_t mask) const noexcept
    {
        int best = -1;
        float bestDist = 1e9f;
        const int base = (int) std::floor (midi);
        for (int n = base - 7; n <= base + 8; ++n)
        {
            const int pc = ((n % 12) + 12) % 12;
            if (! ((mask >> pc) & 1)) continue;
            const float d = std::abs (midi - (float) n);
            if (d < bestDist) { bestDist = d; best = n; }
        }
        return best;
    }

    void analyse (const TuneParams& p) noexcept
    {
        auto r = detector.detect();

        bool voiced = r.voiced && r.confidence > 0.80f;
        float midi = -1.0f;

        if (voiced)
        {
            float m = freqToMidi (r.frequency, p.referenceA);

            // protección contra saltos de octava espurios: exige 2 lecturas seguidas
            if (lastConfident > 0.0f && std::abs (m - lastConfident) > 5.5f && pendingJump < 2)
            {
                ++pendingJump;
                m = lastConfident;
            }
            else
                pendingJump = 0;

            // mediana de 3
            hist[(size_t) histIdx] = m;
            histIdx = (histIdx + 1) % (int) hist.size();
            std::array<float, 3> s = hist;
            std::sort (s.begin(), s.end());
            midi = s[1] > 0.0f ? s[1] : m;
            lastConfident = midi;

            // ventana del shifter: 2 periodos x k (>= 14 ms)
            float period = r.periodSamples;
            float w = 2.0f * period;
            const float minW = (float) (sr * 0.014);
            while (w < minW) w += 2.0f * period;
            windowTarget = std::min (w, (float) (sr * 0.045));

            voicedHold = (int) (sr * 0.060 / hopSize) + 1;
        }
        else if (voicedHold > 0)
        {
            --voicedHold;
            midi = lastConfident;    // mantener un instante (consonantes cortas)
            voiced = midi > 0.0f;
        }

        if (voiced && midi > 0.0f)
        {
            // histéresis para no bailar entre dos notas
            int target = currentTarget;
            const bool curAllowed = target >= 0 && ((p.noteMask >> (((target % 12) + 12) % 12)) & 1);
            if (! (curAllowed && std::abs (midi - (float) target) < 0.65f))
                target = nearestAllowed (midi, p.noteMask);

            if (target != currentTarget) { currentTarget = target; heldSamples = 0; }
            else heldSamples += hopSize;

            if (target >= 0)
            {
                correctionTarget = std::clamp ((float) target - midi, -12.0f, 12.0f);
                targetMidiAtomic.store ((float) target, std::memory_order_relaxed);
            }
            else
            {
                correctionTarget = 0.0f;
                targetMidiAtomic.store (-1.0f, std::memory_order_relaxed);
            }
            detectedMidiAtomic.store (midi, std::memory_order_relaxed);
        }
        else
        {
            // sin voz: la corrección vuelve a 0 suavemente
            correctionTarget = 0.0f;
            currentTarget = -1;
            heldSamples = 0;
            lastConfident = -1.0f;
            hist.fill (-1.0f);
            detectedMidiAtomic.store (-1.0f, std::memory_order_relaxed);
            targetMidiAtomic.store (-1.0f, std::memory_order_relaxed);
        }
    }

    double sr = 44100.0;
    int channels = 2, hopSize = 132, hopCounter = 0;

    PitchDetector detector;
    PitchShifter  shifter;

    float correctionTarget = 0.0f, correctionSmoothed = 0.0f;
    float windowSmoothed = 882.0f, windowTarget = 882.0f;
    int   currentTarget = -1, heldSamples = 0, voicedHold = 0;
    float lastConfident = -1.0f;
    int   pendingJump = 0;
    std::array<float, 3> hist { -1.0f, -1.0f, -1.0f };
    int histIdx = 0;

    std::atomic<float> detectedMidiAtomic { -1.0f }, correctedMidiAtomic { -1.0f }, targetMidiAtomic { -1.0f };
};

} // namespace punto
