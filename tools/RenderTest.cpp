// Offline test: renders audio through the processor, checks the numbers,
// and saves pictures of the front and back of the interface.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_dsp/juce_dsp.h>
#include <chrono>
#include <set>
#include <tuple>

using namespace yoko;
static constexpr double SR = 48000.0;

static void set (YokokokoAudioProcessor& p, const juce::String& id, float realValue)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (realValue));
}

static juce::AudioBuffer<float> run (YokokokoAudioProcessor& p, const juce::AudioBuffer<float>& in)
{
    juce::AudioBuffer<float> out (in);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < out.getNumSamples(); pos += 512)
    {
        const int n = juce::jmin (512, out.getNumSamples() - pos);
        juce::AudioBuffer<float> blk (out.getArrayOfWritePointers(), 2, pos, n);
        p.processBlock (blk, midi);
    }
    return out;
}

static std::unique_ptr<YokokokoAudioProcessor> fresh()
{
    auto p = std::make_unique<YokokokoAudioProcessor>();
    p->setPlayConfigDetails (2, 2, SR, 512);
    p->prepareToPlay (SR, 512);
    return p;
}

// zero-crossing based frequency estimate over a region
static double freqOf (const float* xin, int start, int len)
{
    // smooth first, so the BBD's clock-rate grain can't add false crossings
    std::vector<float> y ((size_t) (start + len), 0.0f);
    double l1 = 0, l2 = 0; const double a = 1.0 - std::exp (-2.0 * kPi * 1500.0 / SR);
    for (int i = 0; i < start + len; ++i) { l1 += (xin[i] - l1) * a; l2 += (l1 - l2) * a; y[(size_t) i] = (float) l2; }
    const float* x = y.data();
    int crossings = 0; int first = -1, last = -1;
    for (int i = start + 1; i < start + len; ++i)
        if (x[i - 1] <= 0 && x[i] > 0) { if (first < 0) first = i; last = i; ++crossings; }
    return crossings > 1 ? (crossings - 1) * SR / (last - first) : 0.0;
}

static double rms (const float* x, int start, int len)
{
    double s = 0; for (int i = start; i < start + len; ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / len);
}

