#pragma once
// Finds the note you're playing (YIN pitch detection on a 12 kHz copy of the
// input), so "random notes" can choose intervals that land inside the key.

#include <cmath>
#include <vector>
#include <algorithm>

namespace yoko
{
class PitchTracker
{
public:
    void prepare (double sampleRate)
    {
        decim = std::max (1, (int) std::round (sampleRate / 12000.0));
        rate = sampleRate / decim;
        lpA = 1.0 - std::exp (-2.0 * 3.14159265358979 * 2500.0 / sampleRate);
        tauMin = (int) (rate / 1100.0);
        tauMax = (int) (rate / 65.0);
        window = tauMax * 2 + 32;
        ring.assign ((size_t) (window + tauMax + 8), 0.0f);
        diff.assign ((size_t) tauMax + 2, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        writePos = 0; count = 0; hop = 0; lp1 = lp2 = 0; acc = 0;
        note = -1;
    }

    // Feed one input sample. Returns true when a new note was detected.
    inline bool push (double x)
    {
        lp1 += (x - lp1) * lpA;
        lp2 += (lp1 - lp2) * lpA;
        acc += lp2;
        if (++count < decim) return false;
        ring[(size_t) writePos] = (float) (acc / decim);
        writePos = (writePos + 1) % (int) ring.size();
        count = 0; acc = 0;
        if (++hop < 256) return false;
        hop = 0;
        return analyse();
    }

    int note = -1;   // MIDI note number of the last confident detection, -1 if none yet

private:
    bool analyse()
    {
        const int n = (int) ring.size();
        const int len = window + tauMax;
        std::vector<float>& f = frame;
        f.resize ((size_t) len);
        for (int i = 0; i < len; ++i)
            f[(size_t) i] = ring[(size_t) ((writePos - len + i + n * 2) % n)];

        double energy = 0;
        for (int i = 0; i < window; ++i) energy += (double) f[(size_t) i] * f[(size_t) i];
        if (energy / window < 2.0e-5)          // too quiet to judge
            return false;

        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double d = 0;
            for (int i = 0; i < window; ++i)
            {
                const double v = (double) f[(size_t) i] - f[(size_t) (i + tau)];
                d += v * v;
            }
            diff[(size_t) tau] = (float) d;
        }

        // cumulative mean normalised difference, first dip below the threshold
        double running = 0;
        int best = -1;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            running += diff[(size_t) tau];
            diff[(size_t) tau] = running > 0 ? (float) (diff[(size_t) tau] * tau / running) : 1.0f;
        }
        for (int tau = std::max (2, tauMin); tau < tauMax; ++tau)
            if (diff[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < tauMax && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        if (best < 0) return false;

        // parabolic refinement
        double t = best;
        const double a = diff[(size_t) best - 1], b = diff[(size_t) best], c = diff[(size_t) best + 1];
        const double den = a - 2 * b + c;
        if (std::abs (den) > 1e-12) t += 0.5 * (a - c) / den;

        const double hz = rate / t;
        note = (int) std::lround (69.0 + 12.0 * std::log2 (hz / 440.0));
        return true;
    }

    std::vector<float> ring, diff, frame;
    int decim = 4, writePos = 0, count = 0, hop = 0, tauMin = 10, tauMax = 180, window = 400;
    double rate = 12000, lpA = 0.3, lp1 = 0, lp2 = 0, acc = 0;
};
} // namespace yoko
