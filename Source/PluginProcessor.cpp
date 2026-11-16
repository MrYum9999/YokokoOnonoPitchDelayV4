#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace yoko;
using APVTS = juce::AudioProcessorValueTreeState;

namespace
{
const juce::StringArray kLengths { "quarter", "dotted eighth", "eighth" };
constexpr double kLengthBeats[3] = { 1.0, 0.75, 0.5 };

// every parameter stored in a memory slot
const juce::StringArray kSnapshotIds { ID::mix, ID::lpf, ID::regen, ID::glide, ID::int1, ID::int2,
                                       ID::len1, ID::len2, ID::len3, ID::time, ID::sync, ID::modulation,
                                       ID::step, ID::slowdown, ID::rampLpf, ID::rampRegen, ID::rampGlide,
                                       ID::trails, ID::momentary, ID::random, ID::weird, ID::keyRoot, ID::keyScale,
                                       ID::bars, ID::meterNum, ID::meterDen, ID::grouping, ID::reverse };

float raw (const APVTS& s, const juce::String& id) { return s.getRawParameterValue (id)->load(); }
bool  on  (const APVTS& s, const juce::String& id) { return raw (s, id) > 0.5f; }

double lpfHzFromNorm (double n) { return 150.0 * std::pow (12000.0 / 150.0, juce::jlimit (0.0, 1.0, n)); }
double glideSecondsFromNorm (double n) { return n < 0.01 ? 0.0 : 0.005 * std::pow (3.0 / 0.005, n); }
}

//==============================================================================
APVTS::ParameterLayout YokokokoAudioProcessor::createLayout()
{
    using namespace juce;
    APVTS::ParameterLayout l;

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v * 100)) + "%"; });

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::mix, 1 },   "Mix",      NormalisableRange<float> (0, 1), 0.5f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::lpf, 1 },   "Filter",   NormalisableRange<float> (0, 1), 0.65f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            auto hz = lpfHzFromNorm (v);
            return hz >= 1000 ? String (hz / 1000.0, 1) + " kHz" : String (juce::roundToInt (hz)) + " Hz";
        })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::regen, 1 }, "Feedback", NormalisableRange<float> (0, 1), 0.4f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::glide, 1 }, "Glide",    NormalisableRange<float> (0, 1), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            auto s = glideSecondsFromNorm (v);
            return s == 0.0 ? String ("none") : s < 1.0 ? String (juce::roundToInt (s * 1000)) + " ms" : String (s, 2) + " s";
        })));

    auto intervalAttr = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (intervalName (intervalIndex (v))); });
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::int1, 1 }, "Interval 1", NormalisableRange<float> (0, 1), 10.0f / 14.0f, intervalAttr));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::int2, 1 }, "Interval 2", NormalisableRange<float> (0, 1), 12.0f / 14.0f, intervalAttr));

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::len1, 1 }, "Step 1 length", kLengths, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::len2, 1 }, "Step 2 length", kLengths, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::len3, 1 }, "Step 3 length", kLengths, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::memory, 1 }, "Memory", StringArray { "one", "now", "two" }, 1));

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::time, 1 }, "Time",
        NormalisableRange<float> (40.0f, 4000.0f, 0.0f, 0.4f), 500.0f,
        AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v)) + " ms"; })));

    l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::on, 1 },         "On", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::sync, 1 },       "Follow host tempo", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::modulation, 1 }, "Modulation", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::step, 1 },       "Step", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::slowdown, 1 },   "Slowdown", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::rampLpf, 1 },    "Ramp filter", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::rampRegen, 1 },  "Ramp feedback", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::rampGlide, 1 },  "Ramp glide", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::trails, 1 },     "Trails", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::momentary, 1 },  "Momentary", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::random, 1 },     "Random notes", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::weird, 1 },      "Make it weird", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::keyRoot, 1 },  "Key", yoko::noteNames(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::keyScale, 1 }, "Major or minor", StringArray { "major", "minor" }, 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::bars, 1 },       "Count bars", false));
    l.add (std::make_unique<AudioParameterInt>  (ParameterID { ID::meterNum, 1 },   "Beats in a bar", 2, yoko::kMaxNumerator, 7));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::meterDen, 1 }, "Beat note", StringArray { "4", "8", "16" }, 1));
    l.add (std::make_unique<AudioParameterInt>  (ParameterID { ID::grouping, 1 },   "Beat groups", 0, yoko::kMaxGroupings - 1, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::reverse, 1 }, "Reverse",
                                                    StringArray { "off", "everything", "echoes", "every other echo" }, 0));
    return l;
}

