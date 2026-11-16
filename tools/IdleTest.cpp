// Idle noise test: plays a short note, then silence, with the host transport
// running at 120 BPM, and reports what comes out once the repeats have died away.
#include "PluginProcessor.h"
#include <juce_dsp/juce_dsp.h>

static constexpr double SR = 48000.0;

struct FakePlayHead : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 120.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        p.setIsPlaying (true);
        return p;
    }
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const bool silentOnly = argc > 1 && juce::String (argv[1]) == "silent";

    YokokokoAudioProcessor p;
    FakePlayHead ph;
    p.setPlayHead (&ph);
    p.setPlayConfigDetails (2, 2, SR, 512);
    p.prepareToPlay (SR, 512);
    if (argc > 2 && juce::String (argv[2]) == "slowdown")
        p.apvts.getParameter ("slowdown")->setValueNotifyingHost (1.0f);

    const int total = (int) (SR * 40);
    juce::AudioBuffer<float> all (2, total);
    all.clear();
    if (! silentOnly)
        for (int i = 0; i < (int) (SR * 0.3); ++i)
        {
            const float v = 0.4f * (float) std::sin (2 * yoko::kPi * 220.0 * i / SR) * (float) std::exp (-i / (SR * 0.1));
            all.setSample (0, i, v); all.setSample (1, i, v);
        }

    juce::MidiBuffer midi;
    for (int pos = 0; pos < total; pos += 512)
    {
        juce::AudioBuffer<float> blk (all.getArrayOfWritePointers(), 2, pos, 512);
        p.processBlock (blk, midi);
        ph.ppq += 512.0 / SR * ph.bpm / 60.0;
    }

    // report each 5 s window: RMS in dBFS and the strongest frequency above 1 kHz
    for (int w = 0; w < 8; ++w)
    {
        const int start = w * (int) (SR * 5);
        double s = 0; float pk = 0;
        for (int i = start; i < start + (int) (SR * 5); ++i)
        {
            const float v = all.getSample (0, i);
            s += (double) v * v; pk = std::max (pk, std::abs (v));
        }
        const double rmsDb = 10.0 * std::log10 (s / (SR * 5) + 1e-30);

        constexpr int order = 15, N = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> buf (2 * N, 0.0f);
        for (int i = 0; i < N; ++i)
            buf[(size_t) i] = all.getSample (0, start + i) * (0.5f - 0.5f * (float) std::cos (2 * yoko::kPi * i / N));
        fft.performFrequencyOnlyForwardTransform (buf.data());
        int best = 0; float bestV = 0;
        for (int k = (int) (1000.0 * N / SR); k < N / 2; ++k)
            if (buf[(size_t) k] > bestV) { bestV = buf[(size_t) k]; best = k; }
        std::printf ("%2d-%2d s  rms %7.1f dBFS  peak %7.1f dBFS  loudest high tone %6.0f Hz\n",
                     w * 5, w * 5 + 5, rmsDb, 20.0 * std::log10 (pk + 1e-30), best * SR / N);
    }
    return 0;
}
