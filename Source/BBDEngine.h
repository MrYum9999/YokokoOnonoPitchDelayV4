#pragma once
// Yokoko Onono Pitch Delay: analogue signal path model.
//
// input -> + regen -> compressor -> anti-alias LPF -> 4x MN3005 (8192 buckets)
//       -> reconstruction LPF -> expander -> 2-pole resonant LPF -> wet out
//                                                  |__________ regen _________|
//
// The BBD is modelled as a real clocked bucket line: one clock drives both
// writing and reading, so pitch only shifts when the clock changes while audio
// is already inside the chips (the Doppler effect the hardware relies on).

#include <cmath>
#include <vector>
#include <cstdint>
#include <algorithm>

namespace yoko
{
static constexpr double kPi = 3.14159265358979323846;

//==============================================================================
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void setLowpass (double fs, double fc, double q)
    {
        fc = std::clamp (fc, 10.0, fs * 0.49);
        const double w = 2.0 * kPi * fc / fs;
        const double c = std::cos (w), s = std::sin (w), alpha = s / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (1.0 - c) * 0.5 / a0;
        b1 = (1.0 - c) / a0;
        b2 = b0;
        a1 = -2.0 * c / a0;
        a2 = (1.0 - alpha) / a0;
    }
    void reset() { z1 = z2 = 0; }
    inline double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// 4th-order Butterworth, the fixed op-amp filters around the BBD chips.
struct Butter4
{
    Biquad s1, s2;
    void set (double fs, double fc) { s1.setLowpass (fs, fc, 0.5412); s2.setLowpass (fs, fc, 1.3066); }
    void reset() { s1.reset(); s2.reset(); }
    inline double process (double x) { return s2.process (s1.process (x)); }
};

//==============================================================================
// 2-pole resonant low-pass (TPT state variable filter) with a little analogue
// soft saturation on the band-pass state, so self-resonance stays musical.
struct ResonantLPF
{
    double ic1 = 0, ic2 = 0, g = 0.1, k = 1.0, fs = 48000;

    void prepare (double sampleRate) { fs = sampleRate; reset(); }
    void reset() { ic1 = ic2 = 0; }
    void set (double fc, double q)
    {
        fc = std::clamp (fc, 20.0, fs * 0.45);
        g = std::tan (kPi * fc / fs);
        k = 1.0 / q;
    }
    inline double process (double x)
    {
        const double a1 = 1.0 / (1.0 + g * (g + k));
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + g * a1 * v3;
        const double v2 = ic2 + g * v1;
        ic1 = 2.0 * v1 - ic1;
        ic1 = 1.6 * std::tanh (ic1 / 1.6);
        ic2 = 2.0 * v2 - ic2;
        return v2;
    }
};

//==============================================================================
// NE570-style compander: 2:1 compression before the BBD, 1:2 expansion after.
struct EnvelopeFollower
{
    double env = 0, att = 0, rel = 0;
    void prepare (double fs, double attackMs, double releaseMs)
    {
        att = 1.0 - std::exp (-1.0 / (fs * attackMs * 0.001));
        rel = 1.0 - std::exp (-1.0 / (fs * releaseMs * 0.001));
        env = 0;
    }
    inline double process (double x)
    {
        const double a = std::abs (x);
        env += (a - env) * (a > env ? att : rel);
        return env;
    }
};

struct Compander
{
    static constexpr double ref = 0.25;
    EnvelopeFollower cEnv, eEnv;

    void prepare (double fs)
    {
        cEnv.prepare (fs, 4.0, 40.0);
        eEnv.prepare (fs, 4.0, 40.0);
    }
    inline double compress (double x)
    {
        const double e = std::max (cEnv.process (x), 0.0008);
        return x * std::sqrt (ref / e);
    }
    inline double expand (double x)
    {
        const double e = eEnv.process (x);
        return x * std::min (e / ref, 6.0);
    }
};

//==============================================================================
// Four MN3005s in series: 16,384 stages, 8,192 charge packets.
// Td = 8192 / fclk. Input is sampled once per clock cycle and held at the output.
class BBDLine
{
public:
    static constexpr int kBuckets = 8192;

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        idx = 0; phase = 0; held = 0; prevIn = 0; parity = 1.0; noiseState = 22222u;
    }