//==============================================================================
YokokokoAudioProcessor::YokokokoAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "YOKOKOKO", createLayout())
{
    groupingsFor (7);   // build the grouping table here, not on the audio thread
    apvts.addParameterListener (ID::memory, this);
}

YokokokoAudioProcessor::~YokokokoAudioProcessor()
{
    apvts.removeParameterListener (ID::memory, this);
    cancelPendingUpdate();
}

bool YokokokoAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void YokokokoAudioProcessor::prepareToPlay (double sampleRate, int)
{
    fs = sampleRate;
    wet.prepare (fs);
    tracker.prepare (fs);
    randPicked = false;
    prevSeqStep = -1;

    for (auto* s : { &sMix, &sLpf, &sRegen })
        s->reset (fs, 0.03);
    sOnWet.reset (fs, 0.015);
    sInGate.reset (fs, 0.008);
    rev.prepare (fs);
    sRevDry.reset (fs, 0.02);
    sRevWet.reset (fs, 0.02);
    {
        const int r = (int) raw (apvts, ID::reverse);
        sRevDry.setCurrentAndTargetValue (r == 1 && on (apvts, ID::on) ? 1.0f : 0.0f);
        sRevWet.setCurrentAndTargetValue (r != 0 ? 1.0f : 0.0f);
    }

    sMix.setCurrentAndTargetValue (raw (apvts, ID::mix));
    sLpf.setCurrentAndTargetValue (raw (apvts, ID::lpf));
    sRegen.setCurrentAndTargetValue (raw (apvts, ID::regen));
    const float o = on (apvts, ID::on) ? 1.0f : 0.0f;
    sOnWet.setCurrentAndTargetValue (o);
    sInGate.setCurrentAndTargetValue (o);

    clockInit = false;
    freeBeatPos = 0.0;
    lastStep = -1;
}

