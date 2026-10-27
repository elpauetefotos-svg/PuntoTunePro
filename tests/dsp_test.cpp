// Prueba offline del motor de afinación: sin JUCE.
// g++ -O2 -std=c++17 -I../Source/dsp dsp_test.cpp -o dsp_test && ./dsp_test
#include "AutotuneEngine.h"
#include <chrono>
#include <cstdio>
#include <random>

using namespace punto;

static const double SR = 48000.0;

// "voz" sintética: diente de sierra filtrado + algo de ruido y vibrato opcional
static std::vector<float> makeVoice (float f0, float seconds, float vibCents = 0.0f, float vibHz = 5.5f)
{
    std::vector<float> v ((size_t) (SR * seconds));
    double phase = 0.0, lp = 0.0;
    std::mt19937 rng (1);
    std::uniform_real_distribution<float> nz (-1.0f, 1.0f);
    for (size_t i = 0; i < v.size(); ++i)
    {
        const double t = (double) i / SR;
        const double f = f0 * std::pow (2.0, vibCents / 1200.0 * std::sin (2.0 * M_PI * vibHz * t));
        phase += f / SR;
        phase -= std::floor (phase);
        double saw = 2.0 * phase - 1.0;
        lp += 0.25 * (saw - lp);
        v[i] = (float) (0.3 * lp + 0.003 * nz (rng));
    }
    return v;
}

static float measureHz (const std::vector<float>& x, size_t from, size_t len)
{
    PitchDetector d;
    d.prepare (SR);
    for (size_t i = from; i < from + len; ++i) d.push (x[i]);
    auto r = d.detect();
    return r.voiced ? r.frequency : -1.0f;
}

static std::vector<float> run (const std::vector<float>& in, TuneParams p)
{
    AutotuneEngine e;
    e.prepare (SR, 1);
    std::vector<float> out = in;
    float* ch[1] = { out.data() };
    const int block = 256;
    for (size_t i = 0; i < out.size(); i += block)
    {
        float* ptr[1] = { out.data() + i };
        e.process (ptr, 1, (int) std::min<size_t> (block, out.size() - i), p);
    }
    (void) ch;
    return out;
}

static double centsOff (float f, float target) { return 1200.0 * std::log2 (f / target); }

int main()
{
    int fails = 0;
    auto check = [&] (const char* name, bool ok, double value)
    {
        std::printf ("%-52s %s  (%.2f)\n", name, ok ? "OK " : "FALLO", value);
        if (! ok) ++fails;
    };

    // 1) 220 Hz con +35 cents -> cromático, speed 0: debe quedar en 220 Hz
    {
        const float f0 = 220.0f * std::pow (2.0f, 35.0f / 1200.0f);
        auto in  = makeVoice (f0, 1.5f);
        TuneParams p; p.speedMs = 0.0f;
        auto out = run (in, p);
        const float fin  = measureHz (in,  (size_t) (SR * 1.2), 4096);
        const float fout = measureHz (out, (size_t) (SR * 1.2), 4096);
        std::printf ("   entrada %.2f Hz -> salida %.2f Hz\n", fin, fout);
        check ("Cromático: +35 cents corregidos a la nota", std::abs (centsOff (fout, 220.0f)) < 8.0, centsOff (fout, 220.0f));
    }

    // 2) 233 Hz (A#3) con escala solo A (resto desactivado): sube/baja a la nota permitida más cercana
    {
        auto in = makeVoice (233.08f, 1.5f);
        TuneParams p; p.speedMs = 0.0f; p.noteMask = (1 << 9);    // solo A
        auto out = run (in, p);
        const float fout = measureHz (out, (size_t) (SR * 1.2), 4096);
        std::printf ("   233.08 Hz -> %.2f Hz\n", fout);
        check ("Máscara solo A: A#3 baja a A3 (220 Hz)", std::abs (centsOff (fout, 220.0f)) < 10.0, centsOff (fout, 220.0f));
    }

    // 3) vibrato fuerte, speed 0 -> pitch casi plano; speed lento -> conserva vibrato
    {
        auto in = makeVoice (220.0f, 2.0f, 40.0f);
        TuneParams p; p.speedMs = 0.0f;
        auto outFast = run (in, p);
        p.speedMs = 400.0f;
        auto outSlow = run (in, p);

        auto spread = [&] (const std::vector<float>& x)
        {
            double mn = 1e9, mx = -1e9;
            for (size_t s = (size_t) (SR * 0.8); s + 4096 < x.size(); s += 2400)
            {
                float f = measureHz (x, s, 4096);
                if (f < 0) continue;
                double c = centsOff (f, 220.0f);
                mn = std::min (mn, c); mx = std::max (mx, c);
            }
            return mx - mn;
        };
        const double sIn = spread (in), sFast = spread (outFast), sSlow = spread (outSlow);
        std::printf ("   rango del vibrato: entrada %.1f, speed0 %.1f, speed400 %.1f cents\n", sIn, sFast, sSlow);
        check ("Speed 0 aplana el vibrato", sFast < sIn * 0.6, sFast);
        check ("Speed 400 conserva más vibrato que speed 0", sSlow > sFast, sSlow);
    }

    // 4) Transposición +12 st con corrección desactivada (amount 0)
    {
        auto in = makeVoice (220.0f, 1.5f);
        TuneParams p; p.amount = 0.0f; p.transposeSemis = 12.0f;
        auto out = run (in, p);
        const float fout = measureHz (out, (size_t) (SR * 1.2), 4096);
        check ("Transposición +12 st -> 440 Hz", std::abs (centsOff (fout, 440.0f)) < 15.0, centsOff (fout, 440.0f));
    }

    // 5) Silencio -> silencio, sin NaN
    {
        std::vector<float> in ((size_t) SR, 0.0f);
        TuneParams p;
        auto out = run (in, p);
        bool bad = false; float mx = 0;
        for (float v : out) { if (! std::isfinite (v)) bad = true; mx = std::max (mx, std::abs (v)); }
        check ("Silencio: sin NaN y salida nula", ! bad && mx < 1e-6f, mx);
    }

    // 6) Rendimiento
    {
        auto in = makeVoice (180.0f, 10.0f, 20.0f);
        TuneParams p;
        auto t0 = std::chrono::steady_clock::now();
        auto out = run (in, p);
        auto t1 = std::chrono::steady_clock::now();
        const double secs = std::chrono::duration<double> (t1 - t0).count();
        std::printf ("   10 s de audio procesados en %.3f s  (CPU ~ %.1f %%)\n", secs, secs / 10.0 * 100.0);
        check ("CPU por debajo del 15% de un núcleo (mono)", secs / 10.0 < 0.15, secs / 10.0 * 100.0);
    }

    std::printf (fails == 0 ? "\nTODO OK\n" : "\n%d pruebas fallaron\n", fails);
    return fails == 0 ? 0 : 1;
}