    // Processes one host sample. Returns the BBD output integrated over the
    // host sample period (exact zero-order-hold averaging), plus clock bleed.
    inline double process (double in, double fclk, double fs, double bleedAmount)
    {
        const double inc = fclk / fs;
        double acc = 0.0, bleedAcc = 0.0, tPrev = 0.0;
        double t = (1.0 - phase) / inc;       // fractional time of next tick in this sample

        while (t <= 1.0)
        {
            acc      += held * (t - tPrev);
            bleedAcc += parity * (t - tPrev);
            tPrev = t;

            const double sample = prevIn + (in - prevIn) * t;
            held = buf[(size_t) idx];
            buf[(size_t) idx] = (float) (saturate (sample) + noise() * 6.0e-5);
            idx = (idx + 1) & (kBuckets - 1);
            parity = -parity;
            t += 1.0 / inc;
        }

        acc      += held * (1.0 - tPrev);
        bleedAcc += parity * (1.0 - tPrev);

        phase += inc;
        phase -= std::floor (phase);
        prevIn = in;
        return acc + bleedAcc * bleedAmount;
    }

    // Charge-transfer saturation: soft and slightly asymmetric (even + odd harmonics).
    static inline double saturate (double x)
    {
        constexpr double bias = 0.18;
        return std::tanh (x * 0.9 + bias) - std::tanh (bias);
    }

private:
    inline double noise()
    {
        noiseState = noiseState * 1664525u + 1013904223u;
        return ((double) (noiseState >> 8) / 8388608.0) - 1.0;
    }

    std::vector<float> buf = std::vector<float> ((size_t) kBuckets, 0.0f);
    int idx = 0;
    double phase = 0, held = 0, prevIn = 0, parity = 1.0;
    uint32_t noiseState = 22222u;
};

//==============================================================================
struct WetPath
{
    BBDLine bbd;
    Butter4 antiAlias, reconstruct;
    Compander compander;
    ResonantLPF lpf;
    EnvelopeFollower bleedEnv;   // clock bleed only sounds while there is signal in the chips
    double fs = 48000, fbState = 0, dcX = 0, dcY = 0, dcR = 0.999;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        antiAlias.set (fs, 7000.0);
        reconstruct.set (fs, 7000.0);
        compander.prepare (fs);
        lpf.prepare (fs);
        bleedEnv.prepare (fs, 5.0, 400.0);
        dcR = std::exp (-2.0 * kPi * 12.0 / fs);
        reset();
    }

    void reset()
    {
        bbd.reset(); antiAlias.reset(); reconstruct.reset(); lpf.reset();
        compander.cEnv.env = compander.eEnv.env = bleedEnv.env = 0;
        fbState = dcX = dcY = 0;
    }

    // in: dry mono input (already gated for trails). Returns the wet signal.
    inline double process (double in, double fclk, double lpfHz, double lpfQ, double regen, double bleed)
    {
        // regen path with DC blocking and a soft limit, so runaway oscillation stays bounded
        const double fb = fbState * regen;
        const double dc = fb - dcX + dcR * dcY;
        dcX = fb; dcY = dc;
        const double fbLimited = 1.4 * std::tanh (dc / 1.4);

        double x = in + fbLimited;
        const double sig = bleedEnv.process (x);
        x = compander.compress (x);
        x = antiAlias.process (x);
        x = bbd.process (x, fclk, fs, bleed * std::min (1.0, sig * 20.0));
        x = reconstruct.process (x);
        x = compander.expand (x);

        lpf.set (lpfHz, lpfQ);
        x = lpf.process (x);

        fbState = x;
        return x;
    }
};
} // namespace yoko
