#pragma once

#include <JuceHeader.h>

const juce::ParameterID gainParamID { "gain", 1 };
const juce::ParameterID delayTimeParamID { "delayTime", 1 };
const juce::ParameterID mixParamID { "mix", 1 };
const juce::ParameterID feedbackParamID { "feedback", 1 };
const juce::ParameterID stereoParamID { "stereo", 1 };
const juce::ParameterID lowCutParamID { "lowCut", 1 };
const juce::ParameterID highCutParamID { "highCut", 1 };
const juce::ParameterID tempoSyncParamID { "tempoSync", 1 };
const juce::ParameterID delayNoteParamID { "delayNote", 1 };

class Parameters
{
public:
    Parameters(juce::AudioProcessorValueTreeState& apvts);
    
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    void prepareToPlay(double sampleRate) noexcept;
    void reset() noexcept;
    void update() noexcept;
    void smoothen() noexcept;

    float gain = 0.0f;
    float delayTime = 0.0f;
    float mix = 1.0f;
    float feedback = 0.0f;
    float panL = 0.0f;
    float panR = 1.0f;
    float lowCut = 20.0f;
    float highCut = 20000.0f;
    int delayNote = 0;
    bool tempoSync = false;
    static constexpr float minDelayTime = 5.0f;
    static constexpr float maxDelayTime = 5000.0f;

    juce::AudioParameterBool* tempoSyncParam;
        
private:
    juce::AudioParameterFloat* gainParam;
    juce::AudioParameterFloat* delayTimeParam;
    juce::AudioParameterFloat* mixParam;
    juce::AudioParameterFloat* feedbackParam;
    juce::AudioParameterFloat* stereoParam;
    juce::AudioParameterFloat* lowCutParam;
    juce::AudioParameterFloat* highCutParam;
    juce::AudioParameterChoice* delayNoteParam;
    
    juce::LinearSmoothedValue<float> gainSmoother;
    juce::LinearSmoothedValue<float> mixSmoother;
    juce::LinearSmoothedValue<float> feedbackSmoother;
    juce::LinearSmoothedValue<float> stereoSmoother;
    juce::LinearSmoothedValue<float> lowCutSmoother;
    juce::LinearSmoothedValue<float> highCutSmoother;
    
    float targetDelayTime = 0.0f;
    float coeff = 0.0f; // one-pole smoothing
};
