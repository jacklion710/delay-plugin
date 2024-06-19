#pragma once

#include <JuceHeader.h>

class RotaryKnob  : public juce::Component
{
public:
    RotaryKnob(const juce::String& text, juce::AudioProcessorValueTreeState& apvts, const juce::ParameterID& parameterID);
    ~RotaryKnob() override;

    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    
    juce::Label label;
    
    void resized() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RotaryKnob)
};