static void writeWav (const juce::AudioBuffer<float>& b, const juce::File& f)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (SR).withNumChannels (2).withBitsPerSample (24));
    if (w) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? argv[1] : "/tmp");
    int failures = 0;
    auto check = [&] (bool ok, const juce::String& what) { std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); if (! ok) ++failures; };

    // 1. unity delay time: a click comes back 500 ms later -------------------------------
    {
        auto p = fresh();
        set (*p, ID::int1, 0.5f); set (*p, ID::int2, 0.5f); set (*p, ID::regen, 0.0f); set (*p, ID::mix, 1.0f);
        juce::AudioBuffer<float> in (2, (int) SR * 2); in.clear();
        for (int i = 0; i < 48; ++i) in.setSample (0, 4800 + i, 0.8f), in.setSample (1, 4800 + i, 0.8f);
        auto out = run (*p, in);
        int peak = 0; float pv = 0;
        for (int i = 6000; i < out.getNumSamples(); ++i) if (std::abs (out.getSample (0, i)) > pv) { pv = std::abs (out.getSample (0, i)); peak = i; }
        const double ms = (peak - 4800) / SR * 1000.0;
        check (std::abs (ms - 500.0) < 3.0, "echo arrives after " + juce::String (ms, 1) + " ms (expect 500)");
    }

    // 2. octave step: what's in the chips comes out an octave up ------------------------------
    {
        auto p = fresh();
        set (*p, ID::int1, 12.0f / 14.0f); set (*p, ID::int2, 0.5f); set (*p, ID::regen, 0.0f); set (*p, ID::mix, 1.0f); set (*p, ID::lpf, 1.0f);
        juce::AudioBuffer<float> in (2, (int) SR * 2); in.clear();
        for (int i = 0; i < (int) (SR * 0.5); ++i) { const float v = 0.4f * (float) std::sin (2 * kPi * 220.0 * i / SR); in.setSample (0, i, v); in.setSample (1, i, v); }
        auto out = run (*p, in);
        const double f = freqOf (out.getReadPointer (0), (int) (SR * 0.52), (int) (SR * 0.2));
        check (std::abs (f - 440.0) < 6.0, "step 2 plays " + juce::String (f, 1) + " Hz from a 220 Hz input (expect 440)");
    }

    // 3. fifth up -------------------------------------------------------------------------------
    {
        auto p = fresh();
        set (*p, ID::int1, 10.0f / 14.0f); set (*p, ID::regen, 0.0f); set (*p, ID::mix, 1.0f); set (*p, ID::lpf, 1.0f);
        juce::AudioBuffer<float> in (2, (int) SR * 2); in.clear();
        for (int i = 0; i < (int) (SR * 0.5); ++i) { const float v = 0.4f * (float) std::sin (2 * kPi * 220.0 * i / SR); in.setSample (0, i, v); in.setSample (1, i, v); }
        auto out = run (*p, in);
        const double f = freqOf (out.getReadPointer (0), (int) (SR * 0.52), (int) (SR * 0.25));
        check (std::abs (f - 220.0 * 1.4983) < 6.0, "fifth up plays " + juce::String (f, 1) + " Hz (expect 329.6)");
    }

    // 3b. the new intervals: 6th up and 9th up ------------------------------------------------
    for (auto [idx, semis, label] : { std::tuple<int, int, const char*> { 11, 9, "6th up" }, { 13, 14, "9th up" }, { 3, -9, "6th down" }, { 1, -14, "9th down" } })
    {
        auto p = fresh();
        set (*p, ID::int1, (float) idx / 14.0f); set (*p, ID::regen, 0.0f); set (*p, ID::mix, 1.0f); set (*p, ID::lpf, 1.0f);
        juce::AudioBuffer<float> in (2, (int) SR * 2); in.clear();
        for (int i = 0; i < (int) (SR * 0.5); ++i) { const float v = 0.4f * (float) std::sin (2 * kPi * 220.0 * i / SR); in.setSample (0, i, v); in.setSample (1, i, v); }
        auto out = run (*p, in);
        const double expect = 220.0 * std::pow (2.0, semis / 12.0);
        const int start = (int) (SR * 0.52);
        const int len = semis > 0 ? (int) (SR * 0.5 / std::pow (2.0, semis / 12.0) * 0.8) : (int) (SR * 0.4);
        const double f = freqOf (out.getReadPointer (0), start, len);
        check (std::abs (f - expect) < expect * 0.015, juce::String (label) + " plays " + juce::String (f, 1) + " Hz (expect " + juce::String (expect, 1) + ")");
    }

    // 3c. random notes stay in the key ----------------------------------------------------------------
    for (int weird = 0; weird < 2; ++weird)
    {
        auto p = fresh();
        set (*p, ID::random, 1.0f); set (*p, ID::weird, (float) weird); set (*p, ID::keyRoot, 0.0f); set (*p, ID::keyScale, 0.0f);
        set (*p, ID::time, 200.0f);
        juce::AudioBuffer<float> in (2, (int) SR * 30);
        for (int i = 0; i < in.getNumSamples(); ++i) { const float v = 0.3f * (float) std::sin (2 * kPi * 220.0 * i / SR); in.setSample (0, i, v); in.setSample (1, i, v); }
        juce::MidiBuffer midi;
        std::set<int> seen;
        for (int pos = 0; pos < in.getNumSamples(); pos += 512)
        {
            juce::AudioBuffer<float> blk (in.getArrayOfWritePointers(), 2, pos, juce::jmin (512, in.getNumSamples() - pos));
            p->processBlock (blk, midi);
            if (pos > SR * 2) { seen.insert (p->randomSemis1.load()); seen.insert (p->randomSemis2.load()); }
        }
        const int heard = p->heardNote.load();
        static const std::set<int> cMajor { 0, 2, 4, 5, 7, 9, 11 }, cExtra { 3, 6, 10 };
        bool ok = heard == 57;
        juce::String list;
        int outside = 0;
        for (int iv : seen)
        {
            const int d = ((57 + iv) % 12 + 12) % 12;
            if (! cMajor.count (d)) { if (weird && cExtra.count (d)) ++outside; else ok = false; }
            list << iv << " ";
        }
        check (ok && seen.size() > 4, juce::String (weird ? "make it weird" : "random notes") + " heard A (" + juce::String (heard)
               + "), C major, picked: " + list + (weird ? "(" + juce::String (outside) + " stretched)" : juce::String()));
    }

    // 3d. preset files save and load ----------------------------------------------------------------------
    {
        auto p = fresh();
        set (*p, ID::regen, 0.77f); set (*p, ID::keyRoot, 9.0f); set (*p, ID::weird, 1.0f);
        const auto f = outDir.getChildFile ("my song sound.yokoko");
        const bool saved = p->savePresetFile (f);
        auto q = fresh();
        const bool loaded = q->loadPresetFile (f);
        check (saved && loaded && std::abs (q->apvts.getRawParameterValue (ID::regen)->load() - 0.77f) < 0.001f
               && (int) q->apvts.getRawParameterValue (ID::keyRoot)->load() == 9 && q->getPresetName() == "my song sound",
               "preset file round trip (name: " + q->getPresetName() + ")");
        juce::MemoryBlock mb; q->getStateInformation (mb);
        auto r = fresh(); r->setStateInformation (mb.getData(), (int) mb.getSize());
        check (r->getPresetName() == "my song sound", "preset name is kept in the DAW project");
        f.deleteFile();
    }

    // 3e. count bars: steps follow the beat groups of odd time signatures --------------------------------
    {
        struct Head : juce::AudioPlayHead
        {
            double ppq = 0, bpm = 120; int num = 7, den = 8; bool giveBarStart = true;
            juce::Optional<PositionInfo> getPosition() const override
            {
                PositionInfo p; p.setBpm (bpm); p.setPpqPosition (ppq); p.setIsPlaying (true);
                p.setTimeSignature (TimeSignature { num, den });
                const double barQ = num * 4.0 / den;
                if (giveBarStart) p.setPpqPositionOfLastBarStart (std::floor (ppq / barQ + 1e-9) * barQ);
                return p;
            }
        };
        struct Case { int num, den, grouping; bool barStart; double startPpq; const char* expect; };
        // expect: the step (1, 2 or 3) heard on each beat of two bars
        const Case cases[] = {
            { 7, 8, 0, true,  0.0, "1122333 1122333" },
            { 7, 8, 2, true,  0.0, "1112233 1112233" },
            { 5, 4, 1, true,  0.0, "11122 11122" },
            { 11, 8, 3, true, 0.0, "11122233311 11122233311" },
            { 11, 8, 3, false, 0.0, "11122233311 11122233311" },
            { 7, 8, 0, true,  1.5, "2333 1122333" },   // starting playback partway through a bar
        };
        for (auto& c : cases)
        {
            auto p = fresh();
            Head h; h.num = c.num; h.den = c.den; h.giveBarStart = c.barStart; h.ppq = c.startPpq;
            p->setPlayHead (&h);
            set (*p, ID::bars, 1.0f); set (*p, ID::grouping, (float) c.grouping);
            const double beatSec = 60.0 / h.bpm * 4.0 / c.den;
            const int blk = 48;   // 1 ms blocks
            juce::AudioBuffer<float> b (2, blk); juce::MidiBuffer m;
            juce::String heard;
            const int beatsToCheck = juce::String (c.expect).removeCharacters (" ").length();
            double t = 0;
            int nextBeat = 0;
            const int bar = c.num;
            while (nextBeat < beatsToCheck)
            {
                b.clear(); p->processBlock (b, m);
                t += blk / SR; h.ppq += blk / SR * h.bpm / 60.0;
                // sample each beat a quarter of the way in
                if (t >= (nextBeat + 0.25) * beatSec)
                {
                    heard << juce::String (p->currentStep.load() + 1);
                    ++nextBeat;
                    const double absBeat = c.startPpq * c.den / 4.0 + nextBeat;
                    if (nextBeat < beatsToCheck && std::fmod (absBeat, (double) bar) < 1e-6) heard << " ";
                }
            }
            const auto& gr = groupingFor (c.num, c.grouping);
            const bool timeOk = std::abs (p->baseDelayMs.load() - beatSec * 1000.0) < 0.5;
            check (heard == c.expect && timeOk,
                   juce::String (c.num) + "/" + juce::String (c.den) + " grouped " + groupingText (gr) + (c.barStart ? "" : " (no bar info)")
                   + ": " + heard + "  repeat " + juce::String (p->baseDelayMs.load(), 0) + " ms");
            p->setPlayHead (nullptr);
        }
    }

    // 3f. v4 reverse: a rising sweep should come back falling --------------------------------------------
    {
        // finds stretches of sound and says whether each one rises or falls in pitch
        auto pieces = [] (const juce::AudioBuffer<float>& b, int from)
        {
            juce::StringArray found;
            const float* x = b.getReadPointer (0);
            const int win = (int) (SR * 0.01);
            int startW = -1;
            for (int i = from; i + win < b.getNumSamples(); i += win)
            {
                const bool loud = rms (x, i, win) > 0.02;
                if (loud && startW < 0) startW = i;
                if ((! loud || i + 2 * win >= b.getNumSamples()) && startW >= 0)
                {
                    const int len = i - startW;
                    if (len > SR * 0.12)
                    {
                        const int third = len / 3;
                        const double a = freqOf (x, startW + win, third - win), c = freqOf (x, startW + 2 * third, third - win);
                        found.add ((c > a * 1.15 ? "up" : c < a / 1.15 ? "down" : "flat") + juce::String ("@") + juce::String ((double) startW / SR, 2));
                    }
                    startW = -1;
                }
            }
            return found;
        };
        auto sweep = [] (int seconds)
        {
            juce::AudioBuffer<float> in (2, (int) SR * seconds); in.clear();
            double ph = 0;
            const int a = (int) (SR * 0.1), len = (int) (SR * 0.2);
            for (int i = 0; i < len; ++i)
            {
                const double f = 300.0 * std::pow (3.0, (double) i / len);   // 300 Hz up to 900 Hz
                ph += 2 * kPi * f / SR;
                const double env = std::min (1.0, std::min (i, len - i) / (SR * 0.01));
                const float v = (float) (0.35 * env * std::sin (ph));
                in.setSample (0, a + i, v); in.setSample (1, a + i, v);
            }
            return in;
        };
        auto setup = [] (YokokokoAudioProcessor& p, int mode, float regen, float mix)
        {
            set (p, ID::int1, 0.5f); set (p, ID::int2, 0.5f); set (p, ID::regen, regen); set (p, ID::mix, mix);
            set (p, ID::lpf, 1.0f); set (p, ID::sync, 0.0f); set (p, ID::time, 500.0f); set (p, ID::reverse, (float) mode);
        };

        {   // echoes: the dry sweep rises, the repeat falls
            auto p = fresh(); setup (*p, 2, 0.0f, 0.5f);
            auto out = run (*p, sweep (3));
            auto f = pieces (out, 0);
            check (f.size() >= 2 && f[0].startsWith ("up@0.1") && f[1].startsWith ("down"),
                   "reverse echoes: " + f.joinIntoString (" "));
        }
        {   // everything: the dry sweep is gone, only backwards sweeps come out
            auto p = fresh(); setup (*p, 1, 0.0f, 0.5f);
            auto out = run (*p, sweep (3));
            auto f = pieces (out, 0);
            bool allDown = f.size() >= 2;
            for (auto& s : f) allDown = allDown && s.startsWith ("down");
            const double early = rms (out.getReadPointer (0), (int) (SR * 0.1), (int) (SR * 0.2));
            check (allDown && early < 0.005, "reverse everything: " + f.joinIntoString (" ") + "  (sound while playing: " + juce::String (early, 4) + ")");
        }
        {   // every other echo: repeats take turns
            auto p = fresh(); setup (*p, 3, 0.75f, 1.0f);
            auto out = run (*p, sweep (5));
            auto f = pieces (out, 0);
            bool turns = f.size() >= 4;
            for (int i = 1; i < f.size(); ++i)
                turns = turns && f[i].upToFirstOccurrenceOf ("@", false, false) != f[i - 1].upToFirstOccurrenceOf ("@", false, false);
            check (turns, "reverse every other echo: " + f.joinIntoString (" "));
        }
        {   // with the song playing, pieces start on the beat
            struct Head : juce::AudioPlayHead
            {
                double ppq = 0.37;
                juce::Optional<PositionInfo> getPosition() const override
                {
                    PositionInfo p; p.setBpm (120.0); p.setPpqPosition (ppq); p.setIsPlaying (true); return p;
                }
            } h;
            auto p = fresh(); setup (*p, 2, 0.4f, 0.5f); set (*p, ID::sync, 1.0f);
            p->setPlayHead (&h);
            juce::AudioBuffer<float> b (2, 480); juce::MidiBuffer m;
            juce::Random r (5);
            bool finite = true;
            for (int k = 0; k < 400; ++k)
            {
                for (int i = 0; i < 480; ++i) { const float v = 0.2f * (r.nextFloat() * 2 - 1); b.setSample (0, i, v); b.setSample (1, i, v); }
                p->processBlock (b, m);
                h.ppq += 480 / SR * 2.0;
                for (int i = 0; i < 480; ++i) finite &= std::isfinite (b.getSample (0, i)) && std::abs (b.getSample (0, i)) < 3.0f;
            }
            p->setPlayHead (nullptr);
            check (finite && std::abs (p->reversePieceMs.load() - 500.0f) < 0.5f, "reverse follows the song: pieces of "
                   + juce::String (p->reversePieceMs.load(), 0) + " ms, clean output");
        }
        {   // every mode with the other switches and long times stays clean
            auto p = fresh();
            juce::AudioBuffer<float> in (2, (int) SR * 6);
            juce::Random r (9);
            for (int i = 0; i < in.getNumSamples(); ++i) { const float v = 0.3f * (r.nextFloat() * 2 - 1); in.setSample (0, i, v); in.setSample (1, i, v); }
            bool finite = true;
            for (int mode = 1; mode <= 3; ++mode)
            {
                set (*p, ID::reverse, (float) mode); set (*p, ID::time, mode == 2 ? 4000.0f : 120.0f); set (*p, ID::slowdown, mode == 2 ? 1.0f : 0.0f);
                set (*p, ID::modulation, mode == 3 ? 1.0f : 0.0f); set (*p, ID::regen, 0.9f);
                auto out = run (*p, in);
                for (int i = 0; i < out.getNumSamples(); ++i) finite &= std::isfinite (out.getSample (0, i)) && std::abs (out.getSample (0, i)) < 3.0f;
            }
            check (finite, "reverse modes with slowdown, modulation and high feedback run clean");
        }
    }

    // 4. runaway feedback stays bounded ----------------------------------------------------------------
    {
        auto p = fresh();
        set (*p, ID::regen, 1.0f); set (*p, ID::mix, 1.0f); set (*p, ID::time, 300.0f);
        juce::AudioBuffer<float> in (2, (int) SR * 20); in.clear();
        for (int i = 0; i < 4800; ++i) { const float v = 0.5f * (float) std::sin (2 * kPi * 330.0 * i / SR); in.setSample (0, i, v); in.setSample (1, i, v); }
        auto out = run (*p, in);
        bool finite = true; float mx = 0;
        for (int i = 0; i < out.getNumSamples(); ++i) { const float v = out.getSample (0, i); finite &= std::isfinite (v); mx = std::max (mx, std::abs (v)); }
        const double late = rms (out.getReadPointer (0), (int) (SR * 18), (int) SR);
        check (finite && mx < 2.0f, "full feedback peaks at " + juce::String (mx, 2) + " with no NaNs");
        check (late > 0.02, "full feedback self-oscillates (rms after 18 s: " + juce::String (late, 3) + ")");
    }

    // 5. slowdown at 2 octaves down: long, grainy, still clean numbers ---------------------------------------
    {
        auto p = fresh();
        set (*p, ID::time, 4000.0f); set (*p, ID::slowdown, 1.0f); set (*p, ID::int1, 0.0f); set (*p, ID::int2, 0.0f);
        set (*p, ID::len1, 2.0f); set (*p, ID::mix, 1.0f); set (*p, ID::regen, 0.0f);
        juce::AudioBuffer<float> in (2, (int) SR * 6); in.clear();
        for (int i = 0; i < (int) SR; ++i) { const float v = 0.3f * (float) std::sin (2 * kPi * 600.0 * i / SR); in.setSample (0, i, v); in.setSample (1, i, v); }
        auto out = run (*p, in);
        bool finite = true; for (int i = 0; i < out.getNumSamples(); ++i) finite &= std::isfinite (out.getSample (0, i));
        const double longest = 8192.0 / p->currentClockHz.load();
        check (finite && longest > 30.0, "slowdown + 2 octaves down reaches " + juce::String (longest, 1) + " s of delay");
    }

    // 6. CPU at the fastest clock (40 ms, 2 octaves up) ---------------------------------------------------------
    {
        auto p = fresh();
        set (*p, ID::time, 40.0f); set (*p, ID::int1, 1.0f); set (*p, ID::int2, 1.0f); set (*p, ID::len1, 2.0f);
        juce::AudioBuffer<float> in (2, (int) SR * 10); in.clear();
        for (int i = 0; i < in.getNumSamples(); ++i) { const float v = 0.3f * (float) std::sin (2 * kPi * 300.0 * i / SR); in.setSample (0, i, v); in.setSample (1, i, v); }
        auto t0 = std::chrono::steady_clock::now();
        auto out = run (*p, in);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        check (secs < 1.0, "10 s of audio at the fastest clock took " + juce::String (secs, 2) + " s to process");
    }

    // 7. the rest of the switches don't break anything --------------------------------------------------------------
    {
        auto p = fresh();
        juce::AudioBuffer<float> in (2, (int) SR * 4);
        juce::Random r (3);
        for (int i = 0; i < in.getNumSamples(); ++i) { const float v = 0.3f * (r.nextFloat() * 2 - 1); in.setSample (0, i, v); in.setSample (1, i, v); }
        bool finite = true;
        for (auto id : { ID::modulation, ID::rampLpf, ID::rampRegen, ID::rampGlide, ID::step, ID::momentary })
        {
            set (*p, id, 1.0f); set (*p, ID::glide, 0.6f); set (*p, ID::len2, 2.0f);
            auto out = run (*p, in);
            for (int i = 0; i < out.getNumSamples(); ++i) finite &= std::isfinite (out.getSample (0, i)) && std::abs (out.getSample (0, i)) < 3.0f;
        }
        set (*p, ID::on, 0.0f); set (*p, ID::trails, 0.0f);
        in.clear();
        auto out = run (*p, in);
        check (finite, "modulation, ramps, step and glide all run clean");
        check (rms (out.getReadPointer (0), (int) SR, (int) SR * 3) < 1.0e-4, "off without trails is silent");
    }

    // 8. a demo to listen to: plucked notes through the default settings ------------------------------------------------
    {
        auto p = fresh();
        set (*p, ID::time, 500.0f); set (*p, ID::regen, 0.55f); set (*p, ID::mix, 0.5f); set (*p, ID::glide, 0.25f);
        juce::AudioBuffer<float> in (2, (int) (SR * 12)); in.clear();
        const double notes[] = { 196.0, 246.9, 293.7, 392.0, 293.7, 246.9 };
        juce::Random r (7);
        for (int k = 0; k < 6; ++k)
        {
            const int start = (int) (SR * (0.25 + k * 0.5));
            const int period = (int) (SR / notes[k]);
            std::vector<float> ks ((size_t) period);
            for (auto& v : ks) v = r.nextFloat() * 2 - 1;
            for (int i = 0; i < (int) (SR * 0.5); ++i)
            {
                const size_t a = (size_t) (i % period), b = (size_t) ((i + 1) % period);
                ks[a] = 0.497f * (ks[a] + ks[b]);
                const float v = 0.35f * ks[a];
                in.addSample (0, start + i, v); in.addSample (1, start + i, v);
            }
        }
        writeWav (run (*p, in), outDir.getChildFile ("YokokoOnono-demo.wav"));
        const char* names[] = { "", "everything", "echoes", "every-other-echo" };
        for (int mode = 1; mode <= 3; ++mode)
        {
            auto q = fresh();
            set (*q, ID::time, 500.0f); set (*q, ID::regen, 0.55f); set (*q, ID::mix, 0.5f); set (*q, ID::glide, 0.25f);
            set (*q, ID::sync, 0.0f); set (*q, ID::reverse, (float) mode);
            writeWav (run (*q, in), outDir.getChildFile ("YokokoOnono-demo-reverse-" + juce::String (names[mode]) + ".wav"));
        }
        std::printf ("wrote demo\n");
    }

    // 9. pictures of the interface ---------------------------------------------------------------------------------------------
    {
        auto p = fresh();
        std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
        auto* e = dynamic_cast<YokokokoAudioProcessorEditor*> (ed.get());
        auto shoot = [&] (const juce::String& name)
        {
            e->refreshLabels();
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
            juce::FileOutputStream fos (outDir.getChildFile (name));
            fos.setPosition (0); fos.truncate();
            juce::PNGImageFormat().writeImageToStream (img, fos);
        };
        shoot ("YokokoOnono-front.png");
        set (*p, ID::reverse, 2.0f);
        shoot ("YokokoOnono-front-reverse.png");
        set (*p, ID::reverse, 0.0f);
        e->showBack (true);
        shoot ("YokokoOnono-back.png");
        set (*p, ID::modulation, 1.0f);
        e->showBack (false);
        {
            juce::AudioBuffer<float> b (2, 512); b.clear(); juce::MidiBuffer m; p->processBlock (b, m);
            e->resized();
        }
        shoot ("YokokoOnono-front-modulation.png");
        set (*p, ID::modulation, 0.0f); set (*p, ID::random, 1.0f); set (*p, ID::weird, 1.0f);
        set (*p, ID::keyRoot, 9.0f); set (*p, ID::keyScale, 1.0f);
        {
            juce::AudioBuffer<float> b (2, (int) SR * 2); juce::MidiBuffer m;
            for (int i = 0; i < b.getNumSamples(); ++i) { const float v = 0.3f * (float) std::sin (2 * kPi * 220.0 * i / SR); b.setSample (0, i, v); b.setSample (1, i, v); }
            for (int pos = 0; pos < b.getNumSamples(); pos += 512) { juce::AudioBuffer<float> blk (b.getArrayOfWritePointers(), 2, pos, juce::jmin (512, b.getNumSamples() - pos)); p->processBlock (blk, m); }
        }
        shoot ("YokokoOnono-front-random.png");
        set (*p, ID::bars, 1.0f); set (*p, ID::random, 0.0f); set (*p, ID::weird, 0.0f);
        set (*p, ID::sync, 0.0f); set (*p, ID::meterNum, 7.0f); set (*p, ID::meterDen, 1.0f);
        {
            juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m;
            for (int k = 0; k < 120; ++k) { b.clear(); p->processBlock (b, m); }
        }
        shoot ("YokokoOnono-front-bars.png");
        e->showBack (true);
        shoot ("YokokoOnono-back.png");
        std::printf ("wrote pictures\n");
    }

    std::printf ("%d failures\n", failures);
    return failures;
}
