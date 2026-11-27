// Test silnika DSP: g++ -O2 -std=c++17 tests/engine_test.cpp -o engine_test
#include "../Source/dsp/MateoEngine.h"
#include <cstdio>
#include <functional>

using namespace mateo;

// niezalezny pomiar f0 (YIN, pelna czestotliwosc probkowania)
static double peakHz (const std::vector<float>& x, double sr, double lo, double hi)
{
    const int W = 4096, maxLag = (int) (sr / lo), minLag = (int) (sr / hi);
    const float* s = x.data() + (x.size() - W - maxLag - 1);
    std::vector<double> d (maxLag + 2, 1.0);
    double run = 0;
    for (int t = 1; t <= maxLag + 1; ++t)
    {
        double acc = 0;
        for (int j = 0; j < W; ++j) { const double df = s[j] - s[j + t]; acc += df * df; }
        run += acc;
        d[t] = acc * t / run;
    }
    int best = -1;
    for (int t = minLag; t < maxLag; ++t)
        if (d[t] < 0.2) { while (t + 1 < maxLag && d[t + 1] < d[t]) ++t; best = t; break; }
    if (best < 0) { double bv = 9; for (int t = minLag; t < maxLag; ++t) if (d[t] < bv) { bv = d[t]; best = t; } }
    const double a = d[best - 1], b = d[best], c = d[best + 1];
    const double den = a + c - 2 * b;
    const double tt = best + (std::abs (den) > 1e-12 ? 0.5 * (a - c) / den : 0);
    return sr / tt;
}

static double rms (const std::vector<float>& x) { double e = 0; for (float v : x) e += v * v; return std::sqrt (e / x.size()); }

struct Result { double hz, level; };

static Result run (double inHz, double sr, std::function<void (EngineParams&)> cfg, bool saw = true, bool midi = false, int midiNote = 0)
{
    HarmonizerEngine eng;
    eng.prepare (sr);
    EngineParams p;
    p.directOn = false;
    cfg (p);
    if (midi) eng.noteOn (midiNote, 1, p);
    const int N = (int) (sr * 2.0), B = 256;
    std::vector<float> L (N), R (N);
    double ph = 0;
    for (int i = 0; i < N; ++i)
    {
        ph += inHz / sr; ph -= std::floor (ph);
        const float v = saw ? (float) (0.3 * (2 * ph - 1)) : (float) (0.3 * std::sin (2 * kPi * ph));
        L[i] = R[i] = v;
    }
    for (int i = 0; i < N; i += B) eng.process (L.data() + i, R.data() + i, std::min (B, N - i), p);
    std::vector<float> tail (L.begin() + N / 2, L.end());
    for (size_t i = 0; i < tail.size(); ++i) tail[i] = 0.5f * (L[N / 2 + i] + R[N / 2 + i]);
    return { peakHz (tail, sr, 50, 1500), rms (tail) };
}

static int fails = 0;
static void check (const char* name, double got, double want, double tolCents = 15)
{
    const double cents = 1200 * std::log2 (got / want);
    const bool ok = std::abs (cents) <= tolCents;
    if (! ok) ++fails;
    std::printf ("%-48s got %8.2f Hz  want %8.2f Hz  (%+6.1f c)  %s\n", name, got, want, cents, ok ? "OK" : "FAIL");
}

int main()
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("--- sample rate %.0f ---\n", sr);
        auto h1 = [] (int scale) { return [scale] (EngineParams& p) { p.harm[0].on = true; p.harm[0].scale = scale; p.harm[0].quantise = 100; }; };

        check ("Direct only (passthrough)", run (220, sr, [] (EngineParams& p) { p.directOn = true; }).hz, 220);
        check ("User 7 (+7 st) from A3", run (220, sr, h1 (31)).hz, 329.63);
        check ("High Octave from A3", run (220, sr, h1 (1)).hz, 440);
        check ("Low Octave from A3", run (220, sr, h1 (2)).hz, 110);
        check ("High 3rd Major, key C, A3 -> C4", run (220, sr, h1 (3)).hz, 261.63);
        check ("Low 3rd Major, key C, A3 -> F3", run (220, sr, h1 (4)).hz, 174.61);
        check ("High 5th Harmonic Minor, key A, A3 -> E4", run (220, sr, [] (EngineParams& p) { p.key = 9; p.harm[0].on = true; p.harm[0].scale = 21; p.harm[0].quantise = 100; }).hz, 329.63);
        check ("High 3rd Harmonic Minor key A, E3 -> G#3", run (164.81, sr, [] (EngineParams& p) { p.key = 9; p.harm[0].on = true; p.harm[0].scale = 19; p.harm[0].quantise = 100; }).hz, 207.65);
        check ("Unison quantise 100: 226 Hz -> 220 (A3)", run (226, sr, [] (EngineParams& p) { p.unison.on = true; p.unison.quantise = 100; }).hz, 220, 10);
        check ("Unison quantise 0: 226 Hz stays", run (226, sr, [] (EngineParams& p) { p.unison.on = true; p.unison.quantise = 0; }).hz, 226, 10);
        check ("MIDI scale, held D4, saw A3 -> D4", run (220, sr, h1 (0), true, true, 62).hz, 293.66);
        check ("Formant +50 keeps pitch (saw)", run (220, sr, [] (EngineParams& p) { p.harm[0].on = true; p.harm[0].scale = 1; p.harm[0].formant = 50; }, true).hz, 440);
        check ("Formant -50 keeps pitch (saw)", run (220, sr, [] (EngineParams& p) { p.harm[0].on = true; p.harm[0].scale = 1; p.harm[0].formant = -50; }, true).hz, 440);
        check ("Dual on, +7, saw", run (196, sr, [] (EngineParams& p) { p.harm[0].on = true; p.harm[0].scale = 31; p.harm[0].dual = true; p.harm[0].quantise = 100; }, true).hz, 293.66, 20);

        auto lv = run (220, sr, h1 (31));
        std::printf ("  level of +7 voice: rms %.3f (input saw rms 0.173)\n", lv.level);
        auto off = run (220, sr, h1 (0)); // MIDI bez nut => cisza
        std::printf ("  MIDI scale without notes: rms %.5f %s\n", off.level, off.level < 1e-3 ? "OK" : "FAIL");
        if (off.level >= 1e-3) ++fails;
    }
    std::printf (fails ? "\n%d FAILED\n" : "\nALL PASSED\n", fails);
    return fails ? 1 : 0;
}