//==============================================================================
void YokokokoAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    // ---- read controls -------------------------------------------------------
    const bool modMode   = on (apvts, ID::modulation);
    const bool stepMode  = on (apvts, ID::step);
    const bool slowdown  = on (apvts, ID::slowdown);
    const bool rLpf = on (apvts, ID::rampLpf), rRegen = on (apvts, ID::rampRegen), rGlide = on (apvts, ID::rampGlide);
    const bool anyRamp   = rLpf || rRegen || rGlide;
    const bool effectOn  = on (apvts, ID::on);
    const bool trails    = on (apvts, ID::trails);
    const float int1     = raw (apvts, ID::int1), int2 = raw (apvts, ID::int2);
    const float glideKnob = raw (apvts, ID::glide);
    const int lens[3] = { (int) raw (apvts, ID::len1), (int) raw (apvts, ID::len2), (int) raw (apvts, ID::len3) };
    const bool randomOn  = on (apvts, ID::random);
    const bool weirdOn   = on (apvts, ID::weird);
    const int  keyRoot   = (int) raw (apvts, ID::keyRoot);
    const bool keyMinor  = raw (apvts, ID::keyScale) > 0.5f;
    if (! randomOn) randPicked = false;

    // ---- tempo -----------------------------------------------------------------
    double tBase = raw (apvts, ID::time) * 0.001;
    bool hostPlaying = false, haveHostTempo = false, haveHostSig = false;
    double ppqStart = 0.0, hostBpm = 120.0, barStartQ = 0.0;
    bool haveBarStart = false;
    int num = (int) raw (apvts, ID::meterNum);
    int den = 4 << (int) raw (apvts, ID::meterDen);
    if (on (apvts, ID::sync))
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                if (auto bpm = pos->getBpm(); bpm && *bpm > 1.0)
                {
                    hostBpm = *bpm;
                    tBase = 60.0 / *bpm;
                    haveHostTempo = true;
                }
                if (auto sig = pos->getTimeSignature(); sig && sig->numerator > 0 && sig->denominator > 0)
                {
                    num = juce::jlimit (1, kMaxNumerator, sig->numerator);
                    den = sig->denominator;
                    haveHostSig = true;
                }
                if (auto bs = pos->getPpqPositionOfLastBarStart())
                {
                    barStartQ = *bs;
                    haveBarStart = true;
                }
                if (auto ppq = pos->getPpqPosition(); ppq && pos->getIsPlaying() && haveHostTempo)
                {
                    hostPlaying = true;
                    ppqStart = *ppq;
                }
            }

    // count bars: one repeat = one beat of the time signature (an eighth in 7/8, a quarter in 5/4)
    const bool barsOn = on (apvts, ID::bars);
    const Grouping& groups = groupingFor (num, (int) raw (apvts, ID::grouping));
    const double unitsPerQuarter = den / 4.0;
    if (barsOn && haveHostTempo)
        tBase = 60.0 / hostBpm / unitsPerQuarter;
    if (barsOn && hostPlaying && ! haveBarStart)
    {
        const double barQ = num / unitsPerQuarter;
        barStartQ = std::floor (ppqStart / barQ) * barQ;
    }
    meterNumNow.store (num);
    meterDenNow.store (den);
    meterFromSong.store (haveHostSig);
    tBase = juce::jlimit (0.04, 4.0, tBase);
    followingHost.store (haveHostTempo);
    baseDelayMs.store ((float) (tBase * 1000.0));

    const double fBase = 8192.0 / tBase * (slowdown ? 0.5 : 1.0);   // f_base = 8192 / T_base
    const double logBase = std::log2 (fBase);
    const double beatsPerSample = 1.0 / (tBase * fs);

    // ---- sequencer events --------------------------------------------------------
    for (const auto meta : midi)
        if (meta.getMessage().isNoteOn())
        {
            if (stepMode) pendingAdvance.fetch_add (1);
            else          pendingReset.store (true);
        }
    midi.clear();

    if (int adv = pendingAdvance.exchange (0); adv > 0)
        manualStep = (manualStep + adv) % 3;
    if (pendingReset.exchange (false))
    {
        freeBeatPos = 0.0;
        manualStep = 0;
        lfoPhase = 0.0;
    }

    const double seqLen = kLengthBeats[lens[0]] + kLengthBeats[lens[1]] + kLengthBeats[lens[2]];
    const double stepSemis[3] = { 0.0, (double) kIntervalSemis[intervalIndex (int1)], (double) kIntervalSemis[intervalIndex (int2)] };

    // ---- modulation settings -------------------------------------------------------
    const double lfoHz = 0.05 * std::pow (12.0 / 0.05, (double) int1);
    const double depthSemis = 12.0 * int2;
    const int shape   = lens[0];   // triangle / sine / square
    const int random  = lens[1];   // none / some / lots
    const int target  = lens[2];   // pitch / filter / both
    const double rampHz = 0.03 * std::pow (3.0 / 0.03, (double) raw (apvts, ID::mix));

    sMix.setTargetValue (anyRamp ? 0.5f : raw (apvts, ID::mix));
    sLpf.setTargetValue (raw (apvts, ID::lpf));
    sRegen.setTargetValue (raw (apvts, ID::regen));
    sOnWet.setTargetValue (effectOn || trails ? 1.0f : 0.0f);
    sInGate.setTargetValue (effectOn ? 1.0f : 0.0f);

    // ---- v4 reverse ------------------------------------------------------------------
    // 0 off, 1 everything (dry and repeats backwards), 2 echoes (repeats backwards),
    // 3 every other echo (repeats alternate forwards and backwards)
    const int reverseMode = (int) raw (apvts, ID::reverse);
    sRevDry.setTargetValue (reverseMode == 1 && effectOn ? 1.0f : 0.0f);
    sRevWet.setTargetValue (reverseMode != 0 ? 1.0f : 0.0f);
    const double pieceSec = 8192.0 / fBase;   // one piece = one repeat at the base time
    const double secondsAtStart = hostPlaying ? ppqStart * 60.0 / hostBpm : 0.0;

    if (! clockInit)
    {
        logF = glideTarget = logBase;
        clockInit = true;
    }

    // clock whine is part of the Slowdown sound only, and follows the signal
    const double bleed = slowdown ? 0.035 : 0.0;
    const float* inL = buffer.getReadPointer (0);
    const float* inR = buffer.getReadPointer (numIn > 1 ? 1 : 0);
    int step = lastStep < 0 ? 0 : lastStep;
    double semisNow = 0.0;

    for (int n = 0; n < numSamples; ++n)
    {
        // ramp generator (triangle 0..1)
        double ramp = 1.0;
        if (anyRamp)
        {
            rampPhase += rampHz / fs;
            if (rampPhase >= 1.0) rampPhase -= 1.0;
            ramp = rampPhase < 0.5 ? rampPhase * 2.0 : 2.0 - rampPhase * 2.0;
        }

        double lpfNorm = sLpf.getNextValue() * (rLpf ? ramp : 1.0);
        const double regenNorm = sRegen.getNextValue() * (rRegen ? ramp : 1.0);
        const double glideSec = glideSecondsFromNorm (glideKnob * (rGlide ? ramp : 1.0));

        double targetSemis = 0.0;
        if (modMode)
        {
            lfoPhase += lfoHz / fs;
            if (lfoPhase >= 1.0)
            {
                lfoPhase -= 1.0;
                rndTarget = rng.nextDouble() * 2.0 - 1.0;
                rndHeld = rndTarget;
            }
            double v = shape == 0 ? 1.0 - 4.0 * std::abs (lfoPhase - 0.5)
                     : shape == 1 ? std::sin (2.0 * kPi * lfoPhase)
                                  : (lfoPhase < 0.5 ? 1.0 : -1.0);
            rndSmooth += (rndTarget - rndSmooth) * (lfoHz * 4.0 / fs);
            if (random == 1) v = 0.5 * v + 0.5 * rndSmooth;
            if (random == 2) v = 0.3 * v + 0.7 * rndHeld;

            if (target != 1) targetSemis = v * depthSemis;
            if (target != 0) lpfNorm += v * (depthSemis / 12.0) * 0.35;
        }
        else
        {
            if (stepMode)
                step = manualStep;
            else if (barsOn)
            {
                // the bar is split into beat groups; each group is a step, starting on beat 1
                double pos;
                if (hostPlaying)
                    pos = (ppqStart - barStartQ) * unitsPerQuarter + n * beatsPerSample;
                else
                {
                    pos = freeBeatPos;
                    freeBeatPos += beatsPerSample;
                }
                pos = std::fmod (pos, (double) num);
                if (pos < 0) pos += num;
                if (! hostPlaying) freeBeatPos = std::fmod (freeBeatPos, (double) num);

                int g = 0, acc = groups.sizes[0];
                while (pos >= acc && g < groups.count - 1)
                    acc += groups.sizes[++g];
                step = g % 3;
                if (n == numSamples - 1)
                {
                    groupNow.store (g);
                    beatNow.store ((int) pos);
                }
            }
            else
            {
                double pos;
                if (hostPlaying)
                    pos = ppqStart + n * beatsPerSample;
                else
                {
                    pos = freeBeatPos;
                    freeBeatPos += beatsPerSample;
                    if (freeBeatPos >= seqLen) freeBeatPos -= seqLen;
                }
                pos = std::fmod (pos, seqLen);
                if (pos < 0) pos += seqLen;
                const double b1 = kLengthBeats[lens[0]], b2 = b1 + kLengthBeats[lens[1]];
                step = pos < b1 ? 0 : pos < b2 ? 1 : 2;
            }
            if (randomOn)
            {
                // a new random interval each time the sequence arrives at step 2 or 3
                if (! randPicked)
                {
                    randSemis[1] = pickRandomInterval (keyRoot, keyMinor, weirdOn);
                    randSemis[2] = pickRandomInterval (keyRoot, keyMinor, weirdOn);
                    randPicked = true;
                }
                else if (step != prevSeqStep && step > 0)
                    randSemis[step] = pickRandomInterval (keyRoot, keyMinor, weirdOn);
                targetSemis = randSemis[step];
            }
            else
                targetSemis = stepSemis[step];
            prevSeqStep = step;
        }

        // ---- clock and glide ----------------------------------------------------------
        const double newTarget = juce::jlimit (std::log2 (200.0), std::log2 (900000.0), logBase + targetSemis / 12.0);
        if (modMode)
        {
            const double tau = std::max (glideSec * 0.5, 0.002);
            logF += (newTarget - logF) * (1.0 - std::exp (-1.0 / (tau * fs)));
            glideTarget = newTarget;
        }
        else
        {
            if (std::abs (newTarget - glideTarget) > 1.0e-9)
            {
                glideTarget = newTarget;
                // exponential sweep: constant semitones per millisecond over the glide time
                glideRate = glideSec > 0.0 ? std::abs (glideTarget - logF) / (glideSec * fs) : 1.0e9;
            }
            if (logF < glideTarget) logF = std::min (glideTarget, logF + glideRate);
            else if (logF > glideTarget) logF = std::max (glideTarget, logF - glideRate);
        }
        const double fclk = std::exp2 (logF);
        semisNow = (logF - logBase) * 12.0;

        // ---- audio --------------------------------------------------------------------
        const double inMono = 0.5 * ((double) inL[n] + (double) inR[n]);
        if (tracker.push (inMono))
            heardNote.store (tracker.note);
        const double gate = sInGate.getNextValue();
        const double regen = 1.2 * regenNorm;
        const double w = wet.process (inMono * gate, fclk, lpfHzFromNorm (lpfNorm), 2.0, regen, bleed);
        double wetOut = 1.5 * std::tanh (w / 1.5);

        // reverse: pieces line up with the beat when the song is playing
        if (rev.needsNewGrain())
        {
            double samples = pieceSec * fs;
            if (hostPlaying)
            {
                const double units = (secondsAtStart + n / fs) / pieceSec;
                double left = (std::ceil (units - 1.0e-6) - units) * pieceSec * fs;
                if (left < 0.5 * pieceSec * fs) left += pieceSec * fs;
                samples = left;
            }
            rev.startGrain ((int) std::lround (samples));
        }
        const double dryInL = inL[n], dryInR = inR[n];
        rev.write (dryInL, dryInR, wetOut);
        const double revDry = sRevDry.getNextValue(), revWet = sRevWet.getNextValue();
        if (revWet > 0.0)
        {
            const double back = rev.reversed (2);
            const double processed = reverseMode == 3 ? (rev.isReversedPiece() ? back : rev.forward (2)) : back;
            wetOut += (processed - wetOut) * revWet;
        }
        const double revL = revDry > 0.0 ? rev.reversed (0) : 0.0;
        const double revR = revDry > 0.0 ? rev.reversed (1) : 0.0;
        rev.advance();
        wetOut *= sOnWet.getNextValue();

        const double m = sMix.getNextValue();
        const double dryGain = effectOn ? std::min (1.0, 2.0 * (1.0 - m)) : 1.0;
        const double wetGain = std::min (1.0, 2.0 * m);

        for (int ch = 0; ch < numOut; ++ch)
        {
            double dry = buffer.getReadPointer (juce::jmin (ch, numIn - 1))[n];
            if (revDry > 0.0)
                dry += ((ch == 0 || numIn < 2 ? revL : revR) - dry) * revDry;
            buffer.getWritePointer (ch)[n] = (float) (dry * dryGain + wetOut * wetGain);
        }
    }

    lastStep = modMode ? 0 : step;
    currentStep.store (lastStep);
    const double fNow = std::exp2 (logF);
    currentClockHz.store ((float) fNow);
    currentDelayMs.store ((float) (8192.0 / fNow * 1000.0));
    currentSemis.store ((float) semisNow);
    randomSemis1.store (randSemis[1]);
    randomSemis2.store (randSemis[2]);
    reversePieceMs.store ((float) (pieceSec * 1000.0));
}

