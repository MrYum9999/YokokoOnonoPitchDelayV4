#include "PluginEditor.h"
#include "BinaryData.h"

using namespace yoko;
using namespace yoko::ui;

namespace
{
constexpr int W = 800, H = 628;   // v4: taller, for the reverse row
constexpr int D = 56;   // v3: everything below the knobs moved down to make room for the key row
}

//==============================================================================
juce::Font yoko::ui::type (float height, bool bold, bool italic)
{
    static auto regular = juce::Typeface::createSystemTypefaceFor (BinaryData::CourierPrimeRegular_ttf, BinaryData::CourierPrimeRegular_ttfSize);
    static auto heavy   = juce::Typeface::createSystemTypefaceFor (BinaryData::CourierPrimeBold_ttf,    BinaryData::CourierPrimeBold_ttfSize);
    static auto slant   = juce::Typeface::createSystemTypefaceFor (BinaryData::CourierPrimeItalic_ttf,  BinaryData::CourierPrimeItalic_ttfSize);
    return juce::Font (juce::FontOptions (bold ? heavy : italic ? slant : regular).withHeight (height));
}

//==============================================================================
void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                             float startAngle, float endAngle, juce::Slider& s)
{
    const auto area = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float r = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 10.0f;
    const auto c = area.getCentre();

    // marks around the knob: 11 detents for the interval knobs, otherwise just the two ends
    const int detents = (int) s.getProperties().getWithDefault ("detents", 0);
    const int marks = detents > 0 ? detents : 2;
    for (int i = 0; i < marks; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / (float) (marks - 1);
        const bool centre = detents > 0 && i == detents / 2;
        const float d = centre ? 3.2f : 2.0f;
        const auto p = c.getPointOnCircumference (r + 7.0f, a);
        g.setColour (ink);
        g.fillEllipse (p.x - d * 0.5f, p.y - d * 0.5f, d, d);
    }

    g.setColour (ink);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);

    const float a = startAngle + pos * (endAngle - startAngle);
    g.setColour (paper);
    g.drawLine (juce::Line<float> (c.getPointOnCircumference (r * 0.28f, a), c.getPointOnCircumference (r * 0.86f, a)), 2.2f);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id)
    : attachment (s, id, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setMouseDragSensitivity (220);
    slider.setDoubleClickReturnValue (true, s.getParameter (id)->convertFrom0to1 (s.getParameter (id)->getDefaultValue()));
    slider.setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    addAndMakeVisible (slider);
}

void Knob::resized()
{
    slider.setBounds (getLocalBounds().removeFromTop (getWidth()));
}

void Knob::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().withTrimmedTop (getWidth() + 2);
    g.setColour (ink);
    g.setFont (type (15.0f, true));
    g.drawText (label, b.removeFromTop (20), juce::Justification::centred);
    g.setColour (grey);
    g.setFont (type (12.5f));
    g.drawText (valueText, b.removeFromTop (18), juce::Justification::centred);
}

