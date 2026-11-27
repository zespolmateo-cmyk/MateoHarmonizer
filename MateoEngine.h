// =============================================================================
//  Mateo Harmonizer - silnik DSP (czyste C++, bez zaleznosci od JUCE)
//
//  * detekcja wysokosci dzwieku: YIN (na sygnale zdecymowanym)
//  * zmiana wysokosci: TD-PSOLA (zachowuje formanty) + niezalezne przesuwanie
//    formantow (pokretlo FORMANT M..F)
//  * 1 glos UNISON + 4 glosy HARMONY (kazdy z opcja DUAL = drugi, rozstrojony glos)
//  * skale diatoniczne (dur / mol / mol harmoniczny), interwaly stale oraz MIDI
// =============================================================================
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace mateo
{

constexpr double kPi    = 3.14159265358979323846;
constexpr float  kMinHz = 75.0f;   // najnizszy wykrywany dzwiek
constexpr float  kMaxHz = 1100.0f; // najwyzszy wykrywany dzwiek

inline float dbToGain (float db) { return db <= -59.9f ? 0.0f : std::pow (10.0f, db / 20.0f); }

inline int floorDiv (int a, int b) { int q = a / b; if ((a % b != 0) && ((a < 0) != (b < 0))) --q; return q; }

// -----------------------------------------------------------------------------
//  Skale / interwaly harmonii (pole SCALE)
// -----------------------------------------------------------------------------
enum class ScaleKind { Midi, Fixed, Diatonic };

struct ScaleDef
{
    const char* name;
    ScaleKind   kind;
    int         semitones; // dla Fixed
    int         degrees;   // dla Diatonic (stopnie skali, + w gore, - w dol)
    int         mode;      // 0 = dur, 1 = mol naturalny, 2 = mol harmoniczny
};

inline const std::vector<ScaleDef>& getScales()
{
    static const std::vector<ScaleDef> s = {
        { "MIDI",                    ScaleKind::Midi,     0,   0, 0 },
        { "High Octave",             ScaleKind::Fixed,    12,  0, 0 },
        { "Low Octave",              ScaleKind::Fixed,    -12, 0, 0 },
        { "High 3rd Major",          ScaleKind::Diatonic, 0,   2, 0 },
        { "Low 3rd Major",           ScaleKind::Diatonic, 0,  -2, 0 },
        { "High 4th Major",          ScaleKind::Diatonic, 0,   3, 0 },
        { "Low 4th Major",           ScaleKind::Diatonic, 0,  -3, 0 },
        { "High 5th Major",          ScaleKind::Diatonic, 0,   4, 0 },
        { "Low 5th Major",           ScaleKind::Diatonic, 0,  -4, 0 },
        { "High 6th Major",          ScaleKind::Diatonic, 0,   5, 0 },
        { "Low 6th Major",           ScaleKind::Diatonic, 0,  -5, 0 },
        { "High 3rd Minor",          ScaleKind::Diatonic, 0,   2, 1 },
        { "Low 3rd Minor",           ScaleKind::Diatonic, 0,  -2, 1 },
        { "High 4th Minor",          ScaleKind::Diatonic, 0,   3, 1 },
        { "Low 4th Minor",           ScaleKind::Diatonic, 0,  -3, 1 },
        { "High 5th Minor",          ScaleKind::Diatonic, 0,   4, 1 },
        { "Low 5th Minor",           ScaleKind::Diatonic, 0,  -4, 1 },
        { "High 6th Minor",          ScaleKind::Diatonic, 0,   5, 1 },
        { "Low 6th Minor",           ScaleKind::Diatonic, 0,  -5, 1 },
        { "High 3rd Harmonic Minor", ScaleKind::Diatonic, 0,   2, 2 },
        { "Low 3rd Harmonic Minor",  ScaleKind::Diatonic, 0,  -2, 2 },
        { "High 5th Harmonic Minor", ScaleKind::Diatonic, 0,   4, 2 },
        { "Low 5th Harmonic Minor",  ScaleKind::Diatonic, 0,  -4, 2 },
        { "High 6th Harmonic Minor", ScaleKind::Diatonic, 0,   5, 2 },
        { "Low 6th Harmonic Minor",  ScaleKind::Diatonic, 0,  -5, 2 },
        { "User 1 (+1 semitone)",    ScaleKind::Fixed,    1,   0, 0 },
        { "User 2 (+2 semitones)",   ScaleKind::Fixed,    2,   0, 0 },
        { "User 3 (+3 semitones)",   ScaleKind::Fixed,    3,   0, 0 },
        { "User 4 (+4 semitones)",   ScaleKind::Fixed,    4,   0, 0 },
        { "User 5 (+5 semitones)",   ScaleKind::Fixed,    5,   0, 0 },
        { "User 6 (+6 semitones)",   ScaleKind::Fixed,    6,   0, 0 },
        { "User 7 (+7 semitones)",   ScaleKind::Fixed,    7,   0, 0 },
        { "User 8 (-3 semitones)",   ScaleKind::Fixed,   -3,   0, 0 },
        { "User 9 (-4 semitones)",   ScaleKind::Fixed,   -4,   0, 0 },
        { "User 10 (-5 semitones)",  ScaleKind::Fixed,   -5,   0, 0 },
        { "User 11 (-7 semitones)",  ScaleKind::Fixed,   -7,   0, 0 },
    };
    return s;
}

inline const int* scaleSteps (int mode)
{
    static const int major[7] = { 0, 2, 4, 5, 7, 9, 11 };
    static const int minor[7] = { 0, 2, 3, 5, 7, 8, 10 };
    static const int hminor[7] = { 0, 2, 3, 5, 7, 8, 11 };
    return mode == 0 ? major : (mode == 1 ? minor : hminor);
}

// Zwraca docelowa nute (MIDI) dla harmonii diatonicznej.
inline int diatonicTarget (int note, int key, int degrees, int mode)
{
    const int* st = scaleSteps (mode);
    const int rel = note - key;
    const int oct = floorDiv (rel, 12);
    const int pc  = rel - oct * 12;

    int best = 0, bestDist = 100;
    for (int i = 0; i <= 7; ++i)
    {
        const int v = (i == 7) ? 12 : st[i];
        const int d = std::abs (v - pc);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    const int idx  = oct * 7 + best + degrees;
    const int tOct = floorDiv (idx, 7);
    const int ti   = idx - tOct * 7;
    return key + tOct * 12 + st[ti];
}

// -----------------------------------------------------------------------------
//  Detektor wysokosci (YIN)
// -----------------------------------------------------------------------------
class PitchDetector
{
public:
    void prepare (double sr)
    {
        decim  = sr > 60000.0 ? 4 : 2;
        dsr    = (float) (sr / decim);
        maxLag = (int) std::ceil (dsr / kMinHz);
        minLag = std::max (2, (int) std::floor (dsr / kMaxHz));
        window = maxLag;
        bufLen = window + maxLag + 1;
        ring.assign ((size_t) bufLen, 0.0f);
        lin.assign ((size_t) bufLen, 0.0f);
        diff.assign ((size_t) maxLag + 2, 0.0f);
        writeIdx = 0; accum = 0; accCount = 0; hopCounter = 0;
        freq = 0; voiced = false; hist = { 0, 0, 0 };
    }

    // zwraca true, gdy pojawila sie nowa estymacja
    bool push (float x)
    {
        accum += x;
        if (++accCount < decim) return false;
        const float y = accum / (float) decim;
        accum = 0; accCount = 0;
        ring[(size_t) writeIdx] = y;
        writeIdx = (writeIdx + 1) % bufLen;
        if (++hopCounter < hop) return false;
        hopCounter = 0;
        analyse();
        return true;
    }

    float getFrequency() const { return voiced ? freq : 0.0f; }

private:
    void analyse()
    {
        for (int i = 0; i < bufLen; ++i)
            lin[(size_t) i] = ring[(size_t) ((writeIdx + i) % bufLen)];

        // bramka energii
        double e = 0;
        for (int i = bufLen - window; i < bufLen; ++i) e += (double) lin[(size_t) i] * lin[(size_t) i];
        const float rms = (float) std::sqrt (e / window);
        if (rms < 0.003f) { voiced = false; return; }

        const float* x = lin.data() + (bufLen - window - maxLag);
        diff[0] = 1.0f;
        double running = 0;
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            double d = 0;
            for (int j = 0; j < window; ++j)
            {
                const float df = x[j] - x[j + tau];
                d += (double) df * df;
            }
            running += d;
            diff[(size_t) tau] = running > 0 ? (float) (d * tau / running) : 1.0f;
        }

        int tauEst = -1;
        for (int tau = minLag; tau < maxLag; ++tau)
        {
            if (diff[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < maxLag && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
                tauEst = tau;
                break;
            }
        }
        if (tauEst < 0)
        {
            int   bt = -1; float bv = 1e9f;
            for (int tau = minLag; tau < maxLag; ++tau)
                if (diff[(size_t) tau] < bv) { bv = diff[(size_t) tau]; bt = tau; }
            if (bv < 0.3f) tauEst = bt;
        }
        if (tauEst < 0) { voiced = false; return; }

        float better = (float) tauEst;
        if (tauEst > 1 && tauEst < maxLag)
        {
            const float s0 = diff[(size_t) tauEst - 1], s1 = diff[(size_t) tauEst], s2 = diff[(size_t) tauEst + 1];
            const float den = s0 + s2 - 2 * s1;
            if (std::abs (den) > 1e-9f) better += 0.5f * (s0 - s2) / den;
        }
        const float f = dsr / better;

        hist[0] = hist[1]; hist[1] = hist[2]; hist[2] = f;
        std::array<float, 3> h = hist;
        std::sort (h.begin(), h.end());
        freq   = (h[0] > 0) ? h[1] : f; // mediana z 3 (odporna na bledy oktawowe)
        voiced = true;
    }

    int decim = 2, maxLag = 0, minLag = 2, window = 0, bufLen = 0;
    int writeIdx = 0, accCount = 0, hopCounter = 0, hop = 128;
    float dsr = 24000, accum = 0, freq = 0;
    bool voiced = false;
    std::array<float, 3> hist { 0, 0, 0 };
    std::vector<float> ring, lin, diff;
};

// -----------------------------------------------------------------------------
//  Powolny losowy dryf (pokretlo RANDOM)
// -----------------------------------------------------------------------------
class RandomDrift
{
public:
    void prepare (double sr, uint32_t seed) { sampleRate = (float) sr; state = seed ? seed : 1u; value = target = 0; counter = 0; }
    float next()
    {
        if (--counter <= 0)
        {
            counter = (int) (sampleRate * (0.15f + 0.25f * rnd01()));
            target  = rnd01() * 2.0f - 1.0f;
        }
        value += (target - value) * (6.0f / sampleRate);
        return value;
    }
private:
    float rnd01() { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return (float) (state & 0xFFFFFF) / 16777215.0f; }
    float sampleRate = 48000, value = 0, target = 0;
    int counter = 0;
    uint32_t state = 1;
};

// -----------------------------------------------------------------------------
//  Glos TD-PSOLA
// -----------------------------------------------------------------------------
class PsolaVoice
{
public:
    static constexpr int kAccSize = 1 << 14;
    static constexpr int kAccMask = kAccSize - 1;

    void prepare() { acc.assign (kAccSize, 0.0f); running = false; }

    void reset (int64_t now)
    {
        std::fill (acc.begin(), acc.end(), 0.0f);
        nextSynth = (double) now;
        lastAna   = (double) now;
        running   = true;
    }

    // ring: bufor wejscia (mono), now: czas bezwzgledny ostatniej probki,
    // period: okres w probkach, ratio: wspolczynnik zmiany wysokosci,
    // formant: wspolczynnik formantu, voiced: czy wykryto dzwiek, delay: latencja
    template <typename RingReader>
    float tick (const RingReader& ring, int64_t now, double period, double ratio, double formant, bool voiced, int delay)
    {
        while ((double) now >= nextSynth + 1.5 * period + 2.0)
            emitGrain (ring, now, period, ratio, formant, voiced);

        float& slot = acc[(size_t) ((now - delay) & kAccMask)];
        const float out = slot;
        slot = 0.0f;
        return out;
    }

    bool running = false;

private:
    template <typename RingReader>
    void emitGrain (const RingReader& ring, int64_t now, double P, double ratio, double f, bool voiced)
    {
        const double s = nextSynth;
        double a = s;
        if (voiced)
        {
            a = lastAna + P * std::round ((s - lastAna) / P);
            if (std::abs (a - s) > P) a = s;
        }
        a = std::min (a, (double) now - P - 1.0);
        lastAna = a;

        const double Lout = P / f;
        // normalizacja energii nakladajacych sie ziaren (nie amplitudy) - stala glosnosc
        const float  g    = (float) std::clamp (std::sqrt (f / ratio), 0.4, 1.6);
        const int64_t j0 = (int64_t) std::ceil (s - Lout);
        const int64_t j1 = (int64_t) std::floor (s + Lout);
        for (int64_t j = j0; j <= j1; ++j)
        {
            const double t = (double) j - s;
            const float  w = (float) (0.5 * (1.0 + std::cos (kPi * t / Lout)));
            acc[(size_t) (j & kAccMask)] += g * w * ring.read (a + t * f);
        }
        nextSynth += P / ratio;
    }

    std::vector<float> acc;
    double nextSynth = 0, lastAna = 0;
};

// -----------------------------------------------------------------------------
//  Parametry silnika
// -----------------------------------------------------------------------------
struct VoiceParams
{
    bool  on       = false;
    int   scale    = 0;
    bool  dual     = false;
    float random   = 0;    // 0..100
    float formant  = 0;    // -50 (M) .. +50 (F)
    float quantise = 0;    // 0..100
    float levelDb  = 0;    // -60..+6
    float pan      = 0;    // -50..+50
};

struct EngineParams
{
    float inputDb   = 0;
    bool  link      = false;  // KEY podaza za nutami MIDI
    int   key       = 0;      // 0 = C ... 11 = B
    int   midiCh    = 0;      // 0 = Omni, 1..16
    float tune      = 440;
    bool  directOn  = true;
    float directDb  = 0;
    float directPan = 0;
    VoiceParams unison;
    std::array<VoiceParams, 4> harm;
};

// -----------------------------------------------------------------------------
//  Silnik
// -----------------------------------------------------------------------------
class HarmonizerEngine
{
public:
    static constexpr int kRingSize = 1 << 17;
    static constexpr int kRingMask = kRingSize - 1;
    static constexpr int kNumVoices = 9; // unison + 4 x (harmonia + dual)

    struct Ring
    {
        std::vector<float> data;
        float read (double pos) const
        {
            const double fl = std::floor (pos);
            const int64_t i = (int64_t) fl;
            const float fr = (float) (pos - fl);
            const float a = data[(size_t) (i & kRingMask)];
            const float b = data[(size_t) ((i + 1) & kRingMask)];
            return a + (b - a) * fr;
        }
    };

    void prepare (double sr)
    {
        sampleRate = sr;
        detector.prepare (sr);
        ring.data.assign (kRingSize, 0.0f);
        dryL.assign (kRingSize, 0.0f);
        dryR.assign (kRingSize, 0.0f);
        const double maxP = sr / kMinHz;
        latency = (int) std::ceil (2.95 * maxP) + 8;
        for (int i = 0; i < kNumVoices; ++i)
        {
            voices[(size_t) i].prepare();
            drift[(size_t) i].prepare (sr, 0x9E3779B9u * (uint32_t) (i + 1));
            env[(size_t) i] = 0;
            shift[(size_t) i] = 0;
            active[(size_t) i] = false;
        }
        now = 0;
        f0 = 0;
        envCoef = (float) (1.0 - std::exp (-1.0 / (0.015 * sr)));
        heldCount = 0;
        effectiveKey = 0;
        inputPeak = 0;
    }

    int getLatency() const { return latency; }
    int getEffectiveKey() const { return effectiveKey; }
    float getDetectedHz() const { return f0; }
    float takeInputPeak() { const float p = inputPeak; inputPeak = 0; return p; }

    // ---- MIDI ----
    void noteOn (int note, int channel, const EngineParams& p)
    {
        if (p.midiCh != 0 && channel != p.midiCh) return;
        for (int i = 0; i < heldCount; ++i) if (held[(size_t) i] == note) return;
        if (heldCount < (int) held.size()) held[(size_t) heldCount++] = note;
        std::sort (held.begin(), held.begin() + heldCount);
        lastMidiNote = note;
    }
    void noteOff (int note, int channel, const EngineParams& p)
    {
        if (p.midiCh != 0 && channel != p.midiCh) return;
        for (int i = 0; i < heldCount; ++i)
            if (held[(size_t) i] == note)
            {
                for (int k = i; k < heldCount - 1; ++k) held[(size_t) k] = held[(size_t) k + 1];
                --heldCount;
                break;
            }
    }
    void allNotesOff() { heldCount = 0; }

    // ---- audio ----  (R moze byc nullptr dla mono)
    void process (float* L, float* R, int numSamples, const EngineParams& p)
    {
        effectiveKey = (p.link && lastMidiNote >= 0) ? (lastMidiNote % 12) : p.key;
        const float inGain = dbToGain (p.inputDb);
        const auto& scales = getScales();

        // stale dla mixu
        const float dirG = p.directOn ? dbToGain (p.directDb) : 0.0f;
        float dirPL, dirPR; panGains (p.directPan, dirPL, dirPR);

        for (int n = 0; n < numSamples; ++n)
        {
            const float inL = L[n] * inGain;
            const float inR = (R != nullptr ? R[n] : L[n]) * inGain;
            const float mono = 0.5f * (inL + inR);
            inputPeak = std::max (inputPeak, std::max (std::abs (inL), std::abs (inR)));

            ++now;
            ring.data[(size_t) (now & kRingMask)] = mono;
            dryL[(size_t) (now & kRingMask)] = inL;
            dryR[(size_t) (now & kRingMask)] = inR;

            if (detector.push (mono))
            {
                f0 = detector.getFrequency();
                updateTargets (p, scales);
            }

            const bool   voiced = f0 > 0;
            const double period = voiced ? sampleRate / f0 : sampleRate / 150.0;

            float outL = 0, outR = 0;
            for (int v = 0; v < kNumVoices; ++v)
            {
                const VoiceParams& vp = voiceParams (p, v);
                const bool isHarm = v > 0;
                const bool isDual = v >= 5;
                bool want = vp.on && active[(size_t) v] && (!isDual || vp.dual);
                if (isHarm && !voiced) want = false; // harmonie milkna na spolgloskach/ciszy

                float& e = env[(size_t) v];
                e += ((want ? 1.0f : 0.0f) - e) * envCoef;
                if (e < 1e-4f && !want)
                {
                    e = 0;
                    if (voices[(size_t) v].running) voices[(size_t) v].running = false;
                    continue;
                }
                if (!voices[(size_t) v].running) voices[(size_t) v].reset (now);

                double semis = voiced ? shift[(size_t) v] : 0.0;
                semis += drift[(size_t) v].next() * vp.random * 0.0035; // do +/-35 centow
                if (isDual) semis += 0.09;                               // +9 centow
                const double ratio   = std::pow (2.0, semis / 12.0);
                const double formant = std::pow (2.0, (vp.formant / 50.0) * 0.5);

                const float y = voices[(size_t) v].tick (ring, now, period, ratio, formant, voiced, latency);

                float pl, pr;
                float pan = vp.pan;
                if (vp.dual && isHarm) pan = std::clamp (pan + (isDual ? 25.0f : -25.0f), -50.0f, 50.0f);
                panGains (pan, pl, pr);
                const float g = y * e * dbToGain (vp.levelDb) * 1.41421356f; // kompensacja prawa panoramy
                outL += g * pl;
                outR += g * pr;
            }

            const int64_t d = (now - latency) & kRingMask;
            outL += dryL[(size_t) d] * dirG * dirPL * 1.41421356f;
            outR += dryR[(size_t) d] * dirG * dirPR * 1.41421356f;

            L[n] = outL;
            if (R != nullptr) R[n] = outR;
        }
    }

private:
    static const VoiceParams& voiceParams (const EngineParams& p, int v)
    {
        if (v == 0) return p.unison;
        return p.harm[(size_t) ((v - 1) % 4)];
    }

    static void panGains (float pan, float& l, float& r)
    {
        const float x = (std::clamp (pan, -50.0f, 50.0f) + 50.0f) / 100.0f; // 0..1
        l = std::cos (x * (float) kPi * 0.5f);
        r = std::sin (x * (float) kPi * 0.5f);
    }

    void updateTargets (const EngineParams& p, const std::vector<ScaleDef>& scales)
    {
        if (f0 <= 0) return;
        const float m = 69.0f + 12.0f * std::log2 (f0 / p.tune); // aktualna nuta (ulamkowa)
        const int   n = (int) std::lround (m);

        // unison: korekcja do najblizszego poltonu
        {
            const float q = p.unison.quantise / 100.0f;
            shift[0] = q * ((float) n - m);
            active[0] = true;
        }

        int midiIdx = 0;
        for (int h = 0; h < 4; ++h)
        {
            const VoiceParams& vp = p.harm[(size_t) h];
            const ScaleDef& sd = scales[(size_t) std::clamp (vp.scale, 0, (int) scales.size() - 1)];
            int target = n;
            bool ok = true;
            switch (sd.kind)
            {
                case ScaleKind::Midi:
                    if (midiIdx < heldCount) target = held[(size_t) midiIdx];
                    else ok = false;
                    ++midiIdx;
                    break;
                case ScaleKind::Fixed:    target = n + sd.semitones; break;
                case ScaleKind::Diatonic: target = diatonicTarget (n, effectiveKey, sd.degrees, sd.mode); break;
            }
            const float q = vp.quantise / 100.0f;
            const float interval = (float) (target - n);              // zachowuje vibrato wejscia
            const float exact    = (float) target - m;                // dokladnie w nute
            const float s = (1.0f - q) * interval + q * exact;
            shift[(size_t) h + 1] = s;
            shift[(size_t) h + 5] = s;
            active[(size_t) h + 1] = ok;
            active[(size_t) h + 5] = ok;
        }
    }

    double sampleRate = 48000;
    int latency = 0;
    int64_t now = 0;
    float f0 = 0, envCoef = 0.01f, inputPeak = 0;
    int effectiveKey = 0, lastMidiNote = -1;

    PitchDetector detector;
    Ring ring;
    std::vector<float> dryL, dryR;
    std::array<PsolaVoice, kNumVoices> voices;
    std::array<RandomDrift, kNumVoices> drift;
    std::array<float, kNumVoices> env {}, shift {};
    std::array<bool, kNumVoices> active {};
    std::array<int, 16> held {};
    int heldCount = 0;
};

} // namespace mateo