//==============================================================================
// Chooses an interval for "random notes". The note it lands on is worked out
// from the last note heard, so the result stays inside the key.
//   normal: consonant intervals (3rds, 4ths, 5ths, 6ths, octaves, 9ths ...) onto notes of the scale
//   weird:  odd intervals (2nds, tritones, 7ths, 11ths, 13ths ...) onto the scale, or onto
//           a few "nearly in key" notes (blue notes and borrowed modal notes)
int YokokokoAudioProcessor::pickRandomInterval (int root, bool minor, bool weird)
{
    static const int major[7] = { 0, 2, 4, 5, 7, 9, 11 }, minorScale[7] = { 0, 2, 3, 5, 7, 8, 10 };
    static const int majorExtra[3] = { 3, 6, 10 }, minorExtra[3] = { 6, 9, 11 };
    static const int normalIv[] = { -24, -19, -17, -16, -15, -14, -12, -9, -8, -7, -5, -4, -3,
                                     3, 4, 5, 7, 8, 9, 12, 14, 15, 16, 17, 19, 24 };
    static const int weirdIv[]  = { -23, -22, -21, -18, -13, -11, -10, -6, -2, -1,
                                    1, 2, 6, 10, 11, 13, 18, 20, 21, 22, 23 };

    const int* scale = minor ? minorScale : major;
    const int* extra = minor ? minorExtra : majorExtra;
    const int heard = heardNote.load();
    const int from = heard >= 0 ? heard : 60 + root;

    auto degree = [&] (int iv) { return ((from + iv - root) % 12 + 12) % 12; };
    auto inScale = [&] (int d) { for (int i = 0; i < 7; ++i) if (scale[i] == d) return true; return false; };
    auto inExtra = [&] (int d) { for (int i = 0; i < 3; ++i) if (extra[i] == d) return true; return false; };

    juce::Array<int> options;
    if (! weird)
    {
        for (int iv : normalIv) if (inScale (degree (iv))) options.add (iv);
    }
    else
    {
        const bool outside = rng.nextFloat() < 0.4f;
        for (int iv : weirdIv)
            if (outside ? inExtra (degree (iv)) : inScale (degree (iv))) options.add (iv);
        if (options.isEmpty())
            for (int iv : normalIv) if (inExtra (degree (iv))) options.add (iv);
    }
    if (options.isEmpty())
        for (int iv : normalIv) options.add (iv);

    // don't repeat the same interval twice in a row if there's a choice
    int pick = options[rng.nextInt (options.size())];
    if (options.size() > 1 && (pick == randSemis[1] || pick == randSemis[2]))
        pick = options[rng.nextInt (options.size())];
    return pick;
}