//==============================================================================
WordSwitch::WordSwitch (juce::RangedAudioParameter& p, juce::StringArray w)
    : words (std::move (w)),
      attachment (p, [this] (float v) { index = juce::roundToInt (v); repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void WordSwitch::paint (juce::Graphics& g)
{
    const float rowH = (float) getHeight() / (float) words.size();
    for (int i = 0; i < words.size(); ++i)
    {
        auto row = juce::Rectangle<float> (0, rowH * (float) i, (float) getWidth(), rowH);
        const bool chosen = i == index;
        g.setColour (chosen ? ink : grey);
        g.setFont (type (13.5f, chosen));
        if (chosen)
            g.fillRect (row.getX(), row.getCentreY() - 1.0f, 6.0f, 2.0f);
        g.drawText (words[i], row.withTrimmedLeft (12), juce::Justification::centredLeft);
    }
}

void WordSwitch::mouseDown (const juce::MouseEvent& e)
{
    const int i = juce::jlimit (0, words.size() - 1, (int) (e.position.y / ((float) getHeight() / (float) words.size())));
    attachment.setValueAsCompleteGesture ((float) i);
}

//==============================================================================
Toggle::Toggle (juce::RangedAudioParameter& p, juce::String n)
    : name (std::move (n)),
      attachment (p, [this] (float v) { state = v > 0.5f; repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void Toggle::paint (juce::Graphics& g)
{
    g.setColour (dim ? grey : ink);
    if (state) g.fillEllipse (0.0f, 7.0f, 9.0f, 9.0f);
    else       g.drawEllipse (0.6f, 7.6f, 7.8f, 7.8f, 1.2f);
    g.setFont (type (14.5f, true));
    g.drawText (name, 16, 0, getWidth() - 16, 20, juce::Justification::centredLeft);
    g.setColour (grey);
    g.setFont (type (12.0f));
    g.drawText (sub, 16, 18, getWidth() - 16, 16, juce::Justification::centredLeft);
}

void Toggle::mouseDown (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (state ? 0.0f : 1.0f);
}

//==============================================================================
RowSwitch::RowSwitch (juce::RangedAudioParameter& p, juce::StringArray w)
    : words (std::move (w)),
      attachment (p, [this] (float v) { index = juce::roundToInt (v); repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void RowSwitch::paint (juce::Graphics& g)
{
    const float cell = (float) getWidth() / (float) words.size();
    for (int i = 0; i < words.size(); ++i)
    {
        auto c = juce::Rectangle<float> (cell * (float) i, 0, cell, (float) getHeight());
        const bool chosen = i == index;
        g.setColour (chosen ? ink : grey);
        g.setFont (type (13.0f, chosen));
        g.drawText (words[i], c.withTrimmedBottom (4), centred ? juce::Justification::centred : juce::Justification::centredLeft);
        if (chosen)
        {
            const float tw = juce::GlyphArrangement::getStringWidth (type (13.0f, true), words[i]);
            g.fillRect (centred ? c.getCentreX() - tw * 0.5f : c.getX(), c.getBottom() - 3.0f, tw, 2.0f);
        }
    }
}

void RowSwitch::mouseDown (const juce::MouseEvent& e)
{
    const int i = juce::jlimit (0, words.size() - 1, (int) (e.position.x / ((float) getWidth() / (float) words.size())));
    attachment.setValueAsCompleteGesture ((float) i);
}

//==============================================================================
namespace
{
void setParam (juce::RangedAudioParameter* p, float realValue)
{
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (realValue));
    p->endChangeGesture();
}
}

MeterReadout::MeterReadout (YokokokoAudioProcessor& p) : proc (p)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void MeterReadout::paint (juce::Graphics& g)
{
    const bool song = proc.meterFromSong.load();
    g.setColour (ink);
    g.setFont (type (14.5f, true));
    g.drawText ("time signature", 0, 0, getWidth(), 20, juce::Justification::centredLeft);
    g.setFont (type (24.0f, true));
    g.drawText (juce::String (proc.meterNumNow.load()) + "/" + juce::String (proc.meterDenNow.load()),
                0, 20, getWidth(), 28, juce::Justification::centredLeft);
    g.setColour (grey);
    g.setFont (type (12.0f));
    g.drawText (song ? "from the song" : "drag " + juce::String (proc.meterNumNow.load()) + ", click " + juce::String (proc.meterDenNow.load()), 0, 48, getWidth(), 16, juce::Justification::centredLeft);
}

void MeterReadout::mouseDown (const juce::MouseEvent& e)
{
    dragging = false;
    if (proc.meterFromSong.load()) return;
    startNum = (int) proc.apvts.getRawParameterValue (ID::meterNum)->load();
    // the bottom number sits to the right of the slash
    const float slashX = juce::GlyphArrangement::getStringWidth (type (24.0f, true), juce::String (proc.meterNumNow.load()));
    if (e.position.x > slashX + 8.0f && e.position.y > 18.0f && e.position.y < 50.0f)
    {
        auto* d = proc.apvts.getParameter (ID::meterDen);
        const int next = ((int) proc.apvts.getRawParameterValue (ID::meterDen)->load() + 1) % 3;
        setParam (d, (float) next);
        repaint();
        return;
    }
    dragging = true;
    proc.apvts.getParameter (ID::meterNum)->beginChangeGesture();
}

void MeterReadout::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging) return;
    auto* p = proc.apvts.getParameter (ID::meterNum);
    const int n = juce::jlimit (2, kMaxNumerator, startNum - e.getDistanceFromDragStartY() / 12);
    p->setValueNotifyingHost (p->convertTo0to1 ((float) n));
    // a new numerator starts from its first grouping
    auto* gp = proc.apvts.getParameter (ID::grouping);
    if ((int) proc.apvts.getRawParameterValue (ID::grouping)->load() != 0) setParam (gp, 0.0f);
    repaint();
}

void MeterReadout::mouseUp (const juce::MouseEvent&)
{
    if (dragging) proc.apvts.getParameter (ID::meterNum)->endChangeGesture();
    dragging = false;
}

void GroupDisplay::paint (juce::Graphics& g)
{
    const int num = proc.meterNumNow.load();
    const auto& gr = groupingFor (num, (int) proc.apvts.getRawParameterValue (ID::grouping)->load());
    const int now = proc.groupNow.load(), beat = proc.beatNow.load();

    g.setColour (ink);
    g.setFont (type (14.5f, true));
    g.drawText ("beat groups", 0, 0, getWidth(), 20, juce::Justification::centredLeft);

    // the beats as dots, a gap between groups, the playing beat filled
    float x = 0.0f;
    int b = 0;
    const float dot = 7.0f, gap = num > 11 ? 9.0f : 11.0f, groupGap = 9.0f;
    for (int i = 0; i < gr.count; ++i)
    {
        for (int k = 0; k < gr.sizes[i]; ++k, ++b)
        {
            g.setColour (i == now ? ink : grey);
            if (b == beat) g.fillEllipse (x, 28.0f, dot, dot);
            else           g.drawEllipse (x + 0.6f, 28.6f, dot - 1.2f, dot - 1.2f, 1.2f);
            x += gap;
        }
        x += groupGap;
    }

    g.setColour (grey);
    g.setFont (type (12.0f));
    g.drawText (groupingText (gr) + ".  click to regroup", 0, 48, getWidth(), 16, juce::Justification::centredLeft);
}

void GroupDisplay::mouseDown (const juce::MouseEvent&)
{
    const int count = groupingsFor (proc.meterNumNow.load()).count;
    const int next = ((int) proc.apvts.getRawParameterValue (ID::grouping)->load() + 1) % juce::jmax (1, count);
    setParam (proc.apvts.getParameter (ID::grouping), (float) next);
    repaint();
}

//==============================================================================
Dip::Dip (juce::RangedAudioParameter& p, juce::String n, juce::String s)
    : name (std::move (n)), sentence (std::move (s)),
      attachment (p, [this] (float v) { state = v > 0.5f; repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void Dip::paint (juce::Graphics& g)
{
    // the switch: a slot with a black block that sits up (on) or down (off)
    auto sw = juce::Rectangle<float> (2, 4, 16, 30);
    g.setColour (ink);
    g.drawRect (sw, 1.2f);
    auto block = sw.reduced (3.0f).withHeight (11.0f);
    if (! state) block.setY (sw.getBottom() - 3.0f - 11.0f);
    g.fillRect (block);

    auto text = getLocalBounds().toFloat().withTrimmedLeft (32);
    g.setFont (type (15.0f, true));
    g.drawText (name + (state ? "  (on)" : "  (off)"), text.removeFromTop (20), juce::Justification::centredLeft);
    g.setColour (grey);
    g.setFont (type (12.5f));
    g.drawFittedText (sentence, text.toNearestInt(), juce::Justification::topLeft, 3, 1.0f);
}

void Dip::mouseDown (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (state ? 0.0f : 1.0f);
}

//==============================================================================
void Circle::paint (juce::Graphics& g)
{
    const float d = juce::jmin ((float) getWidth() - 4.0f, 52.0f);
    auto c = juce::Rectangle<float> (((float) getWidth() - d) * 0.5f, 2, d, d);
    g.setColour (ink);
    if (pressed) c = c.reduced (2.0f);
    g.fillEllipse (c);

    // the light: a small white dot in the middle when lit
    if (lit)
    {
        g.setColour (paper);
        g.fillEllipse (c.getCentreX() - 4.0f, c.getCentreY() - 4.0f, 8.0f, 8.0f);
    }

    g.setColour (ink);
    g.setFont (type (15.0f, true));
    auto below = getLocalBounds().withTrimmedTop ((int) d + 6);
    g.drawText (word, below.removeFromTop (20), juce::Justification::centred);
    g.setColour (grey);
    g.setFont (type (12.0f));
    g.drawText (caption, below.removeFromTop (16), juce::Justification::centred);
}

//==============================================================================
TimeReadout::TimeReadout (juce::RangedAudioParameter& p)
    : param (p), attachment (p, [this] (float) { repaint(); })
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void TimeReadout::paint (juce::Graphics& g)
{
    auto b = getLocalBounds();
    g.setColour (ink);
    g.setFont (type (26.0f, true));
    g.drawText (text, b.removeFromTop (34), juce::Justification::centredLeft);
    g.setColour (grey);
    g.setFont (type (12.0f));
    g.drawFittedText (sub, b, juce::Justification::topLeft, 3, 1.0f);
}

void TimeReadout::mouseDown (const juce::MouseEvent&)
{
    if (onGrab) onGrab();
    startNorm = param.getValue();
    attachment.beginGesture();
}

void TimeReadout::mouseDrag (const juce::MouseEvent& e)
{
    const float norm = juce::jlimit (0.0f, 1.0f, startNorm - (float) e.getDistanceFromDragStartY() / 300.0f);
    attachment.setValueAsPartOfGesture (param.convertFrom0to1 (norm));
}

void TimeReadout::mouseUp (const juce::MouseEvent&)
{
    attachment.endGesture();
}

//==============================================================================
void TextLink::paint (juce::Graphics& g)
{
    g.setColour (flash > 0 ? grey : ink);
    g.setFont (type (13.5f));
    g.drawText (text, getLocalBounds(), just);
    const float tw = juce::GlyphArrangement::getStringWidth (type (13.5f), text);
    const float x = just == juce::Justification::centredRight ? (float) getWidth() - tw : 0.0f;
    g.fillRect (x, (float) getHeight() * 0.5f + 8.0f, tw, 1.0f);
}

//==============================================================================
YokokokoAudioProcessorEditor::YokokokoAudioProcessorEditor (YokokokoAudioProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      mix (p.apvts, ID::mix), lpf (p.apvts, ID::lpf), regen (p.apvts, ID::regen),
      glide (p.apvts, ID::glide), int1 (p.apvts, ID::int1), int2 (p.apvts, ID::int2),
      len1 (*p.apvts.getParameter (ID::len1), { "quarter", "dotted eighth", "eighth" }),
      len2 (*p.apvts.getParameter (ID::len2), { "quarter", "dotted eighth", "eighth" }),
      len3 (*p.apvts.getParameter (ID::len3), { "quarter", "dotted eighth", "eighth" }),
      memory (*p.apvts.getParameter (ID::memory), { "one", "now", "two" }),
      time (*p.apvts.getParameter (ID::time)),
      randomToggle (*p.apvts.getParameter (ID::random), "random notes"),
      weirdToggle (*p.apvts.getParameter (ID::weird), "make it weird"),
      keyRoot (*p.apvts.getParameter (ID::keyRoot), noteNames()),
      keyScale (*p.apvts.getParameter (ID::keyScale), { "major", "minor" }),
      meter (p), groupDisplay (p),
      reverseSwitch (*p.apvts.getParameter (ID::reverse), { "off", "everything", "echoes", "every other echo" })
{
    addAndMakeVisible (reverseSwitch);
    addChildComponent (meter);
    addChildComponent (groupDisplay);
    setLookAndFeel (&look);

    for (auto* k : { &mix, &lpf, &regen, &glide, &int1, &int2 })
        addAndMakeVisible (k);
    for (auto* k : { &int1, &int2 })
        k->slider.getProperties().set ("detents", kNumIntervals);

    for (auto* c : { (juce::Component*) &len1, (juce::Component*) &len2, (juce::Component*) &len3,
                     (juce::Component*) &memory, (juce::Component*) &save, (juce::Component*) &time,
                     (juce::Component*) &tapCircle, (juce::Component*) &onCircle, (juce::Component*) &turnOver })
        addAndMakeVisible (c);
    addChildComponent (turnBack);
    for (auto* c : { (juce::Component*) &randomToggle, (juce::Component*) &weirdToggle, (juce::Component*) &keyRoot,
                     (juce::Component*) &keyScale, (juce::Component*) &savePreset, (juce::Component*) &openPreset })
        addAndMakeVisible (c);

    savePreset.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Save a preset",
                     proc.getPresetFolder().getChildFile (proc.getPresetName() + ".yokoko"), "*.yokoko");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult() != juce::File())
                                      proc.savePresetFile (fc.getResult());
                                  repaint();
                              });
    };
    openPreset.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Open a preset", proc.getPresetFolder(), "*.yokoko");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult().existsAsFile())
                                      proc.loadPresetFile (fc.getResult());
                                  repaint();
                              });
    };
    turnOver.just = turnBack.just = juce::Justification::centredRight;

    save.onClick = [this]
    {
        proc.saveToCurrentSlot();
        repaint();
    };

    tapCircle.onDown = [this] { proc.tap(); };
    time.onGrab = [this]
    {
        // dragging the time means you want your own tempo, not the song's
        if (auto* s = proc.apvts.getParameter (ID::sync); s->getValue() > 0.5f)
        {
            s->beginChangeGesture(); s->setValueNotifyingHost (0.0f); s->endChangeGesture();
        }
    };

    auto* onParam = p.apvts.getParameter (ID::on);
    onCircle.onDown = [this, onParam]
    {
        const bool momentary = proc.apvts.getRawParameterValue (ID::momentary)->load() > 0.5f;
        onParam->beginChangeGesture();
        onParam->setValueNotifyingHost (momentary ? 1.0f : (onParam->getValue() > 0.5f ? 0.0f : 1.0f));
        onParam->endChangeGesture();
    };
    onCircle.onUp = [this, onParam]
    {
        if (proc.apvts.getRawParameterValue (ID::momentary)->load() > 0.5f)
        {
            onParam->beginChangeGesture();
            onParam->setValueNotifyingHost (0.0f);
            onParam->endChangeGesture();
        }
    };

    turnOver.onClick = [this] { showBack (true); };
    turnBack.onClick = [this] { showBack (false); };

    struct D { const juce::String& id; const char* name; const char* sentence; };
    const D defs[] = {
        { ID::sync,       "follow the song",   "Use the host's tempo. Tapping turns this off." },
        { ID::modulation, "modulation",        "Interval 1 and 2 become speed and depth. The step switches become shape, randomness and what moves." },
        { ID::step,       "step",              "The sequence stands still. Tap, or play a MIDI note, to move to the next step." },
        { ID::slowdown,   "slowdown",          "Half the clock. Twice the time. Less of everything else." },
        { ID::rampLpf,    "ramp the filter",   "The filter rises and falls by itself, up to where you set it. Mix becomes ramp speed." },
        { ID::rampRegen,  "ramp the feedback", "The feedback rises and falls by itself, up to where you set it. Mix becomes ramp speed." },
        { ID::rampGlide,  "ramp the glide",    "The glide rises and falls by itself, up to where you set it. Mix becomes ramp speed." },
        { ID::trails,     "trails",            "When you switch it off, the repeats keep falling. Off: everything stops at once." },
        { ID::momentary,  "momentary",         "On only while you hold the circle down." },
        { ID::bars,       "count bars",        "Steps follow the beat groups of the bar, from beat 1. The step switches become the time signature." },
    };
    for (auto& d : defs)
        addChildComponent (dips.add (new Dip (*p.apvts.getParameter (d.id), d.name, d.sentence)));

    setSize (W, H);
    refreshLabels();
    startTimerHz (30);
}

