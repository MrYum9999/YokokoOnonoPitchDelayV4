#pragma once

#include "PluginProcessor.h"

namespace yoko::ui
{
const juce::Colour paper { 0xffffffff }, ink { 0xff111111 }, grey { 0xff9a9a9a }, faint { 0xffdedede };
juce::Font type (float height, bool bold = false, bool italic = false);

//==============================================================================
class Look : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

//==============================================================================
// Black circle knob with a literal label and its value written underneath.
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId);
    void resized() override;
    void paint (juce::Graphics&) override;

    juce::String label, valueText;
    bool detents = false;
    juce::Slider slider;

private:
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

//==============================================================================
// A three-way switch written as three words. The chosen word is black.
class WordSwitch : public juce::Component
{
public:
    WordSwitch (juce::RangedAudioParameter&, juce::StringArray words);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setWords (juce::StringArray w) { if (w != words) { words = w; repaint(); } }

private:
    juce::StringArray words;
    int index = 0;
    juce::ParameterAttachment attachment;
};

//==============================================================================
// An on/off word on the front: a small circle (filled = on), a name and a grey line.
class Toggle : public juce::Component
{
public:
    Toggle (juce::RangedAudioParameter&, juce::String name);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    juce::String sub;
    bool dim = false;

private:
    juce::String name;
    bool state = false;
    juce::ParameterAttachment attachment;
};

//==============================================================================
// A choice written as words in a row. The chosen word is black with a bar under it.
class RowSwitch : public juce::Component
{
public:
    RowSwitch (juce::RangedAudioParameter&, juce::StringArray words);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool centred = false;

private:
    juce::StringArray words;
    int index = 0;
    juce::ParameterAttachment attachment;
};

//==============================================================================
// Time signature, written large: drag the top number, click the bottom one.
class MeterReadout : public juce::Component
{
public:
    explicit MeterReadout (YokokokoAudioProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    YokokokoAudioProcessor& proc;
    int startNum = 7;
    bool dragging = false;
};

// The bar split into its beat groups. The group playing now is black. Click to regroup.
class GroupDisplay : public juce::Component
{
public:
    explicit GroupDisplay (YokokokoAudioProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    YokokokoAudioProcessor& proc;
};

//==============================================================================
// Rear-panel DIP switch: a small slot, a name and one plain sentence.
class Dip : public juce::Component
{
public:
    Dip (juce::RangedAudioParameter&, juce::String name, juce::String sentence);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::String name, sentence;
    bool state = false;
    juce::ParameterAttachment attachment;
};

//==============================================================================
// A black circle you press: "tap" and "on".
class Circle : public juce::Component
{
public:
    explicit Circle (juce::String word) : word (std::move (word)) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { pressed = true; repaint(); if (onDown) onDown(); }
    void mouseUp (const juce::MouseEvent&) override   { pressed = false; repaint(); if (onUp) onUp(); }

    std::function<void()> onDown, onUp;
    bool lit = false, ringStyle = false;
    juce::String word, caption;

private:
    bool pressed = false;
};

//==============================================================================
// A number you can drag up and down.
class TimeReadout : public juce::Component
{
public:
    explicit TimeReadout (juce::RangedAudioParameter&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    juce::String text, sub;
    std::function<void()> onGrab;

private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    float startNorm = 0;
};

//==============================================================================
class TextLink : public juce::Component
{
public:
    explicit TextLink (juce::String t) : text (std::move (t)) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { flash = 8; if (onClick) onClick(); }
    std::function<void()> onClick;
    juce::String text;
    int flash = 0;
    juce::Justification just = juce::Justification::centredLeft;
};
} // namespace yoko::ui

//==============================================================================
class YokokokoAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit YokokokoAudioProcessorEditor (YokokokoAudioProcessor&);
    ~YokokokoAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    void showBack (bool back);
    void refreshLabels();

private:
    void timerCallback() override;

    YokokokoAudioProcessor& proc;
    yoko::ui::Look look;

    yoko::ui::Knob mix, lpf, regen, glide, int1, int2;
    yoko::ui::WordSwitch len1, len2, len3, memory;
    yoko::ui::TextLink save { "save here" }, turnOver { "turn it over" }, turnBack { "turn it back" };
    yoko::ui::TimeReadout time;
    yoko::ui::Circle tapCircle { "tap" }, onCircle { "on" };
    juce::OwnedArray<yoko::ui::Dip> dips;

    // v3: random notes, key, preset files
    yoko::ui::Toggle randomToggle, weirdToggle;
    yoko::ui::RowSwitch keyRoot, keyScale;
    yoko::ui::TextLink savePreset { "save to file" }, openPreset { "open file" };
    std::unique_ptr<juce::FileChooser> chooser;
    yoko::ui::MeterReadout meter;
    yoko::ui::GroupDisplay groupDisplay;

    // v4: reverse
    yoko::ui::RowSwitch reverseSwitch;

    bool back = false;
    juce::Point<float> mouse { -100, -100 };
    juce::Rectangle<float> yesSpot;
    juce::String stepHeads[3], stepSubs[3];
    int litStep = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YokokokoAudioProcessorEditor)
};