//==============================================================================
juce::File YokokokoAudioProcessor::getPresetFolder() const
{
    const juce::File last (apvts.state.getProperty ("presetFolder").toString());
    return last.isDirectory() ? last : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
}

bool YokokokoAudioProcessor::savePresetFile (const juce::File& fileIn)
{
    const auto file = fileIn.withFileExtension ("yokoko");
    juce::XmlElement xml ("YOKOKO_ONONO_PRESET");
    xml.setAttribute ("name", file.getFileNameWithoutExtension());
    xml.setAttribute ("version", 4);
    for (auto& id : kSnapshotIds)
        xml.setAttribute (id, (double) apvts.getParameter (id)->getValue());
    if (! xml.writeTo (file))
        return false;
    apvts.state.setProperty ("presetName", file.getFileNameWithoutExtension(), nullptr);
    apvts.state.setProperty ("presetFolder", file.getParentDirectory().getFullPathName(), nullptr);
    return true;
}

bool YokokokoAudioProcessor::loadPresetFile (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("YOKOKO_ONONO_PRESET"))
        return false;
    {
        const juce::ScopedValueSetter<bool> svs (applyingSnapshot, true);
        for (auto& id : kSnapshotIds)
            if (xml->hasAttribute (id))
                if (auto* p = apvts.getParameter (id))
                {
                    p->beginChangeGesture();
                    p->setValueNotifyingHost ((float) xml->getDoubleAttribute (id));
                    p->endChangeGesture();
                }
    }
    apvts.state.setProperty ("presetName", xml->getStringAttribute ("name", file.getFileNameWithoutExtension()), nullptr);
    apvts.state.setProperty ("presetFolder", file.getParentDirectory().getFullPathName(), nullptr);
    return true;
}