YokokokoAudioProcessorEditor::~YokokokoAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void YokokokoAudioProcessorEditor::showBack (bool b)
{
    back = b;
    for (auto* c : { (juce::Component*) &mix, (juce::Component*) &lpf, (juce::Component*) &regen,
                     (juce::Component*) &glide, (juce::Component*) &int1, (juce::Component*) &int2,
                     (juce::Component*) &len1, (juce::Component*) &len2, (juce::Component*) &len3,
                     (juce::Component*) &memory, (juce::Component*) &save, (juce::Component*) &time,
                     (juce::Component*) &tapCircle, (juce::Component*) &onCircle, (juce::Component*) &turnOver,
                     (juce::Component*) &randomToggle, (juce::Component*) &weirdToggle, (juce::Component*) &keyRoot,
                     (juce::Component*) &keyScale, (juce::Component*) &savePreset, (juce::Component*) &openPreset,
                     (juce::Component*) &reverseSwitch })
        c->setVisible (! back);
    for (auto* d : dips)
        d->setVisible (back);
    turnBack.setVisible (back);
    refreshLabels();
    repaint();
}

//==============================================================================
void YokokokoAudioProcessorEditor::resized()
{
    const int knobW = 96, knobH = knobW + 44;
    for (int i = 0; i < 6; ++i)
    {
        auto* k = std::array<ui::Knob*, 6> { &mix, &lpf, &regen, &glide, &int1, &int2 }[(size_t) i];
        k->setBounds (100 + i * 120 - knobW / 2, 168, knobW, knobH);
    }

    // sequence steps: under the first three knobs
    len1.setBounds (52,  374 + D, 130, 66);
    len2.setBounds (172, 374 + D, 130, 66);
    len3.setBounds (292, 374 + D, 130, 66);

    memory.setBounds (430, 374 + D, 80, 66);
    save.setBounds (442, 444 + D, 90, 20);

    time.setBounds (540, 340 + D, 104, 110);
    tapCircle.setBounds (640, 352 + D, 60, 100);
    onCircle.setBounds (704, 352 + D, 60, 100);

    // count bars: these take the place of the step switches
    meter.setBounds (52, 374 + D, 140, 66);
    groupDisplay.setBounds (212, 374 + D, 220, 66);

    // v3 key row, between the knobs and the sequence
    randomToggle.setBounds (52, 336, 220, 36);
    weirdToggle.setBounds (272, 336, 180, 36);
    keyScale.setBounds (510, 334, 130, 22);
    keyRoot.setBounds (462, 352, 300, 22);
    keyRoot.centred = true;

    // v4 reverse row, along the bottom
    reverseSwitch.setBounds (188, 548, 572, 24);

    // v3 preset line, under the name
    savePreset.setBounds (296, 112, 100, 20);
    openPreset.setBounds (408, 112, 84, 20);

    turnOver.setBounds (W - 40 - 160, H - 34, 160, 22);
    turnBack.setBounds (W - 40 - 160, H - 34, 160, 22);

    for (int i = 0; i < dips.size(); ++i)
    {
        const int col = i % 2, row = i / 2;
        dips[i]->setBounds (40 + col * 370, 132 + row * 66, 350, 60);
    }

    yesSpot = { W - 52.0f, 30.0f, 12.0f, 8.0f };
}

