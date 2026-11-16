#pragma once
// v4 reverse delay.
//
// Audio is cut into pieces ("grains") one delay time long. While one piece is
// being recorded, the piece before it is played backwards. Each channel also
// keeps a forward copy delayed by the same amount, so a reversed piece and its
// forward twin line up exactly (used by "every other echo").
//
// Every piece fades in and out over a few milliseconds so the cuts don't click.

#include <vector>
#include <cmath>
#include <algorithm>

namespace yoko
{
class Reverser
{
public:
    static constexpr int kChannels = 3;   // dry left, dry right, wet
    static constexpr double kMaxGrainSeconds = 4.5;

    void prepare (double fs)
    {
        maxGrain = (int) (fs * kMaxGrainSeconds);
        int need = 2 * maxGrain + 1024, sz = 1;
        while (sz < need) sz <<= 1;
        mask = sz - 1;
        for (auto& b : buf) b.assign ((size_t) sz, 0.0f);
        fade = std::max (8, (int) (fs * 0.006));
        reset();
    }

    void reset()
    {
        for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
        w = 0; pos = 0; len = 0; start = 0; reversedPiece = false;
    }

    bool needsNewGrain() const { return pos >= len; }

    // Start a new piece of this many samples. Every other piece is marked
    // "reversed" for the alternating mode.
    void startGrain (int samples)
    {
        len = std::clamp (samples, 4 * fade, maxGrain);
        pos = 0;
        start = w;
        reversedPiece = ! reversedPiece;
    }

    // Call once per sample: store the inputs, then read with reversed() / forward().
    void write (double dryL, double dryR, double wet)
    {
        buf[0][(size_t) w] = (float) dryL;
        buf[1][(size_t) w] = (float) dryR;
        buf[2][(size_t) w] = (float) wet;
    }

    double window() const
    {
        const int edge = std::min (pos, len - 1 - pos);
        if (edge >= fade) return 1.0;
        return 0.5 - 0.5 * std::cos (3.14159265358979323846 * (edge + 0.5) / fade);
    }

    // the previous piece, backwards
    double reversed (int ch) const { return buf[(size_t) ch][(size_t) ((start - 1 - pos) & mask)] * window(); }
    // the previous piece, forwards (same delay as the reversed copy)
    double forward (int ch) const  { return buf[(size_t) ch][(size_t) ((start - len + pos) & mask)] * window(); }

    void advance()
    {
        w = (w + 1) & mask;
        ++pos;
    }

    bool isReversedPiece() const { return reversedPiece; }
    int  grainLength() const { return len; }

private:
    std::vector<float> buf[kChannels];
    int mask = 0, w = 0, pos = 0, len = 0, start = 0, fade = 288, maxGrain = 0;
    bool reversedPiece = false;
};
} // namespace yoko