//==============================================================================
void YokokokoAudioProcessor::tap()
{
    if (on (apvts, ID::step))
    {
        pendingAdvance.fetch_add (1);
        return;
    }

    const double now = juce::Time::getMillisecondCounterHiRes();
    if (! tapTimes.isEmpty() && now - tapTimes.getLast() > 4000.0)
        tapTimes.clear();
    tapTimes.add (now);
    while (tapTimes.size() > 4)
        tapTimes.remove (0);

    pendingReset.store (true);

    if (tapTimes.size() >= 2)
    {
        const double avg = (tapTimes.getLast() - tapTimes.getFirst()) / (tapTimes.size() - 1);
        if (auto* p = apvts.getParameter (ID::time))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::jlimit (40.0, 4000.0, avg)));
        // a tapped tempo takes over from the host tempo
        if (auto* s = apvts.getParameter (ID::sync))
            s->setValueNotifyingHost (0.0f);
    }
}

//==============================================================================
juce::ValueTree YokokokoAudioProcessor::takeSnapshot() const
{
    juce::ValueTree t ("SLOT");
    for (auto& id : kSnapshotIds)
        t.setProperty (id, apvts.getParameter (id)->getValue(), nullptr);
    return t;
}

void YokokokoAudioProcessor::applySnapshot (const juce::ValueTree& t)
{
    const juce::ScopedValueSetter<bool> svs (applyingSnapshot, true);
    for (auto& id : kSnapshotIds)
        if (t.hasProperty (id))
            if (auto* p = apvts.getParameter (id))
                p->setValueNotifyingHost ((float) t[id]);
}