//==============================================================================
void YokokokoAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (paper);

    // name
    g.setColour (ink);
    g.setFont (type (40.0f, true).withExtraKerningFactor (0.18f));
    g.drawText ("YOKOKO ONONO", 40, 34, 460, 46, juce::Justification::centredLeft);
    g.setFont (type (17.0f).withExtraKerningFactor (0.12f));
    g.drawText ("pitch delay v4", 42, 80, 460, 22, juce::Justification::centredLeft);

    if (back)
    {
        g.setFont (type (15.0f, true));
        g.drawText ("THE BACK", W - 40 - 300, 40, 300, 20, juce::Justification::centredRight);
        g.setColour (grey);
        g.setFont (type (12.5f));
        g.drawText ("switches you set once and forget.", W - 40 - 340, 60, 340, 18, juce::Justification::centredRight);
        g.setColour (ink);
        g.fillRect (40.0f, 116.0f, (float) W - 80.0f, 1.0f);
        g.setColour (grey);
        g.setFont (type (12.0f, false, true));
        g.drawText ("2026 autumn", 40, H - 34, 200, 22, juce::Justification::centredLeft);
        return;
    }

    // an instruction, typed on a card
    {
        auto card = juce::Rectangle<float> (W - 40 - 250, 26, 250, 116);
        g.setColour (ink);
        g.setFont (type (13.0f, true));
        g.drawText ("PITCH PIECE", card.removeFromTop (20).toNearestInt(), juce::Justification::centredLeft);
        g.setFont (type (13.0f));
        const char* lines[] = { "Play one note into it.", "Wait for it to come back", "as a different note.",
                                "Listen to the note", "you did not play." };
        for (auto* l : lines)
            g.drawText (l, card.removeFromTop (16).toNearestInt(), juce::Justification::centredLeft);
        g.setColour (grey);
        g.setFont (type (12.0f, false, true));
        g.drawText ("2026 autumn", card.removeFromTop (18).toNearestInt(), juce::Justification::centredLeft);
    }

    // a very small word, almost not there
    g.setColour (juce::Colour (0xffcfcfcf));
    g.setFont (type (5.5f));
    g.drawText ("yes", yesSpot, juce::Justification::centred);

    g.setColour (ink);
    g.fillRect (40.0f, 150.0f, (float) W - 80.0f, 1.0f);
    g.fillRect (40.0f, 324.0f, (float) W - 80.0f, 1.0f);
    g.fillRect (40.0f, 324.0f + D, (float) W - 80.0f, 1.0f);
    g.fillRect (40.0f, 534.0f, (float) W - 80.0f, 1.0f);

    // v4 reverse row heading
    {
        const int mode = (int) proc.apvts.getRawParameterValue (ID::reverse)->load();
        g.setColour (ink);
        g.setFont (type (14.5f, true));
        g.drawText ("reverse", 42, 546, 140, 20, juce::Justification::centredLeft);
        g.setColour (grey);
        g.setFont (type (12.0f));
        const juce::String sub = mode == 0 ? "forwards"
                               : "pieces of " + juce::String (juce::roundToInt (proc.reversePieceMs.load())) + " ms";
        g.drawText (sub, 42, 564, 140, 16, juce::Justification::centredLeft);
        const char* says[] = { "", "you hear only backwards: what you play and every repeat",
                               "what you play stays forwards, the repeats come back backwards",
                               "the repeats take turns: forwards, backwards, forwards" };
        if (mode > 0)
            g.drawText (says[juce::jlimit (0, 3, mode)], 188, 572, 572, 16, juce::Justification::centredLeft);
    }

    // v3 key row heading, and the preset name
    g.setColour (ink);
    g.setFont (type (14.5f, true));
    g.drawText ("key", 470, 336, 40, 20, juce::Justification::centredLeft);
    g.drawText ("preset", 42, 112, 70, 20, juce::Justification::centredLeft);
    g.setColour (grey);
    g.setFont (type (13.0f));
    g.drawText (proc.getPresetName(), 108, 112, 180, 20, juce::Justification::centredLeft, true);

    // sequence: one column per step
    const bool mod = proc.apvts.getRawParameterValue (ID::modulation)->load() > 0.5f;
    for (int i = 0; i < 3; ++i)
    {
        const float x = 52.0f + (float) i * 120.0f;
        const bool here = ! mod && i == litStep;
        if (! mod)
        {
            g.setColour (ink);
            if (here) g.fillEllipse (x, 343.0f + D, 9.0f, 9.0f);
            else      g.drawEllipse (x + 0.6f, 343.6f + D, 7.8f, 7.8f, 1.2f);
        }
        g.setColour (ink);
        g.setFont (type (14.5f, true));
        g.drawText (stepHeads[i], (int) x + (mod ? 0 : 16), 336 + D, 110, 20, juce::Justification::centredLeft);
        g.setColour (grey);
        g.setFont (type (12.0f));
        g.drawText (stepSubs[i], (int) x + (mod ? 0 : 16), 354 + D, 120, 16, juce::Justification::centredLeft);
    }

    // memory
    g.setColour (ink);
    g.setFont (type (14.5f, true));
    g.drawText ("memory", 442, 336 + D, 90, 20, juce::Justification::centredLeft);
    g.setColour (grey);
    g.setFont (type (12.0f));
    const int mem = (int) proc.apvts.getRawParameterValue (ID::memory)->load();
    const juce::String memSub = mem == 1 ? "what you see"
                              : proc.slotHasData (mem == 0 ? 0 : 1) ? "kept" : "empty";
    g.drawText (memSub, 442, 354 + D, 90, 16, juce::Justification::centredLeft);

    g.setColour (grey);
    g.setFont (type (12.0f, false, true));
    g.drawText ("drag the knobs up and down", 40, H - 34, 300, 22, juce::Justification::centredLeft);
}

void YokokokoAudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    // a magnifying glass appears near the small word
    if (back) return;
    const float dist = mouse.getDistanceFrom (yesSpot.getCentre());
    if (dist > 46.0f) return;

    const float r = 30.0f;
    const auto c = mouse;
    g.setColour (paper);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (ink);
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 2.0f);
    g.drawLine (c.x + r * 0.71f, c.y + r * 0.71f, c.x + r * 1.45f, c.y + r * 1.45f, 4.0f);

    const auto off = (yesSpot.getCentre() - c) * 6.0f;
    g.saveState();
    g.reduceClipRegion (juce::Rectangle<float> (c.x - r, c.y - r, r * 2, r * 2).toNearestInt());
    g.setFont (type (30.0f, true));
    g.setColour (ink.withAlpha (juce::jlimit (0.2f, 1.0f, 1.2f - dist / 46.0f)));
    g.drawText ("yes", juce::Rectangle<float> (0, 0, 80, 40).withCentre (c + off), juce::Justification::centred);
    g.restoreState();
}

void YokokokoAudioProcessorEditor::mouseMove (const juce::MouseEvent& e)
{
    const auto old = mouse;
    mouse = e.getEventRelativeTo (this).position;
    if (old.getDistanceFrom (yesSpot.getCentre()) < 80.0f || mouse.getDistanceFrom (yesSpot.getCentre()) < 80.0f)
        repaint (juce::Rectangle<int> (W - 200, 0, 200, 120));
}

