#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <functional>
#include "BBDEngine.h"
#include "PitchTracker.h"
#include "Reverser.h"

namespace yoko
{
// Interval knob detents (noon = unison).
static constexpr int kNumIntervals = 15;
static constexpr int kIntervalSemis[kNumIntervals] = { -24, -14, -12, -9, -7, -5, -4, 0, 4, 5, 7, 9, 12, 14, 24 };
inline const char* intervalName (int i)
{
    static const char* names[kNumIntervals] = { "2 octaves down", "9th down", "1 octave down", "6th down", "5th down",
                                                "4th down", "3rd down", "same note", "3rd up", "4th up", "5th up",
                                                "6th up", "1 octave up", "9th up", "2 octaves up" };
    return names[juce::jlimit (0, kNumIntervals - 1, i)];
}
inline int intervalIndex (float norm) { return juce::jlimit (0, kNumIntervals - 1, (int) std::round (norm * (kNumIntervals - 1))); }

// Name for any interval in semitones (used by random notes).
inline juce::String semisName (int s)
{
    if (s == 0) return "same note";
    static const char* n[25] = { "", "minor 2nd", "2nd", "minor 3rd", "3rd", "4th", "tritone", "5th", "minor 6th", "6th",
                                 "minor 7th", "7th", "1 octave", "minor 9th", "9th", "minor 10th", "10th", "11th",
                                 "sharp 11th", "12th", "minor 13th", "13th", "minor 14th", "14th", "2 octaves" };
    return juce::String (n[juce::jlimit (1, 24, std::abs (s))]) + (s > 0 ? " up" : " down");
}

inline const juce::StringArray& noteNames()
{
    static const juce::StringArray names { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names;
}

// Time signatures: the ways a bar of n beats can be split into groups.
// Each group is one step of the sequence (steps cycle 1, 2, 3, 1 ...).
static constexpr int kMaxNumerator = 15, kMaxGroupings = 32, kMaxGroups = 16;
struct Grouping { int count = 0; int sizes[kMaxGroups] {}; };
struct GroupingList { int count = 0; Grouping g[kMaxGroupings]; };

inline const GroupingList& groupingsFor (int n)
{
    static const auto table = []
    {
        std::array<GroupingList, kMaxNumerator + 1> t {};
        for (int num = 1; num <= kMaxNumerator; ++num)
        {
            std::vector<std::vector<int>> all;
            std::vector<int> cur;
            std::function<void (int)> build = [&] (int left)
            {
                if (left == 0) { all.push_back (cur); return; }
                for (int part : { 2, 3 })
                    if (part <= left) { cur.push_back (part); build (left - part); cur.pop_back(); }
            };
            build (num);
            // fewest groups first, then 2s before 3s
            std::stable_sort (all.begin(), all.end(), [] (auto& a, auto& b) { return a.size() < b.size(); });
            std::vector<int> everyBeat ((size_t) num, 1);
            if (num <= 4) all.insert (all.begin(), everyBeat); else all.push_back (everyBeat);
            auto& list = t[(size_t) num];
            for (auto& gr : all)
            {
                if (list.count >= kMaxGroupings || (int) gr.size() > kMaxGroups) continue;
                auto& dst = list.g[list.count++];
                dst.count = (int) gr.size();
                for (size_t i = 0; i < gr.size(); ++i) dst.sizes[i] = gr[i];
            }
        }
        return t;
    }();
    return table[(size_t) juce::jlimit (1, kMaxNumerator, n)];
}

inline const Grouping& groupingFor (int n, int index)
{
    const auto& list = groupingsFor (n);
    return list.g[juce::jlimit (0, list.count - 1, index)];
}

inline juce::String groupingText (const Grouping& g)
{
    bool allOnes = g.count > 1;
    for (int i = 0; i < g.count; ++i) allOnes = allOnes && g.sizes[i] == 1;
    if (allOnes) return "every beat";
    juce::String s;
    for (int i = 0; i < g.count; ++i) s << (i > 0 ? "+" : "") << g.sizes[i];
    return s;
}

namespace ID
{
    static const juce::String mix = "mix", lpf = "lpf", regen = "regen", glide = "glide",
                              int1 = "int1", int2 = "int2",
                              len1 = "len1", len2 = "len2", len3 = "len3",
                              memory = "memory", time = "time", on = "on",
                              sync = "sync", modulation = "modulation", step = "step", slowdown = "slowdown",
                              rampLpf = "rampLpf", rampRegen = "rampRegen", rampGlide = "rampGlide",
                              trails = "trails", momentary = "momentary",
                              random = "random", weird = "weird", keyRoot = "keyRoot", keyScale = "keyScale",
                              bars = "bars", meterNum = "meterNum", meterDen = "meterDen", grouping = "grouping",
                              reverse = "reverse";
}
} // namespace yoko

class YokokokoAudioProcessor : public juce::AudioProcessor,
                               private juce::AudioProcessorValueTreeState::Listener,
                               private juce::AsyncUpdater
{
public:
    YokokokoAudioProcessor();
    ~YokokokoAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Yokoko Onono Pitch Delay v4"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 45.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Called from the GUI (message thread)
    void tap();
    void saveToCurrentSlot();
    bool slotHasData (int slot) const { return slots[(size_t) slot].isValid(); }

    // Preset files (.yokoko)
    bool savePresetFile (const juce::File&);
    bool loadPresetFile (const juce::File&);
    juce::String getPresetName() const { return apvts.state.getProperty ("presetName", "untitled").toString(); }
    juce::File getPresetFolder() const;

    // Read by the GUI
    std::atomic<int>   currentStep { 0 };
    std::atomic<float> currentDelayMs { 500.0f }, currentClockHz { 16384.0f }, currentSemis { 0.0f };
    std::atomic<float> baseDelayMs { 500.0f };
    std::atomic<bool>  followingHost { false };
    std::atomic<int>   heardNote { -1 }, randomSemis1 { 0 }, randomSemis2 { 0 };
    std::atomic<int>   meterNumNow { 4 }, meterDenNow { 4 }, groupNow { 0 }, beatNow { 0 };
    std::atomic<bool>  meterFromSong { false };
    std::atomic<float> reversePieceMs { 500.0f };

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void parameterChanged (const juce::String& id, float newValue) override;
    void handleAsyncUpdate() override;
    juce::ValueTree takeSnapshot() const;
    void applySnapshot (const juce::ValueTree&);

    yoko::WetPath wet;
    yoko::Reverser rev;   // v4
    juce::SmoothedValue<float> sRevDry, sRevWet;
    double fs = 48000.0;

    // clock / glide (log2 Hz)
    double logF = std::log2 (16384.0), glideTarget = logF, glideRate = 0.0;
    bool   clockInit = false;

    // sequencer
    double freeBeatPos = 0.0;
    int    manualStep = 0, lastStep = -1;
    std::atomic<int>  pendingAdvance { 0 };
    std::atomic<bool> pendingReset { false };

    // random notes
    int pickRandomInterval (int root, bool minor, bool weird);
    yoko::PitchTracker tracker;
    int randSemis[3] = { 0, 0, 0 }, prevSeqStep = -1;
    bool randPicked = false;

    // modulation LFO / ramp
    double lfoPhase = 0.0, rampPhase = 0.0, rndTarget = 0.0, rndSmooth = 0.0, rndHeld = 0.0;
    juce::Random rng;

    // smoothing
    juce::SmoothedValue<float> sMix, sLpf, sRegen, sOnWet, sInGate;

    // tap tempo
    juce::Array<double> tapTimes;

    // memory slots
    std::array<juce::ValueTree, 2> slots;
    std::atomic<int> pendingRecall { -1 };
    bool applyingSnapshot = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YokokokoAudioProcessor)
};