void YokokokoAudioProcessor::saveToCurrentSlot()
{
    const int mem = (int) raw (apvts, ID::memory);
    if (mem == 1) return;
    slots[(size_t) (mem == 0 ? 0 : 1)] = takeSnapshot();
}

void YokokokoAudioProcessor::parameterChanged (const juce::String& id, float newValue)
{
    if (id == ID::memory && ! applyingSnapshot)
    {
        const int mem = juce::roundToInt (newValue);
        if (mem != 1)
        {
            pendingRecall.store (mem == 0 ? 0 : 1);
            triggerAsyncUpdate();
        }
    }
}

void YokokokoAudioProcessor::handleAsyncUpdate()
{
    const int s = pendingRecall.exchange (-1);
    if (s >= 0 && slots[(size_t) s].isValid())
        applySnapshot (slots[(size_t) s]);
}

//==============================================================================
void YokokokoAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("SLOTS"), nullptr);
    juce::ValueTree s ("SLOTS");
    for (size_t i = 0; i < slots.size(); ++i)
    {
        juce::ValueTree c ("S" + juce::String ((int) i));
        if (slots[i].isValid())
            c.appendChild (slots[i].createCopy(), nullptr);
        s.appendChild (c, nullptr);
    }
    state.appendChild (s, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void YokokokoAudioProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType (apvts.state.getType())) return;

    if (auto s = state.getChildWithName ("SLOTS"); s.isValid())
    {
        for (size_t i = 0; i < slots.size(); ++i)
        {
            auto c = s.getChildWithName ("S" + juce::String ((int) i));
            slots[i] = c.isValid() && c.getNumChildren() > 0 ? c.getChild (0).createCopy() : juce::ValueTree();
        }
        state.removeChild (s, nullptr);
    }
    const juce::ScopedValueSetter<bool> svs (applyingSnapshot, true);
    apvts.replaceState (state);
}

//==============================================================================
juce::AudioProcessorEditor* YokokokoAudioProcessor::createEditor()
{
    return new YokokokoAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new YokokokoAudioProcessor();
}