void YokokokoAudioProcessorEditor::mouseExit (const juce::MouseEvent&)
{
    mouse = { -100, -100 };
    repaint (juce::Rectangle<int> (W - 200, 0, 200, 120));
}

//==============================================================================
void YokokokoAudioProcessorEditor::refreshLabels()
{
    auto& s = proc.apvts;
    auto txt = [&s] (const juce::String& id) { auto* p = s.getParameter (id); return p->getText (p->getValue(), 40); };
    auto rawv = [&s] (const juce::String& id) { return s.getRawParameterValue (id)->load(); };

    const bool mod  = rawv (ID::modulation) > 0.5f;
    const bool step = rawv (ID::step) > 0.5f;
    const bool ramp = rawv (ID::rampLpf) > 0.5f || rawv (ID::rampRegen) > 0.5f || rawv (ID::rampGlide) > 0.5f;

    mix.label = ramp ? "ramp speed" : "mix";
    mix.valueText = ramp ? juce::String (0.03 * std::pow (100.0, (double) rawv (ID::mix)), 2) + " Hz" : txt (ID::mix);
    lpf.label = "filter";      lpf.valueText = txt (ID::lpf);
    regen.label = "feedback";  regen.valueText = txt (ID::regen);
    glide.label = "glide";     glide.valueText = txt (ID::glide);

    if (mod)
    {
        int1.label = "speed";
        int1.valueText = juce::String (0.05 * std::pow (240.0, (double) rawv (ID::int1)), 2) + " Hz";
        int2.label = "depth";
        int2.valueText = juce::String (12.0 * rawv (ID::int2), 1) + " semitones";
        len1.setWords ({ "triangle", "sine", "square" });
        len2.setWords ({ "none", "some", "lots" });
        len3.setWords ({ "pitch", "filter", "both" });
        stepHeads[0] = "shape";  stepHeads[1] = "randomness";  stepHeads[2] = "what moves";
        stepSubs[0] = stepSubs[1] = stepSubs[2] = {};
    }
    else
    {
        int1.label = "interval 1"; int1.valueText = txt (ID::int1);
        int2.label = "interval 2"; int2.valueText = txt (ID::int2);
        for (auto* w : { &len1, &len2, &len3 })
            w->setWords ({ "quarter", "dotted eighth", "eighth" });
        stepHeads[0] = "step 1"; stepHeads[1] = "step 2"; stepHeads[2] = "step 3";
        stepSubs[0] = "same note";
        stepSubs[1] = intervalName (intervalIndex (rawv (ID::int1)));
        stepSubs[2] = intervalName (intervalIndex (rawv (ID::int2)));
        if (rawv (ID::random) > 0.5f)
        {
            int1.valueText = int2.valueText = "random";
            stepSubs[1] = semisName (proc.randomSemis1.load());
            stepSubs[2] = semisName (proc.randomSemis2.load());
        }
    }

    // count bars: the step switches become the time signature
    const bool bars = rawv (ID::bars) > 0.5f && ! mod;
    for (auto* w : { &len1, &len2, &len3 })
        w->setVisible (! back && ! bars);
    meter.setVisible (! back && bars);
    groupDisplay.setVisible (! back && bars);

    // v3 key row
    const bool randomOn = rawv (ID::random) > 0.5f;
    const int heard = proc.heardNote.load();
    randomToggle.sub = mod ? "not with modulation"
                     : ! randomOn ? "instead of interval 1 and 2"
                     : heard >= 0 ? "hearing " + noteNames()[heard % 12] : "listening";
    weirdToggle.sub = randomOn ? "odd notes, near the key" : "needs random notes";
    weirdToggle.dim = ! randomOn;
    for (auto* k : { &int1, &int2 })
        k->slider.getProperties().set ("detents", mod ? 0 : kNumIntervals);

    // time
    const bool host = rawv (ID::sync) > 0.5f && proc.followingHost.load();
    time.text = juce::String (juce::roundToInt (proc.baseDelayMs.load())) + " ms";
    if (! proc.followingHost.load() && ! (rawv (ID::sync) > 0.5f))
        time.text = juce::String (juce::roundToInt (rawv (ID::time))) + " ms";
    time.sub = (host ? "follows\nthe song" : "drag it\nor tap")
             + juce::String ("\nclock ") + juce::String (proc.currentClockHz.load() / 1000.0f, 1) + "k";

    tapCircle.caption = step ? "next step" : "the time";
    const bool isOn = rawv (ID::on) > 0.5f;
    onCircle.word = isOn ? "on" : "off";
    onCircle.lit = isOn;
    onCircle.caption = rawv (ID::momentary) > 0.5f ? "hold it" : "press it";
    litStep = proc.currentStep.load();
}

void YokokokoAudioProcessorEditor::timerCallback()
{
    refreshLabels();
    if (save.flash > 0 && --save.flash == 0) save.repaint();
    if (save.flash > 0) save.repaint();
    for (auto* c : { (juce::Component*) &mix, (juce::Component*) &lpf, (juce::Component*) &regen,
                     (juce::Component*) &glide, (juce::Component*) &int1, (juce::Component*) &int2,
                     (juce::Component*) &len1, (juce::Component*) &len2, (juce::Component*) &len3,
                     (juce::Component*) &time, (juce::Component*) &tapCircle, (juce::Component*) &onCircle })
        c->repaint();
    for (auto* c : { (juce::Component*) &randomToggle, (juce::Component*) &weirdToggle,
                     (juce::Component*) &meter, (juce::Component*) &groupDisplay })
        c->repaint();
    repaint (juce::Rectangle<int> (40, 330 + D, 500, 46));
    repaint (juce::Rectangle<int> (100, 110, 200, 24));
    repaint (juce::Rectangle<int> (40, 536, W - 80, 56));
}
